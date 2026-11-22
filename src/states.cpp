#include <SDL2/SDL.h>
#include "alien.hpp"
#include "engine.hpp"
#include "states.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

// --- Biblical Verses ---
static const char* g_BibleVerses[] = {
    "Search me, O God, and know my heart; test me and know my anxious thoughts. See if there is any offensive way in me, and lead me in the way everlasting. - Psalm 139:23-24",
    "You know when I sit and when I rise; you perceive my thoughts from afar. You discern my going out and my lying down; you are familiar with all my ways. - Psalm 139:2-3",
    "When I consider your heavens, the work of your fingers, the moon and the stars, which you have set in place, what is man that you are mindful of him, the son of man that you care for him? - Psalm 8:3-4",
    "'Let him who boasts boast about this: that he understands and knows me, that I am the Lord, who exercises kindness, justice and righteousness on earth, for in these I delight', declares the Lord. - Jeremiah 9:24",
    "You are not a God who takes pleasure in evil; with you the wicked cannot dwell. The arrogant cannot stand in your presence; you hate all who do wrong. - Psalm 5:4-5",
    "Once you were alienated from God and were enemies in your minds because of your evil behavior. But now he has reconciled you by Christ's physical body through death to present you holy in his sight . . . Colossians 1:21-22A",
    "You were taught . . . to be made new in the attitude of your minds, and to put on the new self, created to be like God in true righteousness and holiness. - Ephesians 4:22-24",
    "For we are God's workmanship, created in Christ Jesus to do good works, which God prepared in advance for us to do. - Ephesians 2:10",
    "Trust in the Lord with all your heart and lean not on your own understanding; in all your ways acknowledge him, and he will make your paths straight. - Proverbs 3:5-6",
    "I am the Lord, and there is no other. I have not spoken in secret, from somewhere in a land of darkness; I have not said to Jacob's descendants, 'Seek me in vain.' I, the Lord, speak the truth; I declare what is right. - Isaiah 45:18B-19",
    "The woman said, 'I know that Messiah' (called Christ) 'is coming. When he comes, he will explain everything to us.' Then Jesus declared, 'I who speak to you am he.' - John 4:25-26",
    "Then Jesus declared, 'I am the bread of life. He who comes to me will never go hungy, and he who believes in me will never be thirsty.' - John 6:35",
    "'I am the light of the world. Whoever follows me will never walk in darkness, but will have the light of life.' - John 8:12",
    "'You are from below; I am from above. You are of this world; I am not of this world. I told you that you would die in your sins; if you do not believe that I am the one I claim to be, you will indeed die in your sins.' - John 8:23-24",
    "'I tell you the truth,' Jesus answered, 'before Abraham was born, I am!'. At this, they picked up stones to stone him, but Jesus hid himself, slipping away from the temple grounds. - John 8:58-59",
    "'The thief comes only to steal and kill and destroy; I have come that they may have life, and have it to the full. I am the good shepherd. The good shepherd lays down his life for the sheep.' - John 10:10-11",
    "'Do not believe me unless I do what my Father does. But if I do it, even though you do not believe me, believe the miracles, that you may learn and understand that the Father is in me, and I in the Father.' - John 10:37-38",
    "Jesus said to her, 'I am the resurrection and the life. He who believes in me will live, even though he dies.' - John 11:25",
    "'You call me 'Teacher' and 'Lord,' and rightly so, for that is what I am. Now that I, your Lord and Teacher, have washed your feet, you also should wash one another's feet.' - John 13:13-14",
    "Jesus answered, 'I am the way and the truth and the life. No one comes to the Father except through me.' - John 14:6",
    "'I am the vine; you are the branches. If a man remains in me and I in him, he will bear much fruit; apart from me you can do nothing.' - John 15:5",
    "'Behold, I am coming soon! My reward is with me, and I will give to everyone according to what he has done. I am the Alpha and Omega, the First and the Last, the Beginning and the End.' - Revelation 22:12-13",
    "'I am God, and there is no other; I am God, and there is none like me.' - Isaiah 46:9",
    "You said in your heart, 'I will ascend to heaven, I will raise my throne above the stars of God... I will make myself like the Most High.' - Isaiah 14:13-14",
    "The devil said to him, 'If you are the Son of God, tell this stone to become bread.' Jesus answered, 'It is written: 'Man does not live on bread alone.'' - Luke 4:3-4",
    "'Come to me, all you who are weary and burdened, and I will give you rest... For my yoke is easy and my burden is light.' - Matthew 11:28,30",
    "'I tell you the truth, unless a kernel of wheat falls into the ground and dies, it remains a single seed. But if it dies, it produces many seeds.' - John 12:24",
    "I urge, then, first of all, that requests, prayers, intercession and thanksgiving be made for everyone--for kings and all those in authority... - 1 Timothy 2:1-2",
    "Honor your father and mother, so that you may live long in the land the Lord your God is giving you. - Exodus 20:12"
};

// --- Shared State Assets ---
static SpriteSheet g_TilesBmp;
static SpriteSheet g_FullscreenBmp; // Shared slot for MainScreen.bmp / Credits.bmp / Instructions.bmp
static SpriteSheet g_SelectorBmp;
static SpriteSheet g_SelectionsBmp;

SoundEffect g_SfxAlienhit;
static SoundEffect g_SfxApplause;
static SoundEffect g_SfxBounce;
SoundEffect g_SfxBroken;
static SoundEffect g_SfxChicken;
static SoundEffect g_SfxChicken2;
static SoundEffect g_SfxClink;
static SoundEffect g_SfxClunk;
static SoundEffect g_SfxDown;
static SoundEffect g_SfxDribble;
static SoundEffect g_SfxDrink;
static SoundEffect g_SfxError;
static SoundEffect g_SfxExcellent;
static SoundEffect g_SfxExitDoor;
static SoundEffect g_SfxExplode;
static SoundEffect g_SfxHarp;
SoundEffect g_SfxHorn;
static SoundEffect g_SfxMenuMove;
static SoundEffect g_SfxOpenDoor;
static SoundEffect g_SfxPickup;
static SoundEffect g_SfxRev;
static SoundEffect g_SfxWarp;
static SoundEffect g_SfxWhistle;
static SoundEffect g_SfxYeah;

// --- Gameplay Sprite Sheets ---
static SpriteSheet g_ShipBmp;
static SpriteSheet g_ChickenBmp;
static SpriteSheet g_ChickensSlurpBmp; 
SpriteSheet g_AliensBmp;
static SpriteSheet g_FuelBmp;
static SpriteSheet g_SeparatorBmp;
static SpriteSheet g_NumberFontBmp;
static SpriteSheet g_TubesBmp;
static SpriteSheet g_PausedBmp;
static SpriteSheet g_ExitBmp;
static SpriteSheet g_ArrowsBmp;
static SpriteSheet g_CrossesBmp;
static SpriteSheet g_WarpTileBmp;
static SpriteSheet g_CupBmp;

static struct MainMenuState {
    int      phase         = 0;
    uint32_t animFrame     = 0;
    uint32_t idleTimer     = 0;
    uint32_t animTimer     = 0;
    uint32_t inputTimer    = 0;
    int      selectedIndex = 0;
} g_Menu;

static SoundEffect g_SfxSplash1;
static SoundEffect g_SfxSplash2;
static int         g_SplashPhase = 0;
static uint32_t    g_SplashTimer = 0;
static bool        g_SplashEnterWasDown = false;

static struct PlayerProfileState {
    int      phase      = 0;
    uint32_t animFrame  = 0;
    uint32_t animTimer  = 0;
    uint32_t inputTimer = 0;
} g_Profile;

static struct OptionsState {
    int          phase       = 0;
    uint32_t     animFrame   = 0;
    uint32_t     animTimer   = 0;
    uint32_t     inputTimer  = 0;
    int          selectedRow = 0;
    SliderWidget sliders[5];
} g_Options;

MainGameState g_Game;

struct LevelSummaryState {
    bool     initialized     = false;  // DAT_0042a934
    bool     progressApplied = false;  // DAT_0042a938
    int      revealStep      = 0;      // DAT_0042a93c: 0..5 = lines, 6 = done
    int      finishedLevel   = 0;      // DAT_0042a940: level BEFORE the increment
    uint32_t stepTimer       = 0;      // DAT_0042a86c
};
static LevelSummaryState g_Summary;

struct EndingState {
    int      phase      = 0;       // DAT_0042a948: 0 start, 1 banner (4 s), 2 jingle, 3 wait for Esc
    uint32_t phaseStart = 0;       // _DAT_0042a944
    bool     imageShown = false;
};
static EndingState g_Ending;

static void SyncOptionsSliders() {
    g_Options.sliders[0] = { 0x58, 5, g_Difficulty,              0x140, 0xb9 };
    g_Options.sliders[1] = { 0x28, 2, g_SoundEnabled ? 1 : 0,    0x140, 0xe1 };
    g_Options.sliders[2] = { 0x28, 2, g_MusicEnabled ? 1 : 0,    0x140, 0x109 };
    g_Options.sliders[3] = { 0x28, 2, g_UseJoystick  ? 1 : 0,    0x140, 0x131 };
    g_Options.sliders[4] = { 0x28, 2, g_AlwaysRun    ? 1 : 0,    0x140, 0x159 };
}

static uint32_t g_CreditsTimer = 0;
static int      g_CreditsPhase = 0;

// Helper matching the game's lazy fullscreen BMP loader
static void EnsureFullscreenBmp(const std::string& filename) {
    if (g_FullscreenBmp.filename != filename) {
        if (g_FullscreenBmp.surface) SDL_FreeSurface(g_FullscreenBmp.surface);
        g_FullscreenBmp = LoadSpriteSheet(filename, 0, 0, true);
        if (g_FullscreenBmp.surface &&
            (filename == "XGames.bmp" || filename == "RockSolid.bmp")) {
            SetActivePaletteFromSurface(g_FullscreenBmp.surface);
        } else if (g_TilesBmp.surface) {
            SetActivePaletteFromSurface(g_TilesBmp.surface);
        }
    }
}

