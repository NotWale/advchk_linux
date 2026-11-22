#pragma once
#include <cstdint> 
#include <vector>
#include "alien.hpp"

struct WarpGate {
    int srcX = 0, srcY = 0;
    int dstX = 0, dstY = 0;
};

struct ChickenEntity {
    int x = 0, y = 0;
    int rescued = 0;
    int anim = 0;
};

struct MainGameState {
    bool     levelLoaded       = false;
    bool     warpTriggered     = false;
    uint8_t  fgMap[240 * 132]  = {0};
    uint8_t  bgMap[240 * 132]  = {0};

    int      tileX             = 0;
    int      tileY             = 0;
    int      spawnX            = 0;
    int      spawnY            = 0;
    int      shipFacing        = 0;
    int      tubeDir           = 0;
    bool     inTubeHidden      = false;

    int      reactorX          = 0;
    int      reactorY          = 0;
    bool     reactorDestroyed  = false;
    bool     levelExitTriggered= false;
    int      exitDoorFrame     = 0;

    int      chickensRescued   = 0;
    int      totalChickens     = 0;
    int      aliensDefeated    = 0;
    int      totalAliens       = 0;
    int      fuel              = 300;
    int      displayedFuel     = 0;
    int      displayedScore    = 0;

    int collectDir             = 0; // 0 = none, 1 = Up, 2 = Right, 3 = Down, 4 = Left
    int collectFrame           = 0; // 0..8 slurp animation frames

    int      escapeCountdown   = -1;
    uint32_t escapeTimer       = 0;
    uint32_t countdownTimer    = 0;
    uint32_t moveTickTimer     = 0;
    uint32_t frameCounter      = 0;
    int      fuelStepCounter   = 0;
    int      slowStepToggle    = 0;

    bool     pauseFlag         = false;
    bool     exitConf          = false;
    bool     deathTrigger      = false;

    int messageOverlay         = 0; // 0 = playing, 1..N = Bible verse index shown at level start
    int      ballOrbActive     = 0;
    uint32_t lastBallSwitchTime= 0;

    std::vector<WarpGate>      warps;
    std::vector<ChickenEntity> chickens;
    std::vector<AlienEntity>   aliens;
};

extern MainGameState g_Game;

uint8_t& FgRef(int x, int y);
uint8_t  FgAt (int x, int y);

// Shared direction tables (down, right, up, left)
inline constexpr int kDX[4] = { 0, 1, 0, -1 };
inline constexpr int kDY[4] = { 1, 0, -1, 0 };

// Shared assets used by alien.cpp and other files
extern SpriteSheet g_AliensBmp;
extern SoundEffect g_SfxHorn;
extern SoundEffect g_SfxBroken;
extern SoundEffect g_SfxAlienhit;

void MovePlayerShip(int dx, int dy);

void InitMenuAndSharedAssets();
void StateSplash();
void State_MainMenu();
void State_Credits();
void State_Instructions();
void State_Options();
void State_PlayerProfile();
void State_MainGame();
void State_LevelSummary();
void State_Ending();