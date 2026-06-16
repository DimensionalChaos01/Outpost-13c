# Phase 2 — Map Editor
## Full Implementation Prompt for Claude Code

Read docs/CLAUDE.md and docs/design_doc.md before starting.
Implement in passes if needed -- tiles and navigation first, then entities,
then UI and validation. Do not remove any existing working code at any point.

---

## PASS 1 — NAVIGATION AND TILE SYSTEM

### Camera and Navigation
- Middle mouse button drag: pan the camera across the map
- Ctrl + scroll wheel: zoom (three levels: 50%, 100%, 200%)
- Zoom keeps the tile under the cursor stationary while zooming
- G key: toggle grid lines on/off
- Minimap in a corner of the editor: shows full map scaled down,
  camera viewport outlined as a rectangle. Click minimap to jump camera there.
- Remove any hardcoded map size limit. Maps should support up to 500x500 tiles.

### Tile Definitions from JSON
Load all tile types from Data/tiles/tile_definitions.json at startup.
No tile types hardcoded in the engine. Each entry:
```json
{
  "id": 0,
  "name": "Void",
  "walkable": false,
  "pathable": false,
  "default_scar_intensity": 0,
  "placeholder_colour": [0, 0, 0],
  "auto_tile": false
}
```
Note: design the tile ID and neighbour system to support bitmask auto-tiling
in a future pass (where walls automatically select corner/edge/junction
variants based on neighbours). Do not implement auto-tiling now -- just
make sure the data format and renderer will support it later without
restructuring. Add a comment in the renderer marking where auto-tile
lookup will go.

### Tile Tools
- **Paint tool (default):** left-click and drag to paint selected tile
- **Erase tool (X):** right-click or X key, paints floor tile
- **Eyedropper (E):** click a tile to select its type, return to paint mode
- **Fill tool (F):** flood-fills a contiguous region with the selected tile.
  Only fills tiles of the same type as the clicked tile.
- **Rectangle room tool (R key while in tile mode):** drag to define a
  rectangle. On release, fills the border with wall tiles and the interior
  with floor tiles. Shift+drag fills the entire rectangle with selected tile
  instead of making a room outline.

### Tile Palette Panel
Tab key toggles the tile palette open/closed.
Panel shows all tiles from tile_definitions.json as labelled colour swatches.
Scrollable if more tiles than fit on screen.
Clicking a swatch selects that tile type.
Current selection highlighted with a border.

### Map Browser
On editor startup, scan Data/levels/ for all .json map files.
Show a map browser panel listing them. Click to open. 
New Map button: prompts for name, width, and height, creates a new .json file.
Currently open map name shown in title bar alongside EDITOR MODE.
Unsaved changes shown as an asterisk in the title bar.

### Create these tile definition files:
Data/tiles/tile_definitions.json with entries for:
- void (id 0, black, not walkable)
- wall (id 1, dark blue-grey, not walkable)
- floor (id 2, medium grey, walkable)
- metal_floor (id 3, slightly lighter grey, walkable)
- carpet_floor (id 4, muted green-grey, walkable)
- reinforced_wall (id 5, darker blue-grey, not walkable)
- hull_wall (id 6, near-black with slight blue, not walkable)

---

## PASS 2 — ENTITY LAYER

### Overview
Entities are a second layer on top of tiles.
They do not replace tiles. They have a world position and face direction.
Stored in the map JSON under an "entities" array.
All entity types loaded from Data/entities/entity_definitions.json.
No entity types hardcoded in the engine.

Each entity definition:
```json
{
  "id": "door_standard",
  "name": "Standard Door",
  "category": "door",
  "walkable_when_open": true,
  "walkable_when_closed": false,
  "facing_required": false,
  "wall_mounted": false,
  "functional": true,
  "animation_placeholder": true,
  "placeholder_colour": [180, 120, 60],
  "notes": "Connects two rooms. Has open/closed state."
}
```

