# Crossfire - SKSE plugin

Copyright (C) 2026 izzydoingit. GPL-3.0-or-later, see `LICENSE`.

Projectiles that meet in the air clash. Skyrim lets spells and arrows fly through each other; with Crossfire an ice
spike and a fireball burst where they meet, an arrow can shoot down an incoming firebolt, the stronger of two spells
flies on weaker, and Unrelenting Force swats projectiles out of the air. Only hostile projectiles clash.

Skyrim SE and AE (no VR build yet). Needs SKSE and Address Library for SKSE Plugins; SKSE Menu Framework is optional
(without it there is no menu, and the rule and settings files still apply).

**What it does, the rule files, and the API for other mods: [`Crossfire/README.md`](Crossfire/README.md).**

## What is here

| path | what |
|---|---|
| `Crossfire/src/` | the plugin. `main.cpp` has the file map; `Core.*` is everything that does not touch the game |
| `Crossfire/data/` | what ships with the DLL: `Crossfire_Rules.ini` and a readme |
| `Crossfire/tests/` | the core's tests, a collision reference and a settings-parser fuzzer |
| `Crossfire/TESTING.md` | what was checked without the game, and what to check in game |
| `tools/wincheck/` | compile-checks the plugin for Windows from Linux (clang 18+, Microsoft's STL, the Windows SDK headers) |
| `.github/workflows/build.yml` | CI |

## Building

On Windows, install git, [xmake](https://xmake.io) 3 and the Visual Studio C++ build tools, then run
`Crossfire\build.bat`. It clones CommonLib at the pinned commit and builds `Crossfire.dll`.

## Testing

- `bash Crossfire/tests/run.sh` (Linux or WSL): the core under g++ and clang++ with the address and undefined-behaviour
  sanitizers, an optimised build, and the collision search against an independent reference. `--fuzz 60` also fuzzes
  the settings parser for 60 seconds (needs clang's libFuzzer).
- `bash tools/wincheck/setup.sh` once, then `bash tools/wincheck/check.sh Crossfire --link` (from this folder):
  every source compiled for Windows against CommonLib, and every game or plugin function it uses checked as defined.
- In game: `Crossfire/TESTING.md`.

Every push runs CI: the tests with a 300-second fuzz on Linux, and the real DLL built with MSVC on Windows, checked
from outside and packaged as a mod-manager zip (the run's artifacts).

## Third-party

`Crossfire/src/SKSEMenuFramework.h` is SKSE Menu Framework's header by Thiago Kaique (Thiago099), MIT, from
[SKSE-Menu-Framework-3-Example](https://github.com/Thiago099/SKSE-Menu-Framework-3-Example), unchanged.
