#include "TextureAtlas.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

void TextureAtlas::cell_rect(int cell_idx, int& ox, int& oy, int& ow, int& oh) const {
    ox = (cell_idx % cols) * cell_size;
    oy = (cell_idx / cols) * cell_size;
    ow = oh = cell_size;
}

const AtlasEntry* TextureAtlas::find_entry(const std::string& name) const {
    for (const auto& e : entries)
        if (e.name == name) return &e;
    return nullptr;
}

bool find_atlas_entry(const std::vector<TextureAtlas>& atlases,
                      const std::string& texture_id,
                      const TextureAtlas*& atlas_out,
                      const AtlasEntry*& entry_out) {
    atlas_out = nullptr; entry_out = nullptr;
    const auto colon = texture_id.find(':');
    if (colon == std::string::npos) return false;
    const std::string aid  = texture_id.substr(0, colon);
    const std::string name = texture_id.substr(colon + 1);
    for (const auto& a : atlases) {
        if (a.id != aid) continue;
        for (const auto& e : a.entries) {
            if (e.name == name) { atlas_out = &a; entry_out = &e; return true; }
        }
    }
    return false;
}

bool load_atlases(const std::string& dir, std::vector<TextureAtlas>& out) {
    out.clear();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        std::cout << "TextureAtlas: directory not found: " << dir << "\n";
        return true; // optional — not an error
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f.is_open()) continue;
        json data;
        try { f >> data; } catch (...) {
            std::cerr << "TextureAtlas: parse error in " << entry.path() << "\n";
            continue;
        }
        TextureAtlas atlas;
        atlas.id         = data.value("id",        entry.path().stem().string());
        atlas.image_path = data.value("image",     "");
        atlas.cell_size  = data.value("cell_size", 32);
        atlas.cols       = data.value("cols",      1);
        atlas.rows       = data.value("rows",      1);
        if (data.contains("entries")) {
            for (const auto& e : data["entries"]) {
                AtlasEntry ae;
                ae.name = e.value("name", "");
                ae.cell = e.value("cell", 0);
                ae.id   = atlas.id + ":" + ae.name;
                if (e.contains("compatible_types"))
                    for (int v : e["compatible_types"])
                        ae.compatible_types.push_back(v);
                if (!ae.name.empty()) atlas.entries.push_back(ae);
            }
        }
        std::cout << "TextureAtlas: loaded '" << atlas.id
                  << "' (" << atlas.entries.size() << " entries)\n";
        out.push_back(std::move(atlas));
    }
    return true;
}
