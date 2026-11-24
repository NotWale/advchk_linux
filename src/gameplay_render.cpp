#include "gameplay.hpp"

#include "bible_verses.hpp"
#include "game_assets.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {

// Layout  --
constexpr int kVerseMaxLineLength = 36;   // characters; matches biblicVerseRender
constexpr int kVerseLineHeight    = 18;
constexpr int kVerseCenterY       = 220;
constexpr int kGlyphWidth         = 11;
constexpr int kScreenCenterX      = 320;
constexpr float kVerseBackdropDim = 0.4f;

constexpr int kEscapeDigitsTensX  = 0x118;
constexpr int kEscapeDigitsOnesX  = 0x136;
constexpr int kNumberFontColumns  = 5;

constexpr int kCupY               = 0x1b3;
constexpr int kCupRightX          = 0x21c;
constexpr int kCounterIconY       = 0x1c5;
constexpr int kCounterNumberY     = 0x1c6;
constexpr int kChickenIconX       = 0xe;
constexpr int kChickenSlashX      = 0x36;
constexpr int kAlienIconX         = 0x22d;
constexpr int kAlienSlashX        = 0x255;
constexpr int kSlashToTotalGap    = 7;    // "total" starts 7 px right of the slash

constexpr int kFuelGaugeX         = 0x267;
constexpr int kFuelGaugeBottomY   = 0x131;
constexpr int kFuelGaugeTopY      = 5;
constexpr int kFuelGaugeWidth     = 20;
constexpr int kFuelTierHeight     = 100;  // the gauge sprite has one 20 px column per 100 fuel

constexpr int kHudTextX           = 5;
constexpr int kLevelTextY         = 5;
constexpr int kScoreTextY         = 0x19;

constexpr int kConfirmBoxX = 0x75, kConfirmBoxY = 200, kConfirmBoxWidth = 0x197, kConfirmBoxHeight = 0x50;
constexpr int kConfirmPromptY = 0xd8;
constexpr int kConfirmHintX = 0xa6, kConfirmHintY = 0xf8;
constexpr int kExitPromptX = 0x9b;
constexpr int kPausedBannerX = 220, kPausedBannerY = 200;

// Tile ids and the animation they use are listed in gameplay.hpp (namespace Tile).
void DrawForegroundTile(uint8_t tile, int dstX, int dstY) {
    const SpriteBank& sprites = g_Sprites;
    const uint32_t    frame   = g_Game.frameCounter;

    switch (tile) {
        case Tile::Wall:
            BlitTile(sprites.tiles, dstX, dstY, 0, 0);
            break;
        case Tile::Hazard:               // flickers between two columns
            BlitTile(sprites.tiles, dstX, dstY, (std::rand() & 1) + 2, 0);
            break;
        case Tile::OrbSwitch:            // lit while pressed
            BlitTile(sprites.tiles, dstX, dstY, 4 + (g_Game.orbSwitchPressed ? 1 : 0), 0);
            break;
        case Tile::ConveyorUp:
        case Tile::ConveyorRight:
        case Tile::ConveyorDown:
        case Tile::ConveyorLeft:
            BlitTile(sprites.arrows, dstX, dstY, (frame >> 1) & 7, tile - Tile::ConveyorUp);
            break;
        case Tile::Warp:
            BlitTile(sprites.warpTile, dstX, dstY, (frame >> 2) % 4, 0);
            break;
        case Tile::ExitClosed:
            BlitTile(sprites.exitDoor, dstX, dstY, 0, 0);
            break;
        case Tile::ExitOpen: // opens over frames 1..3
            BlitTile(sprites.exitDoor, dstX, dstY, g_Game.exitDoorFrame, 0);
            break;
        case Tile::FuelPad:
            BlitTile(sprites.tiles, dstX, dstY, ((frame >> 4) & 3) + 13, 0);
            break;
        case Tile::FuelCanister:
            BlitTile(sprites.tiles, dstX, dstY, 6, 0);
            break;
        case Tile::Barrier:
            BlitTile(sprites.tiles, dstX, dstY, ((frame % 60) / 20) + 10, 0);
            break;
        case Tile::GemSmall:
        case Tile::GemMedium:
        case Tile::GemLarge:
            BlitTile(sprites.crosses, dstX, dstY, (frame >> 2) % 7, tile - Tile::GemSmall);
            break;
        case Tile::Boulder:
            BlitTile(sprites.tiles, dstX, dstY, 1, 0);
            break;
        case Tile::ReactorSwitch:
            BlitTile(sprites.tiles, dstX, dstY, 7, 0);
            break;
        case Tile::ReactorSwitchPressed:
            BlitTile(sprites.tiles, dstX, dstY, (std::rand() & 1) + 8, 0);
            break;
        default:
            if (!Tile::IsPipe(tile)) break;
            // Tubes.bmp has 2 sprite columns per tile id: row 0 for ids up to the "up" mouth,
            // row 1 for the beam emitters, and a final lone sprite for the last pipe tile.
            if (tile < Tile::BeamEmitterUp - 3)      BlitTile(sprites.tubes, dstX, dstY, (tile - Tile::FirstPipe) * 2, 0);
            else if (tile < Tile::LastPipe)          BlitTile(sprites.tubes, dstX, dstY, (tile - Tile::PipeMouthLeft) * 2, 1);
            else                                     BlitTile(sprites.tubes, dstX, dstY, 0, 2);
            break;
    }
}

