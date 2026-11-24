#include "states.hpp"

#include "game_assets.hpp"
#include "gameplay.hpp"
#include "virtual_keys.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

// Shared screen helpers
constexpr int      kScreenWidth        = 640;
constexpr int      kGlyphWidth         = 11;
constexpr int      kFooterY            = 0x1c7;
constexpr uint32_t kSelectorFrameMs    = 100;
constexpr int      kSelectorFrameCount = 20;     // Selector.bmp: 10 columns x 2 rows
constexpr int      kSelectorColumns    = 10;
constexpr uint32_t kKeyRepeatMs        = 120;

// Animated selection cursor shared by the menu screens.
struct SelectorAnimation {
    uint32_t frame       = 0;
    uint32_t lastAdvance = 0;

    void Reset(uint32_t now) { frame = 0; lastAdvance = now; }
    void Update(uint32_t now) {
        if (now - lastAdvance > kSelectorFrameMs) {
            frame = (frame + 1) % kSelectorFrameCount;
            lastAdvance = now;
        }
    }
    void Draw(int left, int top) const {
        BlitTile(g_Sprites.selector, left, top, frame % kSelectorColumns, frame / kSelectorColumns);
    }
};

// Fires immediately on the first press, then once per kKeyRepeatMs while a key stays held.
class KeyRepeat {
public:
    void Reset() { lastFire_ = 0; }
    bool Poll(bool anyKeyHeld, uint32_t now) {
        if (!anyKeyHeld) { Reset(); return false; }
        if (now - lastFire_ < kKeyRepeatMs) return false;
        lastFire_ = now;
        return true;
    }
private:
    uint32_t lastFire_ = 0;
};

bool IsKeyDown(int virtualKey) { return (GetAsyncKeyState(virtualKey) & kKeyDownBit) != 0; }

void ReturnToPreviousState() { SetOrGetGameState(SetOrGetGameState(kQueryPreviousState)); }

// Starfield, then the current fullscreen image on top.
void DrawFullscreenBackdrop() {
    ClearScreen(0);
    DrawStarfield(0, 0, true);
    BlitTile(g_Sprites.fullscreen, 0, 0, 0, 0, kBlitColorKey);
}

// Splash screens (XGames logo, then RockSolid logo)

enum class SplashPhase { LoadSounds, XGamesLogo, RockSolidLogo };

constexpr uint32_t kXGamesLogoMs    = 0xc6d;    // 3181
constexpr uint32_t kRockSolidLogoMs = 0x10cc;   // 4300

SoundEffect  g_SplashLogoSound;
SoundEffect  g_SplashIntroSound;
SplashPhase  g_SplashPhase = SplashPhase::LoadSounds;
uint32_t     g_SplashTimer = 0;

// Shows one logo and plays its sound the first time it is called for that image.
bool ShowSplashImage(const char* filename, SoundEffect& sound) {
    if (g_Sprites.fullscreen.filename == filename) return false;

    sound.Play(0);
    EnsureFullscreenImage(filename);
    ClearScreen(0);
    BlitTile(g_Sprites.fullscreen, 0, 0, 0, 0, kBlitOpaque);
    FlipScreen();
    g_SplashTimer = timeGetTime();
    WasKeyJustPressed(Vk::Return);
    return true;
}

}  

void State_Splash() {
    const uint32_t now = timeGetTime();
    const bool enterJustPressed = WasKeyJustPressed(Vk::Return);

    switch (g_SplashPhase) {
        case SplashPhase::LoadSounds:
            g_SplashLogoSound.Load("XGames.wav", 1);
            g_SplashIntroSound.Load("Intro.wav", 1);
            g_SplashTimer = now;
            g_SplashPhase = SplashPhase::XGamesLogo;
            break;

        case SplashPhase::XGamesLogo:
            if (ShowSplashImage("XGames.bmp", g_SplashLogoSound)) break;
            if (now - g_SplashTimer >= kXGamesLogoMs || enterJustPressed) {
                g_SplashLogoSound.Stop();
                FadeOutToBlack(1);
                g_SplashTimer = timeGetTime();
                g_SplashPhase = SplashPhase::RockSolidLogo;
            }
            break;

        case SplashPhase::RockSolidLogo:
            if (ShowSplashImage("RockSolid.bmp", g_SplashIntroSound)) break;
            if (now - g_SplashTimer >= kRockSolidLogoMs || enterJustPressed) {
                g_SplashIntroSound.Stop();
                FadeOutToBlack(1);
                RestoreMainPalette();
                g_SplashPhase = SplashPhase::LoadSounds;
                SetOrGetGameState(kStateMainMenu);
            }
            break;
    }
}

