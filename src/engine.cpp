#include "engine.hpp"
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <mutex>
#include <algorithm>

#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"

namespace fs = std::filesystem;

static char g_LastTypedChar = '\0';
static std::mutex g_MidiMutex;
static int        g_AudioFreq = 22050;

static SDL_Window*   g_Window        = nullptr;
static SDL_Renderer* g_Renderer      = nullptr;
static SDL_Texture*  g_ScreenTexture = nullptr;
static SDL_Surface*  g_Screen8Bit    = nullptr; // 640x480 8-bit palettized buffer
static SDL_Surface*  g_Screen32Bit   = nullptr; // Conversion buffer for SDL_Texture

static bool          g_WindowActive  = true;
static std::string   g_FatalErrorMsg = "";
static int           g_CurrentState  = 0;
static int           g_PreviousState = 0;
static Mix_Music*    g_CurrentMusic = nullptr;

static tsf*         g_TinySynth = nullptr;
static tml_message* g_MidiHead  = nullptr;
static tml_message* g_MidiCurr  = nullptr;
static double       g_MidiMsec  = 0.0;
static bool         g_MidiLoop  = true;

static SpriteSheet g_FontBmp;
static SpriteSheet g_WindowBackBmp;
static SpriteSheet g_WindowCornersBmp;
static SpriteSheet g_WindowSlideBmp;
static SpriteSheet g_StarsBmp;

static bool g_QuitRequested = false;

int  g_Difficulty   = 2;
bool g_SoundEnabled = true;
bool g_MusicEnabled = true;
bool g_UseJoystick  = false;
bool g_AlwaysRun    = false;
PlayerProfile g_PlayerSlots[12];
int           g_CurrentPlayerIndex = 0;

struct GlomEntry {
    uint32_t offset;
    uint32_t size;
};

struct GlomView {
    const uint8_t* data = nullptr;
    size_t         size = 0;
    explicit operator bool() const { return data != nullptr; }
};

static std::vector<uint8_t> g_GlomData;
static std::unordered_map<std::string, GlomEntry> g_GlomIndex;

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

bool WasKeyJustPressed(int vKey) {
    static std::unordered_map<int, bool> prevDown;
    bool isDown = (GetAsyncKeyState(vKey) & 0x8000) != 0;
    bool justPressed = isDown && !prevDown[vKey];
    prevDown[vKey] = isDown;
    return justPressed;
}

// Exact port of FUN_00409c30 (64-step subtractive VGA/DirectDraw fade-out)
void FadeOutToBlack(int seconds) {
    if (!g_Screen8Bit || !g_Screen8Bit->format->palette) return;

    uint32_t stepDelayMs = (static_cast<uint32_t>(seconds) * 125u) >> 3; // ~15ms per step for 1s

    // Save original 256-color palette (local_80c)
    SDL_Color savedPalette[256];
    SDL_Color currentPalette[256];
    std::memcpy(savedPalette, g_Screen8Bit->format->palette->colors, sizeof(savedPalette));
    std::memcpy(currentPalette, savedPalette, sizeof(currentPalette));

    for (int step = 0; step < 64; ++step) {
        uint32_t stepStart = timeGetTime();

        for (int i = 0; i < 256; ++i) {
            currentPalette[i].r = (currentPalette[i].r < 4) ? 0 : (currentPalette[i].r - 4);
            currentPalette[i].g = (currentPalette[i].g < 4) ? 0 : (currentPalette[i].g - 4);
            currentPalette[i].b = (currentPalette[i].b < 4) ? 0 : (currentPalette[i].b - 4);
        }

        SDL_SetPaletteColors(g_Screen8Bit->format->palette, currentPalette, 0, 256);
        FlipScreen();

        if (ProcessMessages() == 0) break;

        uint32_t elapsed = timeGetTime() - stepStart;
        if (elapsed < stepDelayMs) {
            SDL_Delay(stepDelayMs - elapsed);
        }
    }

    // Clear screen while black and restore palette (matches end of FUN_00409c30)
    ClearScreen(0);
    FlipScreen();
    ClearTypedChar();
    SDL_SetPaletteColors(g_Screen8Bit->format->palette, savedPalette, 0, 256);
}