void StateSplash() {
    uint32_t now = timeGetTime();
    bool enterJustPressed = WasKeyJustPressed(0x0D); // VK_RETURN

    if (g_SplashPhase == 0) {
        g_SfxSplash1.Load("XGames.wav", 1);
        g_SfxSplash2.Load("Intro.wav", 1);
        g_SplashTimer = now;
        g_SplashPhase = 1;
        return;
    }

    if (g_SplashPhase == 1) {
        if (g_FullscreenBmp.filename != "XGames.bmp") {
            g_SfxSplash1.Play(0);
            EnsureFullscreenBmp("XGames.bmp");
            ClearScreen(0);
            BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0);
            FlipScreen();
            g_SplashTimer = timeGetTime();
            WasKeyJustPressed(0x0D);
            return;
        }

        if ((now - g_SplashTimer >= 0xc6d) || enterJustPressed) {
            g_SfxSplash1.Stop();
            FadeOutToBlack(1);
            g_SplashTimer = timeGetTime();
            g_SplashPhase = 2;
        }
        return;
    }

    if (g_SplashPhase == 2) {
        if (g_FullscreenBmp.filename != "RockSolid.bmp") {
            g_SfxSplash2.Play(0);
            EnsureFullscreenBmp("RockSolid.bmp");
            ClearScreen(0);
            BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0);
            FlipScreen();
            g_SplashTimer = timeGetTime();
            WasKeyJustPressed(0x0D);
            return;
        }

        if ((now - g_SplashTimer >= 0x10cc) || enterJustPressed) {
            g_SfxSplash2.Stop();
            FadeOutToBlack(1);
            if (g_TilesBmp.surface) SetActivePaletteFromSurface(g_TilesBmp.surface);
            g_SplashPhase = 0;
            SetOrGetGameState(1);
        }
    }
}

uint8_t& FgRef(int x, int y) { return g_Game.fgMap[y * 240 + x]; }
uint8_t  FgAt (int x, int y) {
    if (x < 0 || y < 0 || x >= 240 || y >= 132) return 1;   // outside the map counts as wall
    return g_Game.fgMap[y * 240 + x];
}