// Main menu

namespace {

enum class MenuPhase { Enter, Browse, Activate };

enum MenuItem : int {
    kMenuNewGame = 0, kMenuInstructions, kMenuOptions, kMenuCredits, kMenuExit,
    kMenuAttractMode = 10,   // idle timeout: behaves like Credits
};

constexpr uint32_t kMenuIdleTimeoutMs = 30000;
constexpr int      kMenuItemCount     = 5;
constexpr int      kMenuCursorX       = 0x8c;
constexpr int      kMenuCursorStepY   = 0x32;
constexpr int      kMenuCursorFirstRowOffset = 3;   // item 0 sits at (0 + 3) * 50
constexpr int      kMenuLabelX        = 0xbe;

// Label positions and Selections.bmp rows (row 3 is unused).
struct MenuLabel { int top; int sheetRow; };
constexpr MenuLabel kMenuLabels[] = { {0x96, 0}, {200, 1}, {0xfa, 2}, {300, 4}, {0x163, 5} };

struct MainMenuScreen {
    MenuPhase         phase = MenuPhase::Enter;
    SelectorAnimation selector;
    KeyRepeat         repeat;
    uint32_t          idleSince = 0;
    int               selectedItem = 0;
} g_Menu;

void DrawMainMenu() {
    DrawFullscreenBackdrop();
    g_Menu.selector.Draw(kMenuCursorX, (g_Menu.selectedItem + kMenuCursorFirstRowOffset) * kMenuCursorStepY);
    for (const MenuLabel& label : kMenuLabels) {
        BlitTile(g_Sprites.selections, kMenuLabelX, label.top, 0, label.sheetRow);
    }
    FlipScreen();
}

void LeaveMenuTo(int targetState, SoundEffect& sound) {
    sound.Play(0);
    FadeOutToBlack(1);
    SetOrGetGameState(targetState);
}

void ActivateMenuItem() {
    if (SetOrGetGameState(kQueryCurrentState) != kStateMainMenu) return;

    switch (g_Menu.selectedItem) {
        case kMenuNewGame:      LeaveMenuTo(kStatePlayerProfile, g_Sounds.warp);     break;
        case kMenuInstructions: LeaveMenuTo(kStateInstructions,  g_Sounds.warp);     break;
        case kMenuOptions:      LeaveMenuTo(kStateOptions,       g_Sounds.warp);     break;
        case kMenuCredits:
        case kMenuAttractMode:  LeaveMenuTo(kStateCredits,       g_Sounds.warp);     break;
        case kMenuExit:         LeaveMenuTo(kStateExiting,       g_Sounds.atticDoor); break;
        default: break;
    }
}

void BrowseMenu() {
    const uint32_t now = timeGetTime();
    g_Menu.selector.Update(now);

    if (WasKeyJustPressed(Vk::Return)) {
        g_Menu.phase = MenuPhase::Activate;
    } else if (WasKeyJustPressed(Vk::Escape)) {
        g_Menu.selectedItem = kMenuExit;
        g_Menu.phase        = MenuPhase::Activate;
    }

    const bool upHeld   = IsKeyDown(Vk::Up);
    const bool downHeld = IsKeyDown(Vk::Down);
    if (g_Menu.repeat.Poll(upHeld || downHeld, now)) {
        if (upHeld && g_Menu.selectedItem > 0) {
            --g_Menu.selectedItem;
            g_Sounds.menuMove.Play(0);
        } else if (downHeld && g_Menu.selectedItem < kMenuItemCount - 1) {
            ++g_Menu.selectedItem;
            g_Sounds.menuMove.Play(0);
        }
        g_Menu.idleSince = now;   // any input restarts the attract-mode timer
    }

    if (now - g_Menu.idleSince > kMenuIdleTimeoutMs) {
        g_Menu.selectedItem = kMenuAttractMode;
        g_Menu.phase        = MenuPhase::Activate;
    }

    DrawMainMenu();
}

}  

