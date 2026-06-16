#include "SettingsPanel.h"
#include "Renderer.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

// ── Keybind row table ─────────────────────────────────────────────────────────

const std::vector<SettingsPanel::KbRow>& SettingsPanel::kb_rows() {
    static const std::vector<KbRow> rows = {
        {"","-- Movement --"},
        {"move_up",    "Move Up"},
        {"move_down",  "Move Down"},
        {"move_left",  "Move Left"},
        {"move_right", "Move Right"},
        {"","-- Gameplay --"},
        {"pause",      "Pause"},
        {"interact",   "Interact"},
        {"inventory",  "Inventory"},
        {"","-- Editor --"},
        {"editor_paint",           "Tile Paint (return to brush)"},
        {"editor_erase",           "Erase"},
        {"editor_eyedropper",      "Eyedropper"},
        {"editor_fill",            "Flood Fill"},
        {"editor_entity",          "Entity Panel"},
        {"editor_save",            "Save"},
        {"editor_undo",            "Undo"},
        {"editor_redo",            "Redo"},
        {"editor_toggle_grid",     "Toggle Grid"},
        {"editor_toggle_path",     "Toggle Pathability"},
        {"editor_toggle_entities", "Toggle Entities"},
        {"editor_play_test",       "Play Test"},
        {"","-- Texture --"},
        {"editor_texture_picker",  "Open Texture Picker  (\\)"},
        {"editor_texture_mode",    "Toggle Texture Paint Mode"},
        {"","-- Playtest --"},
        {"playtest_return_to_editor", "Return to Editor"},
    };
    return rows;
}

// ── Open / Close ──────────────────────────────────────────────────────────────

void SettingsPanel::open(const AppSettings& current) {
    working_      = current;
    tab_          = Tab::Display;
    drag_slider_  = -1;
    rebinding_    = false;
    rebind_idx_   = -1;
    conflict_open_= false;
    kb_scroll_    = 0;
    open_         = true;
}
void SettingsPanel::close() { open_ = false; }

int SettingsPanel::px(Renderer& r) const { return (r.screen_w() - W) / 2; }
int SettingsPanel::py(Renderer& r) const { return (r.screen_h() - H) / 2; }

// ── Events ────────────────────────────────────────────────────────────────────