static void RenderBibleVerseOverlay(int verseId) {
    constexpr int numVerses = static_cast<int>(sizeof(g_BibleVerses) / sizeof(g_BibleVerses[0]));
    if (verseId < 1 || verseId > numVerses) return;

    const char* text = g_BibleVerses[verseId - 1];
    std::vector<std::string> lines;
    std::string currentLine;
    std::string currentWord;

    for (size_t i = 0; ; ++i) {
        char c = text[i];
        if (c == ' ' || c == '\0') {
            if (!currentWord.empty()) {
                // Wrap at 36 chars (matches 0x24 < ~uVar6 + (~uVar5 - 2) in biblicVerseRender)
                if (!currentLine.empty() && (currentLine.size() + currentWord.size() > 36)) {
                    lines.push_back(currentLine);
                    currentLine.clear();
                }
                currentLine += currentWord + " ";
                currentWord.clear();
            }
            if (c == '\0') break;
        } else {
            currentWord += c;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    // Dim the screen behind the verse (matches FUN_0040e830(Dark.bmp))
    DimScreen(0.4f);

    int totalLines = static_cast<int>(lines.size());
    int halfHeight = totalLines * 9;
    for (int row = 0; row < totalLines; ++row) {
        int y = (row * 18 - halfHeight) + 220;
        int x = 320 - ((static_cast<int>(lines[row].size()) * 11) >> 1);
        DrawBitmapText(x, y, lines[row], false);
    }
}

static void CreatePlayer(PlayerProfile& p) {
    p.active   = 1;
    p.level    = 1;
    p.score    = 0;
    p.chickens = 0;
    std::memset(p.extra, 0, sizeof(p.extra));
    SavePlayerData();
}

static void DeletePlayer(PlayerProfile& p) {
    std::memset(p.name, 0, sizeof(p.name));
    p.active   = 0;
    p.score    = 0;
    p.level    = 0;
    p.chickens = 0;
    std::memset(p.extra, 0, sizeof(p.extra));
    SavePlayerData();
}

static const char* GetRankTitle(int score) {
    if (score <= 10000)  return "Goober";
    if (score <= 30000)  return "Lettuce Head";
    if (score <= 50000)  return "Chicken";
    if (score <= 70000)  return "Cadet";
    if (score <= 90000)  return "Walnut";
    if (score <= 110000) return "Flight Man";
    if (score <= 130000) return "Potato";
    if (score <= 150000) return "Captain";
    if (score <= 170000) return "SuperStud";
    if (score <= 190000) return "General";
    if (score <= 210000) return "Super Chicken";
    return "Child of Light";
}

const char* RankForScore(int score) {
    static const struct { int maxScore; const char* title; } kRanks[] = {
        {  10000, "Goober"        }, {  30000, "Lettuce Head" },
        {  50000, "Chicken"       }, {  70000, "Cadet"        },
        {  90000, "Walnut"        }, { 110000, "Flight Man"   },
        { 130000, "Potato"        }, { 150000, "Captain"      },
        { 170000, "SuperStud"     }, { 190000, "General"      },
        { 210000, "Super Chicken" },
    };
    for (const auto& r : kRanks)
        if (score <= r.maxScore) return r.title;
    return "Child of Light";
}

static void LoadLevelFromGlom() {
    g_Game.warpTriggered = false;
    g_Game.collectDir = 0; 
    g_Game.collectFrame = 0;
    g_Game.exitDoorFrame = 0;
    g_Game.escapeCountdown = -1;
    g_Game.levelExitTriggered = 0;
    g_Game.tubeDir = 0;
    g_Game.inTubeHidden = false;

    PlayerProfile& prof = g_PlayerSlots[g_CurrentPlayerIndex];
    if (prof.level < 1 || prof.level > 30) prof.level = 1;
    SavePlayerData();

    std::vector<uint8_t> levelsData;
    if (!GetGlomEntryBytes("Levels.dat", levelsData)) {
        std::fprintf(stderr, "Missing Levels.dat in assets.glom!\n");
        return;
    }

    size_t hdrOff = static_cast<size_t>(prof.level * 3 + 0x0d);
    if (hdrOff + 3 > levelsData.size()) return;

    size_t pos = (static_cast<size_t>(levelsData[hdrOff]) << 16) |
                 (static_cast<size_t>(levelsData[hdrOff + 1]) << 8) |
                 static_cast<size_t>(levelsData[hdrOff + 2]);

    std::memset(g_Game.fgMap, 0, sizeof(g_Game.fgMap));
    std::memset(g_Game.bgMap, 0, sizeof(g_Game.bgMap));
    g_Game.warps.clear();
    g_Game.chickens.clear();
    g_Game.aliens.clear();
    g_Game.chickensRescued  = 0;
    g_Game.totalChickens    = 0;
    g_Game.aliensDefeated   = 0;
    g_Game.totalAliens      = 0;
    g_Game.reactorDestroyed = false;
    g_Game.tubeDir          = 0;
    g_Game.inTubeHidden     = false;
    g_Game.shipFacing       = 0;
    g_Game.fuel             = 300;

    int tileIdx = 0;
    while (tileIdx < 0x7bc0 && pos < levelsData.size()) {
        uint8_t bRaw = levelsData[pos++];
        uint8_t tileVal = static_cast<uint8_t>((bRaw & 0x7f) - 3);

        if (tileVal == 8) {
            g_Game.fgMap[tileIdx] = 8;
            if (pos + 2 <= levelsData.size()) {
                WarpGate w;
                w.srcX = tileIdx % 240;
                w.srcY = tileIdx / 240;
                w.dstX = std::clamp<int>(levelsData[pos++], 0, 239);
                w.dstY = std::clamp<int>(levelsData[pos++], 0, 131);
                g_Game.warps.push_back(w);
            }
            tileIdx += 1;
        } else {
            if (pos >= levelsData.size()) break;
            uint8_t countLow = levelsData[pos++];
            int runLen = ((bRaw & 0xc0) << 2) + countLow;
            for (int r = 0; r < runLen && tileIdx < 0x7bc0; ++r, ++tileIdx) {
                int tx = tileIdx % 240;
                int ty = tileIdx / 240;
                g_Game.fgMap[tileIdx] = tileVal;

                if (tileVal == 9) {
                    g_Game.reactorX = tx;
                    g_Game.reactorY = ty;
                } else if (tileVal == 0xfd) { // Alien spawn
                    AlienEntity alien;
                    alien.x = tx;
                    alien.y = ty;
                    g_Game.aliens.push_back(alien);
                    g_Game.totalAliens++;
                } else if (tileVal == 0xfe) { // Chicken spawn
                    ChickenEntity chicken;
                    chicken.x = tx;
                    chicken.y = ty;
                    g_Game.chickens.push_back(chicken);
                    g_Game.totalChickens++;
                } else if (tileVal == 0xff) { // Player spawn
                    g_Game.spawnX = g_Game.tileX = tx;
                    g_Game.spawnY = g_Game.tileY = ty;
                    g_Game.fgMap[tileIdx] = 0;
                }
            }
        }
    }

    // Cast static laser beams into bgMap from emitters (0x1e..0x21)
    for (int idx = 0; idx < 0x7bc0; ++idx) {
        uint8_t t = g_Game.fgMap[idx];
        int x = idx % 240;
        int y = idx / 240;
        if (t == 0x1e) { // Up beam
            for (int ny = y - 1; ny >= 0; --ny) {
                uint8_t ft = g_Game.fgMap[ny * 240 + x];
                if (ft >= 0x1e && ft <= 0x21) break;
                uint8_t& bg = g_Game.bgMap[ny * 240 + x];
                bg = (bg != 0) ? 3 : 1;
            }
        } else if (t == 0x1f) { // Right beam
            for (int nx = x + 1; nx < 240; ++nx) {
                uint8_t ft = g_Game.fgMap[y * 240 + nx];
                if (ft >= 0x1e && ft <= 0x21) break;
                uint8_t& bg = g_Game.bgMap[y * 240 + nx];
                bg = (bg != 0) ? 3 : 2;
            }
        } else if (t == 0x20) { // Down beam
            for (int ny = y + 1; ny < 132; ++ny) {
                uint8_t ft = g_Game.fgMap[ny * 240 + x];
                if (ft >= 0x1e && ft <= 0x21) break;
                uint8_t& bg = g_Game.bgMap[ny * 240 + x];
                bg = (bg != 0) ? 3 : 1;
            }
        } else if (t == 0x21) { // Left beam
            for (int nx = x - 1; nx >= 0; --nx) {
                uint8_t ft = g_Game.fgMap[y * 240 + nx];
                if (ft >= 0x1e && ft <= 0x21) break;
                uint8_t& bg = g_Game.bgMap[y * 240 + nx];
                bg = (bg != 0) ? 3 : 2;
            }
        }
    }

    // Play rotating level MIDI music ((level - 1) % 5)
    if (g_MusicEnabled) {
        const char* levelTracks[] = {
            "Run.mds", "Organjam.mds", "Sweet.mds", "Jamin.mds", "Rock1.mds"
        };
        PlayMidiMusic(levelTracks[(prof.level - 1) % 5], true);
    }
}

void MovePlayerShip(int dx, int dy) {
    // --- Inside Transport Pipe (tubeDir: 1=Up, 2=Right, 3=Down, 4=Left) ---
    if (g_Game.tubeDir != 0) {
        int tdx = 0, tdy = 0;
        if (g_Game.tubeDir == 1)      tdy = -1;
        else if (g_Game.tubeDir == 2) tdx =  1;
        else if (g_Game.tubeDir == 3) tdy =  1;
        else if (g_Game.tubeDir == 4) tdx = -1;

        g_Game.shipFacing = g_Game.tubeDir;
        int nx = std::clamp(g_Game.tileX + tdx, 0, 239);
        int ny = std::clamp(g_Game.tileY + tdy, 0, 131);
        uint8_t nextTile = g_Game.fgMap[ny * 240 + nx];

        // Corner bend redirection (tiles 0x16..0x19)
        if (!g_Game.inTubeHidden && nextTile >= 0x16 && nextTile <= 0x19) {
            int corner = nextTile - 0x16; // 0..3
            if ((g_Game.tubeDir & 3) == corner) {
                g_Game.tubeDir = corner + 1;
                g_SfxBounce.Play(0);
            } else if (((g_Game.tubeDir + 1) & 3) == corner) {
                g_Game.tubeDir = (((nextTile - 0x15) & 3) + 1);
                g_SfxBounce.Play(0);
            }
        }

        // Exited the end of the pipe
        if ((nextTile < 0x14 || nextTile > 0x22) && !g_Game.inTubeHidden) {
            g_SfxWhistle.Play(0);
            g_Game.tubeDir = 0;
            g_Game.slowStepToggle = 1;
            MovePlayerShip(tdx, tdy);
            return;
        }

        // Tunnel portal toggle (0x1e..0x21)
        if (nextTile >= 0x1e && nextTile <= 0x21) {
            g_Game.inTubeHidden = !g_Game.inTubeHidden;
            if (!g_Game.inTubeHidden) {
                g_Game.tubeDir = (((nextTile - 0x1c) & 3) + 1);
            }
        }

        g_Game.tileX = nx;
        g_Game.tileY = ny;
        g_Game.fuelStepCounter++;
        if (g_Game.fuelStepCounter >= 5) {
            g_Game.fuel = std::max(0, g_Game.fuel - (1 + g_Difficulty));
            g_Game.fuelStepCounter = 0;
        }
        return;
    }

    if (dx == 0 && dy == 0) return;

    bool shiftHeld = (GetAsyncKeyState(0x10) & 0x8000) != 0;
    if (!shiftHeld && !g_AlwaysRun && g_Game.tubeDir == 0) {
        g_Game.slowStepToggle++;
        if (g_Game.slowStepToggle != 2) return;
        g_Game.slowStepToggle = 0;
    }

    if (dy < 0) g_Game.shipFacing = 1;
    else if (dx > 0) g_Game.shipFacing = 2;
    else if (dy > 0) g_Game.shipFacing = 3;
    else if (dx < 0) g_Game.shipFacing = 4;

    int nx = std::clamp(g_Game.tileX + dx, 0, 239);
    int ny = std::clamp(g_Game.tileY + dy, 0, 131);
    int nIdx = ny * 240 + nx;
    uint8_t target = g_Game.fgMap[nIdx];

    // Pushable boulder (0x11)
    if (target == 0x11) {
        int bx = std::clamp(nx + dx, 0, 239);
        int by = std::clamp(ny + dy, 0, 131);
        if (g_Game.fgMap[by * 240 + bx] == 0) {
            g_Game.fgMap[nIdx] = 0;
            g_Game.fgMap[by * 240 + bx] = 0x11;
            g_SfxClunk.Play(0);
            target = 0;
        }
    }

    // magic ball press
    if (target == 3 && g_Game.ballOrbActive == 0) {
        for (int i = 0; i < 0x7bc0; ++i) {
            uint8_t t = g_Game.fgMap[i];
            if (t == 4)      g_Game.fgMap[i] = 5;
            else if (t == 5) g_Game.fgMap[i] = 6;
            else if (t == 6) g_Game.fgMap[i] = 7;
            else if (t == 7) g_Game.fgMap[i] = 4;
        }
        g_Game.lastBallSwitchTime = timeGetTime();
        g_Game.ballOrbActive = 1;
        g_SfxClunk.Play(0);
    }

    // One-way conveyor checks
    if ((target == 7 && dx > 0) || (target == 5 && dx < 0) ||
        (target == 4 && dy > 0) || (target == 6 && dy < 0)) {
        return;
    }

    // Reactor detonator switch (0x12)
    if (target == 0x12 && dy == 1) {
        if (g_Game.chickensRescued == g_Game.totalChickens && !g_Game.reactorDestroyed) {
            g_Game.reactorDestroyed = true;
            for (int i = 0; i < 0x7bc0; ++i) {
                if (g_Game.fgMap[i] == 0x0d) g_Game.fgMap[i] = 0; // Unlock exit barriers
            }
            g_Game.fgMap[g_Game.reactorY * 240 + g_Game.reactorX] = 10; // Spawn exit portal
            g_Game.fgMap[nIdx] = 0x13; // Flip switch
            g_Game.escapeCountdown = 0x2d; // 45s escape timer
            g_Game.escapeTimer = timeGetTime();
            g_SfxError.Play(0);
        } else if(!g_SfxOpenDoor.IsPlaying()) {
            g_SfxOpenDoor.Play(0);
        }
        return;
    }

    bool passable = (target != 1 && target != 0x0d && target != 9 &&
                     target != 0xfd && target != 0xfe && target != 3 &&
                     (target < 0x14 || target > 0x19) &&
                     (target < 0x1e || target > 0x22) &&
                     (target != 0x1a || dy ==  1) &&
                     (target != 0x1b || dx == -1) &&
                     (target != 0x1c || dy == -1) &&
                     (target != 0x1d || dx ==  1) &&
                     target != 0x12 && target != 0x13 && target != 0x11) && !AlienStandsAt(nx, ny);

    if (passable) {
        g_Game.tileX = nx;
        g_Game.tileY = ny;

        // Entering a transport pipe mouth locks tubeDir (1=Up, 2=Right, 3=Down, 4=Left)
        if ((target == 0x1a && dy ==  1) ||
            (target == 0x1b && dx == -1) ||
            (target == 0x1c && dy == -1) ||
            (target == 0x1d && dx ==  1)) {
            g_Game.tubeDir = (((target - 0x18) & 3) + 1);
        }

        g_Game.fuelStepCounter++;
        if (g_Game.fuelStepCounter >= 5) {
            g_Game.fuel = std::max(0, g_Game.fuel - (1 + g_Difficulty));
            g_Game.fuelStepCounter = 0;
        }
    }

    PlayerProfile& prof = g_PlayerSlots[g_CurrentPlayerIndex];
    if (target == 8) {
        g_Game.warpTriggered = true;
    } else if (target == 2) {
        g_Game.fuel = 0;
    } else if (target == 0x0c) { // Fuel canister
        g_Game.fuel = std::min(300, g_Game.fuel + 50);
        g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX] = 0;
        g_SfxYeah.Play(0);
    } else if (target == 0x0e) {
        prof.score += (g_Difficulty + 1) * 50;
        g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX] = 0;
        g_SfxPickup.Play(0);
    } else if (target == 0x0f) {
        prof.score += (g_Difficulty + 8) * 50;
        g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX] = 0;
        g_SfxPickup.Play(0);
    } else if (target == 0x10) {
        prof.score += (g_Difficulty + 18) * 50;
        g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX] = 0;
        g_SfxPickup.Play(0);
    } else if (target == 10) { // Exit portal -> Level Complete!
        g_Game.levelExitTriggered = true;
    }
}

