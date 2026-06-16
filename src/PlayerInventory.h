#pragma once
#include <string>
#include <vector>
#include <map>

struct InventoryEntry {
    std::string item_id;
    int         qty = 0;
};

class PlayerInventory {
public:
    void add(const std::string& item_id, int qty = 1);
    bool remove(const std::string& item_id, int qty = 1);
    bool has(const std::string& item_id) const;
    int  get_quantity(const std::string& item_id) const;

    std::vector<InventoryEntry> get_all()                             const;
    std::vector<InventoryEntry> get_by_category(
        const std::string& category_id,
        const std::vector<struct ItemDef>& defs)                     const;

    bool empty() const { return items_.empty(); }
    void clear()       { items_.clear(); }

private:
    std::map<std::string, int> items_;
};