SettingsPanel::Result SettingsPanel::handle_event(const SDL_Event& e, Renderer& renderer) {
    if (!open_) return Result::None;

    const int spx = px(renderer), spy = py(renderer);

    // ── Conflict dialog ───────────────────────────────────────────────────────
    if (conflict_open_) {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            conflict_open_ = false; return Result::None;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx=e.button.x, my=e.button.y;
            constexpr int CW=320,CH=130; const int cx=(renderer.screen_w()-CW)/2, cy=(renderer.screen_h()-CH)/2;
            if (mx>=cx+10 && mx<cx+140 && my>=cy+CH-44 && my<cy+CH-12) {
                // Reassign: clear the old binding and set new
                const auto& rows = kb_rows();
                int real_idx = 0;
                for (const auto& row : rows) {
                    if (row.id.empty()) continue;
                    if (real_idx == conflict_target_) {
                        for (auto& [k,v] : working_.keybinds) if (v == conflict_key_) v = "";
                        working_.keybinds[row.id] = conflict_key_;
                        break;
                    }
                    ++real_idx;
                }
                conflict_open_ = false; rebinding_ = false;
            } else if (mx>=cx+CW-140 && mx<cx+CW-10 && my>=cy+CH-44 && my<cy+CH-12) {
                conflict_open_ = false; // Cancel
            }
        }
        return Result::None;
    }

    // ── Rebind mode: capture key ──────────────────────────────────────────────
    if (rebinding_ && e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) { rebinding_ = false; return Result::None; }
        const char* key_name = SDL_GetKeyName(e.key.keysym.sym);
        if (!key_name || std::strcmp(key_name,"Unknown Key")==0) return Result::None;
        const std::string new_key = key_name;

        // Find the row's action id
        const auto& rows = kb_rows();
        int real_idx = 0;
        std::string action_id;
        for (const auto& row : rows) {
            if (row.id.empty()) continue;
            if (real_idx == rebind_idx_) { action_id = row.id; break; }
            ++real_idx;
        }
        if (action_id.empty()) { rebinding_ = false; return Result::None; }

        // Check for conflict
        for (const auto& [k, v] : working_.keybinds) {
            if (v == new_key && k != action_id) {
                conflict_key_    = new_key;
                conflict_other_  = k;
                conflict_target_ = rebind_idx_;
                conflict_open_   = true;
                rebinding_       = false;
                return Result::None;
            }
        }
        working_.keybinds[action_id] = new_key;
        rebinding_ = false;
        return Result::None;
    }

    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        close(); return Result::Close;
    }

    // ── Mouse wheel: keybind scroll ───────────────────────────────────────────
    if (e.type == SDL_MOUSEWHEEL && tab_ == Tab::Keybinds) {
        kb_scroll_ -= e.wheel.y * ROW_H;
        kb_scroll_ = std::max(0, kb_scroll_);
        return Result::None;
    }

    // ── Slider drag ───────────────────────────────────────────────────────────
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        drag_slider_ = -1; // reset
    }
    if (drag_slider_ >= 0) {
        if (e.type == SDL_MOUSEMOTION) {
            const int sw = W - 40;
            const int dx = e.motion.x - drag_base_x_;
            int val = drag_base_val_ + dx * 100 / sw;
            val = std::max(0, std::min(100, val));
            switch (drag_slider_) {
                case 0: working_.vol_master    = val; break;
                case 1: working_.vol_music     = val; break;
                case 2: working_.vol_sfx       = val; break;
                case 3: working_.vol_ambient   = val; break;
                case 4: working_.cam_follow_speed = 2.0f + val * 18.0f / 100.0f; break;
            }
            return Result::None;
        }
        if (e.type == SDL_MOUSEBUTTONUP) { drag_slider_ = -1; return Result::None; }
    }

    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT)
        return Result::None;

    const int mx = e.button.x, my = e.button.y;

    // ── Tab clicks ────────────────────────────────────────────────────────────
    {
        const int ty = spy + HDR_H;
        const int tw = W / 3;
        if (my >= ty && my < ty + TAB_H) {
            if (mx >= spx       && mx < spx+tw)      { tab_ = Tab::Display;  return Result::None; }
            if (mx >= spx+tw    && mx < spx+tw*2)    { tab_ = Tab::Audio;    return Result::None; }
            if (mx >= spx+tw*2  && mx < spx+W)       { tab_ = Tab::Keybinds; return Result::None; }
        }
    }

    const int bx = spx + 10, by_content = spy + HDR_H + TAB_H + 8;
    const int bw = W - 20, bh = H - HDR_H - TAB_H - 8 - 50;

    // ── Apply / Cancel ────────────────────────────────────────────────────────
    {
        const int btn_y = spy + H - 44;
        if (my >= btn_y && my < btn_y + 32) {
            if (mx >= spx+10 && mx < spx+10+120) { close(); return Result::Apply; }
            if (mx >= spx+W-130 && mx < spx+W-10) { close(); return Result::Close; }
        }
    }

    // ── Display tab ───────────────────────────────────────────────────────────
    if (tab_ == Tab::Display) {
        int y = by_content + 8;
        // Fullscreen toggle
        y += ROW_H;
        if (my >= y-2 && my < y+ROW_H-2 && mx >= bx+10 && mx < bx+24) {
            working_.fullscreen = !working_.fullscreen; return Result::None;
        }
        y += ROW_H;
        if (my >= y-2 && my < y+ROW_H-2 && mx >= bx+10 && mx < bx+24) {
            working_.vsync = !working_.vsync; return Result::None;
        }
        y += ROW_H;
        if (my >= y-2 && my < y+ROW_H-2 && mx >= bx+10 && mx < bx+24) {
            working_.show_grid = !working_.show_grid; return Result::None;
        }

        // Camera section (matches render_display layout)
        y += 8; y += 16; y += 6; // header + label + divider

        // Smooth recentre toggle
        y += ROW_H;
        if (my >= y-2 && my < y+ROW_H-2 && mx >= bx+10 && mx < bx+24) {
            working_.cam_smooth_follow = !working_.cam_smooth_follow; return Result::None;
        }

        // Default zoom cycle (< >)
        y += ROW_H;
        static constexpr float k_zs[] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f};
        static constexpr int   k_nz   = 6;
        if (my >= y-2 && my < y+ROW_H-2) {
            int zi = 2;
            for (int i = 0; i < k_nz; ++i)
                if (std::abs(k_zs[i] - working_.cam_default_zoom) < 0.01f) { zi = i; break; }
            if (mx >= bx+168 && mx < bx+176) { // "<"
                working_.cam_default_zoom = k_zs[std::max(0, zi-1)]; return Result::None;
            }
            if (mx >= bx+176 && mx < bx+190) { // ">"
                working_.cam_default_zoom = k_zs[std::min(k_nz-1, zi+1)]; return Result::None;
            }
        }

        // Follow speed slider
        y += ROW_H;
        const int spd_x = bx+130, spd_w = bw-150;
        if (my >= y-1 && my < y+12 && mx >= spd_x && mx < spd_x+spd_w) {
            drag_slider_   = 4;
            drag_base_x_   = mx;
            drag_base_val_ = (int)((working_.cam_follow_speed - 2.0f) / 18.0f * 100.0f);
            return Result::None;
        }
    }

    // ── Audio tab: slider clicks ──────────────────────────────────────────────
    if (tab_ == Tab::Audio) {
        const int sx = bx + 140, sw2 = bw - 160;
        int y = by_content + 8;
        for (int i = 0; i < 4; ++i) {
            y += ROW_H;
            if (my >= y-2 && my < y+ROW_H && mx >= sx && mx < sx+sw2) {
                drag_slider_ = i;
                drag_base_x_ = mx;
                int* vol = nullptr;
                switch(i) {
                    case 0: vol=&working_.vol_master;  break;
                    case 1: vol=&working_.vol_music;   break;
                    case 2: vol=&working_.vol_sfx;     break;
                    case 3: vol=&working_.vol_ambient; break;
                }
                drag_base_val_ = *vol;
                return Result::None;
            }
            y += ROW_H;
        }
    }

    // ── Keybinds tab: click row to rebind ─────────────────────────────────────
    if (tab_ == Tab::Keybinds) {
        const int list_y = by_content + 4;
        const int list_h = bh - 8;
        const auto& rows = kb_rows();
        int y = list_y - kb_scroll_;
        int real_idx = 0;
        for (const auto& row : rows) {
            if (row.id.empty()) { y += ROW_H; continue; }
            if (y >= list_y && y < list_y + list_h) {
                if (my >= y && my < y+ROW_H && mx >= bx && mx < bx+bw) {
                    rebinding_  = true;
                    rebind_idx_ = real_idx;
                    return Result::None;
                }
            }
            y += ROW_H;
            ++real_idx;
        }
        // Reset all bindings
        if (my >= by_content + bh - 2 && my < by_content + bh + 24 && mx >= bx) {
            working_.keybinds = AppSettings::default_keybinds();
            return Result::None;
        }
    }

    return Result::None;
}

