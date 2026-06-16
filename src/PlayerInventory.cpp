#include "PlayerInventory.h"
#include "ItemDef.h"
#include <algorithm>

void PlayerInventory::add(const std::string& item_id, int qty) {
    if (qty <= 0) return;
    items_[item_id] += qty;
}

bool PlayerInventory::remove(const std::string& item_id, int qty) {
    auto it = items_.find(item_id);
    if (it == items_.end() || it->second < qty) return false;
    it->second -= qty;
    if (it->second <= 0) items_.erase(it);
    return true;
}

bool PlayerInventory::has(const std::string& item_id) const {
    auto it = items_.find(item_id);
    return it != items_.end() && it->second > 0;
}

int PlayerInventory::get_quantity(const std::string& item_id) const {
    auto it = items_.find(item_id);
    return (it != items_.end()) ? it->second : 0;
}

std::vector<InventoryEntry> PlayerInventory::get_all() const {
    std::vector<InventoryEntry> result;
    result.reserve(items_.size());
    for (const auto& kv : items_)
        result.push_back({kv.first, kv.second});
    return result;
}

std::vector<InventoryEntry> PlayerInventory::get_by_category(
    const std::string& category_id,
    const std::vector<ItemDef>& defs) const {
    std::vector<InventoryEntry> result;
    for (const auto& kv : items_) {
        const ItemDef* def = find_item_def(defs, kv.first);
        if (def && def->category == category_id)
            result.push_back({kv.first, kv.second});
    }
    return result;
}
