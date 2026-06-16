# Outpost 13-C
## Game Development Roadmap
### Tales from the Spiral — Beta Universe
*Version 1.0 | Working document, updated iteratively.*

---

## Overview

A top-down 2D roguelike set in a partially staffed research station on the
edge of the Scar. The player is the station administrator, working the night
shift alone while a skeleton crew handles the day.

The game blends SS13-style station simulation with traditional roguelike
exploration and turn-based combat. Tone is grounded and slow-burn -- mundane
tasks that escalate into things that do not have good explanations.

**Combat reference:** Fear & Hunger, Look Outside
**Simulation reference:** Space Station 13
**Dice system:** Tales from the Spiral TTRPG (already playtested)

---

## Setting

### Outpost 13-C
- Long-range monitoring station, edge of the Scar, Beta Universe
- Rogue planetoid location
- Designed capacity: 50. Current occupancy: ~20%
- Outposts 13-A and 13-B were destroyed. Circumstances TBD.

### The Three Eras
The station was never fully stripped between eras. All three layers coexist,
decaying together. Each era is a distinct visual zone with its own tileset.

| Era | Location | Character |
|-----|----------|-----------|
| Research | Lower levels | Labs, sensor arrays, server rooms |
| Tourism | Mid levels | Visitor centre, gift shop next to the sensor array, tourist plaques on research equipment |
| Current / Abandoned | Upper levels | Residential, docking, maintenance corridors |

### The Scar
Broken spacetime left by the destruction of an Atlantian civilization.
Effects near the station include atmospheric pockets drifting in vacuum,
crystal structures that vanish when approached or scanned, sensor ghosts
on long-range equipment, and Atlantian ruins some of which are sealed
from the outside.

The Scar is not a combat threat. It is an environmental one.
Scar intensity is a value (0--4) set per tile in the editor.
Outer hull and antenna arrays are highest. Residential core is lowest.

| Intensity | Effects |
|-----------|---------|
| 0 | Normal |
| 1 | Minor sanity drain, occasional equipment flicker |
| 2 | Active sanity drain, equipment malfunctions |
| 3 | Visual distortion, sensor ghosts appear on minimap |
| 4 | Room layouts shift. Entities appear. |

---

## Core Gameplay

### The Two Shifts

**Day Shift — Off the Clock**
The visitor centre is open. Generic alien visitors move through public areas.
The player is free to explore, use terminals, and manage the station.
Ordered supplies arrive at the start of this shift. Few events occur.
Crew works autonomously at their assigned stations.

**Night Shift — On the Clock**
The visitor centre closes. The player is responsible for the station.
Primary tasks: monitor the antenna array, clean, maintain, file reports,
and respond to whatever happens. Most events occur during this shift.

### The Day Tracker
A persistent day counter, visible in the HUD and saved to file.
Scripted story beats trigger on specific days.
RNG events roll against probability tables each shift.
Some hires, supplies, and story content unlock after enough days have passed.

### The Antenna Array
The antenna control terminal is in its own dedicated room.
A light on the terminal blinks when a signal is waiting -- visible
from the doorway so the player does not need to check constantly.
Signals range from routine traffic to distress calls to things that
should not exist.

*Optional late feature:* an NPC engineer may eventually rewire the
antenna feed to the main control room. This is an emergent player-driven
improvement, not a scripted event.

---

## Simulation Systems

### Map Structure — One Floor, One Map
Each floor of the station is a single persistent map.
The entire floor loads at once and simulates continuously.

This means NPCs act in rooms the player cannot see, enemies pursue
across doorways, sounds propagate from actual locations, and events
happen in the background. The station behaves like a place, not a
sequence of rooms.

Floors connect via staircases, lifts, and airlocks.
Transitioning between floors is a load point. Within a floor, everything runs.

### NPC and Crew System
The station starts understaffed. NPCs are not predefined -- they arrive
over time or are hired via terminal. Population is dynamic.

