#include "TileDef.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

static const char* k_tile_defs_template = R"([
  {"id":0,"name":"Void",            "walkable":false,"pathable":false,"placeholder_colour":[0,0,0],      "auto_tile":false},
  {"id":1,"name":"Wall",            "walkable":false,"pathable":false,"placeholder_colour":[40,40,50],   "auto_tile":false},
  {"id":2,"name":"Floor",           "walkable":true, "pathable":true, "placeholder_colour":[78,78,90],   "auto_tile":false},
  {"id":3,"name":"Door",            "walkable":false,"pathable":false,"placeholder_colour":[108,72,35],  "auto_tile":false},
  {"id":4,"name":"Metal Floor",     "walkable":true, "pathable":true, "placeholder_colour":[90,90,105],  "auto_tile":false},
  {"id":5,"name":"Carpet Floor",    "walkable":true, "pathable":true, "placeholder_colour":[68,80,72],   "auto_tile":false},
  {"id":6,"name":"Reinforced Wall", "walkable":false,"pathable":false,"placeholder_colour":[28,28,38],   "auto_tile":false},
  {"id":7,"name":"Hull Wall",       "walkable":false,"pathable":false,"placeholder_colour":[18,20,30],   "auto_tile":false}
]
)";

static const TileDef k_void_fallback{};

bool load_tile_defs(const std::string& path, std::vector<TileDef>& out) {
    if (!ensure_file(path, k_tile_defs_template)) return false;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "TileDef: cannot open " << path << "\n";
        return false;
    }

    json data;
    try { file >> data; }
    catch (const json::exception& e) {
        std::cerr << "TileDef: JSON error: " << e.what() << "\n";
        return false;
    }

    out.clear();
    for (const auto& entry : data) {
        TileDef d;
        d.id        = entry.value("id",        0);
        d.name      = entry.value("name",       "Unknown");
        d.walkable  = entry.value("walkable",   false);
        d.pathable  = entry.value("pathable",   false);
        d.auto_tile = entry.value("auto_tile",  false);
        if (entry.contains("placeholder_colour") && entry["placeholder_colour"].size() >= 3) {
            d.r = entry["placeholder_colour"][0].get<Uint8>();
            d.g = entry["placeholder_colour"][1].get<Uint8>();
            d.b = entry["placeholder_colour"][2].get<Uint8>();
        }
        out.push_back(d);
    }
    std::cout << "TileDef: loaded " << out.size() << " tile types\n";
    return true;
}

const TileDef& get_tile_def(const std::vector<TileDef>& defs, int tile_id) {
    for (const auto& d : defs)
        if (d.id == tile_id) return d;
    return k_void_fallback;
}
