#include "Hotbar.h"

static const std::string k_empty;

void Hotbar::set_active_item(int slot) {
    if (slot < -1 || slot >= k_item_slots) return;
    active_item_slot = slot;
}

void Hotbar::assign_item(int slot, const std::string& item_id) {
    if (slot < 0 || slot >= k_item_slots) return;
    item_slots[slot] = item_id;
}

void Hotbar::clear_item_slot(int slot) {
    if (slot < 0 || slot >= k_item_slots) return;
    item_slots[slot].clear();
}

const std::string& Hotbar::active_item_id() const {
    if (active_item_slot < 0 || active_item_slot >= k_item_slots) return k_empty;
    return item_slots[active_item_slot];
}

void Hotbar::cycle_ability(int dir) {
    active_ability_slot = (active_ability_slot + dir + k_ability_slots) % k_ability_slots;
}