void State_MainMenu() {
    switch (g_Menu.phase) {
        case MenuPhase::Enter: {
            EnsureFullscreenImage("MainScreen.bmp");
            g_Sounds.chicken.Play(0);
            PlayMidiMusic("Rock2.mds", true);

            const uint32_t now = timeGetTime();
            g_Menu.repeat.Reset();
            g_Menu.selector.Reset(now);
            g_Menu.idleSince   = now;
            g_Menu.selectedItem = 0;
            g_Menu.phase       = MenuPhase::Browse;
            break;
        }
        case MenuPhase::Browse:
            BrowseMenu();
            break;
        case MenuPhase::Activate:
            ActivateMenuItem();
            g_Menu.phase = MenuPhase::Enter;
            break;
    }
}


// Player profile selection


namespace {

enum class ProfilePhase { Enter, Select, ConfirmDelete, Launch, Back };

constexpr int kNameListLeft       = 0x7d;
constexpr int kNameListTop        = 0x8e;
constexpr int kNameListRowHeight  = 0x16;
constexpr int kStatsLeft          = 0x15e;
constexpr int kStatsRankTop       = 0x8f;
constexpr int kStatsScoreTop      = 0xdc;
constexpr int kStatsLevelTop      = 0x12d;
constexpr int kStatsChickensTop   = 0x17d;
constexpr int kProfileCursorX     = 0x52;
constexpr int kProfileCursorTop   = 0x82;
constexpr int kNameCaretLeftBase  = 0x72;
constexpr int kNameCaretTop       = 0x8d;

struct ProfileScreen {
    ProfilePhase      phase = ProfilePhase::Enter;
    SelectorAnimation selector;
    KeyRepeat         repeat;
} g_Profile;

const char* RankTitleForScore(int score) {
    static const struct { int maxScore; const char* title; } kRanks[] = {
        {  10000, "Goober"        }, {  30000, "Lettuce Head" },
        {  50000, "Chicken"       }, {  70000, "Cadet"        },
        {  90000, "Walnut"        }, { 110000, "Flight Man"   },
        { 130000, "Potato"        }, { 150000, "Captain"      },
        { 170000, "SuperStud"     }, { 190000, "General"      },
        { 210000, "Super Chicken" },
    };
    for (const auto& rank : kRanks) {
        if (score <= rank.maxScore) return rank.title;
    }
    return "Child of Light";
}

void CreatePlayer(PlayerProfile& profile) {
    profile.active   = 1;
    profile.level    = 1;
    profile.score    = 0;
    profile.chickens = 0;
    std::fill(std::begin(profile.extra), std::end(profile.extra), '\0');
    SavePlayerData();
}

void DeletePlayer(PlayerProfile& profile) {
    std::fill(std::begin(profile.name), std::end(profile.name), '\0');
    profile.active   = 0;
    profile.score    = 0;
    profile.level    = 0;
    profile.chickens = 0;
    std::fill(std::begin(profile.extra), std::end(profile.extra), '\0');
    SavePlayerData();
}

// Names are limited to kMaxPlayerNameLength; extra typing overwrites the last character.
void AppendNameCharacter(PlayerProfile& profile, char typed) {
    const size_t length = std::strlen(profile.name);
    if (length < kMaxPlayerNameLength) {
        profile.name[length]     = typed;
        profile.name[length + 1] = '\0';
    } else {
        profile.name[kMaxPlayerNameLength - 1] = typed;
        profile.name[kMaxPlayerNameLength]     = '\0';
    }
}

void DrawPlayerProfiles() {
    const PlayerProfile& current = CurrentProfile();

    DrawFullscreenBackdrop();
    for (int slot = 0; slot < kMaxPlayerSlots; ++slot) {
        DrawBitmapText(kNameListLeft, kNameListTop + slot * kNameListRowHeight, g_PlayerSlots[slot].name);
    }

    if (current.active != 0) {
        DrawBitmapText(kStatsLeft, kStatsRankTop,     RankTitleForScore(current.score));
        DrawBitmapText(kStatsLeft, kStatsScoreTop,    std::to_string(current.score));
        DrawBitmapText(kStatsLeft, kStatsLevelTop,    std::to_string(current.level));
        DrawBitmapText(kStatsLeft, kStatsChickensTop, std::to_string(current.chickens));
    }

    if ((g_Profile.selector.frame % 6) > 2) {   // blinking caret after the name
        const int nameLength = static_cast<int>(std::strlen(current.name));
        DrawBitmapText((nameLength + 1) * kGlyphWidth + kNameCaretLeftBase,
                       g_CurrentPlayerIndex * kNameListRowHeight + kNameCaretTop, "_");
    }

    g_Profile.selector.Draw(kProfileCursorX, g_CurrentPlayerIndex * kNameListRowHeight + kProfileCursorTop);

    if (current.active == 0) DrawBitmapText(0x7f, kFooterY, "Type a name to create new player");
    else                     DrawBitmapText(0x85, kFooterY, "Press Enter to play current level");

    if (g_Profile.phase == ProfilePhase::ConfirmDelete) {
        const std::string prompt = std::string("You want to delete ") + current.name + "?";
        const int promptLeft = (kScreenWidth - static_cast<int>(prompt.size()) * kGlyphWidth) / 2;
        DrawConfirmDialog(promptLeft, prompt);
    }
    FlipScreen();
}

void SelectProfile() {
    PlayerProfile& current = CurrentProfile();
    const uint32_t now = timeGetTime();
    g_Profile.selector.Update(now);

    if (WasKeyJustPressed(Vk::Return) && current.active != 0) {
        g_Profile.phase = ProfilePhase::Launch;
    } else if (WasKeyJustPressed(Vk::Escape)) {
        g_Profile.phase = ProfilePhase::Back;
    } else if (WasKeyJustPressed(Vk::Delete) && current.active != 0) {
        g_Profile.phase = ProfilePhase::ConfirmDelete;
    }

    const char typed = GetTypedChar();
    if (typed != '\0') {
        if (current.active == 0) CreatePlayer(current);
        AppendNameCharacter(current, typed);
        g_Sounds.menuMove.Play(0);
    }

    if (WasKeyJustPressed(Vk::Backspace) && current.active != 0) {
        const size_t length = std::strlen(current.name);
        if (length > 0) {
            current.name[length - 1] = '\0';
            g_Sounds.menuMove.Play(0);
        }
    }

    const bool upHeld   = IsKeyDown(Vk::Up);
    const bool downHeld = IsKeyDown(Vk::Down);
    if (g_Profile.repeat.Poll(upHeld || downHeld, now)) {
        if (upHeld) {
            g_CurrentPlayerIndex = (g_CurrentPlayerIndex + kMaxPlayerSlots - 1) % kMaxPlayerSlots;
            g_Sounds.menuMove.Play(0);
        } else if (downHeld) {
            g_CurrentPlayerIndex = (g_CurrentPlayerIndex + 1) % kMaxPlayerSlots;
            g_Sounds.menuMove.Play(0);
        }
    }

    DrawPlayerProfiles();
}

void ConfirmDeleteProfile() {
    if (WasKeyJustPressed(Vk::Escape)) {
        g_Sounds.menuMove.Play(0);
        SavePlayerData();
        g_Profile.phase = ProfilePhase::Select;
    } else if (WasKeyJustPressed(Vk::Return)) {
        g_Sounds.menuMove.Play(0);
        DeletePlayer(CurrentProfile());
        g_Profile.phase = ProfilePhase::Select;
    }
    DrawPlayerProfiles();
}

void LeaveProfileScreen(SoundEffect& sound, int targetState) {
    sound.Play(0);
    SavePlayerData();
    FadeOutToBlack(1);
    g_Profile.phase = ProfilePhase::Enter;
    SetOrGetGameState(targetState);
}

}  

