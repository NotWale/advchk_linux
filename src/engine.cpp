#include "engine.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"

// Public settings and player data

int           g_Difficulty         = 2;
bool          g_SoundEnabled       = true;
bool          g_MusicEnabled       = true;
bool          g_UseJoystick        = false;
bool          g_AlwaysRun          = false;
PlayerProfile g_PlayerSlots[kMaxPlayerSlots];
int           g_CurrentPlayerIndex = 0;

// Internal state

namespace {

constexpr int kScreenPaletteSize = 256;

SDL_Window*   g_Window        = nullptr;
SDL_Renderer* g_Renderer      = nullptr;
SDL_Texture*  g_ScreenTexture = nullptr;
SDL_Surface*  g_Screen8Bit    = nullptr;   // 640x480 8-bit palettized back buffer
SDL_Surface*  g_Screen32Bit   = nullptr;   // conversion buffer for the streaming texture

bool        g_WindowActive  = true;
bool        g_QuitRequested = false;
char        g_LastTypedChar = '\0';
std::string g_FatalErrorMessage;
int         g_CurrentState  = 0;
int         g_PreviousState = 0;

SpriteSheet g_FontBmp;
SpriteSheet g_WindowBackBmp;
SpriteSheet g_WindowCornersBmp;
SpriteSheet g_WindowSlideBmp;
SpriteSheet g_StarsBmp;

// MIDI playback state. The audio thread only ever try_locks the mutex.
struct MidiPlayer {
    std::mutex   mutex;
    tsf*         synth      = nullptr;
    tml_message* head       = nullptr;   // owned: the loaded song
    tml_message* nextEvent  = nullptr;   // next event to play (nullptr = stopped / finished)
    double       elapsedMs  = 0.0;
    bool         loop       = true;
    int          sampleRate = 22050;
};
MidiPlayer g_Midi;

uint32_t ReadLittleEndian32(const uint8_t* bytes) {
    uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof value);
    return value;
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

}

// Asset archive ("glom"): a flat blob of files, indexed by lower-case name

namespace {

constexpr size_t kExeGlomOffset = 0x4d2a0;    // where the glom starts inside egames.exe
constexpr size_t kExeGlomSize   = 2666449;

struct GlomEntry {
    uint32_t offset;
    uint32_t size;
};

struct GlomView {
    const uint8_t* data = nullptr;
    size_t         size = 0;
    explicit operator bool() const { return data != nullptr; }
};

struct GlomArchive {
    std::vector<uint8_t>                     data;
    std::unordered_map<std::string, GlomEntry> index;
};
GlomArchive g_Glom;

GlomView FindGlomEntry(const std::string& filename) {
    const auto found = g_Glom.index.find(ToLower(filename));
    if (found == g_Glom.index.end()) return {};

    const GlomEntry& entry = found->second;
    // Overflow-safe bounds check against the in-memory archive.
    if (entry.offset > g_Glom.data.size() || entry.size > g_Glom.data.size() - entry.offset) return {};

    return { g_Glom.data.data() + entry.offset, entry.size };
}

// Loads `blobSize` bytes at `baseOffset` of `path` (blobSize 0 = to end of file) and
// parses its table of contents: u32 fileCount, then per file {u32 nameLength, name,
// u32 offset, u32 size}. The global archive is only replaced on success.
bool LoadGlomFromFile(const std::string& path, size_t baseOffset, size_t blobSize) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;

    const std::streampos endPosition = file.tellg();
    if (endPosition <= 0) return false;
    const size_t fileSize = static_cast<size_t>(endPosition);

    if (baseOffset >= fileSize) return false;
    if (blobSize == 0) blobSize = fileSize - baseOffset;
    if (baseOffset + blobSize > fileSize) return false;

    GlomArchive archive;
    archive.data.resize(blobSize);
    file.seekg(static_cast<std::streamoff>(baseOffset));
    if (!file.read(reinterpret_cast<char*>(archive.data.data()), static_cast<std::streamsize>(blobSize))) {
        return false;
    }

    size_t cursor = 0;
    auto readU32 = [&](uint32_t& value) {
        if (cursor + sizeof value > archive.data.size()) return false;
        value = ReadLittleEndian32(&archive.data[cursor]);
        cursor += sizeof value;
        return true;
    };

    uint32_t fileCount = 0;
    if (!readU32(fileCount)) return false;

    for (uint32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        uint32_t nameLength = 0;
        if (!readU32(nameLength)) return false;
        if (cursor + nameLength + 2 * sizeof(uint32_t) > archive.data.size()) return false;

        const std::string name(reinterpret_cast<const char*>(&archive.data[cursor]), nameLength);
        cursor += nameLength;

        GlomEntry entry{};
        readU32(entry.offset);   // cannot fail: space was checked above
        readU32(entry.size);
        archive.index[ToLower(name)] = entry;
    }

    g_Glom = std::move(archive);
    return true;
}

}  // namespace

bool GetGlomEntryBytes(const std::string& filename, std::vector<uint8_t>& outBytes) {
    const GlomView entry = FindGlomEntry(filename);
    if (!entry) return false;
    outBytes.assign(entry.data, entry.data + entry.size);
    return true;
}

// Input (Win32 virtual-key compatibility on top of SDL)

