#include "EntityDef.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

static const char* k_template = R"([
  {"id":"player_spawn","name":"Player Spawn","category":"spawn","walkable":true,
   "facing_required":false,"wall_mounted":false,"functional":false,
   "visible_shift":"both","placeholder_colour":[0,220,80]}
])";

void load_entity_defs(const std::string& path, std::vector<EntityDef>& out) {
    if (!ensure_file(path, k_template)) return;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "EntityDef: cannot open " << path << "\n";
        return;
    }

    json data;
    try { file >> data; }
    catch (const json::exception& e) {
        std::cerr << "EntityDef: JSON error: " << e.what() << "\n";
        return;
    }

    out.clear();
    for (const auto& item : data) {
        EntityDef d;
        d.id                    = item.value("id",                    "");
        d.name                  = item.value("name",                  "");
        d.category              = item.value("category",              "");
        d.walkable              = item.value("walkable",              false);
        d.facing_required       = item.value("facing_required",       false);
        d.wall_mounted          = item.value("wall_mounted",          false);
        d.functional            = item.value("functional",            false);
        d.visible_shift         = item.value("visible_shift",         "both");
        d.animation_placeholder = item.value("animation_placeholder", true);
        d.notes                 = item.value("notes",                 "");

        if (item.contains("placeholder_colour") &&
            item["placeholder_colour"].size() >= 3) {
            d.r = (Uint8)item["placeholder_colour"][0].get<int>();
            d.g = (Uint8)item["placeholder_colour"][1].get<int>();
            d.b = (Uint8)item["placeholder_colour"][2].get<int>();
        }
        if (!d.id.empty()) out.push_back(d);
    }
    std::cout << "EntityDef: loaded " << out.size() << " entity types\n";
}

const EntityDef* find_entity_def(const std::vector<EntityDef>& defs,
                                  const std::string& id) {
    for (const auto& d : defs)
        if (d.id == id) return &d;
    return nullptr;
}
