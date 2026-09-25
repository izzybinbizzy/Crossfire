# Crossfire - testing

## What was checked without the game

- `tests/run.sh --fuzz 90`: the core (`src/Core.*`) under g++ and clang++, with the address and undefined-behaviour
  sanitizers and optimised: about 120,000 checks. They include every shape against every other shape, the fast search
  against a brute-force search over 300 random worlds, every reaction and the symmetry of the contest over 20,000
  random cases, every way a settings line can be wrong, a round trip of every setting, and the shipped
  `Crossfire_Rules.ini` read back to exactly the built-in defaults. libFuzzer ran the parser half a million times.
- `tests/fuzz_touch.cpp` (run by `tests/run.sh`): `FirstTouch` against a reference that shares none of its code (its own
  distance functions, the frame sampled 4,000 times), over shapes from 0.1 to a million units, zero radii, still and
  coincident bodies; and `Resolve` against its rules (an immune side is never destroyed or weakened, nothing grows,
  nothing goes negative). 300,000 cases per compiler. Planting a bug in the search (the last touch instead of the
  first, or a coarse search) makes it fail.
- `../tools/wincheck/check.sh Crossfire --link`: every source compiled for Windows (the MSVC ABI) against CommonLib
  at the pinned commit, SE and AE, with CommonLib's own layout checks, then linked against all of CommonLib: no
  error, no warning in the plugin's files, no game or plugin function used but defined nowhere. The same check
  compiles RELight - Spell Addon cleanly, so it reports only real problems. Two came up and are fixed: CommonLib's
  headers reach `windows.h` without `NOMINMAX` (so `std::min` breaks; PCH.h now defines it), and CommonLib's
  `BSSimpleList` cannot be walked through a const reference.

None of this runs the game. Build it with `build.bat`, then check it in game.

## In game

Turn on **Log every clash** in the menu (or `[Debug] Log=1` in `Crossfire.ini`); each clash is written to
`Documents\My Games\Skyrim Special Edition\SKSE\Crossfire.log`. At load the log also says how many projectile
records the game has of each kind, and how many of their explosions only show (the ones Crossfire will place by
default).

Spells for testing: `help "ice spike" 4` in the console gives a spell's form ID; `player.addspell <id>` teaches it.
A hostile mage to shoot at: any bandit or necromancer mage in a dungeon; or `player.placeatme <id>` a hostile NPC.

| # | do | expect |
|---|---|---|
| 1 | a mage fires a firebolt at you; fire an ice spike into it | both burst in the air, where they met (Fire vs Frost: Annihilate) |
| 2 | a firebolt at you; shoot it with an arrow | both go if close in strength, else the stronger flies on; the log says which |
| 3 | fireball against an enemy firebolt | the fireball flies on ("left N" in the log) and hits softer |
| 4 | Flames against an enemy's Frostbite | the streams stop each other where they meet |
| 5 | Unrelenting Force at incoming arrows and spells | they are destroyed in the cone; the shout goes on |
| 6 | your follower casts past you | nothing: allies pass through each other |
| 7 | two enemy mages, you between: step aside | their spells can meet each other (`Who` = everyone) |
| 8 | arrows stuck in a wall; cast through them | nothing: a projectile that stopped is done |
| 9 | Magelight, Candlelight, any healing or utility spell | never touched |
| 10 | destroy enemy spells with yours | skill experience (Destruction, Archery), at most one award every half second |
| 11 | save with spells in flight, load | no crash; nothing carried over |
| 12 | the menu: change sliders, untick Enabled, Reload | each applies at once; `Crossfire.ini` is written when a control is let go |
| 13 | Lightning Bolt across an incoming spell | the bolt is never destroyed; a weaker spell is |
| 14 | a wall of frost, then fire through it | the fire is stopped at the wall |

Worth watching, because the game could not be run here:

- **Beams and walls.** A lightning bolt is tested as a line from where it starts, along the way it faces, to where it
  hit (or its range). A wall is an upright rectangle `BarrierHeight` tall, its base centre at the wall's position,
  its width the wall's width. If a bolt or wall seems to catch things beside it, turn on the log and compare.
- **"Hits softer".** A weakened spell's power is scaled down. If a weakened spell still hits as hard, that field is
  not what the game reads for its damage; untick "...and hits softer" and tell me.
- **Bursts.** The explosions placed are the projectiles' own, with nobody behind them. With "Only harmless bursts"
  (default) an explosion with any damage, enchantment or spawn of its own is skipped: then the projectile just
  vanishes.
