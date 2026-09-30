# Crossfire - SKSE plugin

Copyright (C) 2026 izzydoingit. GPL-3.0-or-later, see `../LICENSE`.

Projectiles that meet in the air clash. Skyrim lets spells and arrows fly through each other; Morrowind did not, and
nothing on Nexus brings it back. With Crossfire:

- **An ice spike meets a fireball**: both burst in the air (opposite elements always cancel).
- **An arrow meets a firebolt**: close enough in strength, both go. You can shoot an incoming spell down.
- **A fireball meets a firebolt**: the fireball is over twice as strong, so it destroys the firebolt and flies on,
  weaker by what it beat, and hits a little softer.
- **Two sprays meet** (Flames against Frostbite): each particle clashes, so the two streams stop where they meet.
- **Unrelenting Force** swats every projectile in its cone out of the air. A lightning bolt zaps what it crosses.
  A wall of frost stops fire flying through it.
- Enemy mages' spells meet each other too, and your followers' spells pass through yours.
- Destroying an enemy's projectile with one of yours earns skill experience (Destruction for a spell, Archery for an
  arrow).
- **Spell struggles**: two enemy streams (sprays, held beams, breath) that meet lock together and push, and the
  caster with the higher magic skill drives the meeting point back and breaks through.

Every projectile is tested as it moves during the frame, not only where it is at the end of it, so two fast spells
cannot pass through each other between frames. Destroyed projectiles burst with their own explosion, where they met.

Needs SKSE and the Address Library. SE and AE; no VR build yet. SKSE Menu Framework is optional (without it there is
no menu; the files still apply).

## What clashes

Only hostile projectiles: arrows, bolts, and spells with a hostile or harmful effect. A spell that harms nobody
(Magelight, or a mod's invisible scripted projectile) is never touched, so nothing that relies on its projectile
arriving can break. Beams, wall spells and cones (shouts) are never destroyed, but they can destroy.

Each projectile has an **element** (Fire, Frost, Shock, Poison, Arcane, Physical, Force), from its effect's keywords,
then its resist value; arrows are Physical. It also has a **strength**: a spell's magicka cost (dual casting raises it),
an arrow's damage times `ArrowScale`, `ShoutStrength` for a shout. The reaction table says what each pair of elements
does to each other (`Clash`, `Annihilate`, `Wins`, `Loses`, `Pass`). A `Clash` is decided by strength and
`OverpowerRatio`.

## Spell struggles

Sprays (Flames, Frostbite, a staff's), held beams (Sparks, Lightning Storm) and breath (Fire Breath, a dragon's) of two
hostile casters that meet head on lock together instead of clashing particle by particle. Each side pushes; the
meeting point moves toward the weaker caster, and both streams end there (particles past it are snuffed, a locked
beam is cut short). When it reaches the weaker caster's hands they are **overwhelmed**: their spell breaks, they
stagger, take an extra hit (credited to the winner) and reel for a moment while the winner's stream pours over them.

- **Who pushes harder**: magic skill (the stream's own school by default), level, the spell's cost, magicka left,
  dual casting or a staff, and Enemy/Dragon power. With **Higher magic skill always wins** (on by default) the more
  skilled caster always wins when both cast from a school; the rest only changes how fast. A breath is a plain
  contest (it counts as a spell of `BreathStrength`, more for more words).
- **Endings**: overwhelmed; one side stops while losing (counts as overwhelmed) or while even (gave way); both stop;
  or the time limit (whoever is ahead wins, dead even is a draw: both are thrown off).
- **Fire against Frost** locks too when *Fire and frost lock too* is on (the default); off, they cancel particle by
  particle as before. Missiles, arrows, walls, cones and one-shot bolts never lock.
- A struggle belongs to two casters; each caster is in at most one. Your followers never lock with you.
- Settings: `[Struggle]`, `[Power]`, `[Aftermath]`, `[Show]` in `Crossfire.ini`, and the menu's **Spell struggles**
  page, which also has a bar that shows who is pushing while you are locked.
- Messages: "You overwhelm X.", "X overwhelms you.", "X gives way.", "The spells burst between you."

## Files

`Data\SKSE\Plugins\`:

| file | what | written by |
|---|---|---|
| `Crossfire_Rules.ini` | elements, reactions, exclusions | ships with the mod; hand-edited |
| `Crossfire\*.ini` | patches for other mods' spells, same sections | patch authors |
| `Crossfire.ini` | settings (see `Core::WriteSettings` for every key and its note) | the menu |

They are read in that order, each on top of the last. A line a file cannot use is skipped and written to
`Crossfire.log`, with its line number. The menu has a Reload button.

## For other mods

- **Papyrus**: the mod event `Crossfire_Clash` (`RegisterForModEvent("Crossfire_Clash", "OnClash")`): `strArg` is
  the two elements (`"Fire,Frost"`), `numArg` is 1 if the first was destroyed, 2 if the second, 3 if both; `sender`
  is who fired the first. Sent at most 8 a frame and once every 0.25 s for the same two shooters.
- **Papyrus**: `Crossfire_Struggle`: `strArg` is `"<began|overwhelmed|gave way|draw|ended>,<elementA>,<elementB>"`,
  `numArg` 1 if you won, -1 if you lost, 0 otherwise; `sender` the winner (else the first caster).
- **SKSE plugins**: every clash is dispatched on SKSE's messaging interface as `CrossfireAPI::kClash`, and every
  struggle's start and end as `CrossfireAPI::kStruggle`; see
  `src/CrossfireAPI.h` (copy it; it depends on nothing).

## Source

`src/main.cpp` has the file map. `src/Core.*` is everything that does not touch the game (the shapes, finding which
projectiles touched, the reaction table and the contest, the settings files); `tests/run.sh` builds and runs its
tests natively with the address and undefined-behaviour sanitizers under g++ and clang++, and `tests/run.sh --fuzz 60`
also fuzzes the settings parser.

## Building

Install git, [xmake](https://xmake.io) 3 and the Visual Studio C++ build tools, then run `build.bat`. It clones
CommonLib at the pinned commit and builds the DLL.