### Entity Categories
- **furniture** -- decorative, no function yet
- **workstation** -- functional, NPC assignable
- **terminal** -- functional, player-interactable
- **wall_mounted** -- attaches to a wall face (light switch, panel, poster, vent)
- **container** -- holds items (locker, crate, cabinet)
- **door** -- open/closed state, connects rooms
- **trigger** -- invisible zone that fires an event
- **spawn** -- marks a spawn point (player, NPC, enemy)
- **path_node** -- NPC patrol waypoint (see patrol path system below)

### Wall-Mounted Entities
Wall-mounted entities have a facing (N/S/E/W) stored in the instance.
When placing: editor detects the nearest adjacent wall and suggests a facing.
R key rotates facing before confirming placement.
Rendered as a small indicator on the wall-adjacent edge of their tile.

### Entity Placement Mode
E key toggles the entity panel open (separate from tile palette).
Panel groups entities by category with collapsible headers.
Click an entity type to select it, then left-click a tile to place it.
Right-click a placed entity to open its properties panel:
- Entity type name
- Facing (N/S/E/W dropdown, if applicable)
- Custom label (optional name for this instance)
- Functional toggle
- Linked entity (see Entity Linking below)
- Notes field
- Delete button

Placed entities render as distinct coloured squares or symbols on their tile.
Entity layer visibility toggled with V key overlay.

### Player Spawn Entity
A special entity of category "spawn" with id "player_spawn".
There must be exactly one per map.
F5 play mode uses this entity's position to spawn the player.
If no player_spawn exists, F5 shows a warning and places a temporary one
at the centre of the map.
Map validation also checks for this (see Validation below).

### Entity Linking
Some entities need to reference other entities at runtime
(a light switch controls a light, a terminal fires a specific trigger zone).
Select a source entity, click the chain-link icon in its properties panel,
then click the target entity on the map.
A visible line draws between linked entities in the editor.
Link stored as target entity's instance ID in the source entity's JSON.
Multiple links allowed (one switch can control multiple lights).
Clicking an existing link line selects it; Delete removes the link.

### NPC Patrol Path Editor
Select an entity of category "workstation", "furniture", or any NPC-type entity.
Click the "Edit Patrol Path" button in its properties panel.
Then click tiles on the map to place path_node entities in sequence.
Nodes connect with numbered arrows showing patrol order.
Click an existing node to remove it.
Path stored as an ordered array of tile coordinates in the entity's JSON.
Escape exits patrol path editing mode.

### Create these entity definition files:
Data/entities/entity_definitions.json with entries for:
- player_spawn (spawn, not walkable, bright green)
- basic_chair (furniture, walkable)
- basic_table (furniture, not walkable)
- workstation_generic (workstation, not walkable, functional)
- terminal_generic (terminal, not walkable, functional)
- light_switch (wall_mounted, facing_required, functional)
- wall_panel (wall_mounted, facing_required, functional)
- wall_poster (wall_mounted, facing_required, not functional)
- vent_cover (wall_mounted, facing_required, not functional)
- door_standard (door, facing_required, functional)
- container_locker (container, not walkable)
- container_crate (container, not walkable)
- trigger_zone (trigger, walkable, invisible in play mode)
- npc_spawn_point (spawn, walkable)
- enemy_spawn_point (spawn, walkable)

---

## PASS 3 — SELECT, UNDO, AND MULTI-FLOOR

### Select Mode (S key)
Drag to select a rectangular region (tiles and entities both).
Selected region shown with an animated dashed border.

Actions on selection:
- Delete: clear tiles to floor, remove entities
- C: copy region (tiles and entities with relative positions)
- V: paste copied region at cursor position
- Arrow keys: nudge selection one tile (moves entities and repaints tiles)
- Rotate: not in this pass, note as future feature
- Escape: deselect

### Undo / Redo
Ctrl+Z: undo (minimum 20 steps, no maximum)
Ctrl+Y or Ctrl+Shift+Z: redo

Tracks all reversible operations:
- Tile paint (including fill and rectangle room)
- Tile erase
- Entity placement
- Entity deletion
- Entity property change
- Entity link add/remove
- Patrol path edit
- Selection operations (nudge, paste, delete)
- Map resize