static void UpdateChickens() {
    for (auto& chicken : g_Game.chickens) {
        if (chicken.x < 0 || chicken.y < 0) continue;
        int idx = chicken.y * 240 + chicken.x;
        if (g_Game.fgMap[idx] != 0xfe) {
            chicken.x = -1;
            chicken.y = -1;
            continue;
        }

        chicken.rescued++; // reused as 0..6 step timer (param_1)
        if (chicken.rescued > 6) {
            int dir = std::rand() & 3;
            int nx = chicken.x, ny = chicken.y;
            int baseFacing = chicken.anim;
            const bool inShipLine = (ny == g_Game.tileY) || (nx == g_Game.tileX);

            if (dir == 0 && chicken.y + 1 < 132) {
                ny = chicken.y + 1; baseFacing = 0;
            } else if (dir == 1 && chicken.x + 1 < 240) {
                nx = chicken.x + 1; baseFacing = 2;
            } else if (dir == 2 && chicken.y - 1 >= 0) {
                ny = chicken.y - 1; baseFacing = 4;
            } else if (dir == 3 && chicken.x - 1 >= 0) {
                nx = chicken.x - 1; baseFacing = 6;
            }

            if ((nx != chicken.x || ny != chicken.y) &&
                g_Game.fgMap[ny * 240 + nx] == 0 &&
                !(nx == g_Game.tileX && ny == g_Game.tileY) &&
                !inShipLine &&
                !AlienStandsAt(nx, ny)){
                    g_Game.fgMap[idx] = 0;
                    chicken.x = nx;
                    chicken.y = ny;
                    g_Game.fgMap[ny * 240 + nx] = 0xfe;
                    chicken.anim = baseFacing;
            }

            chicken.anim += (std::rand() & 1) ? -1 : 1;
            chicken.anim = std::clamp(chicken.anim, 0, 7);
            chicken.rescued = 0;
        }
    }
}

void State_LevelSummary() {
    LevelSummaryState& s = g_Summary;
    PlayerProfile& prof  = g_PlayerSlots[g_CurrentPlayerIndex];

    const int rescued      = g_Game.chickensRescued;
    const int aliensShorted = g_Game.aliensDefeated;
    const int aliensTotal  = static_cast<int>(g_Game.aliens.size());

    // The countdown is still >= 0 only if the exit was reached in time.
    const bool levelCompleted = (g_Game.escapeCountdown >= 0);

    // build the scene once
    if (!s.initialized) {
        EnsureFullscreenBmp("Explosion.bmp");
        SetActivePaletteFromSurface(g_FullscreenBmp.surface);  // Explosion.bmp carries its own palette

        ClearScreen(0);
        DrawStarfield(0, 0, true);
        BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);
        FlipScreen();
        g_SfxExplode.Play(0);

        s.initialized   = true;
        s.stepTimer     = timeGetTime();
        s.revealStep    = 0;
        s.finishedLevel = prof.level;

        if (!s.progressApplied && levelCompleted) {
            prof.chickens += rescued;
            prof.level     = (prof.level + 1 > 30) ? 1 : prof.level + 1;
            SavePlayerData();
            s.progressApplied = true;
        }
        return;
    }

    // Leave: Enter or Esc
    if (WasKeyJustPressed(0x0D) || WasKeyJustPressed(0x1B)) {
        const int finished = s.finishedLevel;
        g_SfxClink.Play(0);
        FadeOutToBlack();
        SetActivePaletteFromSurface(g_TilesBmp.surface);    // main palette back
        s = LevelSummaryState{};                            // resets initialized + progressApplied
        SetOrGetGameState(finished == 30 ? 10 : 2);         // 10 = ending, 2 = player profile
        return;
    }

    // Failed: static banner every frame, no tally
    if (!levelCompleted) {
        BlitTile(g_SeparatorBmp, 120, 130, 0, 0, 0x8000);
        BlitTile(g_SeparatorBmp, 120, 158, 0, 0, 0x8000);
        DrawBitmapText(227, 139, "LEVEL INCOMPLETE");
        FlipScreen();
        return;
    }

    // Completed: reveal one line per step
    // Each step waits >= 778 ms AND until all sounds have finished.
    const bool timeToReveal = s.revealStep <= 5
                           && (timeGetTime() - s.stepTimer) >= 778
                           && !IsAnySoundPlaying();
    if (timeToReveal) {
        char line[64];
        switch (s.revealStep) {
        case 0:
            BlitTile(g_SeparatorBmp, 120, 130, 0, 0, 0x8000);
            BlitTile(g_SeparatorBmp, 120, 158, 0, 0, 0x8000);
            DrawBitmapText(238, 139, "LEVEL COMPLETE");
            break;
        case 1:
            std::snprintf(line, sizeof line, "Chickens Caught...%9d", rescued);
            DrawBitmapText(150, 173, line);
            break;
        case 2:
            std::snprintf(line, sizeof line, "Aliens Shorted...%8d/%d", aliensShorted, aliensTotal);
            DrawBitmapText(150, 195, line);
            break;
        case 3:
            std::snprintf(line, sizeof line, "Total Score...%13d", prof.score);
            DrawBitmapText(150, 217, line);
            break;
        case 4:
            std::snprintf(line, sizeof line, "Current Rank...%12s", RankForScore(prof.score));
            DrawBitmapText(150, 239, line);
            break;
        case 5:
            BlitTile(g_SeparatorBmp, 120, 263, 0, 0, 0x8000);
            std::snprintf(line, sizeof line, "Go %s!", prof.name);
            DrawBitmapText(320 - static_cast<int>(std::strlen(line)) * 11 / 2, 273, line);
            break;
        }
        g_SfxPickup.Play(0);
        ++s.revealStep;
        s.stepTimer = timeGetTime();
    }
    FlipScreen();
}

// The original draws the image once; the screen buffer keeps it afterwards.
static void ShowCongratulationsOnce(EndingState& ending) {
    if (ending.imageShown) return;

    PlayMidiMusic("End.mds", true);
    EnsureFullscreenBmp("Congratulations.bmp");
    SetActivePaletteFromSurface(g_FullscreenBmp.surface);   // the bitmap brings its own palette
    ClearScreen(0);
    BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);
    ending.imageShown = true;
}

void State_Ending() {
    EndingState& ending = g_Ending;

    switch (ending.phase) {
    case 0:
        ending.phaseStart = timeGetTime();
        ending.phase = 1;
        break;

    case 1:
        if (timeGetTime() - ending.phaseStart < 4001) {
            ShowCongratulationsOnce(ending);
        } else {
            ending.phaseStart = timeGetTime();
            ending.phase = 2;
        }
        break;

    case 2:
        g_SfxExcellent.Play(0);
        ending.phase = 3;
        break;

    case 3:
        ShowCongratulationsOnce(ending);   // safety net, same as the original's re-check
        if (WasKeyJustPressed(0x1B)) {
            FadeOutToBlack();
            SetActivePaletteFromSurface(g_TilesBmp.surface);   // main palette back
            ending = EndingState{};                            // phase 0, image not shown
            SetOrGetGameState(1);
            return;
        }
        break;
    }

    FlipScreen();   // keeps the window refreshed; the picture itself never changes
}

static void EndLevel() {
    g_Game.levelLoaded    = false;   // next time state 3 runs, the level is loaded again
    g_Game.messageOverlay = 0;       // no leftover verse overlay
    // also reset your rolling fuel gauge value to 0 here
    SavePlayerData();
    SetOrGetGameState(8);
}

static void RespawnShipAtStart() {
    g_Game.tileX         = g_Game.spawnX;
    g_Game.tileY         = g_Game.spawnY;
    g_Game.fuel          = 300;
    g_Game.shipFacing    = 0;
    g_Game.tubeDir       = 0;        // not riding a pipe any more
    g_Game.inTubeHidden  = false;
    g_Game.collectDir    = 0;
    g_Game.collectFrame  = 0;
    g_Game.warpTriggered = false;

    // Diagnostic
    if (g_Game.tileX < 0 || g_Game.tileX >= 240 || g_Game.tileY < 0 || g_Game.tileY >= 132) {
        std::printf("RESPAWN OUT OF RANGE: spawn=(%d,%d)\n", g_Game.spawnX, g_Game.spawnY);
    }
}

static void RevEngineIfShiftHeld() {
    static uint32_t lastRevMs = 0;
    static int revCount = 0;
    const uint32_t now = timeGetTime();
    if (now - lastRevMs > 250 && revCount < 2) {
        g_SfxRev.Play(0);
        ++revCount;
        lastRevMs = now;
    }
    if (now - lastRevMs > 2500) revCount = 0;
}

