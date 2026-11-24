#pragma once

// Ids for SetOrGetGameState(); main.cpp dispatches on them every frame.
enum GameStateId : int {
    kStateSplash           = 0,
    kStateMainMenu         = 1,
    kStatePlayerProfile    = 2,
    kStateMainGame         = 3,
    kStateReturnToPrevious = 4,
    kStateCredits          = 5,
    kStateInstructions     = 6,
    kStateExiting          = 7,
    kStateLevelSummary     = 8,
    kStateOptions          = 9,
    kStateEnding           = 10,
};

// One call per frame while the matching state is active.
void State_Splash();          // states.cpp
void State_MainMenu();        // states.cpp
void State_PlayerProfile();   // states.cpp
void State_MainGame();        // gameplay.cpp
void State_Credits();         // states.cpp
void State_Instructions();    // states.cpp
void State_LevelSummary();    // states.cpp
void State_Options();         // states.cpp
void State_Ending();          // states.cpp
