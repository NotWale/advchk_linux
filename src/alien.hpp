#pragma once

enum AlienState {
    ALIEN_CHASE, ALIEN_FLEE, ALIEN_REST, ALIEN_WANDER, ALIEN_ATTACK,
    ALIEN_PATTERN, ALIEN_STUNNED, ALIEN_REPLAN, ALIEN_DYING
};

struct AlienEntity {
    int x = 0, y = 0;      // tile position; (-1,-1) = removed
    int frame     = 0;     // Aliens.bmp column: 0-5 walk, 6 attack, 7 stunned, 8-14 dying
    int tick      = 0;
    int state     = ALIEN_CHASE;
    int moves     = 0;     // successful chase/flee steps since the last re-plan
    int blocked   = 0;     // blocked chase/flee steps since the last re-plan
    int interval  = 0;     // frames per step
    int pattern   = 0;     // state 5: pattern 0..3
    int step      = 0;     // state 5: position in the pattern, 0..7
    int attackDir = 0;     // 1=up 2=right 3=down 4=left (where the ship is)
};

void AlienChaseOrFlee(AlienEntity& a, int sign, int interval);
int AlienRand(int mod);
void AlienTryStep(AlienEntity& a, int dx, int dy);
void DrawAliens(int cameraLeftTile, int cameraTopTile);
bool AlienStandsAt(int tileX, int tileY);
void UpdateAliens();