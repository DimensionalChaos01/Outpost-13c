# Outpost 13-C

A 2D top-down roguelike set in a research station on the edge of the Scar.
Tales from the Spiral — Beta Universe.

## Quick Start

See `docs/CLAUDE.md` for full context and architecture.
See `docs/design_doc.md` for the complete game design document.

## Dependencies

```bash
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
```

## Build

```bash
g++ -std=c++17 src/*.cpp -o build/outpost13c -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf
```

## Run

```bash
./build/outpost13c           # normal
./build/outpost13c --dev     # map editor mode
```

## Status

Pre-development. Folder structure and documentation in place.
No source code yet -- begin with Phase 1.