// Called automatically by SDL2_mixer to mix MIDI audio into the output stream
static void TinySynthAudioCallback(void* /*udata*/, uint8_t* stream, int len) {
    std::memset(stream, 0, len);

    std::unique_lock<std::mutex> lock(g_MidiMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;            // main thread is swapping songs
    if (!g_TinySynth || !g_MidiCurr) return;  // nothing to play

    int      samplesLeft = len / (2 * sizeof(int16_t));   // stereo, 16-bit
    int16_t* out         = reinterpret_cast<int16_t*>(stream);

    while (samplesLeft > 0) {
        const int block = std::min(64, samplesLeft);

        g_MidiMsec += block * (1000.0 / g_AudioFreq);
        while (g_MidiCurr && g_MidiMsec >= g_MidiCurr->time) {
            switch (g_MidiCurr->type) {
                case TML_PROGRAM_CHANGE:
                    tsf_channel_set_presetnumber(g_TinySynth, g_MidiCurr->channel,
                                                 g_MidiCurr->program, g_MidiCurr->channel == 9);
                    break;
                case TML_NOTE_ON:
                    tsf_channel_note_on(g_TinySynth, g_MidiCurr->channel,
                                        g_MidiCurr->key, g_MidiCurr->velocity / 127.0f);
                    break;
                case TML_NOTE_OFF:
                    tsf_channel_note_off(g_TinySynth, g_MidiCurr->channel, g_MidiCurr->key);
                    break;
                case TML_PITCH_BEND:
                    tsf_channel_set_pitchwheel(g_TinySynth, g_MidiCurr->channel, g_MidiCurr->pitch_bend);
                    break;
                case TML_CONTROL_CHANGE:
                    tsf_channel_midi_control(g_TinySynth, g_MidiCurr->channel,
                                             g_MidiCurr->control, g_MidiCurr->control_value);
                    break;
            }
            g_MidiCurr = g_MidiCurr->next;
        }

        if (!g_MidiCurr && g_MidiLoop) {      // song ended: loop
            g_MidiCurr = g_MidiHead;
            g_MidiMsec = 0.0;
        }

        tsf_render_short(g_TinySynth, out, block, 0);
        out         += block * 2;
        samplesLeft -= block;
    }
}

static void WriteVLQ(std::vector<uint8_t>& out, uint32_t value) {
    uint8_t buf[5];
    int count = 0;
    buf[count++] = value & 0x7F;
    while ((value >>= 7) > 0) {
        buf[count++] = (value & 0x7F) | 0x80;
    }
    while (count > 0) {
        out.push_back(buf[--count]);
    }
}

// Converts Microsoft RIFF MIDS (.mds) bytes into a standard Type-0 MIDI (.mid) buffer
static std::vector<uint8_t> ConvertMdsToMidi(const uint8_t* data, size_t size) {
    if (size < 20 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "MIDS", 4) != 0) {
        return {};
    }

    uint32_t timeDivision = 96;
    uint32_t formatFlags  = 0;
    const uint8_t* dataChunk = nullptr;
    uint32_t dataChunkSize = 0;

    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, data + pos + 4, 4);
        if (pos + 8 + chunkSize > size) break;

        if (std::memcmp(data + pos, "fmt ", 4) == 0 && chunkSize >= 4) {
            std::memcpy(&timeDivision, data + pos + 8, 4);
            if (chunkSize >= 12) {
                std::memcpy(&formatFlags, data + pos + 16, 4);
            }
        } else if (std::memcmp(data + pos, "data", 4) == 0) {
            dataChunk     = data + pos + 8;
            dataChunkSize = chunkSize;
        }
        pos += 8 + ((chunkSize + 1) & ~1u);
    }

    if (!dataChunk || dataChunkSize < 4) return {};

    // In MIDS format: dwFlags bit 0 (1) = compact 8-byte events (no dwStreamID); 0 = full 12-byte MIDIEVENT
    const bool hasStreamId = ((formatFlags & 1) == 0);
    const size_t baseHeaderSize = hasStreamId ? 12 : 8;

    std::vector<uint8_t> track;
    uint32_t numBlocks = 0;
    std::memcpy(&numBlocks, dataChunk, 4);
    size_t p = 4;

    uint8_t runningStatus = 0;
    for (uint32_t b = 0; b < numBlocks && p + 8 <= dataChunkSize; ++b) {
        uint32_t blockSize = 0;
        std::memcpy(&blockSize, dataChunk + p + 4, 4);
        p += 8;

        size_t blockEnd = std::min<size_t>(p + blockSize, dataChunkSize);
        while (p + baseHeaderSize <= blockEnd) {
            uint32_t deltaTicks = 0;
            uint32_t eventData  = 0;
            std::memcpy(&deltaTicks, dataChunk + p, 4);
            std::memcpy(&eventData,  dataChunk + p + (baseHeaderSize - 4), 4);
            p += baseHeaderSize;

            bool isLong       = (eventData & 0x80000000u) != 0;
            uint8_t eventType = (eventData >> 24) & 0x7F;
            uint32_t payload  = eventData & 0x00FFFFFFu;

            if (isLong) {
                // Variable-length inline data (padded to 4-byte DWORD boundary)
                size_t paddedLen = (payload + 3u) & ~3u;
                if (p + paddedLen > blockEnd) break;

                if (eventType == 0x00 && payload > 0) { // MEVT_LONGMSG (SysEx)
                    WriteVLQ(track, deltaTicks);
                    track.push_back(0xF0);
                    WriteVLQ(track, payload);
                    track.insert(track.end(), dataChunk + p, dataChunk + p + payload);
                    runningStatus = 0;
                }
                p += paddedLen;
                continue;
            }

            if (eventType == 0x02) { // MEVT_NOP (no-op padding)
                if (deltaTicks > 0) {
                    // Emit harmless CC/meta or accumulate deltaTicks
                    WriteVLQ(track, deltaTicks);
                    track.push_back(0xFF);
                    track.push_back(0x01);
                    track.push_back(0x00);
                    runningStatus = 0;
                }
                continue;
            }

            WriteVLQ(track, deltaTicks);

            if (eventType == 0x01) { // MEVT_TEMPO
                track.push_back(0xFF);
                track.push_back(0x51);
                track.push_back(0x03);
                track.push_back((payload >> 16) & 0xFF);
                track.push_back((payload >> 8)  & 0xFF);
                track.push_back(payload & 0xFF);
                runningStatus = 0;
            } else { // MEVT_SHORTMSG (0x00)
                uint8_t status = payload & 0xFF;
                uint8_t d1     = (payload >> 8)  & 0xFF;
                uint8_t d2     = (payload >> 16) & 0xFF;
                uint8_t cmd    = status & 0xF0;

                if (status != runningStatus) {
                    track.push_back(status);
                    runningStatus = status;
                }
                track.push_back(d1);
                if (cmd != 0xC0 && cmd != 0xD0) {
                    track.push_back(d2);
                }
            }
        }
        p = blockEnd;
    }

    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00);

    std::vector<uint8_t> midi = {
        'M','T','h','d', 0,0,0,6,
        0,0, 0,1,
        static_cast<uint8_t>((timeDivision >> 8) & 0xFF),
        static_cast<uint8_t>(timeDivision & 0xFF),
        'M','T','r','k',
        static_cast<uint8_t>((track.size() >> 24) & 0xFF),
        static_cast<uint8_t>((track.size() >> 16) & 0xFF),
        static_cast<uint8_t>((track.size() >> 8)  & 0xFF),
        static_cast<uint8_t>(track.size() & 0xFF)
    };
    midi.insert(midi.end(), track.begin(), track.end());
    return midi;
}

