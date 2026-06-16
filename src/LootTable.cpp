#include "LootTable.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

void load_loot_tables(const std::string& dir, std::vector<LootTable>& out) {
    out.clear();
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f.is_open()) continue;
        json data;
        try { f >> data; } catch (...) {
            std::cerr << "LootTable: JSON error in " << entry.path() << "\n";
            continue;
        }
        LootTable t;
        t.id    = data.value("id",    "");
        t.rolls = data.value("rolls", 1);
        if (data.contains("entries") && data["entries"].is_array()) {
            for (const auto& e : data["entries"]) {
                LootEntry le;
                le.item_id = e.value("item_id", "nothing");
                le.weight  = e.value("weight",  1);
                if (le.weight > 0) t.entries.push_back(le);
            }
        }
        if (!t.id.empty()) out.push_back(t);
    }
    std::cout << "LootTable: loaded " << out.size() << " tables\n";
}

const LootTable* find_loot_table(const std::vector<LootTable>& tables, const std::string& id) {
    for (const auto& t : tables)
        if (t.id == id) return &t;
    return nullptr;
}

std::vector<std::string> roll_loot(const LootTable& table, std::mt19937& rng) {
    std::vector<std::string> result;
    if (table.entries.empty()) return result;
    int total = 0;
    for (const auto& e : table.entries) total += e.weight;
    if (total <= 0) return result;
    for (int i = 0; i < table.rolls; ++i) {
        int roll = (int)(rng() % (unsigned)total);
        for (const auto& e : table.entries) {
            roll -= e.weight;
            if (roll < 0) { result.push_back(e.item_id); break; }
        }
    }
    return result;
}
