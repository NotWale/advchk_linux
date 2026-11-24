#pragma once

#include "engine.hpp"

#include <string>

// All sound effects used by the game. Names follow the .wav files.
struct SoundBank {
    SoundEffect alienHit, applause, bounce, broken, chicken, chicken2, clink, clunk;
    SoundEffect down, dribble, drink, error, excellent, explode, harp, horn;
    SoundEffect atticDoor, menuMove, openDoor, pickup, rev, warp, whistle, yeah;
};

// All sprite sheets shared between screens and gameplay.
struct SpriteBank {
    SpriteSheet tiles, selector, selections;
    SpriteSheet ship, chicken, chickenSlurp, aliens, fuel, separator, numberFont;
    SpriteSheet tubes, paused, exitDoor, arrows, crosses, warpTile, cup;
    SpriteSheet fullscreen;   // shared slot: MainScreen / Credits / Instructions / ...
};

extern SoundBank  g_Sounds;
extern SpriteBank g_Sprites;

// Loads every shared sprite sheet and sound effect (call once after InitEngine).
void LoadGameAssets();

// Makes `filename` the current fullscreen image and sets the matching palette.
// Does nothing if it is already loaded.
void EnsureFullscreenImage(const std::string& filename);

// Re-applies the palette of Tiles.bmp (used by every screen except the ones with their own).
void RestoreMainPalette();
