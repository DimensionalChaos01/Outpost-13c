# Phase 2.5 — UI, Player Stats, and Inventory Screen
## Implementation Prompt for Claude Code

Read docs/CLAUDE.md, docs/design_doc.md, and docs/phase2_5_design.md
before starting. Do not remove any existing working code.
Implement in passes. Confirm build after each pass.

---

## DESIGN PHILOSOPHY

There is no persistent HUD.
The world is the interface.
Information surfaces in context -- in menus, terminals, and sub-screens.
The player sees only the game world during exploration.
This is intentional. Preserve it.

---

## PASS 1 — PLAYER STATS STRUCT

Add a PlayerStats struct to the Player:

```cpp
struct PlayerStats {
    int hp_current;
    int hp_max;
    int sanity_current;
    int sanity_max;
    int value;          // in-universe currency, station funds = personal funds
    int strength;
    int agility;
    int defense;        // AR
    int perception;
    int endurance;
    int fortitude;
    int discipline;
};
```

HP formula: (endurance + fortitude) * 3, minimum 6.
Recalculate hp_max whenever endurance or fortitude changes.
sanity_max: 100 for now, flat value.
All other stats default to 1 at game start.
value defaults to 100 (starting funds).

Save all stat fields to save file.
Load all stat fields from save file.
If a field is missing from save (older save file), use the default.

No stat display in the world. No HUD. Stats are invisible during exploration.

---

## PASS 2 — INVENTORY SCREEN STATS PANEL

The inventory screen already exists from Phase 2.5 Pass 4.
Add a stats panel to the left side of the inventory screen.

### Layout

```
+----------------+----------------------------------+
| SMITH          | [Weapon][Melee][Armor][Stim]...  |
|                +----------------------------------+
| HP             | > Basic Medkit            x3     |
| [████████░░]   |   Stimpack                x1     |
| 45 / 60        |   Field Dressing          x2     |
|                |                                  |
| SANITY         |                                  |
| [██████░░░░]   |                                  |
| 72 / 100       +----------------------------------+
|                | Basic Medkit                     |
| VAL: 250       | Standard-issue medical kit.      |
|                | Restores 20 HP.                  |
| STR  1         | Value: 25                        |
| AGI  1         | E:Use  R:Drop  I:Inspect         |
| DEF  1         |                                  |
| PER  1         |                                  |
| END  1         |                                  |
| FOR  1         |                                  |
| DIS  1         |                                  |
+----------------+----------------------------------+
```

Left panel: player name, stat bars with numeric values, Value, raw stats.
Right panel: category tabs, item list, item detail -- existing inventory UI.

### Stat Bars
Each bar:
- Label above (HP, SANITY)
- Filled rectangle bar scaled to current/max ratio
- Numeric values below: "45 / 60"

HP bar colour: green when above 50%, yellow 25-50%, red below 25%.
Sanity bar colour: white-blue when above 50%, purple 25-50%,
  deep red below 25%.
Colour transitions are instant, not gradual (for now).
Bar background: dark grey. Bar fill: the colour above.
Bar dimensions: full width of the left panel, ~12px tall.

Value display: "VAL: 250" -- plain text, no bar.
Updates immediately when value changes anywhere (terminal purchase etc).

Raw stats (STR/AGI/DEF etc): small text, two-column list.
Label on left, value on right, right-aligned.

### Morale (Crew Screen Only)
Morale does NOT appear in the main inventory stats panel.
It appears only in the crew management sub-screen (to be built later).
Do not add morale to the player stats panel.

### Player Name
Shown at the top of the left panel.
Taken from the new game setup (entered at main menu).
Stored in save file as "player_name".

---

## PASS 3 — DAY/SHIFT TERMINAL

The day counter and shift indicator do not appear on screen during
exploration. They are shown when the player interacts with a specific
terminal type in the world.

### Day/Shift Terminal Entity
Add a new entity type to entity_definitions.json:
  id: "terminal_dayshift"
  name: "Station Clock Terminal"
  category: "terminal"
  functional: true

### Terminal Display
When the player presses E facing a terminal_dayshift entity,
open a terminal panel (not the text queue -- a distinct full panel).

The terminal panel renders as a monitor/screen:
- Background: near-black
- All text: bright green (classic terminal green: RGB 0, 255, 70)
- Monospace font
- Slight scanline effect (horizontal lines at low opacity, optional)
- Panel framed with a simple border

Content:
```
OUTPOST 13-C STATION CLOCK
===========================

Day: 7
Shift: Night

Local time: 02:14
Next shift change: 05:00

===========================
[ESC] Close
```

Local time is simulated: advances in real seconds at an accelerated
rate (1 real second = 10 in-game minutes). Does not affect gameplay.
Purely atmospheric. Saved to save file.
Next shift change is always 6 hours after current shift start.

Sets GameState to a new TERMINAL_DISPLAY state while open.
Escape closes it and returns to EXPLORING.
No world ticks fire while terminal is open.

### Other Terminals
Other terminal entity types (terminal_generic, terminal_supply,
terminal_hire) will have their own panel states in later passes.
Build the terminal panel as a reusable component that other terminals
will also use -- same green-on-black style, same frame.

---

## PASS 4 — VALUE NOTIFICATION

Value does not show during exploration.
When Value changes (item sold, item purchased, found in world),
push a brief auto-dismiss notification to the TextQueue:

Increase: "+ 50 VAL" (green tinted text if voice profile supports it)
Decrease: "- 25 VAL" (red tinted, or default voice)

Use the "station_ambient" voice profile with dismiss: "auto".
Duration: 1.5 seconds.

Value is visible at any time by opening the inventory screen.
No other persistent display.

---

## PASS 5 — COMBAT SECONDARY DISPLAY

During combat (GameState::COMBAT), show a minimal secondary
stat reference in a corner of the combat screen.

Small panel, corner-anchored (top-right or bottom-left,
whichever fits cleanly with the combat layout):

```
HP  45/60  [████░]
SAN 72/100 [████░]
```

Bars at reduced scale (~80px wide).
This is reference only -- the main HP display is in the combat
character strip (built in Phase 5).
If the combat screen hides this cleanly, it can be removed then.
For now it gives immediate feedback during combat.

---

## STYLING NOTES

Inventory screen left panel: dark background matching inventory.
Stat bars: clean, no gradients, solid fill.
Terminal display: green on near-black, monospace font.
No animations on stat bars yet (no drain animation, no flash on damage).
These are intentionally deferred -- get the data right first.
Visual polish comes in a later pass.

---

## DATA CHANGES

Save file additions:
- player_name (string)
- player_stats (full PlayerStats object)
- simulated_time (int, minutes since day 1 start)
- day_counter (int)
- current_shift (string: "day" or "night")

entity_definitions.json addition:
- terminal_dayshift entry

---

## RULES

No persistent HUD during exploration. This is a design principle.
No stat display in the world at any time outside of menus and terminals.
Do not add a health bar above the player sprite.
Do not add any overlay to the exploration view.
All stat information lives in inventory screen and terminal panels only.
Do not remove any existing working code.
Compile and confirm working after each pass.
