# Phase 2.5 — Core Systems Design
## Outpost 13-C
*Covers: Turn System, Text/Voice, Inventory, Items, Containers*
*These systems underpin Phase 3 and all phases after it.*

---

## The Turn System

The game is tile-based and turn-driven. Time advances by player action,
not by the clock. This is classic roguelike architecture.

**The tick loop:**
1. Wait for player input
2. Player acts (move, interact, use item, open inventory)
3. World ticks once: all entities update, events check, effects proc
4. State updates, screen redraws
5. Return to step 1

Nothing happens between player actions. Enemies do not move while the
player is reading text or browsing inventory. There is no time pressure
outside of combat.

**Implications:**
- Inventory safely pauses the world -- no ticks fire while it is open
- The text queue displays without anything happening in the background
- Events trigger on the tick after the action that caused them
- The SDL game loop still runs every frame for smooth rendering, but
  game logic only advances on player input

**Current state:** The game loop runs update() every frame. This needs
restructuring to separate render loop (every frame) from game tick
(on player input only). The game loop should call tick() only when
a player action is consumed, never on a timer.

---

## Text and Description System

All in-game text -- item descriptions, object inspection, pickup
notifications, NPC dialogue, narrator lines -- flows through a single
TextQueue manager and renders in a bottom bar panel.

### The Bottom Bar
Always present during exploration. Height: approximately 80px.
When empty: invisible or shows a subtle inactive state.
When active: dark background, text types in character by character.
In combat: the bar renders as a translucent overlay at the bottom of
the combat screen, sitting below the action menu.

### TextQueue
A FIFO queue of TextEntry objects. Each entry:
```json
{
  "text": "The note reads: be warned, the south passage is not safe.",
  "narrator": "Someone was scared when they wrote this.",
  "voice_profile_id": "station_ambient",
  "dismiss": "input"
}
```

dismiss values:
- "input" -- waits for player to press E or Space to advance
- "auto" -- displays for a fixed duration then clears (pickup notifications)
- "timed:N" -- displays for N seconds then clears

Multiple texts queue up and display one after another.
The queue is visible in combat with a translucent style.

### Object Inspection (E key)
When the player presses E facing an object or tile with assigned text:
- If description is non-empty: push to TextQueue with voice_profile
- If narrator is non-empty: push as a second entry
- If readable_text is non-empty: push as a third entry after the others
- If all fields are empty: E key does nothing on this object

All three fields are optional and independent.
Assigned per object in the editor via a text properties panel.

### Pickup Notifications (auto-dismiss)
When picking up an item, push to TextQueue with dismiss: "auto":
- "Picked up: [item name]"
- If the item has a pickup_text field defined: append it as a second line
- If pickup_text is empty: no second line

### Voice Profiles
Each text entry references a voice profile by ID.
Profiles defined in Data/voices/voice_profiles.json.
Default profile used when none assigned: "station_ambient".

```json
{
  "id": "station_ambient",
  "display_name": "",
  "text_colour": [200, 200, 215],
  "text_speed": 0.03,
  "blip_sound": "audio/voices/station_blip.wav",
  "portrait": "",
  "name_colour": [150, 150, 180],
  "special_effects": []
}
```

Special effects (optional): "glitch" (random character scramble during
type-in), "shake" (text panel vibrates), "slow" (speed halved).

Blip sound plays once per character typed, at the profile's pitch.
Different characters can have distinct blip sounds and pitches.

Voice profiles are a dropdown in the editor text properties panel.
New profiles can be added by dropping a new entry in the JSON file.

### Editor Text Properties Panel
Accessible from right-click on any tile or entity.
Fields:
- Description (multi-line text field)
- Narrator (single line)
- Readable text (multi-line, shown after description)
- Pickup text (single line, items only)
- Voice profile (dropdown from loaded profiles)
- Dismiss mode (dropdown: input / auto / timed)

All fields optional. Empty fields produce no output at runtime.

---

## Item System

### Item Definition Format
Items defined in Data/items/<category>/<id>.json
or a flat Data/items/ folder -- either structure works.

```json
{
  "id": "medkit_basic",
  "name": "Basic Medkit",
  "category": "stim",
  "description": "Standard-issue medical kit. Restores 20 HP.",
  "pickup_text": "The seal is intact.",
  "value": 25,
  "stackable": true,
  "max_stack": 5,
  "voice_profile": "station_ambient",
  "use_effect": {
    "restore_hp": 20
  },
  "tags": ["medical", "consumable"]
}
```

### Item Categories
Categories loaded from Data/item_categories.json.
Not hardcoded -- swap the config file for a different game/setting.

```json
{
  "categories": [
    { "id": "weapon",   "display_name": "Weapon",   "icon": "ui/cat_weapon.png" },
    { "id": "melee",    "display_name": "Melee",     "icon": "ui/cat_melee.png" },
    { "id": "trap",     "display_name": "Trap",      "icon": "ui/cat_trap.png" },
    { "id": "armor",    "display_name": "Armor",     "icon": "ui/cat_armor.png" },
    { "id": "stim",     "display_name": "Stim",      "icon": "ui/cat_stim.png" },
    { "id": "container","display_name": "Container", "icon": "ui/cat_container.png" },
    { "id": "disabler", "display_name": "Disabler",  "icon": "ui/cat_disabler.png" },
    { "id": "misc",     "display_name": "Misc",      "icon": "ui/cat_misc.png" },
    { "id": "ammo",     "display_name": "Ammo",      "icon": "ui/cat_ammo.png" }
  ]
}
```