void State_PlayerProfile() {
    switch (g_Profile.phase) {
        case ProfilePhase::Enter:
            EnsureFullscreenImage("Character.bmp");
            WasKeyJustPressed(Vk::Return);   // swallow keys still held from the previous screen
            WasKeyJustPressed(Vk::Escape);
            WasKeyJustPressed(Vk::Delete);
            WasKeyJustPressed(Vk::Backspace);
            g_Profile.selector.Reset(timeGetTime());
            g_Profile.repeat.Reset();
            g_Profile.phase = ProfilePhase::Select;
            break;
        case ProfilePhase::Select:        SelectProfile();        break;
        case ProfilePhase::ConfirmDelete: ConfirmDeleteProfile(); break;
        case ProfilePhase::Launch:        LeaveProfileScreen(g_Sounds.chicken2, kStateMainGame); break;
        case ProfilePhase::Back:          LeaveProfileScreen(g_Sounds.pickup,   kStateMainMenu); break;
    }
}


// Options


namespace {

enum class OptionsPhase { Enter, Edit, Leave };

enum OptionsRow : int { kRowVolume = 0, kRowDifficulty, kRowSound, kRowMusic, kRowJoystick, kRowAlwaysRun, kRowCount };

constexpr int kDifficultyCount   = 5;
constexpr int kOptionsLabelX     = 0x91;
constexpr int kOptionsValueX     = 0x174;
constexpr int kOptionsDifficultyValueX = 0x1a9;
constexpr int kOptionsFirstRowTop  = 0x94;
constexpr int kOptionsRowHeight    = 0x28;
constexpr int kOptionsCursorX      = 0x5a;
constexpr int kOptionsCursorTop    = 0x84;
constexpr int kSliderLeft          = 0x140;
constexpr int kSliderFirstTop      = 0xb9;
constexpr int kSliderWidth         = 0x28;
constexpr int kDifficultySliderWidth = 0x58;

struct OptionsScreen {
    OptionsPhase      phase = OptionsPhase::Enter;
    SelectorAnimation selector;
    KeyRepeat         repeat;
    int               selectedRow = kRowVolume;
    SliderWidget      sliders[kRowCount - 1];   // one per row except the volume row
} g_Options;

void SyncOptionsSliders() {
    SliderWidget* sliders = g_Options.sliders;
    const int values[] = { g_Difficulty, g_SoundEnabled, g_MusicEnabled, g_UseJoystick, g_AlwaysRun };
    for (int index = 0; index < kRowCount - 1; ++index) {
        const bool isDifficulty = (index == 0);
        sliders[index] = { isDifficulty ? kDifficultySliderWidth : kSliderWidth,
                           isDifficulty ? kDifficultyCount       : 2,
                           values[index], kSliderLeft, kSliderFirstTop + index * kOptionsRowHeight };
    }
}

void DrawOptionRow(int row, const char* label, const std::string& value, int valueLeft) {
    const int top = kOptionsFirstRowTop + row * kOptionsRowHeight;
    DrawBitmapText(kOptionsLabelX, top, label);
    DrawBitmapText(valueLeft, top, value);
    DrawSliderWidget(g_Options.sliders[row - 1]);
}

void DrawOptions() {
    static const char* const kDifficultyNames[kDifficultyCount] = {
        "Nonchalant", "Simple", "Typical", "Arduous", "Grueling"
    };
    auto onOff  = [](bool value) { return value ? "On"  : "Off"; };
    auto yesNo  = [](bool value) { return value ? "Yes" : "No";  };

    DrawFullscreenBackdrop();
    DrawBitmapText(kOptionsLabelX, kOptionsFirstRowTop, "Master Volume:");
    DrawBitmapText(kSliderLeft,    kOptionsFirstRowTop, "Press ENTER to modify...");

    DrawOptionRow(kRowDifficulty, "Difficulty:",   kDifficultyNames[g_Difficulty], kOptionsDifficultyValueX);
    DrawOptionRow(kRowSound,      "Sound:",        onOff(g_SoundEnabled),          kOptionsValueX);
    DrawOptionRow(kRowMusic,      "Music:",        onOff(g_MusicEnabled),          kOptionsValueX);
    DrawOptionRow(kRowJoystick,   "Use Joystick:", yesNo(g_UseJoystick),           kOptionsValueX);
    DrawOptionRow(kRowAlwaysRun,  "Always Run:",   yesNo(g_AlwaysRun),             kOptionsValueX);

    g_Options.selector.Draw(kOptionsCursorX, g_Options.selectedRow * kOptionsRowHeight + kOptionsCursorTop);
    DrawBitmapText(0xcd, kFooterY, "Press ESCAPE to exit");
    FlipScreen();
}

// Left (-1) / right (+1) changes the value of the selected row.
void ChangeSelectedOption(int delta) {
    switch (g_Options.selectedRow) {
        case kRowDifficulty:
            g_Difficulty = std::clamp(g_Difficulty + delta, 0, kDifficultyCount - 1);
            break;
        case kRowSound:
            g_SoundEnabled = (delta > 0);
            break;
        case kRowMusic:
            g_MusicEnabled = (delta > 0);
            if (g_MusicEnabled) PlayMidiMusic("Rock2.mds", true);
            else                StopMidiMusic();
            break;
        case kRowJoystick:
            g_UseJoystick = (delta > 0);
            break;
        case kRowAlwaysRun:
            g_AlwaysRun = (delta > 0);
            break;
        default:
            return;   // the volume row has no left/right behaviour
    }
    g_Sounds.clunk.Play(0);
}

void EditOptions() {
    const uint32_t now = timeGetTime();
    g_Options.selector.Update(now);

    if (WasKeyJustPressed(Vk::Escape)) {
        g_Options.phase = OptionsPhase::Leave;
    } else if (WasKeyJustPressed(Vk::Return)) {
        if (g_Options.selectedRow != kRowVolume) g_Options.phase = OptionsPhase::Leave;
    }

    const bool up = IsKeyDown(Vk::Up), down = IsKeyDown(Vk::Down);
    const bool left = IsKeyDown(Vk::Left), right = IsKeyDown(Vk::Right);

    if (g_Options.repeat.Poll(up || down || left || right, now)) {
        int delta = 0;
        if (up) {
            if (g_Options.selectedRow > 0) {
                --g_Options.selectedRow;
                g_Sounds.menuMove.Play(0);
            }
        } else if (down) {
            if (g_Options.selectedRow < kRowCount - 1) {
                ++g_Options.selectedRow;
                g_Sounds.menuMove.Play(0);
            }
        } else if (left) {
            delta = -1;
        } else if (right) {
            delta = 1;
        }

        if (delta != 0) {
            ChangeSelectedOption(delta);
            SyncOptionsSliders();
        }
    }

    DrawOptions();
}

}  

