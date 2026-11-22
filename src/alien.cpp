#include "alien.hpp"
#include "engine.hpp"
#include "states.hpp"
#include <algorithm>

int AlienRand(int mod) { return std::abs(std::rand()) % mod; }

// Chase (sign = +1) or flee (sign = -1): each axis steps independently into free tiles.
void AlienChaseOrFlee(AlienEntity& a, int sign, int interval) {
    const int px = g_Game.tileX, py = g_Game.tileY;
    int dx = 0, dy = 0;
    if      (px > a.x) { if (FgAt(a.x + sign, a.y) == 0) dx =  sign; }
    else if (px < a.x) { if (FgAt(a.x - sign, a.y) == 0) dx = -sign; }
    if      (py > a.y) { if (FgAt(a.x, a.y + sign) == 0) dy =  sign; }
    else if (py < a.y) { if (FgAt(a.x, a.y - sign) == 0) dy = -sign; }

    const int tx = a.x + dx, ty = a.y + dy;
    a.interval = interval;
    if ((tx == px && ty == py) || FgAt(tx, ty) != 0) {   // blocked (also when dx = dy = 0)
        ++a.blocked;
        return;
    }
    FgRef(tx, ty)     = 0xfd;
    FgRef(a.x, a.y)   = 0;
    a.x = tx; a.y = ty;
    ++a.moves;
}

// Wander and pattern steps never enter the ship's row or column.
void AlienTryStep(AlienEntity& a, int dx, int dy) {
    const int tx = a.x + dx, ty = a.y + dy;
    if (FgAt(tx, ty) != 0) return;
    if (ty == g_Game.tileY || tx == g_Game.tileX) return;
    FgRef(a.x, a.y) = 0;
    FgRef(tx, ty)   = 0xfd;
    a.x = tx; a.y = ty;
}

void DrawAliens(int cameraLeftTile, int cameraTopTile) {
    for (const AlienEntity& alien : g_Game.aliens) {
        if (alien.x == -1 && alien.y == -1) continue;   // removed
        if (alien.x < cameraLeftTile || alien.x >= cameraLeftTile + 40 ||
            alien.y < cameraTopTile  || alien.y >= cameraTopTile  + 31) continue;
        BlitTile(g_AliensBmp,
                 (alien.x - cameraLeftTile) * 16,
                 (alien.y - cameraTopTile)  * 16 - 8,
                 alien.frame, 0, 0x8000);
    }
}

// True if any alien occupies this tile, including a dying one whose marker is already erased.
bool AlienStandsAt(int tileX, int tileY) {
    for (const AlienEntity& alien : g_Game.aliens) {
        if (alien.x == tileX && alien.y == tileY) return true;
    }
    return false;
}