namespace {

constexpr int kVkBackspace = 0x08;
constexpr int kVkReturn    = 0x0D;
constexpr int kVkShift     = 0x10;
constexpr int kVkControl   = 0x11;
constexpr int kVkAlt       = 0x12;
constexpr int kVkPause     = 0x13;
constexpr int kVkEscape    = 0x1B;
constexpr int kVkSpace     = 0x20;
constexpr int kVkLeft      = 0x25;
constexpr int kVkUp        = 0x26;
constexpr int kVkRight     = 0x27;
constexpr int kVkDown      = 0x28;
constexpr int kVkDelete    = 0x2E;
constexpr int kVkNumpad0   = 0x60;
constexpr int kVkNumpad9   = 0x69;
constexpr int kVkF1        = 0x70;
constexpr int kVkF12       = 0x7B;

SDL_Scancode VkToScancode(int virtualKey) {
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (virtualKey - 'A'));
    }
    if (virtualKey >= '1' && virtualKey <= '9') {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (virtualKey - '1'));
    }
    if (virtualKey == '0') return SDL_SCANCODE_0;
    if (virtualKey >= kVkF1 && virtualKey <= kVkF12) {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (virtualKey - kVkF1));
    }
    if (virtualKey >= kVkNumpad0 && virtualKey <= kVkNumpad9) {
        // SDL orders the keypad 1..9 then 0.
        return virtualKey == kVkNumpad0
            ? SDL_SCANCODE_KP_0
            : static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + (virtualKey - kVkNumpad0 - 1));
    }
    switch (virtualKey) {
        case kVkBackspace: return SDL_SCANCODE_BACKSPACE;
        case kVkReturn:    return SDL_SCANCODE_RETURN;
        case kVkPause:     return SDL_SCANCODE_PAUSE;
        case kVkEscape:    return SDL_SCANCODE_ESCAPE;
        case kVkSpace:     return SDL_SCANCODE_SPACE;
        case kVkLeft:      return SDL_SCANCODE_LEFT;
        case kVkUp:        return SDL_SCANCODE_UP;
        case kVkRight:     return SDL_SCANCODE_RIGHT;
        case kVkDown:      return SDL_SCANCODE_DOWN;
        case kVkDelete:    return SDL_SCANCODE_DELETE;
        default:           return SDL_SCANCODE_UNKNOWN;
    }
}

}  // namespace

uint32_t timeGetTime() { return SDL_GetTicks(); }

uint16_t GetAsyncKeyState(int virtualKey) {
    const uint8_t* keyboard = SDL_GetKeyboardState(nullptr);
    auto eitherDown = [&](SDL_Scancode left, SDL_Scancode right) {
        return (keyboard[left] || keyboard[right]) ? kKeyDownBit : uint16_t{0};
    };

    switch (virtualKey) {
        case kVkShift:   return eitherDown(SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT);
        case kVkControl: return eitherDown(SDL_SCANCODE_LCTRL,  SDL_SCANCODE_RCTRL);
        case kVkAlt:     return eitherDown(SDL_SCANCODE_LALT,   SDL_SCANCODE_RALT);
        default: break;
    }
    const SDL_Scancode scancode = VkToScancode(virtualKey);
    return (scancode != SDL_SCANCODE_UNKNOWN && keyboard[scancode]) ? kKeyDownBit : uint16_t{0};
}

bool WasKeyJustPressed(int virtualKey) {
    static std::unordered_map<int, bool> wasDownLastCall;
    const bool isDown      = (GetAsyncKeyState(virtualKey) & kKeyDownBit) != 0;
    const bool justPressed = isDown && !wasDownLastCall[virtualKey];
    wasDownLastCall[virtualKey] = isDown;
    return justPressed;
}

void ClearTypedChar() {
    g_LastTypedChar = '\0';
    SDL_FlushEvent(SDL_TEXTINPUT);
}

// ',' and '+' are reserved by the Chickens.ini name encoding, so they can't be typed.
char GetTypedChar() {
    const char typed = g_LastTypedChar;
    g_LastTypedChar = '\0';
    return (typed == ',' || typed == '+') ? '\0' : typed;
}

// ==============================================================================
// Software renderer: 8-bit palettized back buffer
// ==============================================================================

namespace {

constexpr int kFontFirstChar    = 0x20;
constexpr int kFontGlyphCount   = 0x80;
constexpr int kFontColumns      = 32;
constexpr int kFontGlyphAdvance = 11;

constexpr int kWindowEdgeSize   = 4;     // WindowCorners.bmp cells are 4x4
constexpr int kSliderHeight     = 24;    // original: 0x18
constexpr int kSliderKnobInset  = 8;

constexpr int kFadeSteps        = 64;
constexpr int kFadeStepAmount   = 4;     // subtracted from every channel per step

uint8_t* PixelAt(const SDL_Surface* surface, int pixelX, int pixelY) {
    return static_cast<uint8_t*>(surface->pixels) + pixelY * surface->pitch + pixelX;
}

void CopyRow(const uint8_t* source, uint8_t* destination, int pixelCount, bool transparent) {
    if (!transparent) {
        std::memcpy(destination, source, pixelCount);
        return;
    }
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        if (source[pixel] != 0) destination[pixel] = source[pixel];   // palette index 0 = transparent
    }
}

// Copies raw palette indices into the 8-bit screen, like DirectDraw's Blt did.
// No palette lookups happen, so palette tricks (flash, fade) work unchanged.
void BlitIndexed(const SDL_Surface* source, const SDL_Rect& sourceRect,
                 int dstX, int dstY, bool transparent) {
    SDL_assert(source->format->BitsPerPixel == 8);

    // Reject source rects that fall outside the sheet (a bad tile index).
    if (sourceRect.x < 0 || sourceRect.y < 0 ||
        sourceRect.x + sourceRect.w > source->w || sourceRect.y + sourceRect.h > source->h) return;

    // 1. Where the blit lands on screen, before clipping.
    const SDL_Rect wanted = { dstX, dstY, sourceRect.w, sourceRect.h };

    // 2. Clip it to the screen. Nothing visible: nothing to do.
    const SDL_Rect screen = { 0, 0, g_Screen8Bit->w, g_Screen8Bit->h };
    SDL_Rect visible;
    if (!SDL_IntersectRect(&wanted, &screen, &visible)) return;

    // 3. Shift the source rect by however much was cut off the top and left.
    const SDL_Rect clippedSource = { sourceRect.x + (visible.x - wanted.x),
                                     sourceRect.y + (visible.y - wanted.y),
                                     visible.w, visible.h };

    // 4. Copy it row by row.
    for (int row = 0; row < clippedSource.h; ++row) {
        CopyRow(PixelAt(source,      clippedSource.x, clippedSource.y + row),
                PixelAt(g_Screen8Bit, visible.x,       visible.y       + row),
                clippedSource.w, transparent);
    }
}

