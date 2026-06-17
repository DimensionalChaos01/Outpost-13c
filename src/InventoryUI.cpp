#include "InventoryUI.h"
#include "Renderer.h"
#include "TextQueue.h"
#include <algorithm>
#include <sstream>

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::vector<std::string> word_wrap(const std::string& text, int max_w, Renderer& rnd) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string word, line;
    while (iss >> word) {
        std::string test = line.empty() ? word : line + " " + word;
        if (rnd.text_width(test) > max_w) {
            if (!line.empty()) lines.push_back(line);
            line = word;
        } else {
            line = test;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void InventoryUI::rebuild_tabs(const PlayerInventory& inv,
                               const std::vector<ItemDef>& defs,
                               const std::vector<ItemCategory>& cats) {
    tabs_.clear();
    tabs_.push_back({"__all__", "All"});
    for (const auto& cat : cats) {
        for (const auto& entry : inv.get_all()) {
            const ItemDef* d = find_item_def(defs, entry.item_id);
            if (d && d->category == cat.id) {
                tabs_.push_back({cat.id, cat.display_name});
                break;
            }
        }
    }
    tab_idx_      = std::clamp(tab_idx_, 0, (int)tabs_.size()-1);
    selected_row_ = 0;
    scroll_       = 0;
}

std::vector<InventoryEntry> InventoryUI::visible_items(const PlayerInventory& inv,
                                                        const std::vector<ItemDef>& defs) const {
    if (tabs_.empty()) return {};
    const std::string& tid = tabs_[tab_idx_].id;
    if (tid == "__all__") return inv.get_all();
    return inv.get_by_category(tid, defs);
}

void InventoryUI::clamp_selection(int item_count) {
    if (item_count == 0) { selected_row_ = 0; scroll_ = 0; return; }
    selected_row_ = std::clamp(selected_row_, 0, item_count - 1);
    if (selected_row_ < scroll_) scroll_ = selected_row_;
    if (selected_row_ >= scroll_ + k_vis_rows) scroll_ = selected_row_ - k_vis_rows + 1;
}

// ── Public API ────────────────────────────────────────────────────────────────

void InventoryUI::open(const PlayerInventory& inv,
                       const std::vector<ItemDef>& defs,
                       const std::vector<ItemCategory>& cats) {
    tab_idx_      = 0;
    selected_row_ = 0;
    scroll_       = 0;
    drop_confirm_ = false;
    rebuild_tabs(inv, defs, cats);
}

InventoryUI::Action InventoryUI::handle_event(const SDL_Event& e,
                                               PlayerInventory& inv,
                                               const std::vector<ItemDef>& defs,
                                               TextQueue& tq,
                                               PlayerStats& stats) {
    if (e.type != SDL_KEYDOWN) return Action::None;
    const SDL_Keycode sym = e.key.keysym.sym;

    if (drop_confirm_) {
        if (sym == SDLK_y) {
            auto items = visible_items(inv, defs);
            if (selected_row_ < (int)items.size()) {
                const std::string& iid = items[selected_row_].item_id;
                const ItemDef* d = find_item_def(defs, iid);
                dropped_id_ = iid;
                inv.remove(iid, inv.get_quantity(iid));
                TextEntry te;
                te.text             = std::string("Dropped: ") + (d ? d->name : iid);
                te.voice_profile_id = "station_ambient";
                te.dismiss          = "auto";
                tq.push(te);
                drop_confirm_ = false;
                auto remaining = visible_items(inv, defs);
                clamp_selection((int)remaining.size());
                return Action::Dropped;
            }
            drop_confirm_ = false;
            auto remaining = visible_items(inv, defs);
            clamp_selection((int)remaining.size());
        } else {
            drop_confirm_ = false;
        }
        return Action::None;
    }

    auto items = visible_items(inv, defs);
    const int n = (int)items.size();

    if (sym == SDLK_ESCAPE) return Action::Close;

    if (sym == SDLK_q || sym == SDLK_LEFT || sym == SDLK_a) {
        if ((int)tabs_.size() > 1) {
            tab_idx_ = (tab_idx_ - 1 + (int)tabs_.size()) % (int)tabs_.size();
            selected_row_ = 0; scroll_ = 0;
        }
        return Action::None;
    }
    if (sym == SDLK_RIGHT || sym == SDLK_d) {
        if ((int)tabs_.size() > 1) {
            tab_idx_ = (tab_idx_ + 1) % (int)tabs_.size();
            selected_row_ = 0; scroll_ = 0;
        }
        return Action::None;
    }

    if (sym == SDLK_w || sym == SDLK_UP) {
        if (selected_row_ > 0) --selected_row_;
        clamp_selection(n);
        return Action::None;
    }
    if (sym == SDLK_s || sym == SDLK_DOWN) {
        if (selected_row_ < n - 1) ++selected_row_;
        clamp_selection(n);
        return Action::None;
    }

    if (n == 0) return Action::None;
    clamp_selection(n);
    const InventoryEntry& sel = items[selected_row_];
    const ItemDef* d = find_item_def(defs, sel.item_id);

    if (sym == SDLK_e) {
        if (d && d->use_effect.restore_hp > 0) {
            const int healed = std::min(d->use_effect.restore_hp, stats.hp_max - stats.hp_current);
            stats.hp_current += healed;
            TextEntry te;
            te.text             = std::string("Used ") + d->name + "."
                                  + (healed > 0 ? " +" + std::to_string(healed) + " HP." : " Already at full HP.");
            te.voice_profile_id = d->voice_profile.empty() ? "crew_log" : d->voice_profile;
            te.dismiss          = "auto";
            tq.push(te);
            inv.remove(sel.item_id, 1);
        } else {
            TextEntry te;
            te.text             = std::string("You can't use ") + (d ? d->name : sel.item_id) + " right now.";
            te.voice_profile_id = "station_ambient";
            te.dismiss          = "auto";
            tq.push(te);
        }
        auto remaining = visible_items(inv, defs);
        clamp_selection((int)remaining.size());
        return Action::Close;
    }

    if (sym == SDLK_r) { drop_confirm_ = true; return Action::None; }

    if (sym == SDLK_i) {
        TextEntry te;
        te.text             = d ? d->description : sel.item_id;
        te.narrator         = d ? d->name : "";
        te.voice_profile_id = (d && !d->voice_profile.empty()) ? d->voice_profile : "crew_log";
        te.dismiss          = "input";
        tq.push(te);
        return Action::Inspecting;
    }

    return Action::None;
}

// ── Render ────────────────────────────────────────────────────────────────────

// Draw a stat bar: label on left, "cur/max" on right, bar below.
static void draw_stat_bar(Renderer& rnd, int x, int y, int w,
                          const char* label, int cur, int max,
                          Uint8 fr, Uint8 fg, Uint8 fb) {
    rnd.draw_text(label, x, y, 160, 165, 180);
    const std::string val_str = std::to_string(cur) + " / " + std::to_string(max);
    rnd.draw_text(val_str, x + w - rnd.text_width(val_str), y, 170, 175, 190);
    y += 14;
    rnd.fill_rect(x, y, w, 10, 20, 22, 32);
    const int fill_w = max > 0 ? (int)((float)cur / max * w) : 0;
    if (fill_w > 0) rnd.fill_rect(x, y, fill_w, 10, fr, fg, fb);
}

void InventoryUI::render(Renderer& rnd,
                          const PlayerInventory& inv,
                          const std::vector<ItemDef>& defs,
                          const std::vector<ItemCategory>& cats,
                          const PlayerStats& stats,
                          const std::string& player_name) const {
    const int sw = rnd.screen_w(), sh = rnd.screen_h();

    constexpr int PAD  = 10;
    const int px = 60, py = 40;
    const int pw = sw - 120, ph = sh - 80;

    // ── Dark overlay + outer panel ────────────────────────────────────────────
    rnd.fill_rect(0, 0, sw, sh, 0, 0, 0, 180);
    rnd.fill_rect(px, py, pw, ph, 14, 18, 26, 248);
    rnd.draw_rect_outline(px, py, pw, ph, 60, 70, 90);

    // ── Left stats panel ──────────────────────────────────────────────────────
    const int spx = px, spw = k_stats_w;

    // Header bar (player name)
    rnd.fill_rect(spx, py, spw, 22, 32, 38, 52);
    {
        std::string nm = player_name.empty() ? "SMITH" : player_name;
        while (!nm.empty() && rnd.text_width(nm) > spw - 8) nm.pop_back();
        rnd.draw_text(nm, spx + PAD, py + 4, 210, 220, 235);
    }

    // Vertical separator
    rnd.draw_line(spx + spw, py + 1, spx + spw, py + ph - 1, 50, 58, 72);

    // Stats content
    int sy = py + 30;

    // HP bar
    {
        const float frac = stats.hp_max > 0 ? (float)stats.hp_current / stats.hp_max : 0.f;
        Uint8 r, g, b;
        if      (frac > 0.5f)  { r=50;  g=200; b=50;  }
        else if (frac > 0.25f) { r=220; g=180; b=20;  }
        else                   { r=220; g=40;  b=40;  }
        draw_stat_bar(rnd, spx+PAD, sy, spw-PAD*2, "HP", stats.hp_current, stats.hp_max, r, g, b);
        sy += 32;
    }

    // Sanity bar
    {
        const float frac = stats.sanity_max > 0 ? (float)stats.sanity_current / stats.sanity_max : 0.f;
        Uint8 r, g, b;
        if      (frac > 0.5f)  { r=180; g=200; b=230; }
        else if (frac > 0.25f) { r=140; g=60;  b=180; }
        else                   { r=160; g=20;  b=40;  }
        draw_stat_bar(rnd, spx+PAD, sy, spw-PAD*2, "SANITY", stats.sanity_current, stats.sanity_max, r, g, b);
        sy += 32;
    }

    // Value
    sy += 6;
    rnd.draw_text("VAL: " + std::to_string(stats.value), spx + PAD, sy, 180, 190, 150);
    sy += 24;

    // Raw stats list
    rnd.draw_line(spx + PAD, sy, spx + spw - PAD, sy, 40, 46, 58);
    sy += 8;

    struct StatRow { const char* label; int val; };
    const StatRow rows[] = {
        {"STR", stats.strength},
        {"AGI", stats.agility},
        {"DEF", stats.defense},
        {"PER", stats.perception},
        {"END", stats.endurance},
        {"FOR", stats.fortitude},
        {"DIS", stats.discipline},
    };
    for (const auto& row : rows) {
        rnd.draw_text(row.label, spx + PAD,    sy, 120, 125, 140);
        const std::string vs = std::to_string(row.val);
        rnd.draw_text(vs, spx + spw - PAD - rnd.text_width(vs), sy, 180, 185, 200);
        sy += 18;
    }

    // ── Right inventory panel ─────────────────────────────────────────────────
    const int rpx = px + spw + 1;   // +1 past separator line
    const int rpw = pw - spw - 1;

    // Header bar
    rnd.fill_rect(rpx, py, rpw, 22, 24, 30, 44);
    rnd.draw_text("INVENTORY", rpx + PAD, py + 4, 200, 210, 220);
    const std::string close_hint = "[Esc] Close";
    rnd.draw_text(close_hint, rpx + rpw - PAD - rnd.text_width(close_hint), py + 4, 80, 85, 100);

    // Tab bar
    const int tab_y  = py + 24;
    const int n_tabs = (int)tabs_.size();
    const int tab_w  = std::min(110, (rpw - 2) / std::max(1, n_tabs));
    for (int i = 0; i < n_tabs; ++i) {
        const bool active = (i == tab_idx_);
        const int  tx     = rpx + 1 + i * tab_w;
        rnd.fill_rect(tx, tab_y, tab_w - 1, k_tab_h,
                      active ? 40 : 22, active ? 55 : 28, active ? 80 : 40);
        if (active) rnd.draw_rect_outline(tx, tab_y, tab_w - 1, k_tab_h, 80, 120, 180);
        const std::string& name = tabs_[i].name;
        rnd.draw_text(name, tx + (tab_w - rnd.text_width(name)) / 2, tab_y + 7,
                      active ? 210 : 130, active ? 220 : 140, active ? 240 : 160);
    }

    // List area
    const int list_y = tab_y + k_tab_h + 4;
    const int list_h = ph - 24 - k_tab_h - 4 - k_detail_h - 4;

    rnd.fill_rect(rpx + 1, list_y, rpw - 2, list_h, 10, 13, 20);

    auto items = visible_items(inv, defs);
    const int n = (int)items.size();

    if (n == 0) {
        const char* msg = inv.empty() ? "Your inventory is empty." : "Nothing here.";
        rnd.draw_text(msg, rpx + PAD, list_y + 8, 80, 85, 100);
    } else {
        const int vis = std::min(k_vis_rows, list_h / k_row_h);
        for (int i = 0; i < vis; ++i) {
            const int  idx = scroll_ + i;
            if (idx >= n) break;
            const int  ry  = list_y + i * k_row_h + 4;
            const bool sel = (idx == selected_row_);
            if (sel) rnd.fill_rect(rpx + 1, ry - 2, rpw - 2, k_row_h, 28, 38, 60, 220);

            rnd.draw_text(sel ? ">" : " ", rpx + PAD, ry, 180, 200, 240);
            const ItemDef* d = find_item_def(defs, items[idx].item_id);
            rnd.draw_text(d ? d->name : items[idx].item_id, rpx + PAD + 14, ry,
                          sel?230:180, sel?235:185, sel?255:200);
            const std::string qty_str = "x" + std::to_string(items[idx].qty);
            rnd.draw_text(qty_str, rpx + rpw - PAD - rnd.text_width(qty_str), ry,
                          sel?180:120, sel?190:130, sel?210:150);
        }

        if (n > k_vis_rows) {
            const int bar_x   = rpx + rpw - 6;
            const int bar_h   = list_h - 4;
            const int thumb_h = std::max(10, bar_h * k_vis_rows / n);
            const int thumb_y = list_y + 2 + (bar_h - thumb_h) * scroll_ / std::max(1, n - k_vis_rows);
            rnd.fill_rect(bar_x, list_y + 2, 4, bar_h, 25, 30, 45);
            rnd.fill_rect(bar_x, thumb_y,    4, thumb_h, 70, 90, 130);
        }
    }

    // Detail panel
    const int det_y = list_y + list_h + 4;
    rnd.fill_rect(rpx + 1, det_y, rpw - 2, k_detail_h, 16, 20, 30);
    rnd.draw_line(rpx + 1, det_y, rpx + rpw - 2, det_y, 50, 60, 80);

    if (n > 0 && selected_row_ < n) {
        const InventoryEntry& sel = items[selected_row_];
        const ItemDef* d = find_item_def(defs, sel.item_id);
        const int dx = rpx + PAD, dw = rpw - PAD * 2;
        int dy = det_y + 6;

        rnd.draw_text(d ? d->name : sel.item_id, dx, dy, 230, 235, 255);
        dy += 18;

        if (d && !d->description.empty()) {
            auto lines = word_wrap(d->description, dw - 20, rnd);
            for (int li = 0; li < (int)lines.size() && li < 2; ++li) {
                rnd.draw_text(lines[li], dx, dy, 160, 165, 180);
                dy += 16;
            }
        }
        dy += 4;

        if (d && d->value > 0)
            rnd.draw_text("Value: " + std::to_string(d->value), dx, dy, 130, 160, 120);

        rnd.draw_text("E: Use    R: Drop    I: Inspect    Esc: Close",
                      dx, det_y + k_detail_h - 18, 70, 80, 100);
    } else {
        rnd.draw_text("E: Use    R: Drop    I: Inspect    Esc: Close",
                      rpx + PAD, det_y + k_detail_h - 18, 70, 80, 100);
    }

    // Drop confirmation overlay
    if (drop_confirm_ && n > 0 && selected_row_ < n) {
        const ItemDef* d = find_item_def(defs, items[selected_row_].item_id);
        const std::string item_name = d ? d->name : items[selected_row_].item_id;
        constexpr int dw = 340, dh = 60;
        const int dlx = (sw - dw) / 2, dly = (sh - dh) / 2;
        rnd.fill_rect(dlx, dly, dw, dh, 24, 28, 40, 252);
        rnd.draw_rect_outline(dlx, dly, dw, dh, 100, 80, 60);
        rnd.draw_text("Drop " + item_name + "?", dlx + 12, dly + 8, 220, 210, 190);
        rnd.draw_text("[Y] Yes        [N] No", dlx + 12, dly + 28, 180, 170, 150);
    }
}

std::string InventoryUI::selected_item_id(const PlayerInventory& inv,
                                            const std::vector<ItemDef>& defs) const {
    auto items = visible_items(inv, defs);
    if (selected_row_ < 0 || selected_row_ >= (int)items.size()) return {};
    return items[selected_row_].item_id;
}

// ── Mouse event handler ───────────────────────────────────────────────────────

InventoryUI::Action InventoryUI::handle_mouse(int mx, int my, int screen_w) {
    const int px  = 60, py = 40;
    const int pw  = screen_w - 120;
    const int rpx = px + k_stats_w + 1;
    const int rpw = pw - k_stats_w - 1;
    const int tab_y = py + 24;
    const int n = (int)tabs_.size();
    if (n == 0) return Action::None;
    const int tab_w = std::min(110, (rpw - 2) / n);
    if (my >= tab_y && my < tab_y + k_tab_h && mx >= rpx + 1 && tab_w > 0) {
        const int clicked = (mx - (rpx + 1)) / tab_w;
        if (clicked >= 0 && clicked < n) {
            tab_idx_ = clicked;
            selected_row_ = 0;
            scroll_ = 0;
        }
    }
    return Action::None;
}
