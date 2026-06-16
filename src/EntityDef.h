#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct EntityDef {
    std::string id;
    std::string name;
    std::string category;
    bool        walkable             = false;
    bool        facing_required      = false;
    bool        wall_mounted         = false;
    bool        functional           = false;
    std::string visible_shift        = "both";
    bool        animation_placeholder = true;
    Uint8       r = 128, g = 128, b = 128;
    std::string notes;
};

void load_entity_defs(const std::string& path, std::vector<EntityDef>& out);

// Returns nullptr if not found.
const EntityDef* find_entity_def(const std::vector<EntityDef>& defs,
                                  const std::string& id);
