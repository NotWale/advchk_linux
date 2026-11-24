#include "alien.hpp"
#include "game_assets.hpp"
#include "gameplay.hpp"
#include "virtual_keys.hpp"

#include <algorithm>
#include <cstdlib>

namespace {

// Sprite frames (Aliens.bmp columns) -----------------------------------
constexpr int kLastWalkFrame    = 5;
constexpr int kAttackFrame      = 6;
constexpr int kStunnedFrame     = 7;
constexpr int kFirstDyingFrame  = 8;
constexpr int kLastDyingFrame   = 14;

// Behaviour tuning (frames per step) ----------------------------------
constexpr int kChaseInterval    = 5;
constexpr int kFleeInterval     = 2;
constexpr int kSlowInterval     = 7;      // rest, wander, pattern, re-plan, dying
constexpr int kStunnedInterval  = 15;

constexpr int kRestAfterMoves       = 45;
constexpr int kReplanAfterBlocked   = 20;
constexpr int kAttackFuelDamage     = 50;

// Pattern table: 4 patterns, stride 7, 8 steps each (so neighbours overlap by
// one entry; the last entry is only read by pattern 3). Values index kWalkOffsets
// directly (0=down 1=right 2=up 3=left); 4 = stand still. Verified against the
// original: jump table at 0x403bb0 and data at 0x421000.
constexpr int     kPatternStride = 7;
constexpr int     kPatternSteps  = 8;
constexpr uint8_t kPatternMoves[] = {
    1,2,2,3,3,3,4,   2,3,3,4,4,4,1,   3,4,4,1,1,1,2,   4,1,1,2,2,2,3,   0
};
constexpr uint8_t kNoMove = 4;

int  Sign(int value)                { return (value > 0) - (value < 0); }
int  AlienRand(int upperBound)      { return std::rand() % upperBound; }
bool OneIn(int chanceDenominator)   { return AlienRand(chanceDenominator) == 0; }

bool IsHornActive() {
    return g_Sounds.horn.IsPlaying()
        || (GetAsyncKeyState(Vk::H)       & kKeyDownBit)
        || (GetAsyncKeyState(Vk::Numpad8) & kKeyDownBit);
}

// Keep the zap shimmer from interrupting a ship explosion.
void ShowZapShimmer() {
    if (g_Game.shipFacing < kShipFacingFirstExplosion) g_Game.shipFacing = kShipFacingZapShimmer;
}

void MoveTo(AlienEntity& alien, int targetX, int targetY) {
    FgRef(alien.tileX, alien.tileY) = 0;
    FgRef(targetX, targetY)         = Tile::AlienMarker;
    alien.tileX = targetX;
    alien.tileY = targetY;
}

// State handlers --

// Chase (direction = +1) or flee (direction = -1): each axis steps independently.
void ChaseOrFlee(AlienEntity& alien, int direction, int interval) {
    const int shipX = g_Game.tileX;
    const int shipY = g_Game.tileY;

    int deltaX = Sign(shipX - alien.tileX) * direction;
    int deltaY = Sign(shipY - alien.tileY) * direction;
    if (FgAt(alien.tileX + deltaX, alien.tileY) != 0) deltaX = 0;
    if (FgAt(alien.tileX, alien.tileY + deltaY) != 0) deltaY = 0;

    const int targetX = alien.tileX + deltaX;
    const int targetY = alien.tileY + deltaY;
    const bool targetIsShip = (targetX == shipX && targetY == shipY);

    alien.interval = interval;
    if (targetIsShip || FgAt(targetX, targetY) != 0) {   // also true when deltaX = deltaY = 0
        ++alien.blocked;
        return;
    }
    MoveTo(alien, targetX, targetY);
    ++alien.moves;
}

// Wander and pattern steps never enter the ship's row or column.
void TryStep(AlienEntity& alien, int deltaX, int deltaY) {
    const int targetX = alien.tileX + deltaX;
    const int targetY = alien.tileY + deltaY;
    if (FgAt(targetX, targetY) != 0)                                return;
    if (targetX == g_Game.tileX || targetY == g_Game.tileY)         return;
    MoveTo(alien, targetX, targetY);
}

void Rest(AlienEntity& alien) {
    alien.moves = 0;
    ++alien.blocked;
    alien.interval = kSlowInterval;
}

void Wander(AlienEntity& alien) {
    const int direction = AlienRand(4);
    TryStep(alien, kWalkOffsets[direction].deltaX, kWalkOffsets[direction].deltaY);
    if (OneIn(7)) alien.state = AlienState::Chase;
    alien.interval = kSlowInterval;
}

void Attack(AlienEntity& alien) {
    if (alien.attackDir != AttackDir::None) {
        // AttackDir uses the same numbering as Heading: the ship is pushed away from the alien.
        const TileOffset& knockback = kHeadingOffsets[static_cast<int>(alien.attackDir)];
        MovePlayerShip(knockback.deltaX, knockback.deltaY);
    }

    g_Game.fuel = std::max(0, g_Game.fuel - kAttackFuelDamage);
    alien.frame = kAttackFrame;
    SetPaletteEntry(1, 255, 0, 0);                  // red flash
    g_Sounds.alienHit.Play(0);
    alien.attackDir = AttackDir::None;
    alien.state     = AlienState::Flee;
}

void FollowPattern(AlienEntity& alien) {
    if (alien.step == 0) alien.pattern = AlienRand(4);
    const uint8_t move = kPatternMoves[alien.pattern * kPatternStride + alien.step];
    if (move != kNoMove) TryStep(alien, kWalkOffsets[move].deltaX, kWalkOffsets[move].deltaY);

    if (++alien.step >= kPatternSteps) {
        alien.step = alien.pattern = 0;
        alien.state = AlienState::Chase;
    }
    alien.interval = kSlowInterval;
}

void Stunned(AlienEntity& alien) {
    alien.frame    = kStunnedFrame;
    alien.interval = kStunnedInterval;
    if (OneIn(3)) alien.state = AlienState::Chase;
}

void Replan(AlienEntity& alien) {
    alien.frame    = 0;
    alien.interval = kSlowInterval;
    alien.blocked  = alien.moves = 0;
    switch (AlienRand(3)) {
        case 0:  alien.state = AlienState::Wander;  break;
        case 1:  alien.state = AlienState::Pattern; break;
        default: alien.state = AlienState::Chase;   break;
    }
}

void Dying(AlienEntity& alien) {
    ShowZapShimmer();
    alien.frame = std::max(alien.frame, kFirstDyingFrame);
    if (alien.frame == kLastDyingFrame) {
        if (g_Game.shipFacing == kShipFacingZapShimmer) g_Game.shipFacing = 0;   // original writes 0
        ++g_Game.aliensDefeated;
        FgRef(alien.tileX, alien.tileY) = 0;
        alien.Remove();
    }
    alien.interval = kSlowInterval;
}

// Per-frame update steps ----------------------------------------------

// The fg-map marker decides life and death: a stunned alien whose marker was
// overwritten (ship shorted it) starts dying; everyone else re-asserts it.
void SyncMapMarker(AlienEntity& alien) {
    uint8_t& marker = FgRef(alien.tileX, alien.tileY);
    if (alien.state == AlienState::Stunned) {
        if (marker == Tile::AlienMarker) return;
        alien.state = AlienState::Dying;
        g_Sounds.broken.Play(0);
        ShowZapShimmer();
    } else if (alien.state != AlienState::Dying) {
        marker = Tile::AlienMarker;
    }
}

void RunState(AlienEntity& alien) {
    switch (alien.state) {
        case AlienState::Chase:   ChaseOrFlee(alien, +1, kChaseInterval); break;
        case AlienState::Flee:    ChaseOrFlee(alien, -1, kFleeInterval);  break;
        case AlienState::Rest:    Rest(alien);          break;
        case AlienState::Wander:  Wander(alien);        break;
        case AlienState::Attack:  Attack(alien);        break;
        case AlienState::Pattern: FollowPattern(alien); break;
        case AlienState::Stunned: Stunned(alien);       break;
        case AlienState::Replan:  Replan(alien);        break;
        case AlienState::Dying:   Dying(alien);         break;
    }
}

// Start an attack if the ship is orthogonally adjacent (re-reads the ship
// position: it may just have been pushed).
void NoticeShip(AlienEntity& alien) {
    if (alien.state == AlienState::Flee || alien.state == AlienState::Dying || g_Game.tubeDir != 0) return;

    for (int heading = kHeadingUp; heading <= kHeadingLeft; ++heading) {
        const TileOffset& offset = kHeadingOffsets[heading];
        if (alien.tileX + offset.deltaX == g_Game.tileX &&
            alien.tileY + offset.deltaY == g_Game.tileY) {
            alien.attackDir = static_cast<AttackDir>(heading);   // same numbering as Heading
            alien.state     = AlienState::Attack;
            return;
        }
    }
}

void CheckCounters(AlienEntity& alien) {
    if (alien.state == AlienState::Dying) return;
    if (alien.moves   > kRestAfterMoves)     alien.state = AlienState::Rest;
    if (alien.blocked > kReplanAfterBlocked) alien.state = AlienState::Replan;
}

void UpdateAlien(AlienEntity& alien) {
    if (alien.IsRemoved()) return;

    if (IsHornActive()) alien.state = AlienState::Stunned;
    SyncMapMarker(alien);

    if (++alien.tick < alien.interval) return;       // step gate
    if (++alien.frame > kLastWalkFrame && alien.state != AlienState::Dying) alien.frame = 0;

    RunState(alien);
    NoticeShip(alien);
    CheckCounters(alien);
    alien.tick = 0;
}

bool IsOnScreen(const AlienEntity& alien, int cameraLeftTile, int cameraTopTile) {
    return alien.tileX >= cameraLeftTile && alien.tileX < cameraLeftTile + kViewTilesX &&
           alien.tileY >= cameraTopTile  && alien.tileY < cameraTopTile  + kViewTilesY;
}

} 

void UpdateAliens() {
    for (AlienEntity& alien : g_Game.aliens) UpdateAlien(alien);
}

void DrawAliens(int cameraLeftTile, int cameraTopTile) {
    for (const AlienEntity& alien : g_Game.aliens) {
        if (alien.IsRemoved() || !IsOnScreen(alien, cameraLeftTile, cameraTopTile)) continue;
        BlitTile(g_Sprites.aliens,
                 (alien.tileX - cameraLeftTile) * kTilePx,
                 (alien.tileY - cameraTopTile)  * kTilePx + kViewYOffsetPx,
                 alien.frame);
    }
}

// True if any alien occupies this tile, including a dying one whose marker is already erased.
bool AlienStandsAt(int tileX, int tileY) {
    return std::any_of(g_Game.aliens.begin(), g_Game.aliens.end(),
        [&](const AlienEntity& alien) { return alien.tileX == tileX && alien.tileY == tileY; });
}