void PlayMidiMusic(const std::string& mdsFilename, bool loop) {
    if (!g_MusicEnabled) return;

    // 1. Slow work, no lock held.
    auto it = g_GlomIndex.find(ToLower(mdsFilename));
    if (it == g_GlomIndex.end()) {
        std::fprintf(stderr, "GLOM: Missing MDS '%s'\n", mdsFilename.c_str());
        return;
    }
    std::vector<uint8_t> midi = ConvertMdsToMidi(&g_GlomData[it->second.offset], it->second.size);
    if (midi.empty()) {
        std::fprintf(stderr, "MDS: Failed to convert '%s'\n", mdsFilename.c_str());
        return;
    }
    tml_message* newHead = tml_load_memory(midi.data(), static_cast<int>(midi.size()));
    if (!newHead) {
        std::fprintf(stderr, "TML: Failed to parse converted MIDI for '%s'\n", mdsFilename.c_str());
        return;
    }

    // 2. Swap songs under the lock (a few microseconds).
    tml_message* oldHead = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_MidiMutex);
        oldHead    = g_MidiHead;
        g_MidiHead = newHead;
        g_MidiCurr = newHead;
        g_MidiMsec = 0.0;
        g_MidiLoop = loop;
        if (g_TinySynth) tsf_reset(g_TinySynth);   // kills hanging notes, resets bend/CCs
    }

    // 3. Free the previous song outside the lock.
    if (oldHead) tml_free(oldHead);
}

void StopMidiMusic() {
    tml_message* oldHead = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_MidiMutex);
        g_MidiCurr = nullptr;
        oldHead    = g_MidiHead;
        g_MidiHead = nullptr;
        if (g_TinySynth) tsf_reset(g_TinySynth);
    }
    if (oldHead) tml_free(oldHead);
}

bool SoundEffect::IsPlaying() const {
    return (channel != -1 && Mix_Playing(channel) != 0);
}

bool IsAnySoundPlaying() { return Mix_Playing(-1) > 0; }

void ClearTypedChar() {
    g_LastTypedChar = '\0';
    SDL_FlushEvent(SDL_TEXTINPUT);
}

void SavePlayerData() {
    std::FILE* f = std::fopen("Chickens.ini", "w");
    if (!f) return;

    for (int i = 0; i < 12; ++i) {
        char encodedName[32] = {0};
        std::strncpy(encodedName, g_PlayerSlots[i].name, 12);
        for (int j = 0; j < 12; ++j) {
            if (encodedName[j] == ' ') encodedName[j] = '+';
        }
        std::fprintf(
            f,
            "Player%d: %d,%s,%d,%d,%d\n",
            i + 1,
            g_PlayerSlots[i].active,
            encodedName,
            g_PlayerSlots[i].score,
            g_PlayerSlots[i].level,
            g_PlayerSlots[i].chickens
        );
    }

    std::fprintf(f, "UseJoystick: %s\n", g_UseJoystick ? "Yes" : "No");
    std::fprintf(f, "AlwaysRun: %s\n", g_AlwaysRun ? "Yes" : "No");
    std::fprintf(f, "Difficulty: %d\n", g_Difficulty);
    std::fprintf(f, "Sound: %s\n", g_SoundEnabled ? "Yes" : "No");
    std::fprintf(f, "Music: %s\n", g_MusicEnabled ? "Yes" : "No");
    std::fclose(f);
}

