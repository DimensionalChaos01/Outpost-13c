#pragma once
#include <string>
#include <vector>
#include <random>

struct LootEntry {
    std::string item_id;
    int         weight = 1;
};

struct LootTable {
    std::string            id;
    int                    rolls = 1;
    std::vector<LootEntry> entries;
};

void load_loot_tables(const std::string& dir, std::vector<LootTable>& out);

const LootTable* find_loot_table(const std::vector<LootTable>& tables, const std::string& id);

// Roll a loot table, returning a list of item_ids (may include "nothing").
std::vector<std::string> roll_loot(const LootTable& table, std::mt19937& rng);
