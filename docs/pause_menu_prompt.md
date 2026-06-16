# Pause Menu, Main Menu, and Settings System
## Implementation Prompt for Claude Code

Read docs/CLAUDE.md before starting. Do not remove any existing working code.
Implement in passes. Confirm build and basic function after each pass.

---

## PASS 1 -- CORE MENU SYSTEM

### Architecture
A single PauseMenu class handles all three pause menu modes.
Menu mode is set by a MenuMode enum: GAMEPLAY, EDITOR, PLAYTEST.
One rendering system, one input handler, mode-aware button list.

The menu renders as:
- A dark translucent overlay over the current screen
- A centred panel with title and buttons
- Subtle colour tint on the overlay differs per mode:
  GAMEPLAY:  neutral dark overlay,     title: "PAUSED"
  EDITOR:    slight cool blue tint,    title: "EDITOR"
  PLAYTEST:  slight warm orange tint,  title: "PLAYTEST"

All button styling, fonts, and layout identical across modes.
Buttons highlight on hover. Click or Enter to confirm.
Escape closes the menu (returns to game/editor/playtest).

### Escape Key Behaviour
GAMEPLAY:    Escape always opens/closes pause menu
EDITOR:      Escape first cancels any active operation (paint stroke,
             selection, entity placement, open panel). Only opens the
             editor menu if no operation is in progress.
PLAYTEST:    Escape opens playtest pause menu.
             Does NOT return to editor directly -- that is a menu button.

---

## PASS 1 BUTTON LISTS

### Gameplay Pause Menu
- Resume
- Save Game
- Load Game
- Settings  (opens Settings panel, see Pass 3)
- Main Menu (confirmation prompt if unsaved progress)
- Quit to Desktop

### Editor Pause Menu
The top bar already handles File/Edit/View/Map.
The editor pause menu is intentionally minimal:
- Close Menu
- Settings  (opens Settings panel)
- Quit to Desktop (confirmation if unsaved map changes)

### Playtest Pause Menu
- Resume Playtest
- Add Playtest Note  (opens note input field, see Pass 2)
- View Playtest Notes  (opens notes panel, see Pass 2)
- Dev Tools  (opens dev tools submenu, see below)
- Return to Editor  (returns to editor, map unchanged)
- Quit to Desktop

### Dev Tools Submenu (Playtest only)
A sub-panel that opens from the playtest menu. Never shown in gameplay.
- Toggle Fog of War  (checkbox, default on)
- God Mode  (checkbox, player takes no damage, default off)
- Teleport to Coordinates  (x/y input fields + Go button)
- Give All Items  (button, adds one of every defined item to inventory)
- Show Collision Overlay  (checkbox, renders walkable/impassable overlay)
- Show Entity IDs  (checkbox, shows entity instance IDs above entities)
Back button returns to playtest pause menu.

### Unsaved Changes Confirmation
When quitting or going to main menu with unsaved changes, show a prompt:
"You have unsaved changes."
  [ Save and Exit ]  [ Discard and Exit ]  [ Cancel ]
Triggered by: quit buttons in editor menu, window close button when in editor.

---

## PASS 2 -- PLAYTEST NOTES SYSTEM

### Adding Notes
"Add Playtest Note" opens a small text input panel over the pause menu.
Player types a note. Enter or Submit button saves it.
Escape cancels without saving.

Notes saved to Data/playtest_notes.json:
{
  "notes": [
    {
      "id": 1,
      "timestamp": "2025-01-01 14:32",
      "map": "test",
      "player_position": {"x": 12, "y": 7},
      "text": "Door in room 3 clips the wall.",
      "read": false
    }
  ]
}

Each note stores: auto-incrementing ID, timestamp, current map name,
player tile position at time of note, note text, read status.

### Viewing Notes in Playtest
"View Playtest Notes" opens a scrollable panel listing all notes.
Each note shows: timestamp, map name, position, text.
Click a note to mark it as read.
Delete button on each note removes it.
"Clear All Read" button removes all read notes.

### Notes Panel in Editor
A notes button in the editor top bar (or View menu) opens the notes panel.
Unread notes show a small indicator badge on the button.
In the editor notes panel, clicking a note that has a saved position
jumps the editor camera to that tile position on the relevant map.
This lets you navigate directly to flagged problems.
Notes can be marked read or deleted from the editor panel too.

---

## PASS 3 -- SETTINGS PANEL

Single Settings panel, accessible from all three pause menus and main menu.
Three tabs across the top: Display | Audio | Keybinds

### Display Tab
- Resolution: dropdown of common resolutions
- Fullscreen: toggle
- VSync: toggle
- Tile grid in editor: toggle (same as G key)
- UI scale: slider (small / normal / large)

Settings saved to Data/settings.json on apply.
Loaded on startup. Defaults used if file missing.

