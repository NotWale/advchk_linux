#include "gameplay.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

// Levels.dat layout:
//   header: a table of 3-byte big-endian level offsets at kLevelTableOffset
//   level:  run-length coded tiles, kMapTileCount in total
//     warp tile (raw value 8):   [tile byte][destination x][destination y]
//     any other tile:            [tile byte][count low byte]  (the tile byte's top 2 bits are count bits 8..9)
// In the tile byte, the tile id is stored as (id + 3) in its low 7 bits.

namespace {

constexpr size_t  kLevelTableOffset = 0x0d;
constexpr uint8_t kTileEncodingBias = 3;
constexpr int     kLevelMusicCount  = 5;

// Where the beam of a BeamEmitter travels, and the bgMap bit it sets (1 vertical, 2 horizontal).
struct BeamEmitter {
    uint8_t    tile;
    TileOffset direction;
    uint8_t    orientation;
};
constexpr BeamEmitter kBeamEmitters[] = {
    { Tile::BeamEmitterUp,    {  0, -1 }, 1 },
    { Tile::BeamEmitterRight, {  1,  0 }, 2 },
    { Tile::BeamEmitterDown,  {  0,  1 }, 1 },
    { Tile::BeamEmitterLeft,  { -1,  0 }, 2 },
};

constexpr const char* kLevelTracks[kLevelMusicCount] = {
    "Run.mds", "Organjam.mds", "Sweet.mds", "Jamin.mds", "Rock1.mds"
};

void ResetLevelState() {
    std::memset(g_Game.fgMap, 0, sizeof(g_Game.fgMap));
    std::memset(g_Game.bgMap, 0, sizeof(g_Game.bgMap));
    g_Game.warps.clear();
    g_Game.chickens.clear();
    g_Game.aliens.clear();
    g_Game.chickensRescued  = 0;
    g_Game.totalChickens    = 0;
    g_Game.aliensDefeated   = 0;
    g_Game.totalAliens      = 0;
    g_Game.reactorDestroyed = false;
    g_Game.tubeDir          = kHeadingNone;
    g_Game.inTubeHidden     = false;
    g_Game.shipFacing       = kShipFacingIdle;
    g_Game.fuel             = kMaxFuel;
}

// Registers the entity or position a spawn tile stands for.
void HandleSpawnTile(uint8_t tile, int tileIndex) {
    const int tileX = tileIndex % kMapWidth;
    const int tileY = tileIndex / kMapWidth;

    switch (tile) {
        case Tile::ExitClosed:
            g_Game.reactorX = tileX;
            g_Game.reactorY = tileY;
            break;
        case Tile::AlienMarker: {
            AlienEntity alien;
            alien.tileX = tileX;
            alien.tileY = tileY;
            g_Game.aliens.push_back(alien);
            ++g_Game.totalAliens;
            break;
        }
        case Tile::ChickenMarker: {
            ChickenEntity chicken;
            chicken.tileX = tileX;
            chicken.tileY = tileY;
            g_Game.chickens.push_back(chicken);
            ++g_Game.totalChickens;
            break;
        }
        case Tile::PlayerSpawn:
            g_Game.spawnX = g_Game.tileX = tileX;
            g_Game.spawnY = g_Game.tileY = tileY;
            g_Game.fgMap[tileIndex] = Tile::Empty;   // the spawn point itself is just floor
            break;
        default:
            break;
    }
}

void DecodeTiles(const std::vector<uint8_t>& levelData, size_t position) {
    int tileIndex = 0;
    while (tileIndex < kMapTileCount && position < levelData.size()) {
        const uint8_t rawByte = levelData[position++];
        const uint8_t tile    = static_cast<uint8_t>((rawByte & 0x7f) - kTileEncodingBias);

        if (tile == Tile::Warp) {
            g_Game.fgMap[tileIndex] = Tile::Warp;
            if (position + 2 <= levelData.size()) {
                WarpGate gate;
                gate.sourceX      = tileIndex % kMapWidth;
                gate.sourceY      = tileIndex / kMapWidth;
                gate.destinationX = std::clamp<int>(levelData[position++], 0, kMapWidth - 1);
                gate.destinationY = std::clamp<int>(levelData[position++], 0, kMapHeight - 1);
                g_Game.warps.push_back(gate);
            }
            ++tileIndex;
            continue;
        }

        if (position >= levelData.size()) break;
        const int countLowByte = levelData[position++];
        const int runLength    = ((rawByte & 0xc0) << 2) + countLowByte;
        for (int run = 0; run < runLength && tileIndex < kMapTileCount; ++run, ++tileIndex) {
            g_Game.fgMap[tileIndex] = tile;
            HandleSpawnTile(tile, tileIndex);
        }
    }
}

// Casts the static laser beams of every emitter into the background map.
void CastEmitterBeams() {
    for (int tileIndex = 0; tileIndex < kMapTileCount; ++tileIndex) {
        const uint8_t tile = g_Game.fgMap[tileIndex];
        if (!Tile::IsBeamEmitter(tile)) continue;

        const BeamEmitter& emitter = kBeamEmitters[tile - Tile::BeamEmitterUp];
        int beamX = tileIndex % kMapWidth + emitter.direction.deltaX;
        int beamY = tileIndex / kMapWidth + emitter.direction.deltaY;
        for (; IsInsideMap(beamX, beamY); beamX += emitter.direction.deltaX, beamY += emitter.direction.deltaY) {
            if (Tile::IsBeamEmitter(FgRef(beamX, beamY))) break;   // another emitter ends the beam
            uint8_t& beam = g_Game.bgMap[TileIndex(beamX, beamY)];
            beam = (beam != 0) ? 3 : emitter.orientation;           // crossing beams = 3
        }
    }
}

}  

void LoadCurrentLevel() {
    g_Game.warpTriggered      = false;
    g_Game.collectDir         = kHeadingNone;
    g_Game.collectFrame       = 0;
    g_Game.exitDoorFrame      = 0;
    g_Game.escapeCountdown    = -1;
    g_Game.levelExitTriggered = false;
    g_Game.tubeDir            = kHeadingNone;
    g_Game.inTubeHidden       = false;

    PlayerProfile& profile = CurrentProfile();
    if (profile.level < 1 || profile.level > kLevelCount) profile.level = 1;
    SavePlayerData();

    std::vector<uint8_t> levelData;
    if (!GetGlomEntryBytes("Levels.dat", levelData)) {
        std::fprintf(stderr, "Missing Levels.dat in assets.glom!\n");
        return;
    }

    const size_t headerOffset = static_cast<size_t>(profile.level) * 3 + kLevelTableOffset;
    if (headerOffset + 3 > levelData.size()) return;

    const size_t levelStart = (static_cast<size_t>(levelData[headerOffset]) << 16) |
                              (static_cast<size_t>(levelData[headerOffset + 1]) << 8) |
                               static_cast<size_t>(levelData[headerOffset + 2]);

    ResetLevelState();
    DecodeTiles(levelData, levelStart);
    CastEmitterBeams();

    if (g_MusicEnabled) {
        PlayMidiMusic(kLevelTracks[(profile.level - 1) % kLevelMusicCount], true);
    }
}
