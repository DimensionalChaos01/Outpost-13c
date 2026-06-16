# Phase 2.5 — Implementation Prompt for Claude Code

Read docs/CLAUDE.md and docs/design_doc.md and docs/phase2_5_design.md
before starting. The phase2_5_design.md file contains the full
specification for everything in this prompt.
Do not remove any existing working code.
Implement in passes. Confirm build after each pass.

---

## PASS 1 — TURN SYSTEM RESTRUCTURE

The game currently runs game logic every frame.
Restructure to separate render loop from game tick.

- Render loop: runs every frame, handles SDL events, redraws screen
- Game tick: fires ONLY when the player consumes an input action
- A player action is: move one tile, press E (interact), press I (inventory),
  use an item, any deliberate input that advances game state
- Opening menus, browsing inventory, reading text: does NOT fire a tick
- The SDL event loop continues running during menus for smooth rendering

Add a GameState enum: EXPLORING, INVENTORY, TEXT, CONTAINER, COMBAT, EDITOR
Only EXPLORING fires world ticks on player input.
Other states consume input for their own systems without ticking the world.

Add a WorldTick() function that:
- Updates all entity positions and states
- Checks and fires pending events
- Processes status effect durations
- Is called exactly once per player action in EXPLORING state

This is a foundational change. Everything in Phase 3+ depends on it.
Confirm the player can still move and the map renders correctly after
this restructure before proceeding to Pass 2.

---

## PASS 2 — VOICE PROFILES AND TEXT QUEUE

### Voice Profiles
Load all voice profiles from Data/voices/voice_profiles.json on startup.
If file missing, use a hardcoded default station_ambient profile as fallback.

Each profile: id, display_name, text_colour, text_speed, blip_sound,
portrait (path, empty = no portrait), name_colour, special_effects array.

Special effects supported: "glitch" (scramble chars during type-in),
"slow" (half speed), "shake" (panel vibration). Store as strings in the
special_effects array. Engine reads and applies them during text rendering.

### TextQueue Manager
A singleton TextQueue that manages a FIFO queue of TextEntry objects.
TextEntry contains: text, narrator, voice_profile_id, dismiss mode.

Dismiss modes:
- "input": waits for E or Space to advance
- "auto": clears after 2 seconds
- "timed:N": clears after N seconds

Methods: push(entry), tick(dt), render(SDL_Renderer*), isActive().
tick() handles auto-dismiss timing and blip sound playback.

### Bottom Bar Renderer
A persistent bottom panel (~80px tall) rendered over the game world.
Invisible when TextQueue is empty.
Active when queue has entries: dark background, text types in character
by character at voice profile speed.
Blip sound plays once per character typed.
Shows narrator line in italic below main text if present.
Shows "[ Press E to continue ]" hint when dismiss mode is "input".

In GameState::TEXT: player input (E/Space) advances or dismisses.
In GameState::COMBAT: bar renders as translucent overlay (alpha ~180).

### Object Inspection
When player presses E and faces a tile or entity with text assigned:
Check description, narrator, readable_text fields in order.
Push non-empty fields as separate TextQueue entries.
If all three empty: E does nothing (no text, no state change).
Set GameState to TEXT while queue is active, return to EXPLORING when done.

### Editor Text Properties Panel
Add to right-click context menu for tiles and entities:
- Description field (multi-line)
- Narrator field (single line)
- Readable text field (multi-line)
- Pickup text field (single line, shown for item entities only)
- Voice profile dropdown (loaded from voice_profiles.json)
- Dismiss mode dropdown (input / auto / timed)

Store all fields in the map JSON under the tile or entity instance.

Create Data/voices/voice_profiles.json with these starter profiles:
- station_ambient (cold blue-white, slow, low blip)
- crew_log (warm white, normal speed, mid blip)
- corrupted_terminal (green-tinted, fast, glitch effect, static blip)
- intercom (slightly distorted, medium speed, click blip)

---

## PASS 3 — ITEM SYSTEM

### Item Categories
Load from Data/item_categories.json on startup.
Categories: weapon, melee, trap, armor, stim, container, disabler, misc, ammo.
Each has: id, display_name, icon path.
No category logic hardcoded in the engine.

### Item Definitions
Load all item JSON files from Data/items/ on startup (scan recursively).
Each item: id, name, category, description, pickup_text, value,
stackable, max_stack, voice_profile, use_effect, tags.

use_effect is an object with optional fields (restore_hp etc).
Fields not present are ignored. Engine reads what it finds.