// ── Rendering helpers ─────────────────────────────────────────────────────────

void SettingsPanel::render_display(Renderer& r, int bx, int by, int bw, int /*bh*/) const {
    int y = by + 8;

    auto draw_toggle = [&](const char* label, bool val) {
        y += ROW_H;
        r.fill_rect(bx+10, y-2, 14, 14, val?20:25, val?130:35, val?55:45);
        r.draw_rect_outline(bx+10, y-2, 14, 14, 70,85,100);
        if (val) {
            r.draw_line(bx+12, y+5, bx+15, y+9, 80,210,80);
            r.draw_line(bx+15, y+9, bx+22, y+2, 80,210,80);
        }
        r.draw_text(label, bx+30, y-2, 180,185,200);
    };

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Resolution:  %dx%d", working_.res_w, working_.res_h);
    r.draw_text(buf, bx+10, y, 160,165,180);

    draw_toggle("Fullscreen",         working_.fullscreen);
    draw_toggle("VSync",              working_.vsync);
    draw_toggle("Show Grid (editor)", working_.show_grid);

    // Camera section
    y += 8;
    r.draw_text("Camera",bx+10,y,120,150,190); y+=16;
    r.draw_line(bx+4,y,bx+bw-4,y,45,50,60); y+=6;

    // Smooth recentre toggle
    y += ROW_H;
    r.fill_rect(bx+10,y-2,14,14, working_.cam_smooth_follow?20:25, working_.cam_smooth_follow?130:35, working_.cam_smooth_follow?55:45);
    r.draw_rect_outline(bx+10,y-2,14,14,70,85,100);
    if(working_.cam_smooth_follow){r.draw_line(bx+12,y+5,bx+15,y+9,80,210,80);r.draw_line(bx+15,y+9,bx+22,y+2,80,210,80);}
    r.draw_text("Smooth Recentre",bx+30,y-2,180,185,200);

    // Default zoom cycle
    y += ROW_H;
    static constexpr float k_zs[] = {0.5f,0.75f,1.0f,1.5f,2.0f,3.0f};
    static constexpr const char* k_zn[] = {"50%","75%","100%","150%","200%","300%"};
    int zi = 2;
    for (int i=0;i<6;++i) if(std::abs(k_zs[i]-working_.cam_default_zoom)<0.01f){zi=i;break;}
    r.draw_text("Default Zoom:",bx+10,y-2,180,185,200);
    r.fill_rect(bx+120,y-4,60,18,28,36,52);
    r.draw_rect_outline(bx+120,y-4,60,18,55,65,90);
    r.draw_text(k_zn[zi],bx+127,y-2,170,180,210);
    r.draw_text("< >",bx+168,y-2,80,95,130);

    // Follow speed slider
    y += ROW_H;
    r.draw_text("Recentre Speed:",bx+10,y-2,180,185,200);
    const int spd_x = bx+130, spd_w = bw-150;
    r.fill_rect(spd_x,y+2,spd_w,6,30,36,48);
    r.draw_rect_outline(spd_x,y+2,spd_w,6,55,65,85);
    const int spd_fill = (int)((working_.cam_follow_speed - 2.0f) / 18.0f * spd_w);
    r.fill_rect(spd_x,y+2,std::max(0,spd_fill),6,60,130,210);
    const int hx=spd_x+spd_fill-4;
    r.fill_rect(hx,y-1,8,12,160,200,240);
}