void LoadPlayerData() {
    std::FILE* f = std::fopen("Chickens.ini", "r");
    if (!f) return;

    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        int slotNum = 0, active = 0, score = 0, level = 0, chickens = 0;
        char encodedName[32] = {0};
        char valStr[16] = {0};
        int diff = 2;

        if (std::sscanf(line, "Player%d: %d,%31[^,],%d,%d,%d",
                        &slotNum, &active, encodedName, &score, &level, &chickens) == 6) {
            if (slotNum >= 1 && slotNum <= 12) {
                PlayerProfile& p = g_PlayerSlots[slotNum - 1];
                p.active   = active;
                p.score    = score;
                p.level    = level;
                p.chickens = chickens;
                std::memset(p.name, 0, sizeof(p.name));
                std::strncpy(p.name, encodedName, 12);
                for (int j = 0; j < 12; ++j) {
                    if (p.name[j] == '+') p.name[j] = ' ';
                }
            }
        } else if (std::sscanf(line, "Player%d: %d,,%d,%d,%d",
                               &slotNum, &active, &score, &level, &chickens) == 5) {
            if (slotNum >= 1 && slotNum <= 12) {
                PlayerProfile& p = g_PlayerSlots[slotNum - 1];
                std::memset(&p, 0, sizeof(PlayerProfile));
                p.active   = active;
                p.score    = score;
                p.level    = level;
                p.chickens = chickens;
            }
        } else if (std::sscanf(line, "UseJoystick: %7s", valStr) == 1) {
            g_UseJoystick = (std::strcmp(valStr, "Yes") == 0);
        } else if (std::sscanf(line, "AlwaysRun: %7s", valStr) == 1) {
            g_AlwaysRun = (std::strcmp(valStr, "Yes") == 0);
        } else if (std::sscanf(line, "Difficulty: %d", &diff) == 1) {
            g_Difficulty = std::clamp(diff, 0, 4);
        } else if (std::sscanf(line, "Sound: %7s", valStr) == 1) {
            g_SoundEnabled = (std::strcmp(valStr, "Yes") == 0);
        } else if (std::sscanf(line, "Music: %7s", valStr) == 1) {
            g_MusicEnabled = (std::strcmp(valStr, "Yes") == 0);
        }
    }
    std::fclose(f);
}

char GetTypedChar() {
    char ch = g_LastTypedChar;
    g_LastTypedChar = '\0';
    if (ch == ',' || ch == '+') return 0;
    return ch;
}

// Helper for Linux case-sensitive filesystems (finds "Tiles.bmp" even if stored as "tiles.bmp")
static std::string ResolveAssetPath(const std::string& filename) {
    fs::path direct = fs::path("assets") / filename;
    if (fs::exists(direct)) return direct.string();

    if (fs::exists("assets")) {
        std::string lowerTarget = filename;
        std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);
        for (const auto& entry : fs::directory_iterator("assets")) {
            std::string candidate = entry.path().filename().string();
            std::string lowerCand = candidate;
            std::transform(lowerCand.begin(), lowerCand.end(), lowerCand.begin(), ::tolower);
            if (lowerCand == lowerTarget) {
                return entry.path().string();
            }
        }
    }
    return direct.string();
}

uint32_t timeGetTime() {
    return SDL_GetTicks();
}

static SDL_Scancode VkToScancode(int vk) {
    if (vk >= 'A' && vk <= 'Z')   return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (vk - 'A'));
    if (vk >= '1' && vk <= '9')   return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (vk - '1'));
    if (vk == '0')                return SDL_SCANCODE_0;
    if (vk >= 0x70 && vk <= 0x7B) return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (vk - 0x70));
    if (vk >= 0x60 && vk <= 0x69) return vk == 0x60 ? SDL_SCANCODE_KP_0 : static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + (vk - 0x61));  // VK_NUMPAD0..9
    switch (vk) {
        case 0x08: return SDL_SCANCODE_BACKSPACE;
        case 0x0D: return SDL_SCANCODE_RETURN;
        case 0x13: return SDL_SCANCODE_PAUSE;
        case 0x1B: return SDL_SCANCODE_ESCAPE;
        case 0x20: return SDL_SCANCODE_SPACE;
        case 0x25: return SDL_SCANCODE_LEFT;
        case 0x26: return SDL_SCANCODE_UP;
        case 0x27: return SDL_SCANCODE_RIGHT;
        case 0x28: return SDL_SCANCODE_DOWN;
        case 0x2E: return SDL_SCANCODE_DELETE;
        default:   return SDL_SCANCODE_UNKNOWN;
    }
}

