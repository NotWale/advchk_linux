#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <cstdint>
#include <string>
#include <vector>

// --- Win32 Compatibility ---
uint32_t timeGetTime();
uint16_t GetAsyncKeyState(int vKey);

struct SpriteSheet {
    SDL_Surface* surface     = nullptr;
    int          totalWidth  = 0;
    int          totalHeight = 0;
    int          tileWidth   = 0;
    int          tileHeight  = 0;
    std::string  filename;
};

struct SliderWidget {
    int width        = 88;
    int numSteps     = 5;
    int currentValue = 0;
    int x            = 320;
    int y            = 185;
};

struct SoundEffect {
    Mix_Chunk*  chunk     = nullptr;
    int         maxVoices = 1;
    int         channel   = -1;
    std::string filename;

    bool IsPlaying() const;
    void Load(const std::string& wavFilename, int voices = 1);
    void Play(int loop = 0);
    void Stop();
    void Free();
};

struct PlayerProfile {
    char name[36]    = {0}; // +0x00 (36 bytes, max 12 chars displayed/typed)
    int  active      = 0;   // +0x24
    int  level       = 0;   // +0x28
    int  chickens    = 0;   // +0x2c
    char extra[16]   = {0}; // +0x30 (16 bytes)
    int  score       = 0;   // +0x40
    int  reserved[3] = {0}; // +0x44..0x4f (pads struct to exact 0x50 = 80 bytes)
};

extern PlayerProfile g_PlayerSlots[12];
extern int           g_CurrentPlayerIndex;

extern int  g_Difficulty;   // 0..4 (default 2 = Typical)
extern bool g_SoundEnabled; // default true
extern bool g_MusicEnabled; // default true
extern bool g_UseJoystick;  // default false
extern bool g_AlwaysRun;    // default false

// --- Engine / Platform Layer (replaces 0040cxxx - 0040fxxx) ---
bool InitEngine(const char* windowTitle, int width = 640, int height = 480);
void ShutdownEngine();
int  ProcessMessages();
bool IsWindowActive(); 

// --- Graphics (8-bit Palettized Software Surface -> SDL2 Renderer) ---
SDL_Surface* LoadBmpAsset(const std::string& filename);
void SetActivePaletteFromSurface(SDL_Surface* bmpWithPalette);
void ClearScreen(uint8_t colorIndex = 0);
void BlitSprite(SDL_Surface* src, int dstX, int dstY, SDL_Rect* srcRect = nullptr, bool transparent = false);
void FlipScreen();
void DrawBitmapText(int x, int y, const std::string& text, bool opaque = false);
void DrawSliderWidget(const SliderWidget& slider);
void DrawStarfield(int cameraX, int cameraY, bool autoScroll);
void FadeOutToBlack(int seconds = 1);
void FillScreenRect(int x, int y, int w, int h, uint8_t colorIndex);
void BlitSubRect(const SpriteSheet& sheet, int dstX, int dstY, int srcX, int srcY, int w, int h, uint32_t flags = 0x8000);
bool GetGlomEntryBytes(const std::string& filename, std::vector<uint8_t>& outBytes);
void DimScreen(float factor = 0.4f);
void SetPaletteEntry(int index, uint8_t r, uint8_t g, uint8_t b);
SDL_Color GetPaletteEntry(int index);
void DrawWindowBox(int boxX, int boxY, int boxWidth, int boxHeight);

// --- Audio ---
Mix_Chunk* LoadWavAsset(const std::string& filename);
void PlaySoundEffect(Mix_Chunk* chunk);
void PlayMidiMusic(const std::string& mdsFilename, bool loop = true);
void StopMidiMusic();
bool IsAnySoundPlaying();

// --- Error Handling ---
void SetFatalError(const std::string& msg);
bool CheckForPendingError();
void ShowFatalErrorDialog();
void ClearTypedChar();

// --- Player ---
void SavePlayerData();
void LoadPlayerData();
char GetTypedChar();

// --- Game State Machine ---
int  SetOrGetGameState(int newState = -1);
bool InitApp();
int  RunFrame();

bool WasKeyJustPressed(int vKey);

SpriteSheet LoadSpriteSheet(const std::string& filename, int tileW = 0, int tileH = 0, bool hasColorKey = true);
void BlitTile(const SpriteSheet& sheet, int dstX, int dstY, int tileCol = 0, int tileRow = 0, uint32_t flags = 0x8000);