void SettingsPanel::render_audio(Renderer& r, int bx, int by, int bw, int /*bh*/) const {
    const int sx = bx + 140, sw = bw - 160;
    int y = by + 8;

    auto draw_slider = [&](const char* label, int val) {
        y += ROW_H;
        r.draw_text(label, bx+10, y, 170,175,190);
        // Track
        r.fill_rect(sx, y+4, sw, 6, 30,36,48);
        r.draw_rect_outline(sx, y+4, sw, 6, 55,65,85);
        // Fill
        const int fill_w = val * sw / 100;
        r.fill_rect(sx, y+4, fill_w, 6, 60,130,210);
        // Handle
        const int hx = sx + fill_w - 4;
        r.fill_rect(hx, y+1, 8, 12, 160,200,240);
        // Value
        char vb[8]; std::snprintf(vb, sizeof(vb), "%d", val);
        r.draw_text(vb, sx+sw+8, y, 140,145,160);
        y += ROW_H;
    };

    draw_slider("Master",  working_.vol_master);
    draw_slider("Music",   working_.vol_music);
    draw_slider("SFX",     working_.vol_sfx);
    draw_slider("Ambient", working_.vol_ambient);
}

void SettingsPanel::render_keybinds(Renderer& r, int bx, int by, int bw, int bh) const {
    const int list_y = by + 4, list_h = bh - 4;
    const auto& rows = kb_rows();
    int y = list_y - kb_scroll_;
    int real_idx = 0;

    for (const auto& row : rows) {
        if (row.id.empty()) {
            // Section header
            if (y >= list_y && y < list_y+list_h) {
                r.fill_rect(bx, y, bw, ROW_H-2, 28,34,46);
                r.draw_text(row.name.c_str(), bx+6, y+6, 120,150,190);
            }
            y += ROW_H; continue;
        }

        if (y >= list_y && y < list_y+list_h) {
            const bool rebind_this = rebinding_ && rebind_idx_ == real_idx;
            if (rebind_this) r.fill_rect(bx, y, bw, ROW_H-1, 45,55,80);
            r.draw_text(row.name.c_str(), bx+8, y+6, 175,180,195);

            std::string key = "(unset)";
            if (working_.keybinds.count(row.id)) key = working_.keybinds.at(row.id);
            if (rebind_this) key = "Press a key...";
            r.draw_text(key.c_str(), bx+bw-120, y+6, rebind_this?220:150, rebind_this?230:160, rebind_this?100:180);

            r.draw_line(bx, y+ROW_H-1, bx+bw, y+ROW_H-1, 35,42,56);
        }
        y += ROW_H;
        ++real_idx;
    }

    // Reset button
    r.fill_rect(bx, by+bh, 160, 24, 32,40,54);
    r.draw_rect_outline(bx, by+bh, 160, 24, 55,68,90);
    r.draw_text("Reset to Defaults", bx+8, by+bh+6, 140,145,165);
}