uint16_t GetAsyncKeyState(int vKey) {
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    if (vKey == 0x10) return (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) ? 0x8000 : 0;
    if (vKey == 0x11) return (keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL])  ? 0x8000 : 0;
    if (vKey == 0x12) return (keys[SDL_SCANCODE_LALT]   || keys[SDL_SCANCODE_RALT])   ? 0x8000 : 0;
    SDL_Scancode sc = VkToScancode(vKey);
    return (sc != SDL_SCANCODE_UNKNOWN && keys[sc]) ? 0x8000 : 0;
}

static inline uint8_t* PixelAt(const SDL_Surface* s, int x, int y) {
    return static_cast<uint8_t*>(s->pixels) + y * s->pitch + x;
}

static void CopyRow(const uint8_t* from, uint8_t* to, int count, bool transparent) {
    if (!transparent) {
        std::memcpy(to, from, count);
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (from[i] != 0) to[i] = from[i];    // palette index 0 = transparent
    }
}

void FillScreenRect(int x, int y, int w, int h, uint8_t colorIndex) {
    if (!g_Screen8Bit) return;
    SDL_Rect rect = { x, y, w, h };
    SDL_FillRect(g_Screen8Bit, &rect, colorIndex);
}

void DrawBitmapText(int x, int y, const std::string& text, bool opaque) {
    uint32_t flags = opaque ? 0 : 0x8000;
    for (char ch : text) {
        int idx = static_cast<unsigned char>(ch) - 0x20;
        if (idx < 0 || idx > 0x7F) idx = 0;
        BlitTile(g_FontBmp, x, y, idx % 32, idx / 32, flags);
        x += 11;
    }
}

void SetPaletteEntry(int index, uint8_t r, uint8_t g, uint8_t b) {
    SDL_Color c = { r, g, b, 255 };
    SDL_SetPaletteColors(g_Screen8Bit->format->palette, &c, index, 1);
}
SDL_Color GetPaletteEntry(int index) { return g_Screen8Bit->format->palette->colors[index]; }

// Tiles one cell of a sprite sheet over a rectangle, starting at its top-left corner.
// Partial tiles on the right and bottom are clipped (original: FUN_0040e8f0).
static void TileFillRect(const SpriteSheet& sheet, int left, int top, int right, int bottom,
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

// Framed window panel (original: FUN_0040f090).
void DrawWindowBox(int boxX, int boxY, int boxWidth, int boxHeight) {
    constexpr int kEdge = 4;                       // WindowCorners cells are 4x4
    const int right  = boxX + boxWidth;
    const int bottom = boxY + boxHeight;

    // 1. Background texture, opaque
    TileFillRect(g_WindowBackBmp, boxX, boxY, right, bottom, 0, 0, 0);

    // 2. Edges, opaque: 1 = top, 3 = left, 4 = right, 6 = bottom
    TileFillRect(g_WindowCornersBmp, boxX + kEdge,  boxY,            right - kEdge, boxY + kEdge,   1, 0, 0);
    TileFillRect(g_WindowCornersBmp, boxX,          boxY + kEdge,    boxX + kEdge,  bottom - kEdge, 3, 0, 0);
    TileFillRect(g_WindowCornersBmp, right - kEdge, boxY + kEdge,    right,         bottom - kEdge, 4, 0, 0);
    TileFillRect(g_WindowCornersBmp, boxX + kEdge,  bottom - kEdge,  right - kEdge, bottom,         6, 0, 0);

    // 3. Corners, color-keyed: 0 = top-left, 2 = top-right, 5 = bottom-left, 7 = bottom-right
    BlitTile(g_WindowCornersBmp, boxX,          boxY,           0, 0, 0x8000);
    BlitTile(g_WindowCornersBmp, right - kEdge, boxY,           2, 0, 0x8000);
    BlitTile(g_WindowCornersBmp, boxX,          bottom - kEdge, 5, 0, 0x8000);
    BlitTile(g_WindowCornersBmp, right - kEdge, bottom - kEdge, 7, 0, 0x8000);
}

void DimScreen(float factor) {
    if (!g_Screen8Bit || !g_Screen8Bit->format->palette) return;
    const SDL_Color* pal = g_Screen8Bit->format->palette->colors;

    uint8_t dimLut[256];
    for (int i = 0; i < 256; ++i) {
        int tr = static_cast<int>(pal[i].r * factor);
        int tg = static_cast<int>(pal[i].g * factor);
        int tb = static_cast<int>(pal[i].b * factor);

        int bestIdx = 0;
        int bestDist = 999999;
        for (int j = 0; j < 256; ++j) {
            int dr = pal[j].r - tr;
            int dg = pal[j].g - tg;
            int db = pal[j].b - tb;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = j;
            }
        }
        dimLut[i] = static_cast<uint8_t>(bestIdx);
    }

    SDL_LockSurface(g_Screen8Bit);
    uint8_t* pixels = static_cast<uint8_t*>(g_Screen8Bit->pixels);
    for (int y = 0; y < g_Screen8Bit->h; ++y) {
        uint8_t* row = pixels + y * g_Screen8Bit->pitch;
        for (int x = 0; x < g_Screen8Bit->w; ++x) {
            row[x] = dimLut[row[x]];
        }
    }
    SDL_UnlockSurface(g_Screen8Bit);
}