void State_Options() {
    switch (g_Options.phase) {
        case OptionsPhase::Enter:
            EnsureFullscreenImage("Options.bmp");
            g_Sounds.warp.Play(0);
            g_Options.selector.Reset(timeGetTime());
            g_Options.repeat.Reset();
            g_Options.selectedRow = kRowVolume;
            SyncOptionsSliders();
            g_Options.phase = OptionsPhase::Edit;
            break;
        case OptionsPhase::Edit:
            EditOptions();
            break;
        case OptionsPhase::Leave:
            SavePlayerData();
            g_Sounds.yeah.Play(0);
            FadeOutToBlack(1);
            g_Options.phase = OptionsPhase::Enter;
            ReturnToPreviousState();
            break;
    }
}


// Credits and instructions


namespace {

constexpr uint32_t kCreditsDurationMs = 0x7531;   // 30001

bool g_CreditsStarted = false;
uint32_t g_CreditsStart = 0;

}  

void State_Credits() {
    if (!g_CreditsStarted) {
        g_CreditsStart = timeGetTime();
        WasKeyJustPressed(Vk::Return);   // swallow Enter still held from the menu
        WasKeyJustPressed(Vk::Escape);
        g_Sounds.applause.Play(0);
        EnsureFullscreenImage("Credits.bmp");
        g_CreditsStarted = true;
        return;
    }

    const bool exitRequested = WasKeyJustPressed(Vk::Return) || WasKeyJustPressed(Vk::Escape);
    if (timeGetTime() - g_CreditsStart < kCreditsDurationMs && !exitRequested) {
        DrawFullscreenBackdrop();
        FlipScreen();
        return;
    }

    g_Sounds.applause.Stop();
    g_Sounds.pickup.Play(0);
    FadeOutToBlack(1);
    g_CreditsStarted = false;
    ReturnToPreviousState();
}