void SettingsPanel::render_conflict(Renderer& r) const {
    constexpr int CW=320, CH=130;
    const int cx=(r.screen_w()-CW)/2, cy=(r.screen_h()-CH)/2;
    r.fill_rect(cx, cy, CW, CH, 28,34,46);
    r.draw_rect_outline(cx, cy, CW, CH, 120,80,60);
    r.fill_rect(cx, cy, CW, 28, 50,35,20);
    r.draw_text("Key Conflict", cx+10, cy+8, 230,180,100);
    char msg[128];
    std::snprintf(msg, sizeof(msg), "'%s' is used by '%s'.",
                  conflict_key_.c_str(), conflict_other_.c_str());
    r.draw_text(msg, cx+10, cy+36, 180,185,200);
    r.draw_text("Reassign?", cx+10, cy+56, 160,165,180);

    r.fill_rect(cx+10,     cy+CH-44, 120, 30, 50,80,50);
    r.draw_rect_outline(cx+10, cy+CH-44, 120, 30, 80,140,80);
    r.draw_text("Reassign", cx+20, cy+CH-33, 160,220,160);

    r.fill_rect(cx+CW-130, cy+CH-44, 120, 30, 50,40,40);
    r.draw_rect_outline(cx+CW-130, cy+CH-44, 120, 30, 100,60,60);
    r.draw_text("Cancel", cx+CW-106, cy+CH-33, 220,170,170);
}

void SettingsPanel::render(Renderer& renderer) const {
    if (!open_) return;

    const int spx = px(renderer), spy = py(renderer);

    renderer.fill_rect(0, 0, renderer.screen_w(), renderer.screen_h(), 0,0,0,130);
    renderer.fill_rect(spx, spy, W, H, 20,26,36);
    renderer.draw_rect_outline(spx, spy, W, H, 70,90,130);

    // Header
    renderer.fill_rect(spx, spy, W, HDR_H, 28,36,54);
    renderer.draw_text("Settings", spx+10, spy+7, 200,210,230);
    renderer.draw_text("[Esc]", spx+W-46, spy+7, 80,85,100);

    // Tabs
    const int ty = spy + HDR_H;
    const char* tab_labels[] = {"Display","Audio","Keybinds"};
    const int tw = W / 3;
    for (int i = 0; i < 3; ++i) {
        const bool active = (tab_ == (Tab)i);
        renderer.fill_rect(spx+i*tw, ty, tw, TAB_H, active?32:22, active?44:28, active?68:40);
        renderer.draw_rect_outline(spx+i*tw, ty, tw, TAB_H, active?90:45, active?110:52, active?160:70);
        const int lw = renderer.text_width(tab_labels[i]);
        renderer.draw_text(tab_labels[i], spx+i*tw+(tw-lw)/2, ty+8,
                           active?220:140, active?225:145, active?240:165);
    }

    const int bx = spx + 10, by = spy + HDR_H + TAB_H + 8;
    const int bw = W - 20,   bh = H - HDR_H - TAB_H - 8 - 50;

    switch (tab_) {
        case Tab::Display:  render_display (renderer, bx, by, bw, bh); break;
        case Tab::Audio:    render_audio   (renderer, bx, by, bw, bh); break;
        case Tab::Keybinds: render_keybinds(renderer, bx, by, bw, bh); break;
    }

    // Apply / Cancel
    const int btn_y = spy + H - 44;
    renderer.fill_rect(spx+10, btn_y, 120, 32, 40,80,55);
    renderer.draw_rect_outline(spx+10, btn_y, 120, 32, 60,130,80);
    renderer.draw_text("Apply", spx+50, btn_y+9, 160,225,170);

    renderer.fill_rect(spx+W-130, btn_y, 120, 32, 32,40,54);
    renderer.draw_rect_outline(spx+W-130, btn_y, 120, 32, 55,68,90);
    renderer.draw_text("Cancel", spx+W-96, btn_y+9, 160,165,185);

    if (conflict_open_) render_conflict(renderer);
}