void State_MainGame() {
    PlayerProfile& prof = g_PlayerSlots[g_CurrentPlayerIndex];

    if (!g_Game.levelLoaded) {
        LoadLevelFromGlom();
        g_Game.displayedFuel = 0;
        g_Game.exitDoorFrame = 1;
        g_Game.displayedScore = prof.score;
        g_Game.escapeCountdown = -1;
        g_Game.pauseFlag = false;
        g_Game.exitConf = false;
        g_Game.deathTrigger = false;
        g_Game.levelLoaded = true;
        WasKeyJustPressed(0x1B);
        WasKeyJustPressed(0x0D);
        constexpr int numVerses = static_cast<int>(sizeof(g_BibleVerses) / sizeof(g_BibleVerses[0]));
        g_Game.messageOverlay = (numVerses > 0) ? ((std::rand() % numVerses) + 1) : 0;
        return;
    }

    uint32_t now = timeGetTime();
    g_Game.frameCounter = (g_Game.frameCounter + 1) % 256;

    if (now - g_Game.lastBallSwitchTime > 500) {
        g_Game.ballOrbActive = 0;
    }

    // Smoothly animate fuel & score counters
    if (g_Game.displayedFuel < g_Game.fuel) g_Game.displayedFuel = std::min(g_Game.fuel, g_Game.displayedFuel + 3);
    else if (g_Game.displayedFuel > g_Game.fuel) g_Game.displayedFuel = std::max(g_Game.fuel, g_Game.displayedFuel - 3);
    if (g_Game.displayedScore < prof.score) g_Game.displayedScore = std::min(prof.score, g_Game.displayedScore + 50);
    else if (g_Game.displayedScore > prof.score) g_Game.displayedScore = std::max(prof.score, g_Game.displayedScore - 50);

    // Out-of-fuel death check
    if (g_Game.fuel == 0 && g_Game.shipFacing < 5) {
        g_Game.shipFacing = 5;
        g_SfxExplode.Play(0);
    }

    // Active gameplay update
    if (g_Game.messageOverlay == 0 && !g_Game.pauseFlag && !g_Game.exitConf) {
        // Fuel refill pad (tile 0x0b)
        if (g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX] == 0x0b && g_Game.fuel < 300) {
            g_Game.fuel = std::min(300, g_Game.fuel + 1);
            if (!g_SfxDrink.IsPlaying()) {
                g_SfxDrink.Play(0);
            }
        }

        // Ship movement (exact original cadence: every 2nd frame normally, every frame when Shift/AlwaysRun)
        if (g_Game.shipFacing < 5 && (g_Game.collectDir == 0 || g_Game.collectFrame >= 6)) {
            if (g_Game.tubeDir != 0) {
                MovePlayerShip(0, 0);
            } else {
                int dx = 0, dy = 0;
                if (GetAsyncKeyState(0x25) & 0x8000) dx = -1;
                else if (GetAsyncKeyState(0x27) & 0x8000) dx = 1;
                else if (GetAsyncKeyState(0x26) & 0x8000) dy = -1;
                else if (GetAsyncKeyState(0x28) & 0x8000) dy = 1;
                else {
                    uint8_t under = g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX];
                    if (under == 4)      { dy = -1; if (!g_SfxDribble.IsPlaying()) g_SfxDribble.Play(0); } // bouncing on them arrows
                    else if (under == 5) { dx =  1; if (!g_SfxDribble.IsPlaying()) g_SfxDribble.Play(0); }
                    else if (under == 6) { dy =  1; if (!g_SfxDribble.IsPlaying()) g_SfxDribble.Play(0); }
                    else if (under == 7) { dx = -1; if (!g_SfxDribble.IsPlaying()) g_SfxDribble.Play(0); }
                    else {
                        g_Game.shipFacing = 0;   // standing still: the original writes state 0 here
                    }
                }
                if (dx != 0 || dy != 0) {
                    MovePlayerShip(dx, dy);
                }
                if (GetAsyncKeyState(0x10) & 0x8000) RevEngineIfShiftHeld();
            }

            // Cheats (original: playerShipUpdate). Shift + Ctrl held, plus one of Q / W / E / R.
            const bool shiftHeld = (GetAsyncKeyState(0x10) & 0x8000) != 0;
            const bool ctrlHeld  = (GetAsyncKeyState(0x11) & 0x8000) != 0;
            if (shiftHeld && ctrlHeld) {
                if (GetAsyncKeyState('Q') & 0x8000) g_Game.fuel = 5;
                if (GetAsyncKeyState('W') & 0x8000) g_Game.fuel = 300;
                if (GetAsyncKeyState('E') & 0x8000) prof.score += 3000;   // per frame while held, like the original
                if (GetAsyncKeyState('R') & 0x8000) {
                    g_Game.escapeCountdown = 2;   // >= 0 means "level completed" for the summary
                    EndLevel();
                }
            }
        }

        // Spacebar rescues adjacent chicken (0xfe)
        if (g_Game.collectDir == 0 && (GetAsyncKeyState(0x20) & 0x8000)) {
            const int ox[] = { 0, 1, 0, -1 };
            const int oy[] = { -1, 0, 1, 0 };
            for (int d = 0; d < 4; ++d) {
                int cx = g_Game.tileX + ox[d];
                int cy = g_Game.tileY + oy[d];
                if (cx >= 0 && cx < 240 && cy >= 0 && cy < 132) {
                    if (g_Game.fgMap[cy * 240 + cx] == 0xfe) {
                        g_Game.fgMap[cy * 240 + cx] = 0;
                        g_Game.collectDir   = d + 1; // 1=Up, 2=Right, 3=Down, 4=Left
                        g_Game.collectFrame = 0;
                        g_Game.shipFacing   = 0;
                        g_Game.chickensRescued++;
                        prof.score += (g_Difficulty + 5) * 50;
                        g_SfxChicken.Play(0);
                        break;
                    }
                }
            }
        }

        UpdateChickens();
        UpdateAliens();

        // Horn
        if (GetAsyncKeyState('H') & 0x8000 || GetAsyncKeyState(0x68) & 0x8000)
        if (!g_SfxHorn.IsPlaying()) g_SfxHorn.Play(0);

        // Ctrl: short an adjacent alien (checks up, right, down, left; first match only).
        if (g_Game.tubeDir == 0 && (GetAsyncKeyState(0x11) & 0x8000)) {
            static const int neighborDeltaX[4] = { 0, 1, 0, -1 };   // up, right, down, left
            static const int neighborDeltaY[4] = { -1, 0, 1, 0 };
            for (int neighborIndex = 0; neighborIndex < 4; ++neighborIndex) {
                const int neighborX = g_Game.tileX + neighborDeltaX[neighborIndex];
                const int neighborY = g_Game.tileY + neighborDeltaY[neighborIndex];
                if (FgAt(neighborX, neighborY) == 0xfd) {
                    FgRef(neighborX, neighborY) = 0;
                    std::printf("SHORT: erased marker at (%d,%d)\n", neighborX, neighborY);   // temporary
                    break;
                }
            }
        }

        SDL_Color c = GetPaletteEntry(1);
        if (c.r > 0) SetPaletteEntry(1, c.r > 4 ? c.r - 4 : 0, c.g, c.b);

        // open the exit door
        if (g_Game.reactorDestroyed && g_Game.exitDoorFrame < 3 && (g_Game.frameCounter % 4) == 0) {
            g_Game.exitDoorFrame++;
        }

        if (g_Game.escapeCountdown >= 0) {
            // 1-second tick
            if (timeGetTime() - g_Game.countdownTimer > 1000) {
                if (g_Game.escapeCountdown < 44) g_SfxClunk.Play(0);
                --g_Game.escapeCountdown;
                g_Game.countdownTimer = timeGetTime();
            }
            // ... draw the two digits ...

            if (g_Game.escapeCountdown < 1) {      // inside the guard!
                g_Game.escapeCountdown = -1;
                EndLevel();                        // helper, see below
            }
        }

    }

    // --- RENDER ---
    ClearScreen(1);
    int camX = g_Game.tileX - 20;
    int camY = g_Game.tileY - 15;
    DrawStarfield(camX * 4, camY * 4, false);

    // Draw 40x31 visible tilemap
    for (int vy = 0; vy < 31; ++vy) {
        int my = camY + vy;
        if (my < 0 || my >= 132) continue;

        for (int vx = 0; vx < 40; ++vx) {
            int mx = camX + vx;
            if (mx < 0 || mx >= 240) continue;
            int idx = my * 240 + mx;
            int dstX = vx * 16;
            int dstY = vy * 16 - 8;
            uint8_t bg = g_Game.bgMap[idx];

            if (bg != 0) {
                BlitTile(g_TilesBmp, dstX, dstY, bg * 2, 2, 0x8000);
            }

            uint8_t fg = g_Game.fgMap[idx];

            if (fg != 0) {
                if (fg == 1) {
                    // Solid rock wall -> Tiles.bmp (0, 0)
                    BlitSubRect(g_TilesBmp, dstX, dstY, 0, 0, 16, 16, 0x8000);
                } else if (fg == 2) {
                    // Hazard -> Tiles.bmp cols 2..3
                    int col = (std::rand() & 1) + 2;
                    BlitSubRect(g_TilesBmp, dstX, dstY, col * 16, 0, 16, 16, 0x8000);
                } else if (fg == 3) {
                    // Magic Orb Switch -> Tiles.bmp col 4 (0x40) or col 5 (0x50) when active
                    BlitSubRect(g_TilesBmp, dstX, dstY, (4 + g_Game.ballOrbActive) * 16, 0, 16, 16, 0x8000);
                } else if (fg >= 4 && fg <= 7) {
                    // Directional conveyor streams -> Arrows.bmp
                    int animCol = (g_Game.frameCounter >> 1) & 7;
                    int dirRow  = fg - 4;
                    BlitSubRect(g_ArrowsBmp, dstX, dstY, animCol * 16, dirRow * 16, 16, 16, 0x8000);
                } else if (fg == 8) {
                    // Warp gate -> WarpTile.bmp
                    BlitTile(g_WarpTileBmp, dstX, dstY, (g_Game.frameCounter >> 2) % 4, 0, 0x8000);
                } else if (fg == 9) {
                    // Closed Exit Door -> Exit.bmp frame 0
                    BlitTile(g_ExitBmp, dstX, dstY, 0, 0, 0x8000);
                } else if (fg == 10) {
                    // Open Exit Door -> Exit.bmp frame 3 (or animating 1..3)
                    BlitTile(g_ExitBmp, dstX, dstY, g_Game.exitDoorFrame, 0, 0x8000);
                } else if (fg == 11) {
                    // Fuel pad -> Tiles.bmp cols 13..16
                    int col = ((g_Game.frameCounter >> 4) & 3) + 13;
                    BlitSubRect(g_TilesBmp, dstX, dstY, col * 16, 0, 16, 16, 0x8000);
                } else if (fg == 12) {
                    // Fuel canister -> Tiles.bmp col 6 (0x60)
                    BlitSubRect(g_TilesBmp, dstX, dstY, 0x60, 0, 16, 16, 0x8000);
                } else if (fg == 13) {
                    // Locked barrier -> Tiles.bmp cols 10..12
                    int col = ((g_Game.frameCounter % 60) / 20) + 10;
                    BlitSubRect(g_TilesBmp, dstX, dstY, col * 16, 0, 16, 16, 0x8000);
                } else if (fg >= 14 && fg <= 16) {
                    // Bonus Xs / gems -> Crosses.bmp (row 0, 1, 2)
                    int animCol = (g_Game.frameCounter >> 2) % 7;
                    int typeRow = fg - 14;
                    BlitSubRect(g_CrossesBmp, dstX, dstY, animCol * 16, typeRow * 16, 16, 16, 0x8000);
                } else if (fg == 17) {
                    // Pushable boulder -> Tiles.bmp col 1 (0x10)
                    BlitSubRect(g_TilesBmp, dstX, dstY, 0x10, 0, 16, 16, 0x8000);
                } else if (fg == 18) {
                    // Reactor switch unpressed -> Tiles.bmp col 7 (0x70)
                    BlitSubRect(g_TilesBmp, dstX, dstY, 0x70, 0, 16, 16, 0x8000);
                } else if (fg == 19) {
                    // Reactor switch pressed -> Tiles.bmp cols 8..9
                    int col = (std::rand() & 1) + 8;
                    BlitSubRect(g_TilesBmp, dstX, dstY, col * 16, 0, 16, 16, 0x8000);
                } else if (fg >= 0x14 && fg <= 0x22) {
                    // Transport pipes / tubes -> Tubes.bmp!
                    int srcX = 0, srcY = 0;
                    if (fg < 0x1b) {
                        srcX = (fg - 0x14) * 32;
                        srcY = 0;
                    } else if (fg < 0x22) {
                        srcX = (fg - 0x1b) * 32;
                        srcY = 16;
                    } else {
                        srcX = 0;
                        srcY = 32;
                    }
                    BlitSubRect(g_TubesBmp, dstX, dstY, srcX, srcY, 16, 16, 0x8000);
                }
            }
        }

        // Draw each live chicken using its individual facing & wing-flap frame (chicken.anim)
        for (const auto& chicken : g_Game.chickens) {
            if (chicken.x >= camX && chicken.x < camX + 40 && chicken.y >= camY && chicken.y < camY + 31) {
                BlitTile(g_ChickenBmp, (chicken.x - camX) * 16, (chicken.y - camY) * 16 - 8, chicken.anim, 0, 0x8000);
            }
        }
    }

    // Draw Player Ship at center (2016, 1516 - 8)
    if (g_Game.tubeDir == 0 && !g_Game.inTubeHidden) {
        int shipScreenX = 20 * 16;
        int shipScreenY = 15 * 16 - 8;

        if (g_Game.collectDir != 0 && g_Game.shipFacing < 5) {
            int dirRow = g_Game.collectDir - 1;

            if (g_Game.collectFrame < 6) {
                BlitTile(g_ChickensSlurpBmp, shipScreenX, shipScreenY, g_Game.collectFrame * 2 + 1, dirRow, 0x8000);
                int extX = shipScreenX + ((g_Game.collectDir == 2) ? 16 : (g_Game.collectDir == 4) ? -16 : 0);
                int extY = shipScreenY + ((g_Game.collectDir == 3) ? 16 : (g_Game.collectDir == 1) ? -16 : 0);
                BlitTile(g_ChickensSlurpBmp, extX, extY, g_Game.collectFrame * 2, dirRow, 0x8000);
            } else {
                BlitTile(g_ChickensSlurpBmp, shipScreenX, shipScreenY, g_Game.collectFrame + 6, dirRow, 0x8000);
            }
            
            if ((g_Game.frameCounter % 3) == 0) {
                g_Game.collectFrame++;
                if (g_Game.collectFrame > 8) {
                    g_Game.collectDir = 0;
                    g_Game.collectFrame = 0;
                }
            }
        } else {
            int drawCol = (g_Game.shipFacing == 15) ? (15 + (std::rand() & 3)) : g_Game.shipFacing;
            BlitTile(g_ShipBmp, shipScreenX, shipScreenY, drawCol, 0, 0x8000);
        }
    }

    // Explosion animation runs strictly for frames 5..13 (NOT 15 = warp shimmer!)
    if (g_Game.shipFacing >= 5 && g_Game.shipFacing <= 13) {
        if ((g_Game.frameCounter % 4) == 0) {
            g_Game.shipFacing++;
            if (g_Game.shipFacing >= 14) {
                bool diedDuringEscape = (g_Game.escapeCountdown >= 0);
                g_Game.shipFacing      = 0;
                g_Game.warpTriggered   = false;
                g_Game.escapeCountdown = -1;
                prof.score             = std::max(0, prof.score - 1000);
                FadeOutToBlack(1);

                if (diedDuringEscape) {
                    // Reload the whole level so reactor, switch, barriers, and timer reset cleanly
                    g_Game.levelLoaded = false;
                } else {
                    RespawnShipAtStart();
                }
            }
        }
    }

    DrawAliens(camX, camY);

    // Escape Countdown Digits (NumberFont.bmp)
    if (g_Game.escapeCountdown >= 0) {
        int tens = (g_Game.escapeCountdown / 10) % 10;
        int ones = g_Game.escapeCountdown % 10;
        BlitTile(g_NumberFontBmp, 0x118, 0, tens % 5, tens / 5, 0x8000);
        BlitTile(g_NumberFontBmp, 0x136, 0, ones % 5, ones / 5, 0x8000);
    }

    // Bottom HUD Bars, Chicken Counter & Alien Counter
    BlitTile(g_CupBmp, 0, 0x1b3, 0, 0, 0x8000);
    BlitTile(g_CupBmp, 0x21c, 0x1b3, 1, 0, 0x8000);
    BlitTile(g_ChickenBmp, 0xe, 0x1c5, (g_Game.frameCounter & 0x1f) >> 2, 0, 0x8000);
    std::string cRescStr = std::to_string(g_Game.chickensRescued);
    DrawBitmapText(0x36 - static_cast<int>(cRescStr.size()) * 11, 0x1c6, cRescStr, false);
    DrawBitmapText(0x36, 0x1c5, "/", false);
    DrawBitmapText(0x3d, 0x1c6, std::to_string(g_Game.totalChickens), false);

    // Vertical Fuel Gauge (Fuel.bmp at X = 0x267, Y = 5)
    if (g_Game.displayedFuel > 0) {
        int fuelH = std::clamp(g_Game.displayedFuel, 0, 300);
        int fuelTier = ((fuelH - 1) / 100) + 1;
        BlitSubRect(g_FuelBmp, 0x267, 0x131 - fuelH, fuelTier * 20, 300 - fuelH, 20, fuelH, 0x8000);
    }
    BlitTile(g_FuelBmp, 0x267, 5, 0, 0, 0x8000);

    // Bottom-Right Alien Counter
    BlitTile(g_AliensBmp, 0x22d, 0x1c5, (g_Game.frameCounter & 0x1f) >> 2, 0, 0x8000);
    std::string aDefStr = std::to_string(g_Game.aliensDefeated);
    DrawBitmapText(0x255 - static_cast<int>(aDefStr.size()) * 11, 0x1c6, aDefStr, false);
    DrawBitmapText(0x255, 0x1c5, "/", false);
    DrawBitmapText(0x25c, 0x1c6, std::to_string(g_Game.totalAliens), false);

    // Top-Left Level & Score
    DrawBitmapText(5, 5, "Level: " + std::to_string(prof.level), false);
    DrawBitmapText(5, 0x19, "Score: " + std::to_string(g_Game.displayedScore), false);

    // Exit Confirmation Dialog (ESC)
    if (!g_Game.pauseFlag && !g_Game.exitConf && WasKeyJustPressed(0x1B)) {
        g_Game.exitConf = true;
    } else if (g_Game.exitConf) {
        FillScreenRect(0x75, 200, 0x197, 0x50, 1);
        DrawBitmapText(0x9b, 0xd8, "You wish to exit this level?", false);
        DrawBitmapText(0xa6, 0xf8, "ENTER=Accept ESCAPE=Cancel", false);
        if (WasKeyJustPressed(0x1B)) {
            g_SfxYeah.Play(0);
            g_Game.exitConf = false;
        } else if (WasKeyJustPressed(0x0D)) {
            g_SfxMenuMove.Play(0);
            FadeOutToBlack(1);
            g_Game.levelLoaded = false;
            g_Game.exitConf = false;
            SetOrGetGameState(2); // Return to PlayerProfile
            return;
        }
    }

    // Bible Verses
    if (g_Game.messageOverlay != 0) {
        RenderBibleVerseOverlay(g_Game.messageOverlay);
        if (WasKeyJustPressed(0x0D)) {
            g_Game.messageOverlay = 0;
        }
    }

    // Pause Toggle ('P')
    if (!g_Game.exitConf && WasKeyJustPressed('P')) {
        g_Game.pauseFlag = !g_Game.pauseFlag;
    }
    if (g_Game.pauseFlag) {
        BlitTile(g_PausedBmp, 220, 200, 0, 0, 0x8000);
        if (WasKeyJustPressed(0x0D)) {
            g_Game.pauseFlag = false;
        }
    }
    FlipScreen();

    // make the tp happen
    if (g_Game.warpTriggered && g_Game.fgMap[g_Game.tileY * 240 + g_Game.tileX] == 8) {
        g_SfxWarp.Play(0);
        for (const auto& w : g_Game.warps) {
            if (w.srcX == g_Game.tileX && w.srcY == g_Game.tileY) {
                g_Game.tileX = w.dstX;
                g_Game.tileY = w.dstY;
                break;
            }
        }
        g_Game.shipFacing = 0;
        FadeOutToBlack(1);
        g_Game.warpTriggered = false;
    }

    if (g_Game.levelExitTriggered) {
        SavePlayerData();
        FadeOutToBlack(1);
        g_Game.levelLoaded        = false;
        g_Game.levelExitTriggered = false;
        EndLevel();
    }
}