void State_Instructions() {
    if (WasKeyJustPressed(Vk::Return) || WasKeyJustPressed(Vk::Escape)) {
        g_Sounds.pickup.Play(0);
        FadeOutToBlack(1);
        ReturnToPreviousState();
        return;
    }

    ClearScreen(0);
    DrawStarfield(0, 0, true);
    EnsureFullscreenImage("Instructions.bmp");
    BlitTile(g_Sprites.fullscreen, 0, 0, 0, 0, kBlitColorKey);
    FlipScreen();
}


// Level summary


namespace {

constexpr uint32_t kRevealStepMs    = 778;
constexpr int      kSummaryLines    = 6;         // reveal steps 0..5
constexpr int      kSeparatorLeft   = 120;
constexpr int      kSeparatorTopA   = 130;
constexpr int      kSeparatorTopB   = 158;
constexpr int      kSeparatorTopC   = 263;
constexpr int      kSummaryTextLeft = 150;

struct LevelSummaryScreen {
    bool     initialized     = false;
    bool     progressApplied = false;   // the profile advances only once per summary
    int      revealStep      = 0;       // 0..5 = lines shown so far, 6 = all done
    int      finishedLevel   = 0;       // the level just played (before the increment)
    uint32_t stepTimer       = 0;
} g_Summary;

void DrawSummaryBanner(int textLeft, const char* title) {
    BlitTile(g_Sprites.separator, kSeparatorLeft, kSeparatorTopA, 0, 0, kBlitColorKey);
    BlitTile(g_Sprites.separator, kSeparatorLeft, kSeparatorTopB, 0, 0, kBlitColorKey);
    DrawBitmapText(textLeft, 139, title);
}

// Draws the line for one reveal step (the tally appears line by line, each with a pickup sound).
void DrawSummaryStep(int step, const PlayerProfile& profile, int rescued, int aliensShorted, int aliensTotal) {
    char line[64];
    switch (step) {
        case 0:
            DrawSummaryBanner(238, "LEVEL COMPLETE");
            break;
        case 1:
            std::snprintf(line, sizeof line, "Chickens Caught...%9d", rescued);
            DrawBitmapText(kSummaryTextLeft, 173, line);
            break;
        case 2:
            std::snprintf(line, sizeof line, "Aliens Shorted...%8d/%d", aliensShorted, aliensTotal);
            DrawBitmapText(kSummaryTextLeft, 195, line);
            break;
        case 3:
            std::snprintf(line, sizeof line, "Total Score...%13d", profile.score);
            DrawBitmapText(kSummaryTextLeft, 217, line);
            break;
        case 4:
            std::snprintf(line, sizeof line, "Current Rank...%12s", RankTitleForScore(profile.score));
            DrawBitmapText(kSummaryTextLeft, 239, line);
            break;
        case 5:
            BlitTile(g_Sprites.separator, kSeparatorLeft, kSeparatorTopC, 0, 0, kBlitColorKey);
            std::snprintf(line, sizeof line, "Go %s!", profile.name);
            DrawBitmapText(320 - static_cast<int>(std::strlen(line)) * kGlyphWidth / 2, 273, line);
            break;
        default:
            break;
    }
}

}  