void DrawSliderWidget(const SliderWidget& slider) {
    DrawWindowBox(slider.x, slider.y, slider.width, 24);   // original: height 0x18
    const int knobX = ((slider.width - 8) / slider.numSteps) * slider.currentValue + 8 + slider.x;
    BlitTile(g_WindowSlideBmp, knobX, slider.y, 0, 0, 0x8000);
}

void DrawStarfield(int cameraX, int cameraY, bool autoScroll) {
    static bool initialized = false;
    static int  starX[480];
    static int  starY[480];
    static int  starType[480];
    static int  autoScrollX = 0;
    static int  autoScrollY = 0;

    if (!initialized) {
        for (int i = 0; i < 480; ++i) {
            starX[i]    = std::rand() % 0x27d;
            starY[i]    = std::rand() % 0x1dd;
            starType[i] = std::rand() % 7;
            if ((std::rand() % 0x32) == 1) {
                starType[i] = (std::rand() % 3) + 7;
            }
        }
        initialized = true;
    }

    int scrollX = cameraX;
    int scrollY = cameraY;

    if (autoScroll) {
        autoScrollX++;
        if (autoScrollX > 0x27d) autoScrollX = 0;
        autoScrollY++;
        if (autoScrollY > 0x1dd) autoScrollY = 0;
        scrollX = autoScrollX;
        scrollY = autoScrollY;
    }

    for (int i = 0; i < 480; ++i) {
        int x = (starX[i] - scrollX) % 0x27d;
        if (x < 0) x += 0x27d;

        int y = (starY[i] - scrollY) % 0x1dd;
        if (y < 0) y += 0x1dd;

        BlitTile(g_StarsBmp, x, y, starType[i], 0, 0x8000);
    }
}

// Copies raw palette indices into the 8-bit screen, like DirectDraw's Blt did.
// No palette lookups happen, so palette tricks (flash, fade) work unchanged.
static void BlitIndexed(const SDL_Surface* src, const SDL_Rect& srcRect,
                        int dstX, int dstY, bool transparent) {
    SDL_assert(src->format->BitsPerPixel == 8);

    // Reject source rects that fall outside the sheet (a bad tile index).
    if (srcRect.x < 0 || srcRect.y < 0 ||
        srcRect.x + srcRect.w > src->w || srcRect.y + srcRect.h > src->h) return;

    // 1. Where the blit lands on screen, before clipping.
    const SDL_Rect wanted = { dstX, dstY, srcRect.w, srcRect.h };

    // 2. Clip it to the screen. Nothing visible: nothing to do.
    const SDL_Rect screen = { 0, 0, g_Screen8Bit->w, g_Screen8Bit->h };
    SDL_Rect visible;
    if (!SDL_IntersectRect(&wanted, &screen, &visible)) return;

    // 3. Shift the source rect by however much was cut off the top and left.
    const SDL_Rect source = { srcRect.x + (visible.x - wanted.x),
                              srcRect.y + (visible.y - wanted.y),
                              visible.w, visible.h };

    // 4. Copy it row by row.
    for (int row = 0; row < source.h; ++row) {
        CopyRow(PixelAt(src,          source.x,  source.y  + row),
                PixelAt(g_Screen8Bit, visible.x, visible.y + row),
                source.w, transparent);
    }
}

void BlitSubRect(const SpriteSheet& sheet, int dstX, int dstY, int srcX, int srcY, int w, int h, uint32_t flags) {
    if (!sheet.surface || !g_Screen8Bit || w <= 0 || h <= 0) return;
    BlitIndexed(sheet.surface, SDL_Rect{ srcX, srcY, w, h }, dstX, dstY, (flags & 0x8000) != 0);
}

static GlomView FindGlomEntry(const std::string& filename) {
    auto it = g_GlomIndex.find(ToLower(filename));
    if (it == g_GlomIndex.end()) return {};

    const GlomEntry& e = it->second;
    // Overflow-safe bounds check against the in-memory archive.
    if (e.offset > g_GlomData.size() || e.size > g_GlomData.size() - e.offset) return {};

    return { g_GlomData.data() + e.offset, e.size };
}

bool GetGlomEntryBytes(const std::string& filename, std::vector<uint8_t>& outBytes) {
    GlomView entry = FindGlomEntry(filename);
    if (!entry) return false;
    outBytes.assign(entry.data, entry.data + entry.size);
    return true;
}

