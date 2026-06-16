#include "ItemDef.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

void load_item_categories(const std::string& path, std::vector<ItemCategory>& out) {
    out.clear();
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "ItemDef: cannot open categories " << path << "\n";
        return;
    }
    json data;
    try { f >> data; } catch (const json::exception& e) {
        std::cerr << "ItemDef: categories JSON error: " << e.what() << "\n";
        return;
    }
    for (const auto& c : data) {
        ItemCategory cat;
        cat.id           = c.value("id",           "");
        cat.display_name = c.value("display_name", "");
        cat.icon         = c.value("icon",         "");
        if (!cat.id.empty()) out.push_back(cat);
    }
    std::cout << "ItemDef: loaded " << out.size() << " categories\n";
}

void load_items(const std::string& dir, std::vector<ItemDef>& out) {
    out.clear();
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        std::cerr << "ItemDef: items dir not found: " << dir << "\n";
        return;
    }
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f.is_open()) continue;
        json data;
        try { f >> data; } catch (...) {
            std::cerr << "ItemDef: JSON error in " << entry.path() << "\n";
            continue;
        }
        ItemDef item;
        item.id           = data.value("id",           "");
        item.name         = data.value("name",         "");
        item.category     = data.value("category",     "");
        item.description  = data.value("description",  "");
        item.pickup_text  = data.value("pickup_text",  "");
        item.value        = data.value("value",        0);
        item.stackable    = data.value("stackable",    false);
        item.max_stack    = data.value("max_stack",    1);
        item.voice_profile= data.value("voice_profile","");
        if (data.contains("use_effect") && data["use_effect"].is_object()) {
            item.use_effect.restore_hp = data["use_effect"].value("restore_hp", 0);
        }
        if (data.contains("tags") && data["tags"].is_array()) {
            for (const auto& t : data["tags"])
                if (t.is_string()) item.tags.push_back(t.get<std::string>());
        }
        if (!item.id.empty()) out.push_back(item);
    }
    std::cout << "ItemDef: loaded " << out.size() << " items\n";
}

const ItemDef* find_item_def(const std::vector<ItemDef>& defs, const std::string& id) {
    for (const auto& d : defs)
        if (d.id == id) return &d;
    return nullptr;
}

const ItemCategory* find_item_category(const std::vector<ItemCategory>& cats, const std::string& id) {
    for (const auto& c : cats)
        if (c.id == id) return &c;
    return nullptr;
}