For a fantasy game: rename "stim" to "Potion", "disabler" to "Charm",
"ammo" to "Quiver" etc. in the config. Engine never sees the display name.

---

## Inventory UI

### Layout (Final Fantasy inspired)
Full-screen panel. Game pauses while open (no ticks fire).
Opened with I key.

```
+--------------------------------------------------+
| [Weapon] [Melee] [Trap] [Armor] [Stim] [Misc].. |  <- category tabs
+--------------------------------------------------+
| > Basic Medkit                            x3     |
|   Stimpack                                x1     |
|   Field Dressing                          x2     |
|   ...                                            |  <- scrollable item list
|                                                  |
+--------------------------------------------------+
| Basic Medkit                                     |
| Standard-issue medical kit. Restores 20 HP.      |
| Value: 25    Use: E    Drop: R    Inspect: I      |  <- item detail panel
+--------------------------------------------------+
```

Category tabs always visible across the top.
Active tab highlighted. Cycle tabs with Q/E (or left/right arrow).
Item list scrolls with W/S or up/down arrow.
Selected item highlighted with a cursor indicator.

Item detail panel at the bottom shows:
- Item name
- Description
- Value
- Available actions (Use / Equip / Drop / Inspect -- context dependent)

### Actions in Inventory
- E: Use or Equip selected item (context dependent on category)
- R: Drop selected item (confirmation prompt)
- I: Inspect (opens full text box using the item's voice profile)
- Q/E (or left/right): cycle category tabs
- Escape or I: close inventory

### No Weight Limit
No carrying limit for now. TBD in a later pass.

---

## Container System

### Opening a Container
When the player presses E facing a container entity,
the container panel opens over the game world (game pauses).

### Container Panel Layout
```
+----------------------------------+
| LOCKER [7 items]                 |
+----------------------------------+
| > Basic Medkit              x2   |
|   9mm Ammo                  x12  |
|   Stimpack                  x1   |
|   ...                            |  <- scrollable list
+----------------------------------+
| [E: Take]  [R: Take All]  [Esc]  |
+----------------------------------+
```

W/S or up/down to scroll.
E to take the selected item (moves to player inventory).
R to take all items at once.
Escape or second E press closes the container.

### Container Entity Properties (Editor)
In the right-click entity properties panel, containers have:
- Guaranteed items list (always present, add by item ID)
- Loot table reference (optional, points to Data/loot_tables/<name>.json)
- RNG rolls (number of random draws from loot table)
- Locked toggle (requires a key item to open -- TBD in later pass)

### Loot Tables
Defined in Data/loot_tables/<name>.json.
Generated once on map load. Fixed for that session -- fair to the player.

```json
{
  "id": "medical_locker",
  "rolls": 2,
  "entries": [
    { "item_id": "medkit_basic",   "weight": 40 },
    { "item_id": "field_dressing", "weight": 35 },
    { "item_id": "stimpack",       "weight": 20 },
    { "item_id": "nothing",        "weight": 5  }
  ]
}
```

Weight is relative probability. Total does not need to equal 100.
"nothing" is a valid entry -- not every roll produces an item.
Multiple containers can share a loot table.

---

## Combat Screen Overview (Phase 5 Reference)

Documented here for planning purposes. Implementation in Phase 5.

The combat screen is a fully separate renderer state.
The world pauses. The bottom text bar moves to a translucent overlay.

Layout:
```
+--------------------------------------------------+
|  [Enemy portrait / illustration]                 |
|  Enemy Name             HP: ██████░░  AR: 3      |
+--------------------------------------------------+
|  [Player]  [NPC 1]  [NPC 2]  ...                 |  <- party strip
|  HP bar    HP bar   HP bar                       |
+--------------------------------------------------+
|  > Attack                                        |
|    Skills                                        |  <- action menu
|    Item                                          |     FF-style
|    Defend                                        |
|    Flee                                          |
+--------------------------------------------------+
| [text queue overlay -- translucent]              |
+--------------------------------------------------+
```

NPC party members can be set to autonomous mode (toggle in combat menu).
Autonomous NPCs act on their own each turn without player input.
Player can toggle autonomy per NPC mid-combat.

Uses TFTS dice system exactly as specced in the TTRPG document.
See Building a TTRPG system chat for complete rules including called
shots, area weapons, conditions, and hacking in combat.

---

## Folder Structure Additions

```
Data/
  items/                  -- item JSON definitions
  item_categories.json    -- category config (swap for fantasy setting)
  loot_tables/            -- shared loot table definitions
  voices/
    voice_profiles.json   -- all voice profile definitions
  inventory/              -- player inventory save state (managed by engine)

Assets/
  audio/
    voices/               -- blip sound files per voice profile
  ui/
    cat_weapon.png        -- category icons (placeholder colours for now)
    cat_melee.png
    cat_stim.png
    ... etc
```

---

## Notes for Fantasy Setting Compatibility

- Item categories: swap display names in item_categories.json
- Voice profiles: add new profiles with fantasy-appropriate blips
- Loot tables: entirely setting-agnostic, just item IDs and weights
- The TextQueue, inventory UI, and container panel are all generic --
  no station-specific code in any of them
- Combat screen (Phase 5) will also be generic by the same principle
