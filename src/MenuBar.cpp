#include "MenuBar.h"
#include "Renderer.h"
#include <algorithm>
#include <cstring>

// ── Constructor — build menu structure ───────────────────────────────────────

MenuBar::MenuBar() {
    auto sep = []() -> MenuItem { return {"","","",true,true}; };

    menus_.push_back({"File", {
        {"New Map",    "Ctrl+N", "file.new"},
        {"Open Map",   "Ctrl+O", "file.open"},
        {"Save",       "Ctrl+S", "file.save"},
        {"Save As",    "",       "file.save_as"},
        sep(),
        {"Resize Map", "",       "file.resize"},
    }});

    menus_.push_back({"Edit", {
        {"Undo",            "Ctrl+Z", "edit.undo",   false},
        {"Redo",            "Ctrl+Y", "edit.redo",   false},
        sep(),
        {"Select All",      "Ctrl+A", "edit.select_all", false},
        {"Copy",            "Ctrl+C", "edit.copy",   false},
        {"Paste",           "Ctrl+V", "edit.paste",  false},
        {"Delete",          "Del",    "edit.delete", false},
    }});

    menus_.push_back({"View", {
        {"Pathability",     "P",      "view.path"},
        {"Rooms",           "O",      "view.rooms"},
        {"Light",           "L",      "view.light"},
        {"Grid",            "G",      "view.grid"},
        sep(),
        {"Zoom In",         "Ctrl++", "view.zoom_in"},
        {"Zoom Out",        "Ctrl+-", "view.zoom_out"},
        {"Reset Zoom",      "Ctrl+0", "view.zoom_reset"},
        sep(),
        {"Day/Night Preview","N",     "view.daynight", false},
        {"Ghost Floor",     "",       "view.ghost",    false},
    }});

    menus_.push_back({"Map", {
        {"Map Metadata",    "",  "map.metadata", false},
        {"Validate Map",    "",  "map.validate", false},
        sep(),
        {"Play Test",       "F5","map.play"},
    }});

    menus_.push_back({"Help", {
        {"Keyboard Shortcuts", "", "help.shortcuts"},
    }});

    // Compute bar x positions (8px left pad, ~10px/char estimate + 16px padding).
    int x = 8;
    for (auto& m : menus_) {
        m.bar_x = x;
        m.bar_w = (int)m.label.size() * 8 + 20;
        x += m.bar_w;
    }
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

int MenuBar::bar_menu_at(int x, int y) const {
    if (y < 0 || y >= k_bar_h) return -1;
    for (int i = 0; i < (int)menus_.size(); ++i) {
        const auto& m = menus_[i];
        if (x >= m.bar_x && x < m.bar_x + m.bar_w)
            return i;
    }
    return -1;
}

int MenuBar::drop_x(int menu_idx, int screen_w) const {
    int x = menus_[menu_idx].bar_x;
    // Clamp so dropdown doesn't spill off the right edge.
    return std::min(x, screen_w - k_drop_w);
}

int MenuBar::drop_h(int menu_idx) const {
    int h = 4;
    for (const auto& item : menus_[menu_idx].items)
        h += item.is_sep ? k_sep_h : k_item_h;
    return h + 4;
}

int MenuBar::drop_item_at(int menu_idx, int x, int y) const {
    const int dx = drop_x(menu_idx, 9999);
    if (x < dx || x >= dx + k_drop_w) return -1;

    int iy = k_bar_h + 4;
    const auto& items = menus_[menu_idx].items;
    for (int i = 0; i < (int)items.size(); ++i) {
        if (items[i].is_sep) { iy += k_sep_h; continue; }
        if (y >= iy && y < iy + k_item_h) return i;
        iy += k_item_h;
    }
    return -1;
}

// ── Event handling ────────────────────────────────────────────────────────────

MenuEvent MenuBar::handle_event(const SDL_Event& e, int /*screen_w*/) {
    MenuEvent result;

    if (e.type == SDL_MOUSEMOTION) {
        const int mx = e.motion.x, my = e.motion.y;
        const int bar_hit = bar_menu_at(mx, my);

        if (open_ >= 0) {
            if (bar_hit >= 0 && bar_hit != open_) {
                open_  = bar_hit;
                hover_ = -1;
            } else if (open_ >= 0) {
                hover_ = drop_item_at(open_, mx, my);
            }
            result.consumed = true;
        } else if (bar_hit >= 0) {
            result.consumed = true;
        }
        return result;
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        const int mx = e.button.x, my = e.button.y;
        const int bar_hit = bar_menu_at(mx, my);

        if (bar_hit >= 0) {
            open_  = (open_ == bar_hit) ? -1 : bar_hit;
            hover_ = -1;
            result.consumed = true;
            return result;
        }

        if (open_ >= 0) {
            const int item = drop_item_at(open_, mx, my);
            if (item >= 0) {
                const MenuItem& mi = menus_[open_].items[item];
                if (mi.enabled && !mi.is_sep) {
                    result.action = mi.action;
                }
                close();
                result.consumed = true;
                return result;
            }
            // Click outside open menu — close it.
            if (my > k_bar_h) {
                close();
                // Don't consume — let the editor handle the click.
            }
        }
        return result;
    }

    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE && open_ >= 0) {
        close();
        result.consumed = true;
        return result;
    }

    // Consume any event that lands in the bar area.
    if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        const int my = e.button.y;
        if (my >= 0 && my < k_bar_h) result.consumed = true;
    }

    return result;
}