// Tiles one cell of a sprite sheet over a rectangle, starting at its top-left corner.
// Partial tiles on the right and bottom are clipped (original: FUN_0040e8f0).
void TileFillRect(const SpriteSheet& sheet, int left, int top, int right, int bottom,
                  int tileColumn, int tileRow, uint32_t flags) {
    const int rectWidth  = right  - left;
    const int rectHeight = bottom - top;
    for (int offsetY = 0; offsetY < rectHeight; offsetY += sheet.tileHeight) {
        for (int offsetX = 0; offsetX < rectWidth; offsetX += sheet.tileWidth) {
            const int copyWidth  = std::min(sheet.tileWidth,  rectWidth  - offsetX);
            const int copyHeight = std::min(sheet.tileHeight, rectHeight - offsetY);
            BlitSubRect(sheet, left + offsetX, top + offsetY,
                        tileColumn * sheet.tileWidth, tileRow * sheet.tileHeight,
                        copyWidth, copyHeight, flags);
        }
    }
}

uint8_t NearestPaletteIndex(const SDL_Color* palette, int targetRed, int targetGreen, int targetBlue) {
    int bestIndex    = 0;
    int bestDistance = INT_MAX;
    for (int candidate = 0; candidate < kScreenPaletteSize; ++candidate) {
        const int deltaRed   = palette[candidate].r - targetRed;
        const int deltaGreen = palette[candidate].g - targetGreen;
        const int deltaBlue  = palette[candidate].b - targetBlue;
        const int distance   = deltaRed * deltaRed + deltaGreen * deltaGreen + deltaBlue * deltaBlue;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex    = candidate;
        }
    }
    return static_cast<uint8_t>(bestIndex);
}

int WrapIntoRange(int value, int range) {
    const int wrapped = value % range;
    return wrapped < 0 ? wrapped + range : wrapped;
}

}  // namespace

SpriteSheet LoadSpriteSheet(const std::string& filename, int tileWidth, int tileHeight,
                            bool /*hasColorKey*/) {
    SpriteSheet sheet;
    sheet.filename = filename;

    const GlomView entry = FindGlomEntry(filename);
    if (!entry) {
        std::fprintf(stderr, "GLOM: Missing or corrupt BMP '%s'\n", filename.c_str());
        return sheet;
    }
    sheet.surface = SDL_LoadBMP_RW(SDL_RWFromConstMem(entry.data, static_cast<int>(entry.size)), 1);
    if (sheet.surface) {
        sheet.totalWidth  = sheet.surface->w;
        sheet.totalHeight = sheet.surface->h;
        sheet.tileWidth   = (tileWidth  == 0) ? sheet.totalWidth  : tileWidth;
        sheet.tileHeight  = (tileHeight == 0) ? sheet.totalHeight : tileHeight;
    }
    return sheet;
}

void BlitTile(const SpriteSheet& sheet, int dstX, int dstY, int tileColumn, int tileRow, uint32_t flags) {
    if (!sheet.surface || !g_Screen8Bit) return;
    const SDL_Rect sourceRect = { tileColumn * sheet.tileWidth, tileRow * sheet.tileHeight,
                                  sheet.tileWidth, sheet.tileHeight };
    BlitIndexed(sheet.surface, sourceRect, dstX, dstY, (flags & kBlitColorKey) != 0);
}

void BlitSubRect(const SpriteSheet& sheet, int dstX, int dstY, int srcX, int srcY,
                 int width, int height, uint32_t flags) {
    if (!sheet.surface || !g_Screen8Bit || width <= 0 || height <= 0) return;
    BlitIndexed(sheet.surface, SDL_Rect{ srcX, srcY, width, height }, dstX, dstY,
                (flags & kBlitColorKey) != 0);
}

void ClearScreen(uint8_t colorIndex) {
    if (g_Screen8Bit) SDL_FillRect(g_Screen8Bit, nullptr, colorIndex);
}

void FillScreenRect(int left, int top, int width, int height, uint8_t colorIndex) {
    if (!g_Screen8Bit) return;
    SDL_Rect rect = { left, top, width, height };
    SDL_FillRect(g_Screen8Bit, &rect, colorIndex);
}

void FlipScreen() {
    if (!g_Screen8Bit || !g_Screen32Bit || !g_ScreenTexture || !g_Renderer) return;
    SDL_BlitSurface(g_Screen8Bit, nullptr, g_Screen32Bit, nullptr);
    SDL_UpdateTexture(g_ScreenTexture, nullptr, g_Screen32Bit->pixels, g_Screen32Bit->pitch);
    SDL_RenderClear(g_Renderer);
    SDL_RenderCopy(g_Renderer, g_ScreenTexture, nullptr, nullptr);
    SDL_RenderPresent(g_Renderer);
}

void DrawBitmapText(int left, int top, const std::string& text, bool opaque) {
    const uint32_t flags = opaque ? kBlitOpaque : kBlitColorKey;
    int cursorX = left;
    for (const char character : text) {
        int glyph = static_cast<unsigned char>(character) - kFontFirstChar;
        if (glyph < 0 || glyph >= kFontGlyphCount) glyph = 0;
        BlitTile(g_FontBmp, cursorX, top, glyph % kFontColumns, glyph / kFontColumns, flags);
        cursorX += kFontGlyphAdvance;
    }
}

// Framed window panel (original: FUN_0040f090).
void DrawWindowBox(int left, int top, int width, int height) {
    const int right  = left + width;
    const int bottom = top  + height;

    // 1. Background texture, opaque.
    TileFillRect(g_WindowBackBmp, left, top, right, bottom, 0, 0, kBlitOpaque);

    // 2. Edges, opaque: cell 1 = top, 3 = left, 4 = right, 6 = bottom.
    TileFillRect(g_WindowCornersBmp, left + kWindowEdgeSize,  top,                       right - kWindowEdgeSize, top + kWindowEdgeSize,       1, 0, kBlitOpaque);
    TileFillRect(g_WindowCornersBmp, left,                    top + kWindowEdgeSize,     left + kWindowEdgeSize,  bottom - kWindowEdgeSize,    3, 0, kBlitOpaque);
    TileFillRect(g_WindowCornersBmp, right - kWindowEdgeSize, top + kWindowEdgeSize,     right,                   bottom - kWindowEdgeSize,    4, 0, kBlitOpaque);
    TileFillRect(g_WindowCornersBmp, left + kWindowEdgeSize,  bottom - kWindowEdgeSize,  right - kWindowEdgeSize, bottom,                      6, 0, kBlitOpaque);

    // 3. Corners, color-keyed: cell 0 = top-left, 2 = top-right, 5 = bottom-left, 7 = bottom-right.
    BlitTile(g_WindowCornersBmp, left,                    top,                      0, 0, kBlitColorKey);
    BlitTile(g_WindowCornersBmp, right - kWindowEdgeSize, top,                      2, 0, kBlitColorKey);
    BlitTile(g_WindowCornersBmp, left,                    bottom - kWindowEdgeSize, 5, 0, kBlitColorKey);
    BlitTile(g_WindowCornersBmp, right - kWindowEdgeSize, bottom - kWindowEdgeSize, 7, 0, kBlitColorKey);
}

