#include "gameplay.hpp"

#include "bible_verses.hpp"
#include "game_assets.hpp"
#include "states.hpp"
#include "virtual_keys.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

MainGameState g_Game;

namespace {

// Tuning  --
constexpr int      kStepsPerFuelUse        = 5;     // fuel drops once every 5 steps
constexpr int      kFuelCanisterAmount     = 50;
constexpr int      kFuelPadRefillPerFrame  = 1;
constexpr int      kScoreUnit              = 50;    // all point values are multiples of this
constexpr int      kCheatScoreBonus        = 3000;
constexpr int      kDeathScorePenalty      = 1000;
constexpr int      kCheatLowFuel           = 5;
constexpr uint32_t kOrbSwitchDurationMs    = 500;
constexpr int      kEscapeSeconds          = 45;    // 0x2d: time to reach the exit after the reactor blows
constexpr int      kFuelGaugeStep          = 3;     // HUD counters ease toward their targets
constexpr int      kScoreCounterStep       = kScoreUnit;
constexpr int      kCollectFramesBeforeWalk = 6;    // walking resumes once the slurp animation passes frame 6
constexpr int      kChickenStepInterval    = 6;     // chickens step on every 7th update
constexpr int      kChickenFrameCount      = 8;
constexpr int      kExitDoorLastFrame      = 3;
constexpr int      kAlertPaletteIndex      = 1;     // flashes red when an alien hits the ship
constexpr uint8_t  kAlertFadeStep          = 4;

// Pipe-tile ids wrap every four, so (tile - base) & 3 yields the heading index.
constexpr uint8_t kPipeMouthHeadingBase    = 0x18;
constexpr uint8_t kPipeCornerHeadingBase   = 0x15;
constexpr uint8_t kBeamEmitterHeadingBase  = 0x1c;

constexpr int HeadingFromPipeTile(uint8_t tile, uint8_t base) { return ((tile - base) & 3) + 1; }

bool IsKeyDown(int virtualKey) { return (GetAsyncKeyState(virtualKey) & kKeyDownBit) != 0; }

// Fuel and score

void SpendFuelForStep() {
    if (++g_Game.stepsSinceFuelUse < kStepsPerFuelUse) return;
    g_Game.fuel = std::max(0, g_Game.fuel - (1 + g_Difficulty));
    g_Game.stepsSinceFuelUse = 0;
}

int GemScore(uint8_t gem) {
    switch (gem) {
        case Tile::GemSmall:  return (g_Difficulty + 1)  * kScoreUnit;
        case Tile::GemMedium: return (g_Difficulty + 8)  * kScoreUnit;
        default:              return (g_Difficulty + 18) * kScoreUnit;
    }
}

int ChickenRescueScore() { return (g_Difficulty + 5) * kScoreUnit; }

// Ship movement

void MoveShipInsideTube() {
    const TileOffset step = kHeadingOffsets[g_Game.tubeDir];
    g_Game.shipFacing = g_Game.tubeDir;

    const int nextX = std::clamp(g_Game.tileX + step.deltaX, 0, kMapWidth - 1);
    const int nextY = std::clamp(g_Game.tileY + step.deltaY, 0, kMapHeight - 1);
    const uint8_t nextTile = FgRef(nextX, nextY);

    // Corner bends redirect the ship.
    if (!g_Game.inTubeHidden && Tile::IsPipeCorner(nextTile)) {
        const int corner = nextTile - Tile::FirstPipeCorner;   // 0..3
        if ((g_Game.tubeDir & 3) == corner) {
            g_Game.tubeDir = corner + 1;
            g_Sounds.bounce.Play(0);
        } else if (((g_Game.tubeDir + 1) & 3) == corner) {
            g_Game.tubeDir = HeadingFromPipeTile(nextTile, kPipeCornerHeadingBase);
            g_Sounds.bounce.Play(0);
        }
    }

    // Leaving the end of the pipe: step out onto the floor.
    if (!Tile::IsPipe(nextTile) && !g_Game.inTubeHidden) {
        g_Sounds.whistle.Play(0);
        g_Game.tubeDir       = kHeadingNone;
        g_Game.walkStepPhase = 1;   // makes the next floor step happen immediately
        MovePlayerShip(step.deltaX, step.deltaY);
        return;
    }

    // Beam emitters toggle whether the ship is hidden inside the tube.
    if (Tile::IsBeamEmitter(nextTile)) {
        g_Game.inTubeHidden = !g_Game.inTubeHidden;
        if (!g_Game.inTubeHidden) {
            g_Game.tubeDir = HeadingFromPipeTile(nextTile, kBeamEmitterHeadingBase);
        }
    }

    g_Game.tileX = nextX;
    g_Game.tileY = nextY;
    SpendFuelForStep();
}

int HeadingOfStep(int deltaX, int deltaY) {
    if (deltaY < 0) return kHeadingUp;
    if (deltaX > 0) return kHeadingRight;
    if (deltaY > 0) return kHeadingDown;
    return kHeadingLeft;
}

// Pressing the orb switch turns every conveyor stream a quarter turn.
void PressOrbSwitch() {
    for (uint8_t& tile : g_Game.fgMap) {
        if (tile >= Tile::ConveyorUp && tile <= Tile::ConveyorLeft) {
            tile = (tile == Tile::ConveyorLeft) ? Tile::ConveyorUp : tile + 1;
        }
    }
    g_Game.orbSwitchTime    = timeGetTime();
    g_Game.orbSwitchPressed = true;
    g_Sounds.clunk.Play(0);
}

// A conveyor stream cannot be entered against its flow.
bool IsAgainstConveyor(uint8_t tile, int deltaX, int deltaY) {
    return (tile == Tile::ConveyorLeft  && deltaX > 0) || (tile == Tile::ConveyorRight && deltaX < 0) ||
           (tile == Tile::ConveyorUp    && deltaY > 0) || (tile == Tile::ConveyorDown  && deltaY < 0);
}

// A pipe mouth can only be entered by moving into it (down into the "down" mouth, ...).
bool IsEnteringPipeMouth(uint8_t tile, int deltaX, int deltaY) {
    return (tile == Tile::PipeMouthDown  && deltaY ==  1) || (tile == Tile::PipeMouthLeft  && deltaX == -1) ||
           (tile == Tile::PipeMouthUp    && deltaY == -1) || (tile == Tile::PipeMouthRight && deltaX ==  1);
}

bool IsPassable(uint8_t tile, int deltaX, int deltaY) {
    switch (tile) {
        case Tile::Wall:
        case Tile::Barrier:
        case Tile::ExitClosed:
        case Tile::AlienMarker:
        case Tile::ChickenMarker:
        case Tile::OrbSwitch:
        case Tile::ReactorSwitch:
        case Tile::ReactorSwitchPressed:
        case Tile::Boulder:
            return false;
        case Tile::PipeMouthDown:
        case Tile::PipeMouthLeft:
        case Tile::PipeMouthUp:
        case Tile::PipeMouthRight:
            return IsEnteringPipeMouth(tile, deltaX, deltaY);
        default:
            return !Tile::IsPipe(tile);
    }
}

void BlowReactor(int switchX, int switchY) {
    g_Game.reactorDestroyed = true;
    for (uint8_t& tile : g_Game.fgMap) {
        if (tile == Tile::Barrier) tile = Tile::Empty;   // unlock the exit barriers
    }
    FgRef(g_Game.reactorX, g_Game.reactorY) = Tile::ExitOpen;
    FgRef(switchX, switchY)                 = Tile::ReactorSwitchPressed;
    g_Game.escapeCountdown = kEscapeSeconds;
    g_Sounds.error.Play(0);
}

// Pressing the reactor switch from above blows the reactor, but only once every chicken is safe.
void PressReactorSwitch(int switchX, int switchY) {
    if (g_Game.chickensRescued == g_Game.totalChickens && !g_Game.reactorDestroyed) {
        BlowReactor(switchX, switchY);
    } else if (!g_Sounds.openDoor.IsPlaying()) {
        g_Sounds.openDoor.Play(0);
    }
}

// Effects of the tile the ship just stepped onto. `tile` is what was there before the move.
void ApplyTileEffect(uint8_t tile) {
    PlayerProfile& profile = CurrentProfile();
    uint8_t& shipTile = FgRef(g_Game.tileX, g_Game.tileY);

    switch (tile) {
        case Tile::Warp:
            g_Game.warpTriggered = true;
            break;
        case Tile::Hazard:
            g_Game.fuel = 0;
            break;
        case Tile::FuelCanister:
            g_Game.fuel = std::min(kMaxFuel, g_Game.fuel + kFuelCanisterAmount);
            shipTile = Tile::Empty;
            g_Sounds.yeah.Play(0);
            break;
        case Tile::GemSmall:
        case Tile::GemMedium:
        case Tile::GemLarge:
            profile.score += GemScore(tile);
            shipTile = Tile::Empty;
            g_Sounds.pickup.Play(0);
            break;
        case Tile::ExitOpen:
            g_Game.levelExitTriggered = true;
            break;
        default:
            break;
    }
}

void MoveShipOnFloor(int deltaX, int deltaY) {
    // Without Shift / Always Run the ship only moves on every 2nd call.
    const bool running = IsKeyDown(Vk::Shift) || g_AlwaysRun;
    if (!running) {
        if (++g_Game.walkStepPhase != 2) return;
        g_Game.walkStepPhase = 0;
    }

    g_Game.shipFacing = HeadingOfStep(deltaX, deltaY);

    const int nextX = std::clamp(g_Game.tileX + deltaX, 0, kMapWidth - 1);
    const int nextY = std::clamp(g_Game.tileY + deltaY, 0, kMapHeight - 1);
    uint8_t target  = FgRef(nextX, nextY);

    if (target == Tile::Boulder) {   // push it if the tile behind is free
        const int pushX = std::clamp(nextX + deltaX, 0, kMapWidth - 1);
        const int pushY = std::clamp(nextY + deltaY, 0, kMapHeight - 1);
        if (FgRef(pushX, pushY) == Tile::Empty) {
            FgRef(nextX, nextY) = Tile::Empty;
            FgRef(pushX, pushY) = Tile::Boulder;
            g_Sounds.clunk.Play(0);
            target = Tile::Empty;
        }
    }

    if (target == Tile::OrbSwitch && !g_Game.orbSwitchPressed) PressOrbSwitch();

    if (IsAgainstConveyor(target, deltaX, deltaY)) return;

    if (target == Tile::ReactorSwitch && deltaY == 1) {
        PressReactorSwitch(nextX, nextY);
        return;
    }

    if (IsPassable(target, deltaX, deltaY) && !AlienStandsAt(nextX, nextY)) {
        g_Game.tileX = nextX;
        g_Game.tileY = nextY;
        if (IsEnteringPipeMouth(target, deltaX, deltaY)) {
            g_Game.tubeDir = HeadingFromPipeTile(target, kPipeMouthHeadingBase);
        }
        SpendFuelForStep();
    }

    ApplyTileEffect(target);
}

// Chickens  --

void UpdateChickens() {
    for (ChickenEntity& chicken : g_Game.chickens) {
        if (chicken.IsRemoved()) continue;

        // The map marker disappears once the ship has beamed the chicken up.
        if (FgRef(chicken.tileX, chicken.tileY) != Tile::ChickenMarker) {
            chicken.Remove();
            continue;
        }

        if (++chicken.stepTimer <= kChickenStepInterval) continue;

        const int direction = std::rand() & 3;
        const TileOffset step = kWalkOffsets[direction];
        const int nextX = chicken.tileX + step.deltaX;
        const int nextY = chicken.tileY + step.deltaY;
        const bool inShipLine = (chicken.tileY == g_Game.tileY) || (chicken.tileX == g_Game.tileX);

        const bool canMove = IsInsideMap(nextX, nextY) &&
                             FgRef(nextX, nextY) == Tile::Empty &&
                             !(nextX == g_Game.tileX && nextY == g_Game.tileY) &&
                             !inShipLine &&
                             !AlienStandsAt(nextX, nextY);
        if (canMove) {
            FgRef(chicken.tileX, chicken.tileY) = Tile::Empty;
            chicken.tileX = nextX;
            chicken.tileY = nextY;
            FgRef(nextX, nextY) = Tile::ChickenMarker;
            chicken.frame = direction * 2;   // facing: 0 down, 2 right, 4 up, 6 left
        }

        chicken.frame += (std::rand() & 1) ? -1 : 1;   // wing flap
        chicken.frame = std::clamp(chicken.frame, 0, kChickenFrameCount - 1);
        chicken.stepTimer = 0;
    }
}

// Level flow  --

void EndLevel() {
    g_Game.levelLoaded     = false;   // the next State_MainGame call loads a level again
    g_Game.verseOverlayId  = 0;
    SavePlayerData();
    SetOrGetGameState(kStateLevelSummary);
}

void RespawnShipAtStart() {
    g_Game.tileX         = g_Game.spawnX;
    g_Game.tileY         = g_Game.spawnY;
    g_Game.fuel          = kMaxFuel;
    g_Game.shipFacing    = kShipFacingIdle;
    g_Game.tubeDir       = kHeadingNone;
    g_Game.inTubeHidden  = false;
    g_Game.collectDir    = kHeadingNone;
    g_Game.collectFrame  = 0;
    g_Game.warpTriggered = false;

    if (!IsInsideMap(g_Game.tileX, g_Game.tileY)) {
        std::fprintf(stderr, "Respawn out of range: spawn=(%d,%d)\n", g_Game.spawnX, g_Game.spawnY);
    }
}

void StartLevel(const PlayerProfile& profile) {
    LoadCurrentLevel();
    g_Game.displayedFuel   = 0;
    g_Game.exitDoorFrame   = 1;
    g_Game.displayedScore  = profile.score;
    g_Game.escapeCountdown = -1;
    g_Game.paused          = false;
    g_Game.exitConfirmOpen = false;
    g_Game.levelLoaded     = true;
    WasKeyJustPressed(Vk::Escape);   // swallow keys still held from the previous screen
    WasKeyJustPressed(Vk::Return);

    const int verseCount = BibleVerseCount();
    g_Game.verseOverlayId = (verseCount > 0) ? (std::rand() % verseCount) + 1 : 0;
}

// Per-frame updates -----------

int StepToward(int current, int target, int maxStep) {
    if (current < target) return std::min(target, current + maxStep);
    if (current > target) return std::max(target, current - maxStep);
    return current;
}

void RefuelOnPad() {
    if (FgRef(g_Game.tileX, g_Game.tileY) != Tile::FuelPad || g_Game.fuel >= kMaxFuel) return;
    g_Game.fuel = std::min(kMaxFuel, g_Game.fuel + kFuelPadRefillPerFrame);
    if (!g_Sounds.drink.IsPlaying()) g_Sounds.drink.Play(0);
}

// Engine revving while Shift is held: at most 2 revs, then a pause.
void PlayEngineRevIfDue() {
    constexpr uint32_t kRevGapMs   = 250;
    constexpr uint32_t kRevResetMs = 2500;
    constexpr int      kMaxRevs    = 2;
    static uint32_t lastRevTime = 0;
    static int      revCount    = 0;

    const uint32_t now = timeGetTime();
    if (now - lastRevTime > kRevGapMs && revCount < kMaxRevs) {
        g_Sounds.rev.Play(0);
        ++revCount;
        lastRevTime = now;
    }
    if (now - lastRevTime > kRevResetMs) revCount = 0;
}

// Arrow keys move the ship; otherwise a conveyor under it pushes it.
void HandleWalkInput() {
    int deltaX = 0, deltaY = 0;
    if (IsKeyDown(Vk::Left))       deltaX = -1;
    else if (IsKeyDown(Vk::Right)) deltaX = 1;
    else if (IsKeyDown(Vk::Up))    deltaY = -1;
    else if (IsKeyDown(Vk::Down))  deltaY = 1;
    else {
        switch (FgRef(g_Game.tileX, g_Game.tileY)) {
            case Tile::ConveyorUp:    deltaY = -1; break;
            case Tile::ConveyorRight: deltaX =  1; break;
            case Tile::ConveyorDown:  deltaY =  1; break;
            case Tile::ConveyorLeft:  deltaX = -1; break;
            default:                  g_Game.shipFacing = kShipFacingIdle; break;   // standing still
        }
        if (deltaX != 0 || deltaY != 0) {
            if (!g_Sounds.dribble.IsPlaying()) g_Sounds.dribble.Play(0);
        }
    }

    if (deltaX != 0 || deltaY != 0) MovePlayerShip(deltaX, deltaY);
    if (IsKeyDown(Vk::Shift)) PlayEngineRevIfDue();
}

// Cheats (original: playerShipUpdate): hold Shift + Ctrl, plus Q / W / E / R.
void HandleCheats(PlayerProfile& profile) {
    if (!IsKeyDown(Vk::Shift) || !IsKeyDown(Vk::Control)) return;

    if (IsKeyDown(Vk::Q)) g_Game.fuel = kCheatLowFuel;
    if (IsKeyDown(Vk::W)) g_Game.fuel = kMaxFuel;
    if (IsKeyDown(Vk::E)) profile.score += kCheatScoreBonus;   // per frame while held, like the original
    if (IsKeyDown(Vk::R)) {
        g_Game.escapeCountdown = 2;   // >= 0 means "level completed" for the summary
        EndLevel();
    }
}

void UpdateShip(PlayerProfile& profile) {
    const bool shipCanAct = g_Game.shipFacing < kShipFacingFirstExplosion &&
                            (g_Game.collectDir == kHeadingNone ||
                             g_Game.collectFrame >= kCollectFramesBeforeWalk);
    if (!shipCanAct) return;

    if (g_Game.tubeDir != kHeadingNone) MovePlayerShip(0, 0);
    else                                HandleWalkInput();
    HandleCheats(profile);
}

// Space beams up a chicken standing next to the ship (up, right, down, left; first match only).
void RescueAdjacentChicken(PlayerProfile& profile) {
    if (g_Game.collectDir != kHeadingNone || !IsKeyDown(Vk::Space)) return;

    for (int heading = kHeadingUp; heading <= kHeadingLeft; ++heading) {
        const int chickenX = g_Game.tileX + kHeadingOffsets[heading].deltaX;
        const int chickenY = g_Game.tileY + kHeadingOffsets[heading].deltaY;
        if (!IsInsideMap(chickenX, chickenY) || FgRef(chickenX, chickenY) != Tile::ChickenMarker) continue;

        FgRef(chickenX, chickenY) = Tile::Empty;
        g_Game.collectDir   = heading;
        g_Game.collectFrame = 0;
        g_Game.shipFacing   = kShipFacingIdle;
        ++g_Game.chickensRescued;
        profile.score += ChickenRescueScore();
        g_Sounds.chicken.Play(0);
        break;
    }
}

void SoundHornIfKeyHeld() {
    if (!IsKeyDown(Vk::H) && !IsKeyDown(Vk::Numpad8)) return;
    if (!g_Sounds.horn.IsPlaying()) g_Sounds.horn.Play(0);
}

// Ctrl shorts out an adjacent alien (checks up, right, down, left; first match only).
void ShortAdjacentAlien() {
    if (g_Game.tubeDir != kHeadingNone || !IsKeyDown(Vk::Control)) return;

    for (int heading = kHeadingUp; heading <= kHeadingLeft; ++heading) {
        const int alienX = g_Game.tileX + kHeadingOffsets[heading].deltaX;
        const int alienY = g_Game.tileY + kHeadingOffsets[heading].deltaY;
        if (FgAt(alienX, alienY) == Tile::AlienMarker) {
            FgRef(alienX, alienY) = Tile::Empty;   // the alien notices its missing marker and dies
            break;
        }
    }
}

// The red alert colour (palette entry 1) fades back after an alien hit.
void FadeAlertColor() {
    const SDL_Color color = GetPaletteEntry(kAlertPaletteIndex);
    if (color.r == 0) return;
    SetPaletteEntry(kAlertPaletteIndex, color.r > kAlertFadeStep ? color.r - kAlertFadeStep : 0, color.g, color.b);
}

void AnimateExitDoor() {
    if (g_Game.reactorDestroyed && g_Game.exitDoorFrame < kExitDoorLastFrame && (g_Game.frameCounter % 4) == 0) {
        ++g_Game.exitDoorFrame;
    }
}

void TickEscapeCountdown() {
    if (g_Game.escapeCountdown < 0) return;

    if (timeGetTime() - g_Game.countdownTimer > 1000) {
        if (g_Game.escapeCountdown < kEscapeSeconds - 1) g_Sounds.clunk.Play(0);
        --g_Game.escapeCountdown;
        g_Game.countdownTimer = timeGetTime();
    }
    if (g_Game.escapeCountdown < 1) {   // out of time
        g_Game.escapeCountdown = -1;
        EndLevel();
    }
}

void UpdateGameplay(PlayerProfile& profile) {
    RefuelOnPad();
    UpdateShip(profile);
    RescueAdjacentChicken(profile);
    UpdateChickens();
    UpdateAliens();
    SoundHornIfKeyHeld();
    ShortAdjacentAlien();
    FadeAlertColor();
    AnimateExitDoor();
    TickEscapeCountdown();
}

// Animations that advance while drawing ----------------------------------------------

// The chicken-slurp animation advances every 3rd frame while the ship is visible.
void AdvanceCollectAnimation() {
    const bool shipVisible = g_Game.tubeDir == kHeadingNone && !g_Game.inTubeHidden;
    if (!shipVisible || g_Game.collectDir == kHeadingNone || g_Game.shipFacing >= kShipFacingFirstExplosion) return;
    if ((g_Game.frameCounter % 3) != 0) return;

    if (++g_Game.collectFrame > 8) {
        g_Game.collectDir   = kHeadingNone;
        g_Game.collectFrame = 0;
    }
}

// The explosion plays strictly for facing 5..13 (not 15, which is the zap shimmer) and
// then costs points and either respawns the ship or reloads the level.
void AdvanceShipExplosion(PlayerProfile& profile) {
    if (g_Game.shipFacing < kShipFacingFirstExplosion || g_Game.shipFacing > kShipFacingLastExplosion) return;
    if ((g_Game.frameCounter % 4) != 0) return;
    if (++g_Game.shipFacing <= kShipFacingLastExplosion) return;

    const bool diedDuringEscape = (g_Game.escapeCountdown >= 0);
    g_Game.shipFacing      = kShipFacingIdle;
    g_Game.warpTriggered   = false;
    g_Game.escapeCountdown = -1;
    profile.score          = std::max(0, profile.score - kDeathScorePenalty);
    FadeOutToBlack(1);

    if (diedDuringEscape) {
        g_Game.levelLoaded = false;   // reload so reactor, switch, barriers and timer reset cleanly
    } else {
        RespawnShipAtStart();
    }
}

// Overlays and end-of-frame events ----------------------------------------------------

// Returns false if the player chose to leave (the frame must end immediately).
bool UpdateExitConfirmation() {
    if (!g_Game.paused && !g_Game.exitConfirmOpen && WasKeyJustPressed(Vk::Escape)) {
        g_Game.exitConfirmOpen = true;
        return true;
    }
    if (!g_Game.exitConfirmOpen) return true;

    DrawExitConfirmDialog();
    if (WasKeyJustPressed(Vk::Escape)) {
        g_Sounds.yeah.Play(0);
        g_Game.exitConfirmOpen = false;
    } else if (WasKeyJustPressed(Vk::Return)) {
        g_Sounds.menuMove.Play(0);
        FadeOutToBlack(1);
        g_Game.levelLoaded     = false;
        g_Game.exitConfirmOpen = false;
        SetOrGetGameState(kStatePlayerProfile);
        return false;
    }
    return true;
}

void UpdateVerseOverlay() {
    if (g_Game.verseOverlayId == 0) return;
    DrawBibleVerseOverlay(g_Game.verseOverlayId);
    if (WasKeyJustPressed(Vk::Return)) g_Game.verseOverlayId = 0;
}

void UpdatePause() {
    if (!g_Game.exitConfirmOpen && WasKeyJustPressed(Vk::P)) g_Game.paused = !g_Game.paused;
    if (!g_Game.paused) return;

    DrawPausedBanner();
    if (WasKeyJustPressed(Vk::Return)) g_Game.paused = false;
}

void TeleportIfOnWarp() {
    if (!g_Game.warpTriggered || FgRef(g_Game.tileX, g_Game.tileY) != Tile::Warp) return;

    g_Sounds.warp.Play(0);
    for (const WarpGate& gate : g_Game.warps) {
        if (gate.sourceX == g_Game.tileX && gate.sourceY == g_Game.tileY) {
            g_Game.tileX = gate.destinationX;
            g_Game.tileY = gate.destinationY;
            break;
        }
    }
    g_Game.shipFacing = kShipFacingIdle;
    FadeOutToBlack(1);
    g_Game.warpTriggered = false;
}

void FinishLevelIfExitReached() {
    if (!g_Game.levelExitTriggered) return;

    SavePlayerData();
    FadeOutToBlack(1);
    g_Game.levelLoaded        = false;
    g_Game.levelExitTriggered = false;
    EndLevel();
}

}  

