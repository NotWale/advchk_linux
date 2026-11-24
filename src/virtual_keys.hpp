#pragma once

// Win32 virtual-key codes used by the game (see GetAsyncKeyState in engine.hpp).
namespace Vk {

constexpr int Backspace = 0x08;
constexpr int Return    = 0x0D;
constexpr int Shift     = 0x10;
constexpr int Control   = 0x11;
constexpr int Escape    = 0x1B;
constexpr int Space     = 0x20;
constexpr int Left      = 0x25;
constexpr int Up        = 0x26;
constexpr int Right     = 0x27;
constexpr int Down      = 0x28;
constexpr int Delete    = 0x2E;
constexpr int Numpad8   = 0x68;

// Letter keys are their upper-case ASCII codes.
constexpr int E = 'E';
constexpr int H = 'H';
constexpr int P = 'P';
constexpr int Q = 'Q';
constexpr int R = 'R';
constexpr int W = 'W';

}