void DrawSliderWidget(const SliderWidget& slider) {
    DrawWindowBox(slider.left, slider.top, slider.width, kSliderHeight);
    const int stepWidth = (slider.width - kSliderKnobInset) / std::max(1, slider.numSteps);
    const int knobX     = stepWidth * slider.currentValue + kSliderKnobInset + slider.left;
    BlitTile(g_WindowSlideBmp, knobX, slider.top, 0, 0, kBlitColorKey);
}

void DrawStarfield(int cameraX, int cameraY, bool autoScroll) {
    constexpr int kStarCount        = 480;
    constexpr int kFieldWidth       = 0x27d;   // 637: the star field wraps just inside the 640 px screen
    constexpr int kFieldHeight      = 0x1dd;   // 477
    constexpr int kCommonStarTypes  = 7;       // Stars.bmp cells 0..6
    constexpr int kRareStarTypes    = 3;       // cells 7..9, one star in kRareStarOdds
    constexpr int kRareStarOdds     = 0x32;

    struct Star { int positionX, positionY, type; };
    static std::array<Star, kStarCount> stars;
    static bool initialized  = false;
    static int  autoScrollX  = 0;
    static int  autoScrollY  = 0;

    if (!initialized) {
        for (Star& star : stars) {
            star.positionX = std::rand() % kFieldWidth;
            star.positionY = std::rand() % kFieldHeight;
            star.type      = std::rand() % kCommonStarTypes;
            if (std::rand() % kRareStarOdds == 1) {
                star.type = std::rand() % kRareStarTypes + kCommonStarTypes;
            }
        }
        initialized = true;
    }

    int scrollX = cameraX;
    int scrollY = cameraY;
    if (autoScroll) {
        if (++autoScrollX > kFieldWidth)  autoScrollX = 0;
        if (++autoScrollY > kFieldHeight) autoScrollY = 0;
        scrollX = autoScrollX;
        scrollY = autoScrollY;
    }

    for (const Star& star : stars) {
        const int screenX = WrapIntoRange(star.positionX - scrollX, kFieldWidth);
        const int screenY = WrapIntoRange(star.positionY - scrollY, kFieldHeight);
        BlitTile(g_StarsBmp, screenX, screenY, star.type, 0, kBlitColorKey);
    }
}

// ---- Palette effects --------------------------------------------------------

void SetPaletteEntry(int index, uint8_t red, uint8_t green, uint8_t blue) {
    if (!g_Screen8Bit) return;
    const SDL_Color color = { red, green, blue, 255 };
    SDL_SetPaletteColors(g_Screen8Bit->format->palette, &color, index, 1);
}

SDL_Color GetPaletteEntry(int index) {
    if (!g_Screen8Bit) return SDL_Color{ 0, 0, 0, 255 };
    return g_Screen8Bit->format->palette->colors[index];
}

void SetActivePaletteFromSurface(SDL_Surface* bmpWithPalette) {
    if (!bmpWithPalette || !bmpWithPalette->format->palette || !g_Screen8Bit) return;
    SDL_SetPaletteColors(g_Screen8Bit->format->palette,
                         bmpWithPalette->format->palette->colors,
                         0, bmpWithPalette->format->palette->ncolors);
}

// Remaps every pixel to the palette entry closest to a dimmed version of its colour.
void DimScreen(float factor) {
    if (!g_Screen8Bit || !g_Screen8Bit->format->palette) return;
    const SDL_Color* palette = g_Screen8Bit->format->palette->colors;

    std::array<uint8_t, kScreenPaletteSize> dimmedIndex;
    for (int colorIndex = 0; colorIndex < kScreenPaletteSize; ++colorIndex) {
        dimmedIndex[colorIndex] = NearestPaletteIndex(palette,
            static_cast<int>(palette[colorIndex].r * factor),
            static_cast<int>(palette[colorIndex].g * factor),
            static_cast<int>(palette[colorIndex].b * factor));
    }

    SDL_LockSurface(g_Screen8Bit);
    for (int row = 0; row < g_Screen8Bit->h; ++row) {
        uint8_t* pixels = PixelAt(g_Screen8Bit, 0, row);
        for (int column = 0; column < g_Screen8Bit->w; ++column) {
            pixels[column] = dimmedIndex[pixels[column]];
        }
    }
    SDL_UnlockSurface(g_Screen8Bit);
}

// Exact port of FUN_00409c30: a 64-step subtractive fade-out of the whole palette.
void FadeOutToBlack(int seconds) {
    if (!g_Screen8Bit || !g_Screen8Bit->format->palette) return;
    SDL_Palette* palette = g_Screen8Bit->format->palette;

    const uint32_t stepDelayMs = static_cast<uint32_t>(seconds) * 1000u / kFadeSteps;   // ~15 ms for 1 s

    std::array<SDL_Color, kScreenPaletteSize> originalPalette;
    std::copy_n(palette->colors, kScreenPaletteSize, originalPalette.begin());
    std::array<SDL_Color, kScreenPaletteSize> fadingPalette = originalPalette;

    auto darken = [](uint8_t channel) {
        return static_cast<uint8_t>(channel < kFadeStepAmount ? 0 : channel - kFadeStepAmount);
    };

    for (int step = 0; step < kFadeSteps; ++step) {
        const uint32_t stepStart = timeGetTime();

        for (SDL_Color& color : fadingPalette) {
            color.r = darken(color.r);
            color.g = darken(color.g);
            color.b = darken(color.b);
        }
        SDL_SetPaletteColors(palette, fadingPalette.data(), 0, kScreenPaletteSize);
        FlipScreen();

        if (!ProcessMessages()) break;

        const uint32_t elapsedMs = timeGetTime() - stepStart;
        if (elapsedMs < stepDelayMs) SDL_Delay(stepDelayMs - elapsedMs);
    }

    // Clear the screen while it is black, then restore the palette (end of FUN_00409c30).
    ClearScreen(0);
    FlipScreen();
    ClearTypedChar();
    SDL_SetPaletteColors(palette, originalPalette.data(), 0, kScreenPaletteSize);
}