bool IsOnScreen(int tileX, int tileY, const Camera& camera) {
    return tileX >= camera.leftTile && tileX < camera.leftTile + kViewTilesX &&
           tileY >= camera.topTile  && tileY < camera.topTile  + kViewTilesY;
}

// "current/total" with the number right-aligned against the slash.
void DrawFractionCounter(int slashX, int current, int total) {
    const std::string currentText = std::to_string(current);
    DrawBitmapText(slashX - static_cast<int>(currentText.size()) * kGlyphWidth, kCounterNumberY, currentText);
    DrawBitmapText(slashX, kCounterIconY, "/");
    DrawBitmapText(slashX + kSlashToTotalGap, kCounterNumberY, std::to_string(total));
}

void DrawFuelGauge() {
    if (g_Game.displayedFuel > 0) {
        const int fuelHeight = std::clamp(g_Game.displayedFuel, 0, kMaxFuel);
        const int fuelTier   = ((fuelHeight - 1) / kFuelTierHeight) + 1;   // colour band: 1..3
        BlitSubRect(g_Sprites.fuel, kFuelGaugeX, kFuelGaugeBottomY - fuelHeight,
                    fuelTier * kFuelGaugeWidth, kMaxFuel - fuelHeight, kFuelGaugeWidth, fuelHeight);
    }
    BlitTile(g_Sprites.fuel, kFuelGaugeX, kFuelGaugeTopY, 0, 0);   // the empty gauge frame
}

}  

void DrawWorld(const Camera& camera) {
    for (int viewRow = 0; viewRow < kViewTilesY; ++viewRow) {
        const int mapY = camera.topTile + viewRow;
        if (mapY < 0 || mapY >= kMapHeight) continue;

        for (int viewColumn = 0; viewColumn < kViewTilesX; ++viewColumn) {
            const int mapX = camera.leftTile + viewColumn;
            if (mapX < 0 || mapX >= kMapWidth) continue;

            const int dstX = viewColumn * kTilePx;
            const int dstY = viewRow * kTilePx + kViewYOffsetPx;
            const int tileIndex = TileIndex(mapX, mapY);

            const uint8_t beam = g_Game.bgMap[tileIndex];
            if (beam != 0) BlitTile(g_Sprites.tiles, dstX, dstY, beam * 2, 2);

            const uint8_t tile = g_Game.fgMap[tileIndex];
            if (tile != Tile::Empty) DrawForegroundTile(tile, dstX, dstY);
        }
    }

    // Live chickens use their own facing and wing-flap frame.
    for (const ChickenEntity& chicken : g_Game.chickens) {
        if (chicken.IsRemoved() || !IsOnScreen(chicken.tileX, chicken.tileY, camera)) continue;
        BlitTile(g_Sprites.chicken,
                 (chicken.tileX - camera.leftTile) * kTilePx,
                 (chicken.tileY - camera.topTile)  * kTilePx + kViewYOffsetPx,
                 chicken.frame, 0);
    }
}