void PlayerProfile_Render() {
    ClearScreen(0);
    DrawStarfield(0, 0, true);
    BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);

    // Draw all 12 player names (X = 0x7d, Y = 0x8e + i * 0x16)
    for (int i = 0; i < 12; ++i) {
        DrawBitmapText(0x7d, 0x8e + i * 0x16, g_PlayerSlots[i].name, false);
    }

    // Draw selected player's Rank, Score, Level, and Chickens on right side
    PlayerProfile& curr = g_PlayerSlots[g_CurrentPlayerIndex];
    if (curr.active != 0) {
        DrawBitmapText(0x15e, 0x8f,  GetRankTitle(curr.score), false);
        DrawBitmapText(0x15e, 0xdc,  std::to_string(curr.score), false);
        DrawBitmapText(0x15e, 0x12d, std::to_string(curr.level), false);
        DrawBitmapText(0x15e, 0x17d, std::to_string(curr.chickens), false);
    }

    // Blinking underscore cursor '_' at end of current player's name
    if ((g_Profile.animFrame % 6) > 2) {
        int nameLen = static_cast<int>(std::strlen(curr.name));
        DrawBitmapText((nameLen + 1) * 0xb + 0x72, g_CurrentPlayerIndex * 0x16 + 0x8d, "_", false);
    }

    // Animated Selector cursor
    BlitTile(
        g_SelectorBmp,
        0x52,
        g_CurrentPlayerIndex * 0x16 + 0x82,
        g_Profile.animFrame % 10,
        g_Profile.animFrame / 10,
        0x8000
    );

    // Context-sensitive footer
    if (curr.active == 0) {
        DrawBitmapText(0x7f, 0x1c7, "Type a name to create new player", false);
    } else {
        DrawBitmapText(0x85, 0x1c7, "Press Enter to play current level", false);
    }

    // Delete Confirmation Modal (phase == 2)
    if (g_Profile.phase == 2) {
        FillScreenRect(0x75, 200, 0x197, 0x50, 1);

        std::string prompt = std::string("You want to delete ") + curr.name + "?";
        int promptX = (0x280 - static_cast<int>(prompt.size()) * 0xb) / 2;
        DrawBitmapText(promptX, 0xd8, prompt, false);
        DrawBitmapText(0xa6, 0xf8, "ENTER=Accept ESCAPE=Cancel", false);
    }

    FlipScreen();
}

