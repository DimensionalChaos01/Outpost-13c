#include "TexturePicker.h"
#include "Renderer.h"
#include <algorithm>
#include <cstring>

void TexturePicker::open(const std::vector<TextureAtlas>* atlases, int preferred_tile_type) {
    atlases_             = atlases;
    preferred_tile_type_ = preferred_tile_type;
    scroll_              = 0;
    search_buf_.clear();
    search_active_ = false;
    open_          = true;
}

void TexturePicker::close() {
    if (search_active_) SDL_StopTextInput();
    search_active_ = false;
    open_ = false;
}

int TexturePicker::panel_x(const Renderer& r) const { return (r.screen_w() - W) / 2; }
int TexturePicker::panel_y(const Renderer& r) const { return k_menu_bar_h + 30; }

std::vector<TexturePicker::PickEntry> TexturePicker::filtered(int tile_type) const {
    std::vector<PickEntry> compatible, other;
    if (!atlases_) return {};
    for (const auto& atlas : *atlases_) {
        for (const auto& entry : atlas.entries) {
            if (!search_buf_.empty()) {
                const std::string lo_name = entry.name;
                const std::string lo_buf  = search_buf_;
                bool found = lo_name.find(lo_buf) != std::string::npos;
                if (!found) continue;
            }
            PickEntry pe{&atlas, &entry};
            bool compat = entry.compatible_types.empty() || tile_type < 0;
            if (!compat)
                for (int ct : entry.compatible_types)
                    if (ct == tile_type) { compat = true; break; }
            if (compat) compatible.push_back(pe);
            else        other.push_back(pe);
        }
    }
    compatible.insert(compatible.end(), other.begin(), other.end());
    return compatible;
}

TexturePicker::Result TexturePicker::handle_event(const SDL_Event& e, const Renderer& renderer) {
    if (!open_) return Result::None;
    const int ppx = panel_x(renderer), ppy = panel_y(renderer);

    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) { close(); return Result::Close; }
        if (search_active_) {
            if (e.key.keysym.sym == SDLK_BACKSPACE && !search_buf_.empty())
                search_buf_.pop_back();
            else if (e.key.keysym.sym == SDLK_RETURN) {
                search_active_ = false; SDL_StopTextInput();
            }
        }
        return Result::None;
    }
    if (e.type == SDL_TEXTINPUT && search_active_) {
        search_buf_ += e.text.text;
        return Result::None;
    }
    if (e.type == SDL_MOUSEWHEEL) {
        scroll_ -= e.wheel.y * CELL;
        scroll_ = std::max(0, scroll_);
        return Result::None;
    }
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT)
        return Result::None;

    const int mx = e.button.x, my = e.button.y;
    // Click outside panel → close
    if (mx < ppx || mx >= ppx+W || my < ppy || my >= ppy+H) { close(); return Result::Close; }

    // Search bar
    const int sb_y = ppy + HDR_H + 4;
    if (my >= sb_y && my < sb_y + SB_H - 4 && mx >= ppx+8 && mx < ppx+W-8) {
        if (!search_active_) { search_active_ = true; SDL_StartTextInput(); }
        return Result::None;
    }
    if (search_active_) { search_active_ = false; SDL_StopTextInput(); }

    // Clear button
    const int bot_y = ppy + H - BOT_H;
    if (!selected_id_.empty() && my >= bot_y+4 && my < bot_y+24 && mx >= ppx+W-64) {
        selected_id_.clear();
        return Result::Selected;
    }

    // Thumbnail grid
    const int grid_y = ppy + HDR_H + SB_H;
    const int grid_h = H - HDR_H - SB_H - BOT_H;
    if (my >= grid_y && my < grid_y + grid_h) {
        const int cols = (W - 8) / CELL;
        auto entries = filtered(preferred_tile_type_);
        int x = ppx + 4, y = grid_y + 4 - scroll_, i = 0;
        for (const auto& pe : entries) {
            if (mx >= x && mx < x+CELL-2 && my >= y && my < y+CELL-2) {
                selected_id_ = pe.entry->id;
                return Result::Selected;
            }
            x += CELL;
            if (++i % cols == 0) { x = ppx + 4; y += CELL; }
        }
    }
    return Result::None;
}