void DrawPlayerShip() {
    if (g_Game.tubeDir != kHeadingNone || g_Game.inTubeHidden) return;   // hidden inside a pipe

    const int shipX = kShipCenterTileX * kTilePx;
    const int shipY = kShipCenterTileY * kTilePx + kViewYOffsetPx;
    const SpriteSheet& slurp = g_Sprites.chickenSlurp;

    if (g_Game.collectDir != kHeadingNone && g_Game.shipFacing < kShipFacingFirstExplosion) {
        const int row = g_Game.collectDir - 1;
        if (g_Game.collectFrame < 6) {
            // The ship's mouth, plus the extended tongue one tile toward the chicken.
            const TileOffset toward = kHeadingOffsets[g_Game.collectDir];
            BlitTile(slurp, shipX, shipY, g_Game.collectFrame * 2 + 1, row);
            BlitTile(slurp, shipX + toward.deltaX * kTilePx, shipY + toward.deltaY * kTilePx,
                     g_Game.collectFrame * 2, row);
        } else {
            BlitTile(slurp, shipX, shipY, g_Game.collectFrame + 6, row);
        }
        return;
    }

    // The zap shimmer picks one of 4 random sprites after the 15 idle/move/explosion ones.
    const int column = (g_Game.shipFacing == kShipFacingZapShimmer)
                           ? kShipFacingZapShimmer + (std::rand() & 3)
                           : g_Game.shipFacing;
    BlitTile(g_Sprites.ship, shipX, shipY, column, 0);
}

void DrawEscapeCountdown() {
    if (g_Game.escapeCountdown < 0) return;
    const int tens = (g_Game.escapeCountdown / 10) % 10;
    const int ones = g_Game.escapeCountdown % 10;
    BlitTile(g_Sprites.numberFont, kEscapeDigitsTensX, 0, tens % kNumberFontColumns, tens / kNumberFontColumns);
    BlitTile(g_Sprites.numberFont, kEscapeDigitsOnesX, 0, ones % kNumberFontColumns, ones / kNumberFontColumns);
}

void DrawHud(const PlayerProfile& profile) {
    const int iconFrame = (g_Game.frameCounter & 0x1f) >> 2;   // 8-frame idle animation

    BlitTile(g_Sprites.cup, 0,           kCupY, 0, 0);
    BlitTile(g_Sprites.cup, kCupRightX,  kCupY, 1, 0);
    BlitTile(g_Sprites.chicken, kChickenIconX, kCounterIconY, iconFrame, 0);
    DrawFractionCounter(kChickenSlashX, g_Game.chickensRescued, g_Game.totalChickens);

    DrawFuelGauge();

    BlitTile(g_Sprites.aliens, kAlienIconX, kCounterIconY, iconFrame, 0);
    DrawFractionCounter(kAlienSlashX, g_Game.aliensDefeated, g_Game.totalAliens);

    DrawBitmapText(kHudTextX, kLevelTextY, "Level: " + std::to_string(profile.level));
    DrawBitmapText(kHudTextX, kScoreTextY, "Score: " + std::to_string(g_Game.displayedScore));
}

void DrawConfirmDialog(int promptLeft, const std::string& prompt) {
    FillScreenRect(kConfirmBoxX, kConfirmBoxY, kConfirmBoxWidth, kConfirmBoxHeight, 1);
    DrawBitmapText(promptLeft, kConfirmPromptY, prompt);
    DrawBitmapText(kConfirmHintX, kConfirmHintY, "ENTER=Accept ESCAPE=Cancel");
}

void DrawExitConfirmDialog() {
    DrawConfirmDialog(kExitPromptX, "You wish to exit this level?");
}

void DrawPausedBanner() {
    BlitTile(g_Sprites.paused, kPausedBannerX, kPausedBannerY, 0, 0);
}

void DrawBibleVerseOverlay(int verseId) {
    const char* verse = BibleVerse(verseId);
    if (!verse) return;

    const std::vector<std::string> lines = WrapVerseText(verse, kVerseMaxLineLength);
    DimScreen(kVerseBackdropDim);   // matches FUN_0040e830(Dark.bmp)

    const int lineCount  = static_cast<int>(lines.size());
    const int halfHeight = lineCount * kVerseLineHeight / 2;
    for (int row = 0; row < lineCount; ++row) {
        const int top  = row * kVerseLineHeight - halfHeight + kVerseCenterY;
        const int left = kScreenCenterX - (static_cast<int>(lines[row].size()) * kGlyphWidth) / 2;
        DrawBitmapText(left, top, lines[row]);
    }
}
