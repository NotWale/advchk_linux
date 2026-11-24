#pragma once

#include "alien.hpp"
#include "engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Map and screen geometry --------------------------------------------------
constexpr int kMapWidth     = 240;
constexpr int kMapHeight    = 132;
constexpr int kMapTileCount = kMapWidth * kMapHeight;   // 0x7bc0

constexpr int kTilePx        = 16;
constexpr int kViewTilesX    = 40;
constexpr int kViewTilesY    = 31;
constexpr int kViewYOffsetPx = -8;    // the playfield is drawn 8 px above the screen top

// The ship is always drawn in the middle of the view; the camera follows it.
constexpr int kShipCenterTileX = kViewTilesX / 2;
constexpr int kShipCenterTileY = kViewTilesY / 2;

constexpr int kMaxFuel = 300;
constexpr int kLevelCount = 30;

// Foreground tile ids (values stored in MainGameState::fgMap) --------------
namespace Tile {

constexpr uint8_t Empty = 0;
constexpr uint8_t Wall = 1;
constexpr uint8_t Hazard = 2;
constexpr uint8_t OrbSwitch = 3;
// Conveyor streams (they also rotate 4 -> 5 -> 6 -> 7 when the orb switch is pressed).
constexpr uint8_t ConveyorUp = 4;
constexpr uint8_t ConveyorRight = 5;
constexpr uint8_t ConveyorDown = 6;
constexpr uint8_t ConveyorLeft = 7;
constexpr uint8_t Warp = 8;
constexpr uint8_t ExitClosed = 9;       // the reactor
constexpr uint8_t ExitOpen = 10;
constexpr uint8_t FuelPad = 0x0b;
constexpr uint8_t FuelCanister = 0x0c;
constexpr uint8_t Barrier = 0x0d;       // locked until the reactor is destroyed
constexpr uint8_t GemSmall = 0x0e;
constexpr uint8_t GemMedium = 0x0f;
constexpr uint8_t GemLarge = 0x10;
constexpr uint8_t Boulder = 0x11;
constexpr uint8_t ReactorSwitch = 0x12;
constexpr uint8_t ReactorSwitchPressed = 0x13;

// Transport pipes. Entering at a mouth locks the ship into the pipe.
constexpr uint8_t FirstPipe = 0x14;
constexpr uint8_t FirstPipeCorner = 0x16;
constexpr uint8_t LastPipeCorner = 0x19;
constexpr uint8_t PipeMouthDown = 0x1a;
constexpr uint8_t PipeMouthLeft = 0x1b;
constexpr uint8_t PipeMouthUp = 0x1c;
constexpr uint8_t PipeMouthRight = 0x1d;
constexpr uint8_t BeamEmitterUp = 0x1e;      // also toggles "hidden in tube" when entered
constexpr uint8_t BeamEmitterRight = 0x1f;
constexpr uint8_t BeamEmitterDown = 0x20;
constexpr uint8_t BeamEmitterLeft = 0x21;
constexpr uint8_t LastPipe = 0x22;

// Spawn values in the level data. Alien and chicken markers stay in the map as live markers.
constexpr uint8_t AlienMarker = 0xfd;
constexpr uint8_t ChickenMarker = 0xfe;
constexpr uint8_t PlayerSpawn = 0xff;

constexpr bool IsPipe(uint8_t tile)        { return tile >= FirstPipe && tile <= LastPipe; }
constexpr bool IsPipeCorner(uint8_t tile)  { return tile >= FirstPipeCorner && tile <= LastPipeCorner; }
constexpr bool IsBeamEmitter(uint8_t tile) { return tile >= BeamEmitterUp && tile <= BeamEmitterLeft; }

}

// Directions ----------
// Headings are used by the tube, the chicken-collect animation and alien attacks.
enum Heading : int { kHeadingNone = 0, kHeadingUp, kHeadingRight, kHeadingDown, kHeadingLeft };

struct TileOffset {
    int deltaX;
    int deltaY;
};

// Unit steps indexed by Heading (none, up, right, down, left).
inline constexpr TileOffset kHeadingOffsets[5] = { {0, 0}, {0, -1}, {1, 0}, {0, 1}, {-1, 0} };

// Unit steps in the order the original's random-walk code uses: down, right, up, left.
inline constexpr TileOffset kWalkOffsets[4] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };

// Ship facing values (MainGameState::shipFacing) ---------------------------
// 0 = idle, 1..4 = Heading, 5..13 = explosion frames, 15 = zap shimmer.
constexpr int kShipFacingIdle           = 0;
constexpr int kShipFacingFirstExplosion = 5;
constexpr int kShipFacingLastExplosion  = 13;
constexpr int kShipFacingZapShimmer     = 15;

// Game state ----------
struct WarpGate {
    int sourceX = 0, sourceY = 0;
    int destinationX = 0, destinationY = 0;
};

struct ChickenEntity {
    int tileX = 0, tileY = 0;
    int stepTimer = 0;   // updates since the last random step
    int frame     = 0;   // Chicken.bmp column: facing (0 down, 2 right, 4 up, 6 left) + wing flap

    bool IsRemoved() const { return tileX < 0 || tileY < 0; }
    void Remove()          { tileX = tileY = -1; }
};

struct MainGameState {
    // Level
    bool    levelLoaded = false;
    uint8_t fgMap[kMapTileCount] = {0};   // tiles plus live alien / chicken markers
    uint8_t bgMap[kMapTileCount] = {0};   // static laser beams: 1 vertical, 2 horizontal, 3 both
    std::vector<WarpGate>      warps;
    std::vector<ChickenEntity> chickens;
    std::vector<AlienEntity>   aliens;
    int reactorX = 0, reactorY = 0;
    bool reactorDestroyed   = false;
    bool levelExitTriggered = false;
    int  exitDoorFrame      = 0;

    // Ship
    int  tileX = 0, tileY = 0;
    int  spawnX = 0, spawnY = 0;
    int  shipFacing    = kShipFacingIdle;
    int  tubeDir       = kHeadingNone;   // heading while riding a transport pipe
    bool inTubeHidden  = false;
    bool warpTriggered = false;
    int  collectDir    = kHeadingNone;   // chicken being beamed up, as a Heading
    int  collectFrame  = 0;              // 0..8 slurp animation frames
    int  fuel          = kMaxFuel;
    int  stepsSinceFuelUse = 0;
    int  walkStepPhase     = 0;          // slows normal walking to every 2nd call

    // Progress
    int chickensRescued = 0, totalChickens = 0;
    int aliensDefeated  = 0, totalAliens   = 0;
    int escapeCountdown = -1;            // seconds left after the reactor blew; -1 = inactive
    uint32_t countdownTimer = 0;

    // Presentation
    int      displayedFuel  = 0;
    int      displayedScore = 0;
    uint32_t frameCounter   = 0;         // 0..255, drives sprite animation
    bool     paused           = false;
    bool     exitConfirmOpen  = false;
    int      verseOverlayId   = 0;       // 0 = none, else 1-based Bible verse shown at level start
    bool     orbSwitchPressed = false;
    uint32_t orbSwitchTime    = 0;
};

extern MainGameState g_Game;

// Map helpers ---------
constexpr int  TileIndex(int tileX, int tileY)   { return tileY * kMapWidth + tileX; }
constexpr bool IsInsideMap(int tileX, int tileY) {
    return tileX >= 0 && tileY >= 0 && tileX < kMapWidth && tileY < kMapHeight;
}

// Foreground tile access. FgRef requires a tile inside the map; FgAt treats everything
// outside as a wall.
inline uint8_t& FgRef(int tileX, int tileY) { return g_Game.fgMap[TileIndex(tileX, tileY)]; }
inline uint8_t  FgAt(int tileX, int tileY) {
    return IsInsideMap(tileX, tileY) ? g_Game.fgMap[TileIndex(tileX, tileY)] : Tile::Wall;
}

inline PlayerProfile& CurrentProfile() { return g_PlayerSlots[g_CurrentPlayerIndex]; }

// Gameplay (gameplay.cpp) --------------------------------------------------
void MovePlayerShip(int deltaX, int deltaY);

// Level loading (level.cpp) ------------------------------------------------
void LoadCurrentLevel();

// Drawing (gameplay_render.cpp) --------------------------------------------
struct Camera {
    int leftTile;
    int topTile;
};

void DrawWorld(const Camera& camera);
void DrawPlayerShip();
void DrawEscapeCountdown();
void DrawHud(const PlayerProfile& profile);
void DrawExitConfirmDialog();
// Box with a prompt line and "ENTER=Accept ESCAPE=Cancel" (also used by the profile screen).
void DrawConfirmDialog(int promptLeft, const std::string& prompt);
void DrawPausedBanner();
void DrawBibleVerseOverlay(int verseId);
