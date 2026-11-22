#include <SDL2/SDL.h>
#include "engine.hpp"
#include "states.hpp"

bool InitApp() {
    if (!InitEngine("AdvChk", 640, 480)) {
        return false;
    }
    InitMenuAndSharedAssets();
    SetOrGetGameState(0); // Launch into StateSplash
    return !CheckForPendingError();
}

// Replaces RunFrame (00404c90)
int RunFrame() {
    if (!IsWindowActive()) {
        SDL_Delay(16);
        return 1;
    }

    uint32_t frameStart = timeGetTime();

    int state = SetOrGetGameState(-1);
    switch (state) {
        case 0:
            StateSplash();
            break;
        case 1:
            State_MainMenu();
            break;
        case 2:
            State_PlayerProfile();
            break;
        case 3:
            State_MainGame();
            break;
        case 4:
            SetOrGetGameState(-1);
            break;
        case 5:
            State_Credits();
            break;
        case 6:
            State_Instructions();
            break;
        case 7: // State_Exiting
            return 0;
        case 8:
            State_LevelSummary();
            break;
        case 9:
            State_Options();
            break;
        case 10:
            State_Ending();
            break;
        default:
            ClearScreen(0);
            FlipScreen();
            break;
    }

    // ~30 FPS cap (33ms = 0x21ms, using SDL_Delay instead of a CPU-burning spin loop)
    uint32_t elapsed = timeGetTime() - frameStart;
    if (elapsed < 33) {
        SDL_Delay(33 - elapsed);
    }

    return CheckForPendingError() ? 0 : 1;
}

// Replaces WinMain (0040b810)
int main(int argc, char* argv[]) {
    if (InitApp()) {
        int keepRunning = ProcessMessages();
        while (keepRunning != 0 && RunFrame() != 0) {
            keepRunning = ProcessMessages();
        }
    }
    if (CheckForPendingError()) {
        ShowFatalErrorDialog();
    }
    ShutdownEngine();
    return 0;
}
