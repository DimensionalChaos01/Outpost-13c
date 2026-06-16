#pragma once
#include <string>
#include <vector>
#include <map>

struct ItemCategory {
    std::string id;
    std::string display_name;
    std::string icon;
};

struct UseEffect {
    int restore_hp = 0;
    // Additional effect fields added here as needed.
};

struct ItemDef {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::string pickup_text;
    int         value      = 0;
    bool        stackable  = false;
    int         max_stack  = 1;
    std::string voice_profile;
    UseEffect   use_effect;
    std::vector<std::string> tags;
};

void load_item_categories(const std::string& path, std::vector<ItemCategory>& out);
void load_items(const std::string& dir, std::vector<ItemDef>& out);

const ItemDef*      find_item_def(const std::vector<ItemDef>& defs, const std::string& id);
const ItemCategory* find_item_category(const std::vector<ItemCategory>& cats, const std::string& id);