### Day/Night Preview Toggle
N key cycles through three preview modes:
- Normal (editor default, all entities visible)
- Day shift preview (only day-shift entities visible, slight warm tint)
- Night shift preview (only night-shift entities visible, slight cool/dark tint)

Each entity definition has a "visible_shift" field: "day", "night", or "both".
Add this field to entity_definitions.json for all entries (default "both").
Preview mode does not affect saving. It is view-only.
Current preview mode shown in the status bar.

### Multi-Floor Ghost View
When a map has connected floor IDs defined in its metadata,
Shift+G toggles a ghost view of the connected floor above or below.
The ghost renders at 25% opacity underneath the current floor.
Toggle which connected floor to ghost with Shift+Up / Shift+Down.
Ghost is read-only -- you cannot edit it. For alignment reference only.
If no connected floors are defined, Shift+G shows a "no connected floors" 
message in the status bar.

---

## PASS 4 — UI, MENUS, AND VALIDATION

### Top Menu Bar
Clickable dropdowns. Keyboard shortcuts shown next to items.

**File**
- New Map (Ctrl+N): name and size prompt, creates Data/levels/<name>.json
- Open Map (Ctrl+O): opens map browser panel
- Save (Ctrl+S): saves current map
- Save As: saves with new name
- Resize Map: prompts for new width/height, preserves existing tiles
- Recent Maps: last 5 opened maps

**Edit**
- Undo (Ctrl+Z)
- Redo (Ctrl+Y)
- Select All (Ctrl+A)
- Clear Selection (Escape)
- Copy (Ctrl+C)
- Paste (Ctrl+V)
- Delete Selection (Delete)

**View**
- Toggle overlays: Path, Rooms, Scar, Light, Entities, Grid, Minimap
- Zoom In / Zoom Out / Reset Zoom (Ctrl+0)
- Day/Night Preview cycle
- Multi-floor ghost toggle

**Map**
- Map Metadata: opens a panel to set name, floor number, era, ambient audio
  track filename, default Scar intensity, connected floor IDs (up/down),
  day/night availability
- Validate Map: runs all checks and shows results panel (see below)
- Play Test (F5)

**Help**
- Keyboard shortcuts reference panel

### Bottom Status Bar
Always visible. Shows:
- Current tool name
- Selected tile or entity type name  
- Cursor tile coordinates (x, y)
- Map dimensions (w x h)
- Current zoom level
- Day/night preview mode
- Active overlays (compact icon row)
- Unsaved changes indicator (asterisk)
- Contextual shortcut hints that update based on current tool

### Map Validation
Accessible from Map menu or Ctrl+Shift+V.
Runs checks and displays a results panel with pass/fail per check.
Double-clicking a warning jumps the camera to the problem tile or entity.

Checks:
- Player spawn point exists (error if missing)
- No orphaned rooms (all floor regions reachable from spawn)
- No wall-mounted entities floating away from walls
- No entities placed on impassable tiles (except wall_mounted and triggers)
- No broken entity links (link points to a deleted entity)
- All doors have at least one floor tile on each facing side
- No map metadata fields blank (name, floor number, era)
- Scar intensity values in valid range (0-4)
- No overlapping entities on the same tile (except triggers)
- Play test reachability: can the player reach all floor tiles from spawn?

Results shown as: errors (red, block play test), warnings (yellow, advisory).
F5 play test is blocked if there are any errors. Shows validation results first.

---

## SUMMARY OF NEW FILES TO CREATE

Data/tiles/tile_definitions.json
Data/entities/entity_definitions.json

Update docs/CLAUDE.md to note:
- Auto-tiling is a planned future feature. Tile format is designed for it.
- The renderer has a marked placeholder where auto-tile lookup will go.
- entity_definitions.json is the single source of truth for entity types.
- Map validation must pass (no errors) before a map can be play-tested.

---

## ABSOLUTE RULES
- Do not remove any existing working code.
- Do not hardcode tile types, entity types, or game-specific values.
- All data from JSON files.
- If a feature requires more than one pass, complete each pass fully
  before starting the next. Do not leave partial implementations.
- After each pass, confirm the build compiles and F5 play mode still works.