static void UpdateAlien(AlienEntity& a) {
    if (FgRef(a.x, a.y) != 0xfd) {
        std::printf("marker missing at (%d,%d), alien state=%d (6 = stunned)\n", a.x, a.y, a.state);
    }
    
    if (a.x == -1 && a.y == -1) return;

    // 1. The horn (sounding, or H / numpad 8 held) stuns every alien.
    if (g_SfxHorn.IsPlaying() || (GetAsyncKeyState('H') & 0x8000) || (GetAsyncKeyState(0x68) & 0x8000))
        a.state = ALIEN_STUNNED;

    // 2. The map marker decides life and death.
    uint8_t& marker = FgRef(a.x, a.y);
    if (a.state == ALIEN_STUNNED) {
        if (marker != 0xfd) {
            a.state = ALIEN_DYING;
            g_SfxBroken.Play(0);
            if (g_Game.shipFacing < 5 || g_Game.shipFacing == 15) g_Game.shipFacing = 15;   // new line
        }
    } else if (marker != 0xfd && a.state != ALIEN_DYING) {
        marker = 0xfd;
    }

    // 3. Step gate.
    if (++a.tick < a.interval) return;
    ++a.frame;
    if (a.state != ALIEN_DYING && a.frame > 5) a.frame = 0;

    switch (a.state) {
    case ALIEN_CHASE: AlienChaseOrFlee(a, +1, 5); break;
    case ALIEN_FLEE:  AlienChaseOrFlee(a, -1, 2); break;

    case ALIEN_REST:
        a.moves = 0; ++a.blocked; a.interval = 7;
        break;

    case ALIEN_WANDER: {
        const int r = AlienRand(4);
        AlienTryStep(a, kDX[r], kDY[r]);
        if (AlienRand(7) == 0) a.state = ALIEN_CHASE;
        a.interval = 7;
        break; }

    case ALIEN_ATTACK: {
        static const int pushDX[5] = { 0, 0, 1, 0, -1 }; // indexed by attackDir
        static const int pushDY[5] = { 0, -1, 0, 1, 0 };
        if (a.attackDir >= 1 && a.attackDir <= 4)
            MovePlayerShip(pushDX[a.attackDir], pushDY[a.attackDir]); // knock the ship away
        g_Game.fuel = std::max(0, g_Game.fuel - 50);
        a.frame = 6;
        SetPaletteEntry(1, 255, 0, 0); // red flash
        g_SfxAlienhit.Play(0);
        a.attackDir = 0;
        a.state = ALIEN_FLEE;
        break; }

    case ALIEN_PATTERN: {
        // Original quirk kept: values 1..4 index handlers 0..3, so 4 = no move and 0 = down.
        static const int kPattern[29] = { 1,2,2,3,3,3,4,  2,3,3,4,4,4,1,
                                          3,4,4,1,1,1,2,  4,1,1,2,2,2,3,  0 };
        if (a.step == 0) a.pattern = AlienRand(4);
        const int v = kPattern[a.pattern * 7 + a.step];
        if (v <= 3) AlienTryStep(a, kDX[v], kDY[v]);
        if (++a.step > 7) { a.step = 0; a.pattern = 0; a.state = ALIEN_CHASE; }
        a.interval = 7;
        break; }

    case ALIEN_STUNNED:
        a.frame = 7; a.interval = 15;
        if (AlienRand(3) == 0) a.state = ALIEN_CHASE;
        break;

    case ALIEN_REPLAN:
        a.frame = 0; a.interval = 7; a.blocked = 0; a.moves = 0;
        switch (AlienRand(3)) {
            case 0:  a.state = ALIEN_WANDER;  break;
            case 1:  a.state = ALIEN_PATTERN; break;
            default: a.state = ALIEN_CHASE;   break;
        }
        break;

    case ALIEN_DYING:
        // Don't interrupt a ship explosion (facing 5..13) with the zap shimmer.
        if (g_Game.shipFacing < 5 || g_Game.shipFacing == 15) g_Game.shipFacing = 15;
        if (a.frame < 8) a.frame = 8;
        if (a.frame == 14) {
            if (g_Game.shipFacing == 15) g_Game.shipFacing = 0;   // the original writes 0 here
            ++g_Game.aliensDefeated;
            FgRef(a.x, a.y) = 0;
            a.x = a.y = -1;
        }
        a.interval = 7;
        break;
    }

    // Shared tail: notice the ship, then check the rest/re-plan counters.
    if (a.state != ALIEN_FLEE && a.state != ALIEN_DYING && g_Game.tubeDir == 0) {
        const int px = g_Game.tileX, py = g_Game.tileY;   // re-read: the ship may just have been pushed
        if      (a.y - 1 == py && a.x == px) { a.attackDir = 1; a.state = ALIEN_ATTACK; }
        else if (a.y == py && a.x + 1 == px) { a.attackDir = 2; a.state = ALIEN_ATTACK; }
        else if (a.y + 1 == py && a.x == px) { a.attackDir = 3; a.state = ALIEN_ATTACK; }
        else if (a.y == py && a.x - 1 == px) { a.attackDir = 4; a.state = ALIEN_ATTACK; }
    }
    if (a.moves   > 45 && a.state != ALIEN_DYING) a.state = ALIEN_REST;
    if (a.blocked > 20 && a.state != ALIEN_DYING) a.state = ALIEN_REPLAN;
    a.tick = 0;
}

void UpdateAliens() { for (AlienEntity& a : g_Game.aliens) UpdateAlien(a); }

