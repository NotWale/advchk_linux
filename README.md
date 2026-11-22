Modern (Arch) Linux Port of Adventures with Chickens

A linux port of the classic (and basically unknown) game "Adventures with Chickens".
This port uses v3.0 of the game, specifically the egames.exe.
You need either the original egames.exe or the assets.glom file for this to run.
I haven't checked if it works with other versions.

## Requirements & Setup (Linux)

### 1. System Dependencies
Install **SDL2**, **SDL2_mixer**, and a General MIDI **SoundFont** (for music):

* **Ubuntu / Debian / Mint:**
  ```bash
  sudo apt install libsdl2-2.0-0 libsdl2-mixer-2.0-0 fluid-soundfont-gm
  ```
* **Arch Linux / SteamOS:**
  ```bash
  sudo pacman -S sdl2 sdl2_mixer soundfont-fluid
  ```
* **Fedora:**
  ```bash
  sudo dnf install SDL2 SDL2_mixer fluid-soundfont-gm
  ```

### 2. Game Data
Copy your original `egames.exe` (or `assets.glom`) into the same folder as the `game` executable before launching.

### 3. Compiling yourself
You can compile using cmake:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build
gdb -ex run -ex bt --batch ./game

Or just use the binary. 
Make sure it is placed together with egames.exe (or assets.glom) as well as assets folder containing TimGM6mb.sf2.

## Third-Party Libraries
* **[TinySoundFont](https://github.com)** (`tsf.h`, `tml.h`) — Copyright (c) Bernhard Schelling, licensed under the [MIT License](third_party/tinysoundfont/LICENSE).