void MovePlayerShip(int deltaX, int deltaY) {
    if (g_Game.tubeDir != kHeadingNone) {
        MoveShipInsideTube();
    } else if (deltaX != 0 || deltaY != 0) {
        MoveShipOnFloor(deltaX, deltaY);
    }
}

void State_MainGame() {
    PlayerProfile& profile = CurrentProfile();

    if (!g_Game.levelLoaded) {
        StartLevel(profile);
        return;
    }

    g_Game.frameCounter = (g_Game.frameCounter + 1) % 256;
    if (g_Game.orbSwitchPressed && timeGetTime() - g_Game.orbSwitchTime > kOrbSwitchDurationMs) {
        g_Game.orbSwitchPressed = false;
    }

    g_Game.displayedFuel  = StepToward(g_Game.displayedFuel,  g_Game.fuel,  kFuelGaugeStep);
    g_Game.displayedScore = StepToward(g_Game.displayedScore, profile.score, kScoreCounterStep);

    if (g_Game.fuel == 0 && g_Game.shipFacing < kShipFacingFirstExplosion) {   // out of fuel: explode
        g_Game.shipFacing = kShipFacingFirstExplosion;
        g_Sounds.explode.Play(0);
    }

    const bool overlayOpen = g_Game.verseOverlayId != 0 || g_Game.paused || g_Game.exitConfirmOpen;
    if (!overlayOpen) UpdateGameplay(profile);

    // The ship is drawn in the screen centre; the camera follows it.
    const Camera camera = { g_Game.tileX - kShipCenterTileX, g_Game.tileY - kShipCenterTileY };

    ClearScreen(1);
    DrawStarfield(camera.leftTile * 4, camera.topTile * 4, false);   // stars scroll 4 px per tile
    DrawWorld(camera);
    DrawPlayerShip();
    AdvanceCollectAnimation();
    AdvanceShipExplosion(profile);
    DrawAliens(camera.leftTile, camera.topTile);
    DrawEscapeCountdown();
    DrawHud(profile);

    if (!UpdateExitConfirmation()) return;
    UpdateVerseOverlay();
    UpdatePause();
    FlipScreen();

    TeleportIfOnWarp();
    FinishLevelIfExitReached();
}