**Hire Terminal** (Day shift)
Browse available contractors by role and species. Hire for Value.
Hired NPCs arrive on the next day shift.

**Supply Terminal** (Day shift)
Order food, medical supplies, equipment, amenities, and spare parts.
Delivered at the start of the next day shift.
Amenities specifically improve crew morale.

**NPC Roles:** Engineer, Technician, Security, Mercenary, Medical,
General Worker, Researcher.

### Morale
A single station-wide stat visible as a bar in the crew management screen.

| Improves With | Degrades With |
|---------------|---------------|
| Good food and amenities | Deaths and injuries |
| Clean station | Scar anomaly events |
| Positive events | Neglected maintenance |
| Well-supplied crew | Poor living conditions |

Low morale causes slower work, more errors, and higher NPC quit chance.
High morale improves work output and event outcomes.

### NPC Behaviour and Pathfinding
NPCs pathfind to rooms matching their role using the room purpose tag system.
Each room is tagged with a purpose, a valid role list, an active shift,
and a capacity. NPCs find their highest-priority available matching room
and work there.

**NPC Memory System** *(Dwarf Fortress inspired -- full implementation deferred)*
NPCs remember things they have witnessed:
- Last known location of items and characters
- Events they were present for
- Their assigned workstation and schedule

Memory is used for pathfinding decisions, autonomous behaviour, and dialogue.
It degrades over time or is updated by new observations.

**NPC Schedules** *(Placeholder -- design deferred)*
NPCs will have time-block schedules assigning them to rooms or behaviours
during specific shift periods. Structure TBD.

### Day Shift Simulation
While the player sleeps between shifts, NPCs act on their schedules.
When the night shift begins the player finds the evidence:
completed repairs, restocked supplies, mess from the day crew,
workstation logs written in NPC voice, and occasionally something
that went wrong.

### Value and Economy
Value is the in-universe currency. Station budget and personal funds
are the same pool. Spent on: hiring, supplies, repairs, bribes, equipment.

Crossover with character advancement: **1 XP = 2 Value**, convertible
both ways. A funded station can accelerate character growth.
A skilled character can liquidate XP for operating cash.

### In-Game Internet
A Union public information network accessible at terminals.
Functions as an in-setting encyclopedia.

Content is JSON entries organised by category:
species, factions, lore, and station equipment manuals.
Some entries are redacted. Some entries are wrong in ways that matter later.

---

## Combat

### Overview
Combat is a fully separate screen. The overworld pauses when it begins.
Enemies on the overworld only move when the player moves.
There is no battle map or tile grid -- positions are abstract.

Visual style: higher resolution illustrations for enemies and player,
location background art per zone. Inspired by Fear & Hunger and Look Outside.

### Turn Structure
Player side and enemy side alternate. No initiative rolls.
Each character has **2 Actions and 1 Reaction** per turn.

| Action Options | Reaction Options |
|----------------|-----------------|
| Move (abstract) | Dodge melee attack |
| Attack | Trigger a Ready action |
| Use item or skill | Help an ally (+1 die) |
| Ready (hold for trigger) | |

### Attack Resolution
Uses the TTRPG dice system directly.

1. Roll a d6 pool equal to Ability + Skill
2. Each die showing 4 or higher is a success
3. A 6 is a success that also rerolls (chains indefinitely)
4. Successes must meet or beat the target's Armor Rating to hit
5. **Damage = Weapon Bonus + (successes - AR)**

### Armor and Shields
These are two separate defensive layers.

- **Armor Rating (AR):** Prevents hits outright. Must be overcome to deal damage.
- **Shields:** Reduce damage after the AR check. Class value subtracted from damage.
- Plasma weapons degrade shield class by 1 per hit (stacking until collapse)
- Kinetic weapons gain +2 successes against shielded targets

### Character Stats
HP = (Endurance + Fortitude) × 3, minimum 6.

