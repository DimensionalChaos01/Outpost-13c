#pragma once
#include <string>
#include <array>

class Hotbar {
public:
    static constexpr int k_item_slots    = 10;
    static constexpr int k_ability_slots = 5;

    // Item hotbar — 1-9 maps to slots 0-8, 0 maps to slot 9.
    std::array<std::string, k_item_slots>    item_slots    = {};
    // Ability hotbar — cycled with scroll wheel; content TBD per setting.
    std::array<std::string, k_ability_slots> ability_slots = {};

    int active_item_slot    = -1;  // -1 = nothing selected
    int active_ability_slot = 0;

    void set_active_item(int slot);                       // 0-9; -1 deselects
    void assign_item(int slot, const std::string& item_id);
    void clear_item_slot(int slot);
    const std::string& active_item_id() const;

    void cycle_ability(int dir);                          // +1 = next, -1 = prev
};
