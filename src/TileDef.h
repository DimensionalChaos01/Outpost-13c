#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct TileDef {
    int         id        = 0;
    std::string name;
    bool        walkable  = false;
    bool        pathable  = false;
    bool        auto_tile = false;   // reserved for bitmask auto-tiling (Phase 7)
    Uint8       r = 0, g = 0, b = 0; // placeholder_colour
};

// Load tile definitions from a JSON file into out. Creates the file from a
// built-in template if it does not exist. Returns false on hard error.
bool load_tile_defs(const std::string& path, std::vector<TileDef>& out);

// Return the definition for a given tile_id, or a default void tile if unknown.
const TileDef& get_tile_def(const std::vector<TileDef>& defs, int tile_id);