### Audio Tab
- Master volume: slider 0-100
- Music volume: slider 0-100
- SFX volume: slider 0-100
- Ambient volume: slider 0-100
(No audio system yet -- sliders present, values saved, applied when
audio is implemented in a later phase.)

### Keybinds Tab
A scrollable list of all named actions and their current key binding.
Click a binding to enter rebind mode: "Press a key..."
Press the desired key to assign it. Escape cancels rebind.
If the key is already bound to another action, show a conflict warning:
"This key is used by [action name]. Reassign?"
  [ Reassign ] [ Cancel ]
"Reset to Defaults" button at the bottom resets all bindings.

Keybinds saved to Data/settings.json alongside display and audio.

Named actions to include (at minimum):
  move_up, move_down, move_left, move_right
  pause, interact, inventory
  editor_paint, editor_erase, editor_eyedropper
  editor_fill, editor_select, editor_entity
  editor_save, editor_undo, editor_redo
  editor_toggle_grid, editor_toggle_path, editor_toggle_entities
  editor_play_test, editor_texture_picker
  playtest_return_to_editor

---

## PASS 4 -- MAIN MENU

Shown on application startup.

Layout:
- Title: "OUTPOST 13-C" centred near top
- Subtitle: "Tales from the Spiral" smaller text below title
- Background: solid dark colour for now with a subtle noise/grain effect.
  Leave a clearly marked placeholder comment where a background
  image or scene will be loaded in future. Load from
  Assets/ui/main_menu_bg.png if the file exists, dark fallback if not.
- Buttons centred in the lower half:
  New Game
  Load Game
  Map Editor
  Settings
  Quit

### New Game
Opens a new game setup panel:
- Player name field
- Difficulty (Normal / Hard) -- placeholder, no mechanical difference yet
- Confirm button starts a new game from the starting map

### Load Game
Opens a save file browser showing existing saves from Data/saves/.
Each entry shows: save name, date, current map, play time.
Click to load. Delete button on each entry.
"No saves found" message if folder is empty.

### Map Editor
Opens the editor directly, showing the map browser.

### Settings
Opens the Settings panel (same component as pause menu settings).

---

## PASS 5 -- SAVE AND LOAD SYSTEM

Save files stored in Data/saves/<savename>.json.

Save file contents:
{
  "save_name": "Save 1",
  "timestamp": "2025-01-01 14:32",
  "play_time_seconds": 3600,
  "current_map": "station_floor_01",
  "player_position": {"x": 12, "y": 7},
  "player_stats": {},
  "inventory": [],
  "flags": {},
  "day_counter": 1,
  "current_shift": "night",
  "morale": 75,
  "value": 500
}

Player stats and inventory are empty objects/arrays for now.
They will be populated when those systems are implemented.

Save triggered by: Save Game in pause menu, Ctrl+S in gameplay,
autosave on floor transition, autosave at shift change.

Load triggered by: Load Game on main menu, Load Game in pause menu.

Autosave slot is separate from manual saves.
Autosave file: Data/saves/autosave.json
Autosave shown at top of load game list labelled "Autosave".

---

## SETTINGS FILE FORMAT

Data/settings.json:
{
  "display": {
    "resolution_w": 1280,
    "resolution_h": 720,
    "fullscreen": false,
    "vsync": true,
    "show_grid": true,
    "ui_scale": 1
  },
  "audio": {
    "master": 80,
    "music": 70,
    "sfx": 80,
    "ambient": 60
  },
  "keybinds": {
    "move_up": "W",
    "move_down": "S",
    "move_left": "A",
    "move_right": "D",
    "pause": "Escape",
    "interact": "F",
    "inventory": "I",
    "editor_paint": "P",
    "editor_erase": "X",
    "editor_eyedropper": "E",
    "editor_fill": "F",
    "editor_select": "S",
    "editor_entity": "E",
    "editor_save": "Ctrl+S",
    "editor_undo": "Ctrl+Z",
    "editor_redo": "Ctrl+Y",
    "editor_toggle_grid": "G",
    "editor_toggle_path": "P",
    "editor_toggle_entities": "V",
    "editor_play_test": "F5",
    "editor_texture_picker": "Backslash",
    "playtest_return_to_editor": "Escape"
  }
}

---

## FOLDER STRUCTURE ADDITIONS

Data/saves/           -- save files
Data/playtest_notes.json  -- playtest notes
Data/settings.json    -- display, audio, keybind settings
Assets/ui/main_menu_bg.png  -- placeholder path, file not required

---

## RULES
- Do not remove any existing working code.
- All data (saves, settings, notes) from JSON files.
- Settings load on startup, save on apply or quit.
- Play mode (F5) must continue to work.
- Editor must continue to work.
- Implement passes in order. Confirm build after each pass.
- The Settings panel is one component called from multiple places,
  not three separate implementations.
