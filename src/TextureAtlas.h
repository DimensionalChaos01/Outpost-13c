#pragma once
#include <string>
#include <vector>

struct AtlasEntry {
    std::string id;                    // composite key "atlas_id:entry_name"
    std::string name;                  // display name
    int         cell = 0;             // flat cell index: row*cols + col
    std::vector<int> compatible_types; // tile_ids this entry is designed for (empty = any)
};

struct TextureAtlas {
    std::string id;
    std::string image_path;
    int         cell_size = 32;
    int         cols = 1, rows = 1;
    std::vector<AtlasEntry> entries;

    void cell_rect(int cell_idx, int& ox, int& oy, int& ow, int& oh) const;
    const AtlasEntry* find_entry(const std::string& name) const;
};

// Finds atlas + entry from "atlas_id:entry_name" composite id.
bool find_atlas_entry(const std::vector<TextureAtlas>& atlases,
                      const std::string& texture_id,
                      const TextureAtlas*& atlas_out,
                      const AtlasEntry*& entry_out);

bool load_atlases(const std::string& dir, std::vector<TextureAtlas>& out);
