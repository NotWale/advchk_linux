#pragma once

#include <cstdint>

enum class AlienState : std::uint8_t {
    Chase, Flee, Rest, Wander, Attack, Pattern, Stunned, Replan, Dying
};

// Direction (as seen from the alien) in which the ship is adjacent.
enum class AttackDir : std::uint8_t { None, Up, Right, Down, Left };

struct AlienEntity {
    static constexpr int kRemoved = -1;

    int tileX = 0, tileY = 0;   // tile position; (kRemoved, kRemoved) = removed
    int frame    = 0;       // Aliens.bmp column: 0-5 walk, 6 attack, 7 stunned, 8-14 dying
    int tick     = 0;       // frames since the last step
    int moves    = 0;       // successful chase/flee steps since the last re-plan
    int blocked  = 0;       // blocked chase/flee steps since the last re-plan
    int interval = 0;       // frames per step
    int pattern  = 0;       // Pattern state: pattern 0..3
    int step     = 0;       // Pattern state: position in the pattern, 0..7
    AlienState state     = AlienState::Chase;
    AttackDir  attackDir = AttackDir::None;

    bool IsRemoved() const { return tileX == kRemoved && tileY == kRemoved; }
    void Remove()          { tileX = tileY = kRemoved; }
};

void UpdateAliens();
void DrawAliens(int cameraLeftTile, int cameraTopTile);
bool AlienStandsAt(int tileX, int tileY);