void State_LevelSummary() {
    LevelSummaryScreen& summary = g_Summary;
    PlayerProfile& profile = CurrentProfile();

    const int rescued       = g_Game.chickensRescued;
    const int aliensShorted = g_Game.aliensDefeated;
    const int aliensTotal   = static_cast<int>(g_Game.aliens.size());

    // The countdown is still >= 0 only if the exit was reached in time.
    const bool levelCompleted = (g_Game.escapeCountdown >= 0);

    if (!summary.initialized) {   // build the scene once
        EnsureFullscreenImage("Explosion.bmp");
        SetActivePaletteFromSurface(g_Sprites.fullscreen.surface);   // Explosion.bmp has its own palette

        ClearScreen(0);
        DrawStarfield(0, 0, true);
        BlitTile(g_Sprites.fullscreen, 0, 0, 0, 0, kBlitColorKey);
        FlipScreen();
        g_Sounds.explode.Play(0);

        summary.initialized   = true;
        summary.stepTimer     = timeGetTime();
        summary.revealStep    = 0;
        summary.finishedLevel = profile.level;

        if (!summary.progressApplied && levelCompleted) {
            profile.chickens += rescued;
            profile.level = (profile.level + 1 > kLevelCount) ? 1 : profile.level + 1;
            SavePlayerData();
            summary.progressApplied = true;
        }
        return;
    }

    if (WasKeyJustPressed(Vk::Return) || WasKeyJustPressed(Vk::Escape)) {
        const int finishedLevel = summary.finishedLevel;
        g_Sounds.clink.Play(0);
        FadeOutToBlack();
        RestoreMainPalette();
        summary = LevelSummaryScreen{};
        SetOrGetGameState(finishedLevel == kLevelCount ? kStateEnding : kStatePlayerProfile);
        return;
    }

    if (!levelCompleted) {   // failed: a static banner, no tally
        DrawSummaryBanner(227, "LEVEL INCOMPLETE");
        FlipScreen();
        return;
    }

    // Each step waits at least kRevealStepMs and until all sounds have finished.
    const bool timeToReveal = summary.revealStep < kSummaryLines &&
                              (timeGetTime() - summary.stepTimer) >= kRevealStepMs &&
                              !IsAnySoundPlaying();
    if (timeToReveal) {
        DrawSummaryStep(summary.revealStep, profile, rescued, aliensShorted, aliensTotal);
        g_Sounds.pickup.Play(0);
        ++summary.revealStep;
        summary.stepTimer = timeGetTime();
    }
    FlipScreen();
}