| Stat | Function |
|------|----------|
| Strength | Melee damage bonus |
| Agility | Hit chance, dodge, turn priority |
| Defense (AR) | Damage prevention via armour |
| Perception | Ranged accuracy, spotting hidden things |
| Endurance | Contributes to HP |
| Fortitude | Contributes to HP |
| Discipline | Resists fear and Scar effects |
| Sanity | Degrades near the Scar. Low sanity causes penalties and hallucinations. |

### Dying Track — No Permadeath
Death requires being hit three times past zero HP.

0 HP = **Dying** (prone, crawl only, unappealing target) →
Hit while Dying = **Unconscious** →
Hit while Unconscious = **Dead**

A party member with a medkit can revive a downed ally.
Solo: stabilise automatically after combat at 1 HP, lose consumables.

Surprise attacks deal double damage.

### Fear
Four escalating stages, each applying stacking dice penalties.
Escalates on a zero-success Discipline check.

**Shaken → Scared → Fearful → Terrified**

Triggers include: Scar entities, anomalies, witnessing deaths,
and things that should not be where they are.

### Edge Points
Narrative currency earned through good play and roleplay.

| Spend | Effect |
|-------|--------|
| 1 EP | Reroll failed dice |
| 1 EP | Act before enemies this turn |
| 1 EP | Declare that cover exists |
| 1 EP | Ignore a fear condition for 1 action |
| 1 EP | Declare a clean narrative outcome |

---

## Art and Audio

### Tile and Sprite Art (Your Work)
- **Tile size: 32x32 pixels**
- **Palette:** SS13-adjacent. Desaturated greys and metallics for station
  surfaces. Accent colours for hazards, equipment states, and alien species.
- Entity sprites: 4-directional facing, idle and walk animations (4 frames
  per direction), action state, downed state

### Combat Art (Your Work)
- Higher resolution enemy and player illustrations (not sprites)
- Background art per zone: research, tourism, abandoned, scar-adjacent
- Fear & Hunger and Look Outside are the visual reference

### Portrait Art (Your Work)
- Character portrait per NPC and crew member
- Used in dialogue boxes and the crew management screen

### UI Art (Your Work)
- Inventory screen (Fear & Hunger inspired)
- Crew management screen with morale bar
- Supply and hire terminal screens
- HUD: HP, sanity, Value, day counter, shift indicator

### Audio
- Ambient sound zones: station hum, machinery, Scar interference
- Triggered sound effects: interactions, combat hits, anomaly events
- Formats: .wav or .ogg, referenced by filename from Assets/audio/

---

## The Map Editor

A full level editor built into the game. No external software required.
Accessed via a launch flag or in-game settings toggle.
The station's floors are authored entirely within this tool.

### Play From Editor
Save and launch a temporary play session from the current map state.
The player spawns at the cursor position with dev flags active.
Exiting returns to the editor. Fastest possible iteration loop.

### Tile Painting
- Tileset toolbar showing 32x32 tile previews
- Left-click to place, right-click to erase, drag to fill
- Eyedropper tool to sample placed tiles
- **Selection tool:** drag to select a region
- **Copy and paste:** duplicate selected regions anywhere on the map
- **Room templates:** save a selected region as a named template.
  Place from a template library. Designed for repeated room types
  such as corridors, crew quarters, and storage bays.

### Tile Properties
Every tile has a configurable property panel:

| Property | Description |
|----------|-------------|
| Walkable | Whether entities can move here |
| Pathable | Whether AI navigation uses this tile |
| Scar intensity | 0--4, sets anomaly frequency and severity |
| Light level | Affects visibility |
| Room tag | Assigns tile to a named room purpose |

### Pathability Layer
Toggle an overlay showing pathable tiles (green) and impassable tiles (red).
Click any tile to toggle its pathability without changing its appearance.
Doors automatically update their pathability at runtime when opened or closed.

### Room Tagging
Select a region of tiles and assign a room purpose tag.
Each tag has a colour for the overlay. Tags are defined in a JSON file --
new tags can be added without any code changes.
NPCs read room tags to find their assigned work locations.