// ==============================================================================
// MIDI music: Microsoft MIDS (.mds) -> standard MIDI -> TinySoundFont
// ==============================================================================

namespace {

// MIDS stream-event constants (see MIDIEVENT / MEVT_* in the Win32 SDK).
constexpr uint32_t kMdsLongEventFlag     = 0x80000000u;
constexpr uint8_t  kMevtShortMessage     = 0x00;   // with the long flag set: SysEx
constexpr uint8_t  kMevtTempo            = 0x01;
constexpr uint8_t  kMevtNoOperation      = 0x02;
constexpr uint32_t kMdsCompactEventsFlag = 1;      // dwFlags bit 0: events have no dwStreamID field
constexpr size_t   kMdsMinimumFileSize   = 20;
constexpr uint32_t kDefaultTimeDivision  = 96;

constexpr int kRenderBlockFrames   = 64;
constexpr int kBytesPerStereoFrame = 2 * sizeof(int16_t);
constexpr int kPercussionChannel   = 9;

void AppendVariableLengthQuantity(std::vector<uint8_t>& out, uint32_t value) {
    uint8_t reversed[5];
    int count = 0;
    reversed[count++] = value & 0x7F;
    while ((value >>= 7) > 0) {
        reversed[count++] = (value & 0x7F) | 0x80;
    }
    while (count > 0) out.push_back(reversed[--count]);
}

void AppendBigEndian32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

// Builds a single MIDI track, tracking running status the way the file format needs.
class MidiTrackBuilder {
public:
    void AddSysEx(uint32_t deltaTicks, const uint8_t* payload, uint32_t length) {
        AppendVariableLengthQuantity(bytes_, deltaTicks);
        bytes_.push_back(0xF0);
        AppendVariableLengthQuantity(bytes_, length);
        bytes_.insert(bytes_.end(), payload, payload + length);
        runningStatus_ = 0;
    }

    // An empty text meta event: keeps the delta time of a MIDS no-op.
    void AddPadding(uint32_t deltaTicks) {
        AppendVariableLengthQuantity(bytes_, deltaTicks);
        bytes_.insert(bytes_.end(), { 0xFF, 0x01, 0x00 });
        runningStatus_ = 0;
    }

    void AddTempo(uint32_t deltaTicks, uint32_t microsecondsPerQuarter) {
        AppendVariableLengthQuantity(bytes_, deltaTicks);
        bytes_.insert(bytes_.end(), { 0xFF, 0x51, 0x03,
                                      static_cast<uint8_t>(microsecondsPerQuarter >> 16),
                                      static_cast<uint8_t>(microsecondsPerQuarter >> 8),
                                      static_cast<uint8_t>(microsecondsPerQuarter) });
        runningStatus_ = 0;
    }

    void AddShortMessage(uint32_t deltaTicks, uint8_t status, uint8_t firstData, uint8_t secondData) {
        AppendVariableLengthQuantity(bytes_, deltaTicks);
        if (status != runningStatus_) {
            bytes_.push_back(status);
            runningStatus_ = status;
        }
        bytes_.push_back(firstData);
        const uint8_t command = status & 0xF0;
        if (command != 0xC0 && command != 0xD0) bytes_.push_back(secondData);   // program / channel pressure: 1 data byte
    }

