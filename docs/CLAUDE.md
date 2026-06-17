# Outpost 13-C — Claude Code Context File
*Read this at the start of every session.*

---

## Project Summary

A top-down 2D roguelike set in Outpost 13-C, a partially staffed research
station on the edge of the Scar, in the Tales from the Spiral Beta Universe.
Blend of SS13-style station simulation and Fear & Hunger-style combat.

**Language:** C++
**Library:** SDL2 only. No game engine. Everything written by hand.
**Style:** Grounded, slow-burn, mundane tasks escalating into horror.

---

## Build Instructions

**Platform:** Windows via WSL2, or Linux native.

### Linux / WSL2 build

**Dependencies:**
```bash
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
```

**Compile:**
```bash
bash build.sh linux        # outputs build/outpost13c
```

**Run:**
```bash
./build/outpost13c
./build/outpost13c --dev   # launches map editor
```

### Windows .exe build (MinGW cross-compile from WSL2)

**Dependencies:**
```bash
sudo apt install mingw-w64
# SDL2 MinGW dev package (one-time setup):
# 1. Download SDL2-devel-X.X.X-mingw.tar.gz from https://libsdl.org
# 2. Extract and rename so that
#    libs/SDL2-mingw/x86_64-w64-mingw32/include/SDL2/SDL.h exists.
#    SDL2.dll lives in libs/SDL2-mingw/x86_64-w64-mingw32/bin/
```

**Compile:**
```bash
bash build.sh windows      # outputs build/outpost13c.exe + SDL2.dll
bash build.sh all          # builds both targets
```

**Run from Windows (recompiles first):**
```
run.bat
```
`run.bat` invokes the MinGW build via WSL, then launches the .exe natively.
No display server required — SDL2 uses the Windows native video driver.

**Note:** `-mconsole` is set so debug output (stdout/stderr) appears in the
terminal during development. Change to `-mwindows` in build.sh before shipping.

---

## Folder Structure

```
Outpost13C/
  src/                  -- all C++ source files (.cpp and .h)
  Data/
    items/              -- item JSON definitions
    enemies/            -- enemy JSON definitions
    levels/             -- floor map JSON files (one per floor)
    npcs/               -- NPC contractor pool definitions
    dialogue/           -- NPC dialogue trees
    events/             -- scripted and RNG event definitions
    rooms/
      room_purposes.json
    terminal/
      supply_catalogue.json
      hire_catalogue.json
    internet/
      species_entries/
      faction_entries/
      lore_entries/
    scripts/            -- reusable behaviour script definitions
  Assets/
    tilesets/           -- 32x32 tile sprite sheets
    sprites/
      player/
      enemies/
      npcs/
    combat/             -- higher resolution combat illustrations
    portraits/          -- NPC portrait art for dialogue and crew screen
    ui/                 -- interface art
    audio/              -- .wav and .ogg files
  docs/
    design_doc.md       -- full game design document, read this
    CLAUDE.md           -- this file
  build/                -- compiled output, not committed to version control
```

---

## Core Architecture Principles

**One map per floor.**
Each floor is a single persistent JSON map, loaded entirely, simulating
continuously. NPCs act in rooms the player cannot see. Everything ticks.

**JSON content files for all game data.**
Items, enemies, levels, NPCs, dialogue, events -- all data files.
Adding content never requires touching engine code.

**Strict separation of engine and content.**
The engine reads data. It does not hardcode game-specific values.

**Build one system fully before starting the next.**
Do not stub or partially implement. Complete Phase N before Phase N+1.

---

## Development Phases

1. **Foundation** -- window, tile renderer, movement, collision, camera, fog of war
2. **Map Editor** -- tile painting, copy/paste, entity placement, pathability,
   room tagging, light sources, sound zones, scripting panel, play-from-editor
3. **Interaction** -- entity system, inventory, doors, object inspection, HUD
4. **Station Systems** -- day/night cycle, terminals, morale, NPC pathfinding,
   autonomous behaviour, day shift evidence
5. **Combat Screen** -- separate screen state, turn system, dice resolution,
   armor/shields, dying track, fear, Edge Points
6. **Depth** -- party system, save/load, RNG events, dialogue, sanity,
   in-game internet, NPC memory system
7. **Content** -- all floors, rosters, items, story events, internet database

---