void TexturePicker::render(Renderer& renderer) const {
    if (!open_) return;
    const int ppx = panel_x(renderer), ppy = panel_y(renderer);

    renderer.fill_rect(ppx, ppy, W, H, 20, 26, 36, 248);
    renderer.draw_rect_outline(ppx, ppy, W, H, 70, 90, 130);
    renderer.fill_rect(ppx, ppy, W, HDR_H, 28, 36, 54);
    renderer.draw_text("TEXTURE PICKER", ppx+10, ppy+7, 200, 210, 230);
    renderer.draw_text("[Esc]", ppx+W-46, ppy+7, 80, 85, 100);
    if (preferred_tile_type_ >= 0) {
        char pb[32]; std::snprintf(pb, sizeof(pb), "tile:%d", preferred_tile_type_);
        renderer.draw_text(pb, ppx+W-100, ppy+7, 80, 100, 130);
    }

    // Search bar
    const int sb_y = ppy + HDR_H + 4;
    renderer.fill_rect(ppx+8, sb_y, W-16, SB_H-6, 18, 22, 32);
    renderer.draw_rect_outline(ppx+8, sb_y, W-16, SB_H-6,
                               search_active_?80:50, search_active_?110:60, search_active_?180:90);
    std::string disp = search_buf_;
    if (disp.empty() && !search_active_) disp = "Search...";
    if (search_active_ && (SDL_GetTicks()/500)%2==0) disp += '|';
    renderer.draw_text(disp, ppx+12, sb_y+6, search_buf_.empty()?80:180, search_buf_.empty()?85:185, search_buf_.empty()?100:200);

    // Thumbnail grid
    const int grid_y = ppy + HDR_H + SB_H;
    const int grid_h = H - HDR_H - SB_H - BOT_H;
    renderer.fill_rect(ppx, grid_y, W, grid_h, 16, 20, 28);

    const int cols = (W - 8) / CELL;
    auto entries = filtered(preferred_tile_type_);
    int x = ppx + 4, y = grid_y + 4 - scroll_, i = 0;

    for (const auto& pe : entries) {
        if (y + CELL >= grid_y && y < grid_y + grid_h) {
            const bool sel = pe.entry->id == selected_id_;
            // Check compatibility
            bool compat = pe.entry->compatible_types.empty() || preferred_tile_type_ < 0;
            if (!compat)
                for (int ct : pe.entry->compatible_types)
                    if (ct == preferred_tile_type_) { compat = true; break; }

            renderer.fill_rect(x, y, CELL-2, CELL-2, sel?50:28, sel?70:34, sel?100:46);
            if (sel) renderer.draw_rect_outline(x-1, y-1, CELL, CELL, 120, 160, 220);

            // Draw atlas cell thumbnail
            renderer.draw_atlas_cell(*pe.atlas, pe.entry->cell, x+2, y+2, CELL-6, CELL-6);

            // Mismatch indicator (orange dot top-right)
            if (!compat)
                renderer.fill_rect(x+CELL-9, y+1, 7, 7, 255, 100, 30, 220);
        }
        x += CELL;
        if (++i % cols == 0) { x = ppx + 4; y += CELL; }
    }

    if (entries.empty()) {
        renderer.draw_text("No textures loaded.", ppx+12, grid_y+8, 80, 85, 100);
        renderer.draw_text("Add .json files to Data/atlases/", ppx+12, grid_y+24, 70, 75, 90);
    }

    // Bottom bar
    const int bot_y = ppy + H - BOT_H;
    renderer.fill_rect(ppx, bot_y, W, BOT_H, 24, 30, 42);
    renderer.draw_line(ppx, bot_y, ppx+W, bot_y, 50, 60, 80);
    if (!selected_id_.empty()) {
        // Truncate if too long
        std::string disp2 = selected_id_;
        while (!disp2.empty() && renderer.text_width(disp2) > W - 80) disp2.pop_back();
        renderer.draw_text(disp2, ppx+8, bot_y+8, 180, 210, 240);
        renderer.fill_rect(ppx+W-62, bot_y+4, 54, 22, 55, 30, 30);
        renderer.draw_rect_outline(ppx+W-62, bot_y+4, 54, 22, 100, 50, 50);
        renderer.draw_text("Clear", ppx+W-49, bot_y+9, 220, 170, 170);
    } else {
        renderer.draw_text("Click a texture to select", ppx+8, bot_y+8, 80, 85, 100);
    }
}