// Option A: LoadGlomFromFile("assets.glom", 0, 0);
// Option B: LoadGlomFromFile("egames.exe", 0x4d2a0, 2666449);
bool LoadGlomFromFile(const std::string& path, size_t baseOffset = 0, size_t blobSize = 0) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;

    std::streampos endPos = file.tellg();
    if (endPos <= 0) return false;
    size_t totalFileSize = static_cast<size_t>(endPos);

    if (baseOffset >= totalFileSize) return false;
    if (blobSize == 0) blobSize = totalFileSize - baseOffset;
    if (baseOffset + blobSize > totalFileSize) return false;

    g_GlomData.resize(blobSize);
    file.seekg(baseOffset);
    if (!file.read(reinterpret_cast<char*>(g_GlomData.data()), blobSize)) return false;

    size_t cursor = 0;
    auto readU32 = [&](uint32_t& out) -> bool {
        if (cursor + 4 > g_GlomData.size()) return false;
        std::memcpy(&out, &g_GlomData[cursor], 4);
        cursor += 4;
        return true;
    };

    uint32_t fileCount = 0;
    if (!readU32(fileCount)) return false;

    g_GlomIndex.clear();
    for (uint32_t i = 0; i < fileCount; ++i) {
        uint32_t nameLen = 0;
        if (!readU32(nameLen)) return false;
        if (cursor + nameLen + 8 > g_GlomData.size()) return false; // Prevent out-of-bounds read!

        std::string name(reinterpret_cast<char*>(&g_GlomData[cursor]), nameLen);
        cursor += nameLen;

        uint32_t offset = 0, size = 0;
        readU32(offset);
        readU32(size);

        g_GlomIndex[ToLower(name)] = { offset, size };
    }
    return true;
}

bool InitEngine(const char* windowTitle, int width, int height) {
    std::string baseDir = "./";
    if (char* rawBase = SDL_GetBasePath()) {
        baseDir = rawBase;
        SDL_free(rawBase);
    }

    // Check next to the executable AND in the parent folder (for build/ runs)
    bool loaded =
        LoadGlomFromFile(baseDir + "assets.glom", 0, 0) ||
        LoadGlomFromFile(baseDir + "../assets.glom", 0, 0) ||
        LoadGlomFromFile(baseDir + "egames.exe", 0x4d2a0, 2666449) ||
        LoadGlomFromFile(baseDir + "../egames.exe", 0x4d2a0, 2666449);

    if (!loaded) {
        SetFatalError("Could not load assets.glom or egames.exe!");
        return false;
    }

    LoadPlayerData();

    g_FontBmp          = LoadSpriteSheet("Font.bmp", 11, 16, true);
    g_WindowBackBmp    = LoadSpriteSheet("WindowBack.bmp", 128, 128, true);
    g_WindowCornersBmp = LoadSpriteSheet("WindowCorners.bmp", 4, 4, true);
    g_WindowSlideBmp   = LoadSpriteSheet("WindowSlide.bmp", 0, 0, true);
    g_StarsBmp         = LoadSpriteSheet("Stars.bmp", 3, 3, true);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SetFatalError(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    if (Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 1024) < 0) {
        std::fprintf(stderr, "Warning: Audio init failed: %s\n", Mix_GetError());
    }

    int freq = 0, channels = 0; Uint16 format = 0;
    if (Mix_QuerySpec(&freq, &format, &channels)) g_AudioFreq = freq;

    g_TinySynth = tsf_load_filename("assets/TimGM6mb.sf2");
    if (!g_TinySynth) g_TinySynth = tsf_load_filename("../assets/TimGM6mb.sf2");
    if (!g_TinySynth) g_TinySynth = tsf_load_filename("/usr/share/soundfonts/FluidR3_GM.sf2");

    if (!g_TinySynth) {
        std::fprintf(stderr, "TSF: Failed to load any .sf2 SoundFont!\n");
    } else {
        tsf_set_output(g_TinySynth, TSF_STEREO_INTERLEAVED, 22050, 0.0f);
        Mix_HookMusic(TinySynthAudioCallback, nullptr);
    }

    g_Window = SDL_CreateWindow(
        windowTitle,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_Window) {
        SetFatalError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    SDL_StartTextInput();

    g_Renderer = SDL_CreateRenderer(g_Window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_Renderer) {
        g_Renderer = SDL_CreateRenderer(g_Window, -1, SDL_RENDERER_SOFTWARE);
    }
    SDL_RenderSetLogicalSize(g_Renderer, width, height);

    g_Screen8Bit    = SDL_CreateRGBSurface(0, width, height, 8, 0, 0, 0, 0);
    g_Screen32Bit   = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    g_ScreenTexture = SDL_CreateTexture(g_Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);

    tsf_set_output(g_TinySynth, TSF_STEREO_INTERLEAVED, g_AudioFreq, 0.0f);

    return (g_Screen8Bit && g_Screen32Bit && g_ScreenTexture);
}

void ShutdownEngine() {
    if (g_ScreenTexture) SDL_DestroyTexture(g_ScreenTexture);
    if (g_Screen32Bit)   SDL_FreeSurface(g_Screen32Bit);
    if (g_Screen8Bit)    SDL_FreeSurface(g_Screen8Bit);
    if (g_Renderer)      SDL_DestroyRenderer(g_Renderer);
    if (g_Window)        SDL_DestroyWindow(g_Window);
    Mix_HookMusic(nullptr, nullptr);
    StopMidiMusic();
    if (g_TinySynth) {
        tsf_close(g_TinySynth);
        g_TinySynth = nullptr;
    }
    Mix_CloseAudio();
    SDL_Quit();
}

int ProcessMessages() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_QuitRequested = true;
            //return 0;
        }
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                g_WindowActive = false;
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                g_WindowActive = true;
            }
        }
        if (event.type == SDL_TEXTINPUT) {
            char ch = event.text.text[0];
            if (ch >= 0x20 && ch <= 0x7E) {
                g_LastTypedChar = ch;
            }
        }
    }
    return g_QuitRequested ? 0 : 1;
}