### Entity Placement
Place and configure any entity type:

| Entity | Configurable Properties |
|--------|------------------------|
| Enemy | Type, patrol path, aggro radius |
| NPC | Role, workstation assignment |
| Item | Specific item or loot table reference |
| Workstation | Type, valid roles, active shift |
| Light source | Radius, falloff, colour. Auto-calculates surrounding tile light levels. |
| Ambient sound zone | Audio file, volume, loop behaviour, region boundary |
| Triggered sound | Audio file, trigger condition, falloff radius |
| Event trigger | See Event System |

### Scripting and Node System
Objects and entities can have scripts attached via a property panel.
Scripts are small JSON behaviour definitions -- not code.
Authors select from a list of named script types and configure parameters.

Examples:
- On interact: play sound, set flag, display text
- On enter radius: trigger event
- On night shift only: activate or deactivate this entity
- Repeat every N days: reset this item or state
- If flag X is set: change this tile's appearance

The engine handles execution. Authors never write code.

### Event System
Trigger zones can fire events configured entirely in the editor.

- **Simple events:** set a flag, play a sound, spawn an entity, display text
- **Sequenced events:** a linear list of steps firing in order,
  with optional conditions and delays between steps
- Stored as JSON in Data/events/

*Full event sequences are optional complexity. Simple triggers cover
most cases. Build simple first and expand as needed.*

### Light Sources
Place a light source entity with a radius, falloff value, and colour.
The engine calculates light levels on surrounding tiles automatically.
Much faster than painting light levels tile by tile.

### Connection Mapper
Toggle an overlay showing:
- Which doors connect which rooms
- Any rooms with no exits, highlighted as warnings
- Staircase and lift connections to other floors

Catches dead-end rooms and disconnected areas before play testing.

### Map Metadata Panel
Per-floor settings applied to the whole map:

| Setting | Description |
|---------|-------------|
| Map name and floor number | Identification |
| Station era | Sets default tileset |
| Ambient audio track | Background sound for this floor |
| Default Scar intensity | Overridden by per-tile values |
| Day/night availability | Which shifts load this floor |
| Connected floor IDs | Which floors this one leads to |

### Decal Layer
A separate visual layer for purely cosmetic detail:
bloodstains, scorch marks, tourist graffiti, pinned notes.
No gameplay properties. Can be toggled off while editing
so it does not clutter the property panel.

### Developer Overlays
All toggleable independently:

- Pathfinding grid
- Room tags
- Entity spawn points
- Light level values
- Scar intensity values
- Event trigger zones
- Sound zones
- Day/night entity visibility
- Connection mapper
- Decal layer

---

## Random Events

| Event | Shift | Description |
|-------|-------|-------------|
| Ship crash | Night or scripted | New explorable area, possible survivor NPC |
| Cosmic horror flyby | Night | Sanity drain, equipment effects, no combat. Various entities. |
| Entity visit | Night | Encounter appears in a previously cleared room |
| Strange distress signal | Night | New mission available, may be a trap |
| Equipment anomaly | Night | A station system behaves unexpectedly |
| Day shift mess | Shift transition | Environmental storytelling, resource changes |
| Crystal formation | Night, near hull | Appears in the Scar. Vanishes if approached. |
| Supply delivery | Day start | Ordered items arrive from previous shift |

The station is very unlucky. Events are seeded per shift from probability
tables. Scripted events fire on specific day numbers.

---

## Development Phases

### Phase 1 — Foundation
The core engine. Nothing is playable yet, but the infrastructure exists.

- Window creation and main game loop
- 32x32 tile-based map rendering from JSON
- Player movement: 8-directional, tile-based
- Collision detection: walls, doors, impassable tiles
- Camera following the player
- Fog of war: unseen / explored / currently visible

### Phase 2 — Map Editor
Built before any game content. Every level depends on it.