    // Appends the end-of-track marker and returns the finished track bytes.
    std::vector<uint8_t> Finish() {
        bytes_.insert(bytes_.end(), { 0x00, 0xFF, 0x2F, 0x00 });
        return std::move(bytes_);
    }

private:
    std::vector<uint8_t> bytes_;
    uint8_t              runningStatus_ = 0;
};

// Converts Microsoft RIFF MIDS (.mds) bytes into a standard Type-0 MIDI (.mid) buffer.
// Returns an empty vector if the data is not a valid MIDS file.
std::vector<uint8_t> ConvertMdsToMidi(const uint8_t* data, size_t size) {
    if (size < kMdsMinimumFileSize || std::memcmp(data, "RIFF", 4) != 0 ||
        std::memcmp(data + 8, "MIDS", 4) != 0) {
        return {};
    }

    uint32_t       timeDivision  = kDefaultTimeDivision;
    uint32_t       formatFlags   = 0;
    const uint8_t* dataChunk     = nullptr;
    uint32_t       dataChunkSize = 0;

    // RIFF chunks: "fmt " (time division, flags) and "data" (the event blocks).
    size_t chunkPos = 12;
    while (chunkPos + 8 <= size) {
        const uint32_t chunkSize = ReadLittleEndian32(data + chunkPos + 4);
        if (chunkPos + 8 + chunkSize > size) break;

        if (std::memcmp(data + chunkPos, "fmt ", 4) == 0 && chunkSize >= 4) {
            timeDivision = ReadLittleEndian32(data + chunkPos + 8);
            if (chunkSize >= 12) formatFlags = ReadLittleEndian32(data + chunkPos + 16);
        } else if (std::memcmp(data + chunkPos, "data", 4) == 0) {
            dataChunk     = data + chunkPos + 8;
            dataChunkSize = chunkSize;
        }
        chunkPos += 8 + ((chunkSize + 1) & ~1u);   // chunks are padded to even sizes
    }
    if (!dataChunk || dataChunkSize < 4) return {};

    // Compact events are {deltaTime, event}; full MIDIEVENTs are {deltaTime, streamId, event}.
    const bool   hasStreamId     = (formatFlags & kMdsCompactEventsFlag) == 0;
    const size_t eventHeaderSize = hasStreamId ? 12 : 8;

    MidiTrackBuilder track;
    const uint32_t blockCount = ReadLittleEndian32(dataChunk);
    size_t cursor = 4;

    for (uint32_t blockIndex = 0; blockIndex < blockCount && cursor + 8 <= dataChunkSize; ++blockIndex) {
        const uint32_t blockSize = ReadLittleEndian32(dataChunk + cursor + 4);
        cursor += 8;
        const size_t blockEnd = std::min<size_t>(cursor + blockSize, dataChunkSize);

        while (cursor + eventHeaderSize <= blockEnd) {
            const uint32_t deltaTicks = ReadLittleEndian32(dataChunk + cursor);
            const uint32_t eventData  = ReadLittleEndian32(dataChunk + cursor + (eventHeaderSize - 4));
            cursor += eventHeaderSize;

            const bool     isLongEvent = (eventData & kMdsLongEventFlag) != 0;
            const uint8_t  eventType   = (eventData >> 24) & 0x7F;
            const uint32_t payload     = eventData & 0x00FFFFFFu;

            if (isLongEvent) {
                // Inline variable-length data, padded to a 4-byte boundary.
                const size_t paddedLength = (payload + 3u) & ~3u;
                if (cursor + paddedLength > blockEnd) break;
                if (eventType == kMevtShortMessage && payload > 0) {   // MEVT_LONGMSG (SysEx)
                    track.AddSysEx(deltaTicks, dataChunk + cursor, payload);
                }
                cursor += paddedLength;
                continue;
            }

            if (eventType == kMevtNoOperation) {
                if (deltaTicks > 0) track.AddPadding(deltaTicks);
                continue;
            }

            if (eventType == kMevtTempo) {
                track.AddTempo(deltaTicks, payload);
            } else {   // MEVT_SHORTMSG (and, as in the original, any unknown type)
                track.AddShortMessage(deltaTicks,
                                      static_cast<uint8_t>(payload),
                                      static_cast<uint8_t>(payload >> 8),
                                      static_cast<uint8_t>(payload >> 16));
            }
        }
        cursor = blockEnd;
    }

    const std::vector<uint8_t> trackBytes = track.Finish();

    std::vector<uint8_t> midi = {
        'M','T','h','d', 0,0,0,6,
        0,0,                                         // format 0
        0,1,                                         // one track
        static_cast<uint8_t>(timeDivision >> 8),
        static_cast<uint8_t>(timeDivision),
        'M','T','r','k'
    };
    AppendBigEndian32(midi, static_cast<uint32_t>(trackBytes.size()));
    midi.insert(midi.end(), trackBytes.begin(), trackBytes.end());
    return midi;
}

void DispatchMidiEvent(tsf* synth, const tml_message& event) {
    switch (event.type) {
        case TML_PROGRAM_CHANGE:
            tsf_channel_set_presetnumber(synth, event.channel, event.program,
                                         event.channel == kPercussionChannel);
            break;
        case TML_NOTE_ON:
            tsf_channel_note_on(synth, event.channel, event.key, event.velocity / 127.0f);
            break;
        case TML_NOTE_OFF:
            tsf_channel_note_off(synth, event.channel, event.key);
            break;
        case TML_PITCH_BEND:
            tsf_channel_set_pitchwheel(synth, event.channel, event.pitch_bend);
            break;
        case TML_CONTROL_CHANGE:
            tsf_channel_midi_control(synth, event.channel, event.control, event.control_value);
            break;
        default:
            break;
    }
}

// Called by SDL2_mixer on the audio thread to mix MIDI audio into the output stream.
void TinySynthAudioCallback(void* /*userData*/, uint8_t* stream, int byteCount) {
    std::memset(stream, 0, byteCount);

    std::unique_lock<std::mutex> lock(g_Midi.mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;                        // main thread is swapping songs
    if (!g_Midi.synth || !g_Midi.nextEvent) return;       // nothing to play

    int      framesLeft = byteCount / kBytesPerStereoFrame;
    int16_t* output     = reinterpret_cast<int16_t*>(stream);

    while (framesLeft > 0) {
        const int blockFrames = std::min(kRenderBlockFrames, framesLeft);

        g_Midi.elapsedMs += blockFrames * (1000.0 / g_Midi.sampleRate);
        while (g_Midi.nextEvent && g_Midi.elapsedMs >= g_Midi.nextEvent->time) {
            DispatchMidiEvent(g_Midi.synth, *g_Midi.nextEvent);
            g_Midi.nextEvent = g_Midi.nextEvent->next;
        }

        if (!g_Midi.nextEvent && g_Midi.loop) {           // song ended: loop
            g_Midi.nextEvent = g_Midi.head;
            g_Midi.elapsedMs = 0.0;
        }

        tsf_render_short(g_Midi.synth, output, blockFrames, 0);
        output     += blockFrames * 2;
        framesLeft -= blockFrames;
    }
}

}  // namespace

void PlayMidiMusic(const std::string& mdsFilename, bool loop) {
    if (!g_MusicEnabled || !g_Midi.synth) return;

    // 1. Slow work, no lock held.
    const GlomView entry = FindGlomEntry(mdsFilename);
    if (!entry) {
        std::fprintf(stderr, "GLOM: Missing MDS '%s'\n", mdsFilename.c_str());
        return;
    }
    const std::vector<uint8_t> midi = ConvertMdsToMidi(entry.data, entry.size);
    if (midi.empty()) {
        std::fprintf(stderr, "MDS: Failed to convert '%s'\n", mdsFilename.c_str());
        return;
    }
    tml_message* const newSong = tml_load_memory(midi.data(), static_cast<int>(midi.size()));
    if (!newSong) {
        std::fprintf(stderr, "TML: Failed to parse converted MIDI for '%s'\n", mdsFilename.c_str());
        return;
    }

    // 2. Swap songs under the lock (a few microseconds).
    tml_message* previousSong = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_Midi.mutex);
        previousSong     = g_Midi.head;
        g_Midi.head      = newSong;
        g_Midi.nextEvent = newSong;
        g_Midi.elapsedMs = 0.0;
        g_Midi.loop      = loop;
        tsf_reset(g_Midi.synth);   // kills hanging notes, resets bend/CCs
    }

    // 3. Free the previous song outside the lock.
    if (previousSong) tml_free(previousSong);
}

