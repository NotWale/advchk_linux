#include "engine.hpp"
#include "game_assets.hpp"
#include "states.hpp"

#include <SDL2/SDL.h>

namespace {

constexpr uint32_t kFrameTimeMs = 33;   // ~30 FPS (the original spins; SDL_Delay keeps the CPU idle)
constexpr uint32_t kInactiveDelayMs = 16;

bool InitApp() {
    if (!InitEngine("AdvChk", 640, 480)) return false;
    LoadGameAssets();
    SetOrGetGameState(kStateSplash);
    return !CheckForPendingError();
}

// Runs the state handler for the current game state. Returns false to quit (RunFrame, 00404c90).
bool RunFrame() {
    if (!IsWindowActive()) {
        SDL_Delay(kInactiveDelayMs);
        return true;
    }

    const uint32_t frameStart = timeGetTime();

    switch (SetOrGetGameState(kQueryCurrentState)) {
        case kStateSplash:           State_Splash();        break;
        case kStateMainMenu:         State_MainMenu();      break;
        case kStatePlayerProfile:    State_PlayerProfile(); break;
        case kStateMainGame:         State_MainGame();      break;
        case kStateReturnToPrevious: break;                 // handled by the states themselves
        case kStateCredits:          State_Credits();       break;
        case kStateInstructions:     State_Instructions();  break;
        case kStateExiting:          return false;
        case kStateLevelSummary:     State_LevelSummary();  break;
        case kStateOptions:          State_Options();       break;
        case kStateEnding:           State_Ending();        break;
        default:
            ClearScreen(0);
            FlipScreen();
            break;
    }

    const uint32_t elapsed = timeGetTime() - frameStart;
    if (elapsed < kFrameTimeMs) SDL_Delay(kFrameTimeMs - elapsed);

    return !CheckForPendingError();
}

}  

// Replaces WinMain (0040b810).
int main(int /*argc*/, char* /*argv*/[]) {
    if (InitApp()) {
        while (ProcessMessages() && RunFrame()) {}
    }
    if (CheckForPendingError()) ShowFatalErrorDialog();
    ShutdownEngine();
    return 0;
}