// ── Drawing ───────────────────────────────────────────────────────────────────

void MenuBar::draw(Renderer& r, int screen_w) const {
    // Bar background.
    r.fill_rect(0, 0, screen_w, k_bar_h, 22, 26, 32);
    r.draw_line(0, k_bar_h - 1, screen_w, k_bar_h - 1, 45, 52, 62);

    for (int i = 0; i < (int)menus_.size(); ++i) {
        const auto& m = menus_[i];
        const bool active = (open_ == i);

        if (active) {
            r.fill_rect(m.bar_x, 0, m.bar_w, k_bar_h, 50, 58, 72);
        }
        r.draw_text(m.label, m.bar_x + 8, 5,
                    active ? 220 : 180, active ? 225 : 185, active ? 230 : 190);

        if (!active) continue;

        // Dropdown background.
        const int dx = drop_x(i, screen_w);
        const int dh = drop_h(i);
        r.fill_rect(dx, k_bar_h, k_drop_w, dh, 28, 32, 40);
        r.draw_rect_outline(dx, k_bar_h, k_drop_w, dh, 55, 62, 74);

        int iy = k_bar_h + 4;
        for (int j = 0; j < (int)m.items.size(); ++j) {
            const auto& item = m.items[j];
            if (item.is_sep) {
                r.draw_line(dx + 6, iy + k_sep_h/2,
                            dx + k_drop_w - 6, iy + k_sep_h/2, 50, 56, 68);
                iy += k_sep_h;
                continue;
            }

            const bool hov = (hover_ == j);
            if (hov && item.enabled)
                r.fill_rect(dx + 2, iy, k_drop_w - 4, k_item_h, 55, 80, 130);

            const Uint8 lr = item.enabled ? (hov ? 230 : 200) : 90;
            const Uint8 lg = item.enabled ? (hov ? 232 : 202) : 92;
            const Uint8 lb = item.enabled ? (hov ? 236 : 208) : 96;
            r.draw_text(item.label, dx + 10, iy + 4, lr, lg, lb);

            if (!item.shortcut.empty()) {
                // Right-align shortcut text.
                const int tx = dx + k_drop_w - (int)item.shortcut.size() * 7 - 8;
                r.draw_text(item.shortcut, tx, iy + 4, 110, 115, 125);
            }
            iy += k_item_h;
        }
    }
}