void StopMidiMusic() {
    tml_message* previousSong = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_Midi.mutex);
        g_Midi.nextEvent = nullptr;
        previousSong     = g_Midi.head;
        g_Midi.head      = nullptr;
        if (g_Midi.synth) tsf_reset(g_Midi.synth);
    }
    if (previousSong) tml_free(previousSong);
}

// ==============================================================================
// Sound effects
// ==============================================================================

namespace {

Mix_Chunk* LoadWavAsset(const std::string& filename) {
    const GlomView entry = FindGlomEntry(filename);   // bounds-checked
    if (!entry) {
        std::fprintf(stderr, "GLOM: Missing WAV '%s'\n", filename.c_str());
        return nullptr;
    }
    return Mix_LoadWAV_RW(SDL_RWFromConstMem(entry.data, static_cast<int>(entry.size)), 1);
}

}  // namespace

bool SoundEffect::IsPlaying() const {
    return channel != -1 && Mix_Playing(channel) != 0;
}

void SoundEffect::Load(const std::string& wavFilename, int voices) {
    Free();
    filename  = wavFilename;
    maxVoices = std::max(1, voices);
    chunk     = LoadWavAsset(wavFilename);
}

void SoundEffect::Play(int loopCount) {
    if (!g_SoundEnabled || chunk == nullptr) return;
    if (maxVoices <= 1 && IsPlaying()) return;   // single-voice sounds never retrigger while playing
    channel = Mix_PlayChannel(-1, chunk, loopCount);
}

void SoundEffect::Stop() {
    if (channel == -1) return;
    Mix_HaltChannel(channel);
    channel = -1;
}

void SoundEffect::Free() {
    Stop();
    if (chunk) {
        Mix_FreeChunk(chunk);
        chunk = nullptr;
    }
}

bool IsAnySoundPlaying() { return Mix_Playing(-1) > 0; }

// ==============================================================================
// Player data (Chickens.ini)
// ==============================================================================

namespace {

constexpr const char* kSettingsFile = "Chickens.ini";

// Names are stored with spaces as '+', because the fields are comma separated.
std::string EncodeProfileName(const PlayerProfile& profile) {
    const char* nameEnd = std::find(profile.name, profile.name + kMaxPlayerNameLength, '\0');
    std::string encoded(profile.name, nameEnd);
    std::replace(encoded.begin(), encoded.end(), ' ', '+');
    return encoded;
}

void DecodeProfileName(PlayerProfile& profile, const char* encoded) {
    std::fill(std::begin(profile.name), std::end(profile.name), '\0');
    std::strncpy(profile.name, encoded, kMaxPlayerNameLength);
    std::replace(profile.name, profile.name + kMaxPlayerNameLength, '+', ' ');
}

// If `line` starts with "<key>:", returns the text after the colon; otherwise nullptr.
const char* SettingValue(const char* line, const char* key) {
    const size_t keyLength = std::strlen(key);
    if (std::strncmp(line, key, keyLength) != 0 || line[keyLength] != ':') return nullptr;
    return line + keyLength + 1;
}

bool ParseYesNoSetting(const char* line, const char* key, bool& setting) {
    const char* text = SettingValue(line, key);
    char word[16] = {0};
    if (!text || std::sscanf(text, "%7s", word) != 1) return false;
    setting = (std::strcmp(word, "Yes") == 0);
    return true;
}

// "PlayerN: active,name,score,level,chickens" -- the name may be empty.
// Returns true if the line was a player line (even one with an out-of-range slot).
bool ParsePlayerLine(const char* line) {
    int  slotNumber = 0, active = 0, score = 0, level = 0, chickens = 0;
    char encodedName[32] = {0};

    PlayerProfile* profile = nullptr;
    auto selectSlot = [&] {
        if (slotNumber >= 1 && slotNumber <= kMaxPlayerSlots) profile = &g_PlayerSlots[slotNumber - 1];
    };

    if (std::sscanf(line, "Player%d: %d,%31[^,],%d,%d,%d",
                    &slotNumber, &active, encodedName, &score, &level, &chickens) == 6) {
        selectSlot();
        if (profile) DecodeProfileName(*profile, encodedName);
    } else if (std::sscanf(line, "Player%d: %d,,%d,%d,%d",
                           &slotNumber, &active, &score, &level, &chickens) == 5) {
        selectSlot();
        if (profile) *profile = PlayerProfile{};   // empty name: reset the whole slot
    } else {
        return false;
    }

    if (profile) {
        profile->active   = active;
        profile->score    = score;
        profile->level    = level;
        profile->chickens = chickens;
    }
    return true;
}

void ParseSettingLine(const char* line) {
    if (ParseYesNoSetting(line, "UseJoystick", g_UseJoystick)) return;
    if (ParseYesNoSetting(line, "AlwaysRun",   g_AlwaysRun))   return;
    if (ParseYesNoSetting(line, "Sound",       g_SoundEnabled)) return;
    if (ParseYesNoSetting(line, "Music",       g_MusicEnabled)) return;

    int difficulty = 0;
    const char* text = SettingValue(line, "Difficulty");
    if (text && std::sscanf(text, "%d", &difficulty) == 1) {
        g_Difficulty = std::clamp(difficulty, 0, 4);
    }
}

const char* YesNo(bool value) { return value ? "Yes" : "No"; }

}  // namespace

void SavePlayerData() {
    std::ofstream file(kSettingsFile);
    if (!file) return;

    for (int slotIndex = 0; slotIndex < kMaxPlayerSlots; ++slotIndex) {
        const PlayerProfile& profile = g_PlayerSlots[slotIndex];
        file << "Player" << (slotIndex + 1) << ": " << profile.active << ','
             << EncodeProfileName(profile) << ',' << profile.score << ','
             << profile.level << ',' << profile.chickens << '\n';
    }

    file << "UseJoystick: " << YesNo(g_UseJoystick)  << '\n'
         << "AlwaysRun: "   << YesNo(g_AlwaysRun)    << '\n'
         << "Difficulty: "  << g_Difficulty          << '\n'
         << "Sound: "       << YesNo(g_SoundEnabled) << '\n'
         << "Music: "       << YesNo(g_MusicEnabled) << '\n';
}