Create these starter item definitions:
Data/items/medkit_basic.json       (stim, restore_hp: 20, value: 25)
Data/items/stimpack.json           (stim, restore_hp: 10, value: 15)
Data/items/field_dressing.json     (stim, restore_hp: 5, value: 8)
Data/items/ammo_9mm.json           (ammo, stackable, max_stack: 50)
Data/items/power_cell.json         (misc, value: 30)
Data/items/access_card_basic.json  (misc, value: 50)

### Item Entities in the World
Items can be placed as entities in the editor (category: "item_pickup").
Add item_pickup to entity_definitions.json.
Properties: item_id (text field, must match a loaded item definition).

When player presses E facing an item_pickup entity:
- Add item to player inventory (create PlayerInventory if not exists)
- Remove entity from map
- Push pickup notification to TextQueue: "Picked up: [item name]"
- If item has pickup_text: push as second auto-dismiss entry
- Fire WorldTick()

### PlayerInventory
A simple inventory container: map of item_id to quantity.
Methods: add(item_id, qty), remove(item_id, qty), has(item_id), 
         getAll(), getByCategory(category_id), getQuantity(item_id).
Saved to and loaded from the save file (Data/saves/<name>.json).
No weight limit. No slot limit.

---

## PASS 4 — INVENTORY UI

Full-screen panel. Opens with I key. Sets GameState to INVENTORY.
Game pauses -- no world ticks fire while inventory is open.

### Layout
Category tabs across the top (always visible).
Active tab highlighted. Q/left-arrow and E/right-arrow cycle tabs.
"All" tab shows every item regardless of category.

Scrollable item list below tabs.
Each row: item name on left, quantity (x3) on right.
Selected row highlighted with cursor indicator (> arrow).
W/S or up/down arrow to navigate.

Item detail panel at the bottom of the inventory screen:
- Item name (larger)
- Description text
- Value
- Available action hints: E:Use  R:Drop  I:Inspect

### Actions
- E: use or equip selected item
- R: drop (confirmation: "Drop [item name]? [Y/N]")
- I: inspect (pushes full text to TextQueue, closes inventory, 
     enters TEXT state using item's voice_profile)
- Escape or I: close inventory, return to EXPLORING

### Empty State
If a category tab has no items: show "Nothing here." in the list area.
If inventory is completely empty: show "Your inventory is empty."

---

## PASS 5 — CONTAINER SYSTEM

### Container Entity Properties (Editor Addition)
Add to container entity right-click properties panel:
- Guaranteed items: list of item IDs, add/remove buttons
- Loot table: text field for loot table ID (references Data/loot_tables/)
- RNG rolls: number input (0 = no random items)
- Locked: toggle (placeholder, locking logic in a later pass)

### Loot Tables
Load all files from Data/loot_tables/ on startup.
Each file: id, rolls, entries array (item_id, weight).
"nothing" is a valid item_id (produces no item).

Container contents generated once when the map loads.
Generated contents cached in the map's runtime state (not saved to disk --
regenerates each time the map loads fresh, which is correct).
Contents fixed for the duration of the session.

### Opening a Container
When player presses E facing a container entity:
Set GameState to CONTAINER.
Open container panel over the game world.

### Container Panel Layout
Header: container name and item count.
Scrollable item list: item name on left, quantity on right.
W/S or up/down to navigate.
Footer: "E: Take  R: Take All  Esc: Close"

Actions:
- E: take selected item, add to player inventory, remove from container list
- R: take all items at once, close panel
- Escape: close panel, return to EXPLORING
- When container is emptied: show "Empty." and close on next input

### Pickup Text on Container Items
When taking an item from a container, push auto-dismiss pickup
notification to TextQueue same as floor item pickup.

Create Data/loot_tables/medical_locker.json as a starter loot table.

---

## DATA FILES TO CREATE

Data/voices/voice_profiles.json        (4 starter profiles)
Data/item_categories.json              (9 categories)
Data/items/medkit_basic.json
Data/items/stimpack.json
Data/items/field_dressing.json
Data/items/ammo_9mm.json
Data/items/power_cell.json
Data/items/access_card_basic.json
Data/loot_tables/medical_locker.json

---

## RULES

Do not remove any existing working code.
All data from JSON files. No item, category, or voice data hardcoded.
The TextQueue, inventory, and container systems must contain zero
station-specific or sci-fi-specific code -- they must work identically
for a fantasy setting with different config files.
Play mode (F5) and editor must continue to work after each pass.
Compile and confirm working after each pass before starting the next.