- Dev mode toggle
- Tile painting, copy/paste, room templates
- Tile properties panel
- Pathability layer with toggle overlay
- Room tagging system
- Entity placement: enemies, NPCs, items, workstations
- Light source entities
- Sound zone placement
- Scripting and node panel
- Event trigger system (simple first, sequences later)
- Connection mapper
- Map metadata panel
- Decal layer
- All dev overlays
- **Play from editor**

### Phase 3 — Interaction
The world becomes interactive.

- Entity system (shared base for player, enemies, NPCs, items)
- Item pickup and basic inventory
- Door and airlock interaction (pathability updates on open/close)
- Object inspection (reads description from JSON)
- Basic HUD: HP, sanity, Value, day counter, shift indicator

### Phase 4 — Station Systems
The simulation comes alive.

- Day/night cycle with shift transition logic
- Day tracker (persistent, visible in HUD)
- Supply terminal UI and delivery system
- Hire terminal UI and NPC arrival system
- Morale system with crew screen bar
- NPC autonomous pathfinding to room purpose tags
- Day shift evidence generation
- Value and economy (supply costs, hire costs, XP conversion)

### Phase 5 — Combat Screen
A separate screen state with its own renderer.

- Screen state switch: overworld pauses, combat loads
- Turn order system: player side / enemy side alternation
- 2 actions + 1 reaction per character per turn
- TTRPG dice resolution: pool, 4+ success, exploding 6s
- Damage formula: Weapon Bonus + (successes - AR)
- Armor Rating and shield system (separate layers)
- Enemy AI: patrol, chase, attack states
- Status effects: bleeding, stunned, poisoned, compromised
- Dying track: Dying / Unconscious / Dead
- Fear system: four stages, Discipline checks
- Surprise: double damage
- Edge Points system

### Phase 6 — Depth
Systems that make the world feel real.

- Party system: crew follow, fight alongside, can be assigned
- Save and load: per-floor autosave and manual save
- RNG event system: per-shift rolls against event tables
- Dialogue system: JSON-driven NPC conversation
- Sanity mechanic: Scar proximity degradation, low-sanity effects
- In-game internet: terminal browser with species/faction/lore entries
- Antenna interface: signal detection, response choices
- NPC memory system: witnessed events, last known locations (full pass)
- NPC schedules: time-block room assignments

### Phase 7 — Content
The station becomes a real place.

- All station floors authored in the editor
- Full enemy roster with art and stats
- Full item pool
- Full NPC roster with portraits and dialogue
- All scripted story beats and day-triggered events
- Internet database entries (species, factions, lore, station history)
- Complete RNG event table
- Open-ended progression loop

---

## Roles

| You | The Engine |
|-----|-----------|
| Tile and sprite art (32x32) | Rendering and animation |
| Combat illustrations | Combat screen state |
| NPC and enemy portraits | Dialogue and profile display |
| UI art | All interface logic |
| Level design (in the editor) | Pathfinding and collision |
| Item definitions (JSON) | Inventory and equip logic |
| Enemy definitions (JSON) | AI behaviour |
| Dialogue (JSON) | Conversation system |
| Event scripts (editor) | Event execution |
| Audio files (.wav/.ogg) | Sound playback and zones |
| Story and lore | Save/load, RNG, simulation |

---

## Key Principles

**One map per floor, always.**
The entire floor simulates continuously. NPCs act whether or not the player
is watching. This is what makes the station feel alive.

**Build the editor before the game.**
Every system from Phase 3 onward depends on authored levels.
A working editor makes everything else faster.

**JSON content files scale cleanly.**
Proven by the text adventure engine. Engine and content stay separate.
New content never requires code changes.

**One system fully working before the next begins.**
No partially built systems. Finish Phase 1 before starting Phase 2.

**Reuse the TTRPG dice system exactly.**
It is already designed, balanced, and playtested.
Reusing it keeps mechanical lore consistent across the tabletop
and video game versions of the setting.