bool IsWindowActive() {
    return g_WindowActive;
}

SDL_Surface* LoadBmpAsset(const std::string& filename) {
    GlomView entry = FindGlomEntry(filename);
    if (!entry) {
        std::fprintf(stderr, "GLOM: Missing or corrupt BMP '%s'\n", filename.c_str());
        return nullptr;
    }
    return SDL_LoadBMP_RW(SDL_RWFromConstMem(entry.data, static_cast<int>(entry.size)), 1);
}

void SetActivePaletteFromSurface(SDL_Surface* bmpWithPalette) {
    if (bmpWithPalette && bmpWithPalette->format->palette && g_Screen8Bit) {
        SDL_SetPaletteColors(
            g_Screen8Bit->format->palette,
            bmpWithPalette->format->palette->colors,
            0,
            bmpWithPalette->format->palette->ncolors
        );
    }
}

SpriteSheet LoadSpriteSheet(const std::string& filename, int tileW, int tileH, bool hasColorKey) {
    SpriteSheet sheet;
    sheet.filename = filename;
    sheet.surface  = LoadBmpAsset(filename);
    if (sheet.surface) {
        sheet.totalWidth  = sheet.surface->w;
        sheet.totalHeight = sheet.surface->h;
        sheet.tileWidth   = (tileW == 0) ? sheet.totalWidth  : tileW;
        sheet.tileHeight  = (tileH == 0) ? sheet.totalHeight : tileH;
    }
    return sheet;
}

void BlitTile(const SpriteSheet& sheet, int dstX, int dstY, int tileCol, int tileRow, uint32_t flags) {
    if (!sheet.surface || !g_Screen8Bit) return;
    const SDL_Rect src = { tileCol * sheet.tileWidth, tileRow * sheet.tileHeight,
                           sheet.tileWidth, sheet.tileHeight };
    BlitIndexed(sheet.surface, src, dstX, dstY, (flags & 0x8000) != 0);
}

void ClearScreen(uint8_t colorIndex) {
    if (g_Screen8Bit) {
        SDL_FillRect(g_Screen8Bit, nullptr, colorIndex);
    }
}

void BlitSprite(SDL_Surface* src, int dstX, int dstY, SDL_Rect* srcRect, bool transparent) {
    if (!src || !g_Screen8Bit) return;
    const SDL_Rect whole = { 0, 0, src->w, src->h };
    BlitIndexed(src, srcRect ? *srcRect : whole, dstX, dstY, transparent);
}

void FlipScreen() {
    if (!g_Screen8Bit || !g_Screen32Bit || !g_ScreenTexture || !g_Renderer) return;
    SDL_BlitSurface(g_Screen8Bit, nullptr, g_Screen32Bit, nullptr);
    SDL_UpdateTexture(g_ScreenTexture, nullptr, g_Screen32Bit->pixels, g_Screen32Bit->pitch);
    SDL_RenderClear(g_Renderer);
    SDL_RenderCopy(g_Renderer, g_ScreenTexture, nullptr, nullptr);
    SDL_RenderPresent(g_Renderer);
}

Mix_Chunk* LoadWavAsset(const std::string& filename) {
    auto it = g_GlomIndex.find(ToLower(filename));
    if (it == g_GlomIndex.end()) {
        std::fprintf(stderr, "GLOM: Missing WAV '%s'\n", filename.c_str());
        return nullptr;
    }
    SDL_RWops* rw = SDL_RWFromConstMem(&g_GlomData[it->second.offset], it->second.size);
    return Mix_LoadWAV_RW(rw, 1);
}

void PlaySoundEffect(Mix_Chunk* chunk) {
    if (chunk) {
        Mix_PlayChannel(-1, chunk, 0);
    }
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
    if (channel != -1) {
        Mix_HaltChannel(channel);
        channel = -1;
    }
}

void SoundEffect::Free() {
    Stop();
    if (chunk) {
        Mix_FreeChunk(chunk);
        chunk = nullptr;
    }
}

void SetFatalError(const std::string& msg) {
    g_FatalErrorMsg = msg;
}

bool CheckForPendingError() {
    return !g_FatalErrorMsg.empty();
}

void ShowFatalErrorDialog() {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", g_FatalErrorMsg.c_str(), g_Window);
}

int SetOrGetGameState(int newState) {
    int prev = g_CurrentState;
    if (newState >= 0) {
        g_CurrentState  = newState;
        g_PreviousState = prev;
        ClearTypedChar();
        return newState;
    }
    return (newState == -2) ? g_PreviousState : g_CurrentState;
}