# Prompt for your build agent

You are verifying and building **Crossfire**, an SKSE plugin for Skyrim SE/AE (CommonLibSSE NG, C++23, xmake). The
source is in this folder: `Crossfire/` (the plugin), `tools/wincheck/` (a Linux compile-check helper), `LICENSE`.
It was written and tested without access to Windows or the game. Your job is to build it for real, fix anything
that fails, and report back plainly. Do not add features.

## 1. Read first
- `Crossfire/README.md` (what it does), `Crossfire/TESTING.md` (what was already checked, and the in-game test list),
  and the file map at the top of `Crossfire/src/main.cpp`.

## 2. Run the core tests (Linux or WSL; skip if neither is available and say so)
    Crossfire/tests/run.sh --fuzz 60
Needs g++ and/or clang++ (clang's sanitizer runtime: `libclang-rt-18-dev` on Ubuntu). Expect about 120,000 checks
per build, `0 failed` every time, and the fuzzer finishing with no crash. Any failure is a real bug: fix the code
in `Crossfire/src/Core.*`, not the test, unless the test itself is provably wrong (explain why if so).

## 3. Build the DLL on Windows
Needs git, xmake 3, and Visual Studio 2022 with the C++ build tools. From `Crossfire\`:
    build.bat
It clones CommonLibSSE NG at the pinned commit `d13d10a0ccb4945870eb841bf1ad8a6cf5ed84dd` into `lib\` and builds
`Crossfire.dll` (release with debug info). Expected: no errors. Report any MSVC warnings from `Crossfire\src\` files
(ignore ones inside CommonLib or `SKSEMenuFramework.h`) and fix them if the fix is small and obviously safe.

If the build fails:
- Fix the error in the plugin's own files. Never edit CommonLib, never change the pinned commit.
- Keep `#define NOMINMAX` at the top of `src/PCH.h` (CommonLib pulls in windows.h; without it std::min/max break).
- After each fix, rerun step 2 if you touched `Core.*`.

## 4. Package
Make a mod-manager-installable zip:
    Crossfire.zip
      SKSE/Plugins/Crossfire.dll
      SKSE/Plugins/Crossfire.pdb          (optional, helps crash logs)
      SKSE/Plugins/Crossfire_Rules.ini    (from Crossfire/data/SKSE/Plugins/)
      SKSE/Plugins/Crossfire/README.txt   (from Crossfire/data/SKSE/Plugins/Crossfire/)
Do not ship `Crossfire.ini`; the in-game menu writes it.

## 5. Smoke test in game (if you can launch Skyrim with SKSE; otherwise hand this list to the user)
Requirements: SKSE, Address Library for SKSE Plugins; SKSE Menu Framework is optional (it gives the menu).
- Start the game, load a save. Open `Documents\My Games\Skyrim Special Edition\SKSE\Crossfire.log`: it must show
  "loading", a settings line, a projectile-records survey line, "the player's update hooked", and (with SKSE Menu
  Framework) "settings page added". No crash at the main menu, on load, or on save/load while spells are in flight.
- Turn on "Log every clash" in the menu, then go through the table in `Crossfire/TESTING.md` (ice spike into an enemy
  firebolt, arrow into a firebolt, fireball against firebolt, Flames against Frostbite, Unrelenting Force against
  arrows, a follower casting past you, etc.) and note which rows behave as described.

## 6. Report back
- Test results (step 2), the build result with any warnings/errors and exactly what you changed (a diff), the zip
  location, and the in-game results or "not run".
- Call out these three things specifically if you could test in game (they could not be verified without the game):
  1. whether a weakened spell really hits softer ("...and hits softer" scales the projectile's `power`),
  2. whether lightning bolts and wall spells catch only what actually crosses them,
  3. what the survey line says about how many explosions "only show", and whether bursts appear when projectiles
     are destroyed.
