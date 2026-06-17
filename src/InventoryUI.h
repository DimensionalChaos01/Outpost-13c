#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "ItemDef.h"
#include "PlayerInventory.h"
#include "Player.h"

class Renderer;
class TextQueue;

class InventoryUI {
public:
    enum class Action { None, Close, Inspecting, Dropped };

    void open(const PlayerInventory& inv,
              const std::vector<ItemDef>& defs,
              const std::vector<ItemCategory>& cats);

    Action handle_event(const SDL_Event& e,
                        PlayerInventory& inv,
                        const std::vector<ItemDef>& defs,
                        TextQueue& tq,
                        PlayerStats& stats);

    // Hit-test tab bar; call on SDL_MOUSEBUTTONDOWN when inventory is open.
    Action handle_mouse(int mx, int my, int screen_w);

    const std::string& last_dropped_id() const { return dropped_id_; }

    // Returns item_id of the currently highlighted row, or "" if none.
    std::string selected_item_id(const PlayerInventory& inv,
                                 const std::vector<ItemDef>& defs) const;

    void render(Renderer& rnd,
                const PlayerInventory& inv,
                const std::vector<ItemDef>& defs,
                const std::vector<ItemCategory>& cats,
                const PlayerStats& stats,
                const std::string& player_name) const;

private:
    struct Tab { std::string id; std::string name; };

    void rebuild_tabs(const PlayerInventory& inv,
                      const std::vector<ItemDef>& defs,
                      const std::vector<ItemCategory>& cats);

    std::vector<InventoryEntry> visible_items(const PlayerInventory& inv,
                                              const std::vector<ItemDef>& defs) const;

    void clamp_selection(int item_count);

    std::vector<Tab> tabs_;
    int         tab_idx_      = 0;
    int         selected_row_ = 0;
    int         scroll_       = 0;
    bool        drop_confirm_ = false;
    std::string dropped_id_;

    static constexpr int k_row_h    = 22;
    static constexpr int k_tab_h    = 28;
    static constexpr int k_detail_h = 110;
    static constexpr int k_vis_rows = 14;
    static constexpr int k_stats_w  = 170;  // left stats panel width
};
