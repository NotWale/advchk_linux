#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <cstdint>
#include <string>
#include <vector>

// ---- Win32 compatibility ----------------------------------------------------
uint32_t timeGetTime();                      // milliseconds since SDL started
uint16_t GetAsyncKeyState(int virtualKey);   // kKeyDownBit is set while the key is held

constexpr uint16_t kKeyDownBit = 0x8000;

// ---- Blit flags (the original DirectDraw DDBLT_KEYSRC bit) ------------------
constexpr uint32_t kBlitOpaque   = 0;
constexpr uint32_t kBlitColorKey = 0x8000;   // palette index 0 is transparent

// ---- Sprite sheets ----------------------------------------------------------
struct SpriteSheet {
    SDL_Surface* surface     = nullptr;
    int          totalWidth  = 0;
    int          totalHeight = 0;
    int          tileWidth   = 0;
    int          tileHeight  = 0;
    std::string  filename;
};

// tileWidth / tileHeight of 0 mean "the whole sheet is one tile".
// hasColorKey is currently ignored: transparency is chosen per blit via kBlitColorKey.
SpriteSheet LoadSpriteSheet(const std::string& filename, int tileWidth = 0, int tileHeight = 0,
                            bool hasColorKey = true);
void BlitTile(const SpriteSheet& sheet, int dstX, int dstY, int tileColumn = 0, int tileRow = 0,
              uint32_t flags = kBlitColorKey);
void BlitSubRect(const SpriteSheet& sheet, int dstX, int dstY, int srcX, int srcY,
                 int width, int height, uint32_t flags = kBlitColorKey);

// ---- UI widgets -------------------------------------------------------------
// Field order matters: the options screen aggregate-initialises this positionally.
struct SliderWidget {
    int width        = 88;
    int numSteps     = 5;
    int currentValue = 0;
    int left         = 320;
    int top          = 185;
};

// ---- Sound ------------------------------------------------------------------
struct SoundEffect {
    Mix_Chunk*  chunk     = nullptr;
    int         maxVoices = 1;
    int         channel   = -1;
    std::string filename;

    bool IsPlaying() const;
    void Load(const std::string& wavFilename, int voices = 1);
    void Play(int loopCount = 0);
    void Stop();
    void Free();
};

// ---- Player profiles --------------------------------------------------------
constexpr int kMaxPlayerSlots      = 12;
constexpr int kMaxPlayerNameLength = 12;

struct PlayerProfile {
    char name[36]    = {0}; // +0x00 (only the first kMaxPlayerNameLength chars are used)
    int  active      = 0;   // +0x24
    int  level       = 0;   // +0x28
    int  chickens    = 0;   // +0x2c
    char extra[16]   = {0}; // +0x30
    int  score       = 0;   // +0x40
    int  reserved[3] = {0}; // +0x44..0x4f (pads the struct to the original 0x50 bytes)
};

extern PlayerProfile g_PlayerSlots[kMaxPlayerSlots];
extern int           g_CurrentPlayerIndex;

// ---- Settings (persisted in Chickens.ini) -----------------------------------
extern int  g_Difficulty;   // 0..4 (default 2 = Typical)
extern bool g_SoundEnabled; // default true
extern bool g_MusicEnabled; // default true
extern bool g_UseJoystick;  // default false
extern bool g_AlwaysRun;    // default false

void SavePlayerData();
void LoadPlayerData();

// ---- Engine / platform layer (replaces 0040cxxx - 0040fxxx) -----------------
bool InitEngine(const char* windowTitle, int width = 640, int height = 480);
void ShutdownEngine();
bool ProcessMessages();      // pumps events; returns false once the user asked to quit
bool IsWindowActive();

// ---- Graphics (8-bit palettized software surface -> SDL2 renderer) ----------
void ClearScreen(uint8_t colorIndex = 0);
void FlipScreen();
void FillScreenRect(int left, int top, int width, int height, uint8_t colorIndex);
void DrawBitmapText(int left, int top, const std::string& text, bool opaque = false);
void DrawWindowBox(int left, int top, int width, int height);
void DrawSliderWidget(const SliderWidget& slider);
void DrawStarfield(int cameraX, int cameraY, bool autoScroll);
void FadeOutToBlack(int seconds = 1);
void DimScreen(float factor = 0.4f);
void SetPaletteEntry(int index, uint8_t red, uint8_t green, uint8_t blue);
SDL_Color GetPaletteEntry(int index);
void SetActivePaletteFromSurface(SDL_Surface* bmpWithPalette);

// ---- Assets -----------------------------------------------------------------
bool GetGlomEntryBytes(const std::string& filename, std::vector<uint8_t>& outBytes);

// ---- Audio ------------------------------------------------------------------
void PlayMidiMusic(const std::string& mdsFilename, bool loop = true);
void StopMidiMusic();
bool IsAnySoundPlaying();

// ---- Input ------------------------------------------------------------------
bool WasKeyJustPressed(int virtualKey);
char GetTypedChar();         // last typed printable char, consumed on read; 0 if none
void ClearTypedChar();

// ---- Errors -----------------------------------------------------------------
void SetFatalError(const std::string& message);
bool CheckForPendingError();
void ShowFatalErrorDialog();

// ---- Game state machine -----------------------------------------------------
constexpr int kQueryCurrentState  = -1;
constexpr int kQueryPreviousState = -2;

// newState >= 0 switches state and returns it; otherwise returns the current
// (kQueryCurrentState) or previous (kQueryPreviousState) state.
int SetOrGetGameState(int newState = kQueryCurrentState);