void State_PlayerProfile() {
    PlayerProfile& curr = g_PlayerSlots[g_CurrentPlayerIndex];

    if (g_Profile.phase == 0) {
        EnsureFullscreenBmp("Character.bmp");
        WasKeyJustPressed(0x0D);
        WasKeyJustPressed(0x1B);
        WasKeyJustPressed(0x2E);
        WasKeyJustPressed(0x08);
        uint32_t now = timeGetTime();
        g_Profile.animFrame  = 0;
        g_Profile.animTimer  = now;
        g_Profile.inputTimer = 0;
        g_Profile.phase      = 1;
        return;
    }

    if (g_Profile.phase == 2) {
        if (WasKeyJustPressed(0x1B)) { // Cancel delete
            g_SfxMenuMove.Play(0);
            SavePlayerData();
            g_Profile.phase = 1;
        } else if (WasKeyJustPressed(0x0D)) { // Confirm delete
            g_SfxMenuMove.Play(0);
            DeletePlayer(curr);
            g_Profile.phase = 1;
        }
        PlayerProfile_Render();
        return;
    }

    if (g_Profile.phase == 3) { // Launch MainGame!
        g_SfxChicken2.Play(0);
        SavePlayerData();
        FadeOutToBlack(1);
        g_Profile.phase = 0;
        SetOrGetGameState(3);
        return;
    }

    if (g_Profile.phase == 4) { // Return to MainMenu
        g_SfxPickup.Play(0);
        SavePlayerData();
        FadeOutToBlack(1);
        g_Profile.phase = 0;
        SetOrGetGameState(1);
        return;
    }

    // phase == 1: Slot selection & inline typing
    uint32_t now = timeGetTime();
    if (now - g_Profile.animTimer > 100) {
        g_Profile.animFrame = (g_Profile.animFrame + 1) % 20;
        g_Profile.animTimer = now;
    }

    if (WasKeyJustPressed(0x0D) && curr.active != 0) {
        g_Profile.phase = 3;
    } else if (WasKeyJustPressed(0x1B)) {
        g_Profile.phase = 4;
    } else if (WasKeyJustPressed(0x2E) && curr.active != 0) { // VK_DELETE
        g_Profile.phase = 2;
    }

    // Inline character typing (up to 12 chars)
    char typed = GetTypedChar();
    if (typed != '\0') {
        if (curr.active == 0) {
            CreatePlayer(curr);
        }
        size_t len = std::strlen(curr.name);
        if (len < 12) {
            curr.name[len]     = typed;
            curr.name[len + 1] = '\0';
        } else {
            curr.name[11] = typed;
            curr.name[12] = '\0';
        }
        g_SfxMenuMove.Play(0);
    }

    // Backspace deletes last character
    if (WasKeyJustPressed(0x08) && curr.active != 0) {
        size_t len = std::strlen(curr.name);
        if (len > 0) {
            curr.name[len - 1] = '\0';
            g_SfxMenuMove.Play(0);
        }
    }

    // Up / Down slot navigation
    bool up   = (GetAsyncKeyState(0x26) & 0x8000) != 0;
    bool down = (GetAsyncKeyState(0x28) & 0x8000) != 0;
    if (!up && !down) {
        g_Profile.inputTimer = 0;
    } else if (now - g_Profile.inputTimer >= 120) {
        if (up) {
            g_CurrentPlayerIndex = (g_CurrentPlayerIndex + 11) % 12;
            g_SfxMenuMove.Play(0);
        } else if (down) {
            g_CurrentPlayerIndex = (g_CurrentPlayerIndex + 1) % 12;
            g_SfxMenuMove.Play(0);
        }
        g_Profile.inputTimer = now;
    }

    PlayerProfile_Render();
}

void MainMenu_Render() {
    ClearScreen(0);
    DrawStarfield(0, 0, true);
    BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);

    // Animated Selector cursor (X = 140, Y = (selected + 3) * 50)
    BlitTile(
        g_SelectorBmp,
        0x8c,
        (g_Menu.selectedIndex + 3) * 0x32,
        g_Menu.animFrame % 10,
        g_Menu.animFrame / 10,
        0x8000
    );

    // 5 Menu Labels from Selections.bmp (X = 190, rows 0, 1, 2, 4, 5)
    BlitTile(g_SelectionsBmp, 0xbe, 0x96, 0, 0, 0x8000);
    BlitTile(g_SelectionsBmp, 0xbe, 200,  0, 1, 0x8000);
    BlitTile(g_SelectionsBmp, 0xbe, 0xfa, 0, 2, 0x8000);
    BlitTile(g_SelectionsBmp, 0xbe, 300,  0, 4, 0x8000);
    BlitTile(g_SelectionsBmp, 0xbe, 0x163,0, 5, 0x8000);

    FlipScreen();
}

void State_MainMenu() {
    if (g_Menu.phase == 0) {
        EnsureFullscreenBmp("MainScreen.bmp");
        g_SfxChicken.Play(0);

        PlayMidiMusic("Rock2.mds", true);

        uint32_t now = timeGetTime();
        g_Menu.inputTimer    = 0;
        g_Menu.animTimer     = now;
        g_Menu.idleTimer     = now;
        g_Menu.animFrame     = 0;
        g_Menu.selectedIndex = 0;
        g_Menu.phase         = 1;
        return;
    }

    if (g_Menu.phase == 1) {
        uint32_t now = timeGetTime();
        if (now - g_Menu.animTimer > 100) {
            g_Menu.animFrame = (g_Menu.animFrame + 1) % 20;
            g_Menu.animTimer = now;
        }

        if (WasKeyJustPressed(0x0D)) { // VK_RETURN
            g_Menu.phase = 2;
        } else if (WasKeyJustPressed(0x1B)) { // VK_ESCAPE
            g_Menu.selectedIndex = 4;
            g_Menu.phase = 2;
        }

        bool upPressed   = (GetAsyncKeyState(0x26) & 0x8000) != 0;
        bool downPressed = (GetAsyncKeyState(0x28) & 0x8000) != 0;

        if (!upPressed && !downPressed) {
            g_Menu.inputTimer = 0; // Instant response on next tap!
        } else if (now - g_Menu.inputTimer >= 120) {
            if (upPressed && g_Menu.selectedIndex > 0) {
                g_Menu.selectedIndex--;
                g_SfxMenuMove.Play(0);
            } else if (downPressed && g_Menu.selectedIndex < 4) {
                g_Menu.selectedIndex++;
                g_SfxMenuMove.Play(0);
            }
            g_Menu.inputTimer = now;
            g_Menu.idleTimer  = now; // Reset 30s attract timer on input
        }

        if (now - g_Menu.idleTimer > 30000) {
            g_Menu.selectedIndex = 10; // Attract mode -> Credits
            g_Menu.phase = 2;
        }

        MainMenu_Render();
        return;
    }

    if (g_Menu.phase == 2) {
        if (SetOrGetGameState(-1) == 1) {
            switch (g_Menu.selectedIndex) {
                case 0: // New Game / Player Profile
                    g_SfxWarp.Play(0);
                    FadeOutToBlack(1);
                    SetOrGetGameState(2);
                    break;
                case 1: // Instructions
                    g_SfxWarp.Play(0);
                    FadeOutToBlack(1);
                    SetOrGetGameState(6);
                    break;
                case 2: // Options
                    g_SfxWarp.Play(0);
                    FadeOutToBlack(1);
                    SetOrGetGameState(9);
                    break;
                case 3:
                case 10: // Credits
                    g_SfxWarp.Play(0);
                    FadeOutToBlack(1);
                    SetOrGetGameState(5);
                    break;
                case 4: // Exit
                    g_SfxExitDoor.Play(0);
                    FadeOutToBlack(1);
                    SetOrGetGameState(7);
                    break;
            }
        }
        g_Menu.phase = 0;
    }
}