// Ending


namespace {

enum class EndingPhase { Start, Banner, Jingle, WaitForEscape };

constexpr uint32_t kEndingBannerMs = 4001;

struct EndingScreen {
    EndingPhase phase      = EndingPhase::Start;
    uint32_t    phaseStart = 0;
    bool        imageShown = false;
} g_Ending;

// The picture is drawn once; the screen buffer keeps it afterwards.
void ShowCongratulationsOnce() {
    if (g_Ending.imageShown) return;

    PlayMidiMusic("End.mds", true);
    EnsureFullscreenImage("Congratulations.bmp");
    SetActivePaletteFromSurface(g_Sprites.fullscreen.surface);   // the bitmap brings its own palette
    ClearScreen(0);
    BlitTile(g_Sprites.fullscreen, 0, 0, 0, 0, kBlitColorKey);
    g_Ending.imageShown = true;
}

}  

void State_Ending() {
    switch (g_Ending.phase) {
        case EndingPhase::Start:
            g_Ending.phaseStart = timeGetTime();
            g_Ending.phase      = EndingPhase::Banner;
            break;

        case EndingPhase::Banner:
            if (timeGetTime() - g_Ending.phaseStart < kEndingBannerMs) {
                ShowCongratulationsOnce();
            } else {
                g_Ending.phaseStart = timeGetTime();
                g_Ending.phase      = EndingPhase::Jingle;
            }
            break;

        case EndingPhase::Jingle:
            g_Sounds.excellent.Play(0);
            g_Ending.phase = EndingPhase::WaitForEscape;
            break;

        case EndingPhase::WaitForEscape:
            ShowCongratulationsOnce();   // safety net, same as the original's re-check
            if (WasKeyJustPressed(Vk::Escape)) {
                FadeOutToBlack();
                RestoreMainPalette();
                g_Ending = EndingScreen{};
                SetOrGetGameState(kStateMainMenu);
                return;
            }
            break;
    }

    FlipScreen();   // keeps the window refreshed; the picture itself never changes
}