## Combat System (TFTS Dice -- Do Not Invent Your Own)

Reuse the Tales from the Spiral TTRPG system exactly.

- Pool = Ability + Skill as d6
- 4+ = success per die
- 6 = success AND reroll (chains indefinitely)
- Hit if successes >= target Armor Rating
- Damage = Weapon Bonus + (successes - AR)
- Shields reduce damage AFTER the AR check (separate layer)
- Plasma: degrades shield class by 1 per hit
- Kinetic: +2 successes vs shielded targets
- HP = (Endurance + Fortitude) x 3, minimum 6
- 2 Actions + 1 Reaction per turn
- No initiative rolls -- player side then enemy side alternate
- Dying track: 0 HP = Dying, hit again = Unconscious, hit again = Dead
- Surprise attack = double damage
- Fear: Shaken / Scared / Fearful / Terrified (escalates on 0-success Discipline check)

---

## Tile and Map Spec

- Tile size: 32x32 pixels
- Maps are JSON files in Data/levels/
- One JSON file = one complete floor
- Tiles have: walkable, pathable, scar_intensity (0-4), light_level, room_tag
- Scar intensity 0 = normal. 4 = room layouts shift, entities appear.

---

## Key Game Systems

**Day/Night Cycle**
Two shifts. Day: visitor centre open, player off clock, terminals available,
supplies arrive. Night: player on clock, events fire, crew skeleton only.

**Morale**
Single station-wide stat. Visible as a bar in crew screen.
Improved by amenities, food, clean station. Degraded by deaths, neglect, Scar.

**Value (Currency)**
In-universe currency. Station budget = player funds (same pool).
1 XP = 2 Value, convertible both ways.

**NPC Memory**
NPCs remember last known locations of items and characters, witnessed events.
Memory degrades over time. Used for pathfinding and autonomous behaviour.
Full implementation in Phase 6.

**Antenna Array**
Dedicated room. Terminal light blinks when signal present (visible from doorway).
Player checks periodically. Signals range from routine to impossible.

**In-Game Internet**
Union public information network. Terminal access.
JSON entries: species, factions, lore, station manuals.
Some entries redacted. Some wrong in ways that matter later.

---

## Coding Conventions

- C++17 standard
- Snake_case for variables and functions
- PascalCase for classes and structs
- All game data loaded from JSON at runtime, never hardcoded
- Use nlohmann/json for JSON parsing (header-only, include in src/)
- SDL2 for all rendering, input, audio -- no other graphics libraries
- Comments on any non-obvious logic
- Each major system in its own .cpp and .h file pair

---

## Planned Systems (Not Yet Implemented)

**Multiplayer — Co-op Architecture**
Target: 2-4 players, host-client model, same map loaded by all clients.
The authoritative game state lives on the host. Clients send actions, receive state diffs.

Core prep decisions already made:
- `PlayerAction` enum will replace inline keyboard handling. In SP: keyboard → PlayerAction.
  In MP: (keyboard OR network packet) → PlayerAction. Game logic only sees PlayerAction.
- Map entity UIDs are already `uint32_t` — safe to reference across clients.
- SaveSystem already serialises full game state — host snapshots use this.
- `std::vector<Player>` replaces single `Player` when MP is wired in.
  `local_player_idx` identifies which player this client controls.

Networking library: SDL2_net (already available) or enet (preferred for reliability).
Do not implement until single-player is feature-complete.
Tag multiplayer prep points in code with `// MP_PREP:` comments.

---

**Hotbar — Items (1–0 keys)**
Ten-slot horizontal hotbar at the bottom of the screen.
Number keys 1–0 assign the held item. Holding an item makes it the active
tool/weapon for interaction and combat. Slots are persisted in save files.

**Hotbar — Abilities / Augmentations (Scroll Wheel, 1–5 slots)**
A second vertical hotbar on the right side, five slots, cycled with the scroll
wheel. Holds active abilities, spells, or augmentations (depending on final
setting). Separate from the item hotbar. Not usable during movement — must
be activated explicitly.

---

## What NOT to Do

- Do not use a game engine (no Godot, no Unity, no SFML game loop wrappers)
- Do not hardcode tile types, enemy stats, or item properties
- Do not use global variables outside of clearly marked engine singletons
- Do not implement multiple phases at once
- Do not remove working code when fixing a bug -- fix the bug, leave the rest
