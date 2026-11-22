Modern (Arch) Linux Port of Adventures with Chickens

A linux port of the classic (and basically unknown) game "Adventures with Chickens".
This port uses v3.0 of the game, specifically the egames.exe.
You need either the original egames.exe or the assets.glom file for this to run.
I haven't checked if it works with other versions.

You can compile using cmake:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build
gdb -ex run -ex bt --batch ./game

Or just use the binary. 
Make sure it is placed together with egames.exe (or assets.glom) as well as assets folder containing TimGM6mb.sf2.

## Third-Party Libraries
* **[TinySoundFont](https://github.com)** (`tsf.h`, `tml.h`) — Copyright (c) Bernhard Schelling, licensed under the [MIT License](third_party/tinysoundfont/LICENSE).
