#include "game_assets.hpp"

SoundBank  g_Sounds;
SpriteBank g_Sprites;

namespace {

constexpr int kSingleVoice = 1;
constexpr int kTwoVoices   = 2;   // may overlap itself (voice counts match the original's InitApp)

}  

void RestoreMainPalette() {
    SetActivePaletteFromSurface(g_Sprites.tiles.surface);
}

void EnsureFullscreenImage(const std::string& filename) {
    SpriteSheet& fullscreen = g_Sprites.fullscreen;
    if (fullscreen.filename == filename) return;

    if (fullscreen.surface) SDL_FreeSurface(fullscreen.surface);
    fullscreen = LoadSpriteSheet(filename);

    // The two splash logos bring their own palette; everything else uses the game palette.
    const bool hasOwnPalette = (filename == "XGames.bmp" || filename == "RockSolid.bmp");
    if (fullscreen.surface && hasOwnPalette) {
        SetActivePaletteFromSurface(fullscreen.surface);
    } else {
        RestoreMainPalette();
    }
}

void LoadGameAssets() {
    SpriteBank& sprites = g_Sprites;
    SoundBank&  sounds  = g_Sounds;

    sprites.tiles = LoadSpriteSheet("Tiles.bmp", 16, 16);
    RestoreMainPalette();
    sprites.selector   = LoadSpriteSheet("Selector.bmp",   0x28, 0x28);
    sprites.selections = LoadSpriteSheet("Selections.bmp", 0x159, 0x32);

    sounds.alienHit.Load("Alienhit.wav",   kTwoVoices);
    sounds.applause.Load("Applause.wav",   kSingleVoice);
    sounds.atticDoor.Load("AtticDoor.wav", kSingleVoice);
    sounds.bounce.Load("Bounce.wav",       kTwoVoices);
    sounds.broken.Load("Broken.wav",       kSingleVoice);
    sounds.chicken.Load("Chicken.wav",     kTwoVoices);
    sounds.chicken2.Load("Chicken2.wav",   kTwoVoices);
    // NOTE: the original loads Move.wav (2 voices) for the menu cursor, plus Clink.wav and
    // Clunk.wav. This port loads Clink.wav for the cursor and plays Clunk.wav as "clink".
    sounds.menuMove.Load("Clink.wav",      kTwoVoices);
    sounds.clunk.Load("Clunk.wav",         kTwoVoices);
    sounds.clink.Load("Clunk.wav",         kSingleVoice);
    sounds.down.Load("Down.wav",           kSingleVoice);
    sounds.dribble.Load("Dribble.wav",     kSingleVoice);
    sounds.drink.Load("Drink.wav",         kSingleVoice);
    sounds.error.Load("Error.wav",         kSingleVoice);
    sounds.excellent.Load("Excellent.wav", kSingleVoice);
    sounds.explode.Load("Explode.wav",     kSingleVoice);
    sounds.harp.Load("Harp.wav",           kSingleVoice);
    sounds.horn.Load("Horn.wav",           kSingleVoice);
    sounds.openDoor.Load("OpenDoor.wav",   kSingleVoice);
    sounds.pickup.Load("Pickup.wav",       kTwoVoices);
    sounds.rev.Load("Rev.wav",             kTwoVoices);
    sounds.warp.Load("Warp.wav",           kSingleVoice);
    sounds.whistle.Load("Whistle.wav",     kSingleVoice);
    sounds.yeah.Load("Yeah.wav",           kTwoVoices);

    sprites.ship         = LoadSpriteSheet("Ship.bmp",       16, 16);
    sprites.chicken      = LoadSpriteSheet("Chicken.bmp",    16, 16);
    sprites.chickenSlurp = LoadSpriteSheet("Chickens.bmp",   16, 16);
    sprites.aliens       = LoadSpriteSheet("Aliens.bmp",     16, 16);
    sprites.fuel         = LoadSpriteSheet("Fuel.bmp",       20, 300);
    sprites.separator    = LoadSpriteSheet("Separator.bmp");
    sprites.numberFont   = LoadSpriteSheet("NumberFont.bmp", 40, 40);
    sprites.tubes        = LoadSpriteSheet("Tubes.bmp",      16, 16);
    sprites.paused       = LoadSpriteSheet("Paused.bmp");
    sprites.exitDoor     = LoadSpriteSheet("Exit.bmp",       16, 16);
    sprites.arrows       = LoadSpriteSheet("Arrows.bmp",     16, 16);
    sprites.crosses      = LoadSpriteSheet("Crosses.bmp",    16, 16);
    sprites.warpTile     = LoadSpriteSheet("WarpTile.bmp",   16, 16);
    sprites.cup          = LoadSpriteSheet("Cup.bmp",        100, 45);
}