void Options_Render() {
    ClearScreen(0);
    DrawStarfield(0, 0, true);
    BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);

    // Row 0: Master Volume
    DrawBitmapText(0x91, 0x94, "Master Volume:", false);
    DrawBitmapText(0x140, 0x94, "Press ENTER to modify...", false);

    // Row 1: Difficulty
    const char* diffNames[5] = { "Nonchalant", "Simple", "Typical", "Arduous", "Grueling" };
    DrawBitmapText(0x91, 0xbc, "Difficulty:", false);
    DrawBitmapText(0x1a9, 0xbc, diffNames[g_Difficulty], false);
    DrawSliderWidget(g_Options.sliders[0]);

    // Row 2: Sound
    DrawBitmapText(0x91, 0xe4, "Sound:", false);
    DrawBitmapText(0x174, 0xe4, g_SoundEnabled ? "On" : "Off", false);
    DrawSliderWidget(g_Options.sliders[1]);

    // Row 3: Music
    DrawBitmapText(0x91, 0x10c, "Music:", false);
    DrawBitmapText(0x174, 0x10c, g_MusicEnabled ? "On" : "Off", false);
    DrawSliderWidget(g_Options.sliders[2]);

    // Row 4: Use Joystick
    DrawBitmapText(0x91, 0x134, "Use Joystick:", false);
    DrawBitmapText(0x174, 0x134, g_UseJoystick ? "Yes" : "No", false);
    DrawSliderWidget(g_Options.sliders[3]);

    // Row 5: Always Run
    DrawBitmapText(0x91, 0x15c, "Always Run:", false);
    DrawBitmapText(0x174, 0x15c, g_AlwaysRun ? "Yes" : "No", false);
    DrawSliderWidget(g_Options.sliders[4]);

    // Animated Selector cursor (X = 0x5a, Y = selectedRow * 0x28 + 0x84)
    BlitTile(
        g_SelectorBmp,
        0x5a,
        g_Options.selectedRow * 0x28 + 0x84,
        g_Options.animFrame % 10,
        g_Options.animFrame / 10,
        0x8000
    );

    // Footer
    DrawBitmapText(0xcd, 0x1c7, "Press ESCAPE to exit", false);

    FlipScreen();
}

// Replaces case 9: State_Options (004040e0)
void State_Options() {
    if (g_Options.phase == 0) {
        EnsureFullscreenBmp("Options.bmp");
        g_SfxWarp.Play(0);
        uint32_t now = timeGetTime();
        g_Options.animFrame   = 0;
        g_Options.animTimer   = now;
        g_Options.inputTimer  = 0;
        g_Options.selectedRow = 0;
        SyncOptionsSliders();
        g_Options.phase = 1;
        return;
    }

    if (g_Options.phase == 1) {
        uint32_t now = timeGetTime();
        if (now - g_Options.animTimer > 100) {
            g_Options.animFrame = (g_Options.animFrame + 1) % 20;
            g_Options.animTimer = now;
        }

        if (WasKeyJustPressed(0x1B)) { // VK_ESCAPE
            g_Options.phase = 2;
        } else if (WasKeyJustPressed(0x0D)) { // VK_RETURN
            if (g_Options.selectedRow != 0) {
                g_Options.phase = 2;
            }
        }

        bool up    = (GetAsyncKeyState(0x26) & 0x8000) != 0;
        bool down  = (GetAsyncKeyState(0x28) & 0x8000) != 0;
        bool left  = (GetAsyncKeyState(0x25) & 0x8000) != 0;
        bool right = (GetAsyncKeyState(0x27) & 0x8000) != 0;

        if (!up && !down && !left && !right) {
            g_Options.inputTimer = 0;
        } else if (now - g_Options.inputTimer >= 120) {
            int delta = 0;
            if (up) {
                if (g_Options.selectedRow > 0) {
                    g_Options.selectedRow--;
                    g_SfxMenuMove.Play(0);
                }
            } else if (down) {
                if (g_Options.selectedRow < 5) {
                    g_Options.selectedRow++;
                    g_SfxMenuMove.Play(0);
                }
            } else if (left) {
                delta = -1;
            } else if (right) {
                delta = 1;
            }

            if (delta != 0) {
                switch (g_Options.selectedRow) {
                    case 1:
                        g_Difficulty = std::clamp(g_Difficulty + delta, 0, 4);
                        g_SfxClunk.Play(0);
                        break;
                    case 2:
                        g_SoundEnabled = (delta > 0);
                        g_SfxClunk.Play(0);
                        break;
                    case 3:
                        g_MusicEnabled = (delta > 0);
                        if (!g_MusicEnabled) StopMidiMusic();
                        else PlayMidiMusic("Rock2.mds", true);
                        g_SfxClunk.Play(0);
                        break;
                    case 4:
                        g_UseJoystick = (delta > 0);
                        g_SfxClunk.Play(0);
                        break;
                    case 5:
                        g_AlwaysRun = (delta > 0);
                        g_SfxClunk.Play(0);
                        break;
                }
                SyncOptionsSliders();
            }
            g_Options.inputTimer = now;
        }

        Options_Render();
        return;
    }

    if (g_Options.phase == 2) {
        SavePlayerData();
        g_SfxYeah.Play(0);
        FadeOutToBlack(1);
        g_Options.phase = 0;
        SetOrGetGameState(SetOrGetGameState(-2)); // Return to MainMenu
    }
}

// Replaces case 5: State_Credits (004063a0)
void State_Credits() {
    if (g_CreditsPhase == 0) {
        g_CreditsTimer = timeGetTime();
        WasKeyJustPressed(0x0D); // Clear any Enter held over from MainMenu
        WasKeyJustPressed(0x1B);
        g_SfxApplause.Play(0);
        EnsureFullscreenBmp("Credits.bmp");
        g_CreditsPhase = 1;
        return;
    }

    bool exitRequested = WasKeyJustPressed(0x0D) || WasKeyJustPressed(0x1B);
    if ((timeGetTime() - g_CreditsTimer < 0x7531) && !exitRequested) {
        ClearScreen(0);
        DrawStarfield(0, 0, true);
        BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);
        FlipScreen();
        return;
    }

    g_SfxApplause.Stop();
    g_SfxPickup.Play(0);
    FadeOutToBlack(1);
    g_CreditsPhase = 0;
    SetOrGetGameState(SetOrGetGameState(-2)); // Return to MainMenu
}

// Replaces case 6: State_Instructions (00406af0)
void State_Instructions() {
    if (WasKeyJustPressed(0x0D) || WasKeyJustPressed(0x1B)) {
        g_SfxPickup.Play(0);
        FadeOutToBlack(1);
        SetOrGetGameState(SetOrGetGameState(-2)); // Return to MainMenu
        return;
    }

    ClearScreen(0);
    DrawStarfield(0, 0, true);
    EnsureFullscreenBmp("Instructions.bmp");
    BlitTile(g_FullscreenBmp, 0, 0, 0, 0, 0x8000);
    FlipScreen();
}

void InitMenuAndSharedAssets() {
    g_TilesBmp = LoadSpriteSheet("Tiles.bmp", 16, 16, true);
    if (g_TilesBmp.surface) {
        SetActivePaletteFromSurface(g_TilesBmp.surface);
    }
    g_SelectorBmp   = LoadSpriteSheet("Selector.bmp", 0x28, 0x28, true);
    g_SelectionsBmp = LoadSpriteSheet("Selections.bmp", 0x159, 0x32, true);

    g_SfxApplause.Load("Applause.wav", 1);
    g_SfxExitDoor.Load("AtticDoor.wav", 1);
    g_SfxBounce.Load("Bounce.wav", 2);
    g_SfxBroken.Load("Broken.wav", 1);
    g_SfxChicken.Load("Chicken.wav", 1);
    g_SfxChicken2.Load("Chicken2.wav", 1);
    g_SfxMenuMove.Load("Clink.wav", 2);
    g_SfxClunk.Load("Clunk.wav", 2);
    g_SfxClink.Load("Clunk.wav", 1);
    g_SfxDown.Load("Down.wav", 1);
    g_SfxDribble.Load("Dribble.wav", 1);
    g_SfxDrink.Load("Drink.wav", 1);
    g_SfxError.Load("Error.wav", 1);
    g_SfxExcellent.Load("Excellent.wav", 1);
    g_SfxExplode.Load("Explode.wav", 1);
    g_SfxHarp.Load("Harp.wav", 1);
    g_SfxHorn.Load("Horn.wav", 1);
    g_SfxOpenDoor.Load("OpenDoor.wav", 1);
    g_SfxPickup.Load("Pickup.wav", 1);
    g_SfxRev.Load("Rev.wav", 1);
    g_SfxWarp.Load("Warp.wav", 1);
    g_SfxWhistle.Load("Whistle.wav", 1);
    g_SfxYeah.Load("Yeah.wav", 1);

    g_ShipBmp       = LoadSpriteSheet("Ship.bmp", 16, 16, true);
    g_ChickenBmp    = LoadSpriteSheet("Chicken.bmp", 16, 16, true);
    g_ChickensSlurpBmp = LoadSpriteSheet("Chickens.bmp", 16, 16, true);
    g_AliensBmp     = LoadSpriteSheet("Aliens.bmp", 16, 16, true);
    g_FuelBmp       = LoadSpriteSheet("Fuel.bmp", 20, 300, true);
    g_SeparatorBmp  = LoadSpriteSheet("Separator.bmp", 0, 0, true);
    g_NumberFontBmp = LoadSpriteSheet("NumberFont.bmp", 40, 40, true);
    g_TubesBmp      = LoadSpriteSheet("Tubes.bmp", 16, 16, true);
    g_PausedBmp     = LoadSpriteSheet("Paused.bmp", 0, 0, true);
    g_ExitBmp     = LoadSpriteSheet("Exit.bmp", 16, 16, true);
    g_ArrowsBmp   = LoadSpriteSheet("Arrows.bmp", 16, 16, true);
    g_CrossesBmp  = LoadSpriteSheet("Crosses.bmp", 16, 16, true);
    g_WarpTileBmp = LoadSpriteSheet("WarpTile.bmp", 16, 16, true);
    g_CupBmp      = LoadSpriteSheet("Cup.bmp", 100, 45, true);
}