void LoadPlayerData() {
    std::ifstream file(kSettingsFile);
    if (!file) return;

    std::string line;
    while (std::getline(file, line)) {
        if (!ParsePlayerLine(line.c_str())) ParseSettingLine(line.c_str());
    }
}

// ==============================================================================
// Errors and game state
// ==============================================================================

void SetFatalError(const std::string& message) { g_FatalErrorMessage = message; }

bool CheckForPendingError() { return !g_FatalErrorMessage.empty(); }

void ShowFatalErrorDialog() {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", g_FatalErrorMessage.c_str(), g_Window);
}

int SetOrGetGameState(int newState) {
    if (newState >= 0) {
        g_PreviousState = g_CurrentState;
        g_CurrentState  = newState;
        ClearTypedChar();
        return newState;
    }
    return (newState == kQueryPreviousState) ? g_PreviousState : g_CurrentState;
}

// ==============================================================================
// Engine lifecycle
// ==============================================================================

namespace {

struct GlomSource {
    const char* path;       // relative to the executable
    size_t      offset;
    size_t      size;       // 0 = whole file
};

// Checked next to the executable AND in the parent folder (for build/ runs).
constexpr GlomSource kGlomSources[] = {
    { "assets.glom",     0,               0            },
    { "../assets.glom",  0,               0            },
    { "egames.exe",      kExeGlomOffset,  kExeGlomSize },
    { "../egames.exe",   kExeGlomOffset,  kExeGlomSize },
};

constexpr const char* kSoundFontPaths[] = {
    "assets/TimGM6mb.sf2",
    "../assets/TimGM6mb.sf2",
    "/usr/share/soundfonts/FluidR3_GM.sf2",
};

constexpr int kMixerSampleRate  = 22050;
constexpr int kMixerChannels    = 2;
constexpr int kMixerBufferBytes = 1024;

bool LoadAssetArchive() {
    std::string baseDir = "./";
    if (char* rawBaseDir = SDL_GetBasePath()) {
        baseDir = rawBaseDir;
        SDL_free(rawBaseDir);
    }
    for (const GlomSource& source : kGlomSources) {
        if (LoadGlomFromFile(baseDir + source.path, source.offset, source.size)) return true;
    }
    return false;
}

void LoadEngineSpriteSheets() {
    g_FontBmp          = LoadSpriteSheet("Font.bmp",          11,  16);
    g_WindowBackBmp    = LoadSpriteSheet("WindowBack.bmp",    128, 128);
    g_WindowCornersBmp = LoadSpriteSheet("WindowCorners.bmp", 4,   4);
    g_WindowSlideBmp   = LoadSpriteSheet("WindowSlide.bmp");
    g_StarsBmp         = LoadSpriteSheet("Stars.bmp",         3,   3);
}

// Audio is optional: failures are reported but never fatal.
void InitAudio() {
    if (Mix_OpenAudio(kMixerSampleRate, AUDIO_S16SYS, kMixerChannels, kMixerBufferBytes) < 0) {
        std::fprintf(stderr, "Warning: Audio init failed: %s\n", Mix_GetError());
    }

    int sampleRate = 0, channelCount = 0;
    Uint16 sampleFormat = 0;
    if (Mix_QuerySpec(&sampleRate, &sampleFormat, &channelCount)) g_Midi.sampleRate = sampleRate;

    for (const char* soundFontPath : kSoundFontPaths) {
        g_Midi.synth = tsf_load_filename(soundFontPath);
        if (g_Midi.synth) break;
    }
    if (!g_Midi.synth) {
        std::fprintf(stderr, "TSF: Failed to load any .sf2 SoundFont!\n");
        return;
    }
    tsf_set_output(g_Midi.synth, TSF_STEREO_INTERLEAVED, g_Midi.sampleRate, 0.0f);
    Mix_HookMusic(TinySynthAudioCallback, nullptr);
}

bool InitVideo(const char* windowTitle, int width, int height) {
    g_Window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_Window) {
        SetFatalError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    SDL_StartTextInput();

    g_Renderer = SDL_CreateRenderer(g_Window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_Renderer) g_Renderer = SDL_CreateRenderer(g_Window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_Renderer) {
        SetFatalError(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(g_Renderer, width, height);

    g_Screen8Bit    = SDL_CreateRGBSurface(0, width, height, 8, 0, 0, 0, 0);
    g_Screen32Bit   = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    g_ScreenTexture = SDL_CreateTexture(g_Renderer, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING, width, height);
    return g_Screen8Bit && g_Screen32Bit && g_ScreenTexture;
}

}  // namespace

bool InitEngine(const char* windowTitle, int width, int height) {
    if (!LoadAssetArchive()) {
        SetFatalError("Could not load assets.glom or egames.exe!");
        return false;
    }

    LoadPlayerData();
    LoadEngineSpriteSheets();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SetFatalError(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    InitAudio();
    return InitVideo(windowTitle, width, height);
}

void ShutdownEngine() {
    if (g_ScreenTexture) SDL_DestroyTexture(g_ScreenTexture);
    if (g_Screen32Bit)   SDL_FreeSurface(g_Screen32Bit);
    if (g_Screen8Bit)    SDL_FreeSurface(g_Screen8Bit);
    if (g_Renderer)      SDL_DestroyRenderer(g_Renderer);
    if (g_Window)        SDL_DestroyWindow(g_Window);

    Mix_HookMusic(nullptr, nullptr);
    StopMidiMusic();
    if (g_Midi.synth) {
        tsf_close(g_Midi.synth);
        g_Midi.synth = nullptr;
    }
    Mix_CloseAudio();
    SDL_Quit();
}

bool ProcessMessages() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                g_QuitRequested = true;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)        g_WindowActive = false;
                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) g_WindowActive = true;
                break;
            case SDL_TEXTINPUT: {
                const char typed = event.text.text[0];
                if (typed >= 0x20 && typed <= 0x7E) g_LastTypedChar = typed;   // printable ASCII only
                break;
            }
            default:
                break;
        }
    }
    return !g_QuitRequested;
}

bool IsWindowActive() { return g_WindowActive; }
