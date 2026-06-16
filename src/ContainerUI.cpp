#include "ContainerUI.h"
#include "Renderer.h"
#include "PlayerInventory.h"
#include "TextQueue.h"
#include "ItemDef.h"
#include "Map.h"
#include <algorithm>

void ContainerUI::clamp(int count) {
    if (count == 0) { selected_ = 0; scroll_ = 0; return; }
    selected_ = std::clamp(selected_, 0, count - 1);
    if (selected_ < scroll_) scroll_ = selected_;
    if (selected_ >= scroll_ + k_vis_rows) scroll_ = selected_ - k_vis_rows + 1;
}

void ContainerUI::open(EntityInstance& entity) {
    entity_uid_   = entity.uid;
    selected_     = 0;
    scroll_       = 0;
    just_emptied_ = false;
}

// ── Take helper ───────────────────────────────────────────────────────────────

static void take_item(std::pair<std::string,int>& slot,
                       int qty,
                       PlayerInventory& inv,
                       const std::vector<ItemDef>& defs,
                       TextQueue& tq) {
    const ItemDef* d = find_item_def(defs, slot.first);
    inv.add(slot.first, qty);
    slot.second -= qty;

    TextEntry te;
    te.text             = "Picked up: " + (d ? d->name : slot.first);
    te.voice_profile_id = "station_ambient";
    te.dismiss          = "auto";
    tq.push(te);

    if (d && !d->pickup_text.empty()) {
        TextEntry te2;
        te2.text             = d->pickup_text;
        te2.voice_profile_id = d->voice_profile.empty() ? "station_ambient" : d->voice_profile;
        te2.dismiss          = "auto";
        tq.push(te2);
    }
}

// ── Event handling ────────────────────────────────────────────────────────────

ContainerUI::Action ContainerUI::handle_event(const SDL_Event& e,
                                               PlayerInventory& inv,
                                               const std::vector<ItemDef>& defs,
                                               TextQueue& tq) {
    // Caller must locate the EntityInstance from the map using entity_uid_.
    // We receive it via the event — but actually we need it here.
    // Handled by caller passing the entity by mutating map directly;
    // this function receives the live EntityInstance via the public open() uid.
    // The actual container_contents mutation happens in main.cpp which has map access.
    // This function only signals Close / None; item logic is in main.cpp.
    (void)inv; (void)defs; (void)tq;

    if (e.type != SDL_KEYDOWN) return Action::None;
    const SDL_Keycode sym = e.key.keysym.sym;

    if (sym == SDLK_ESCAPE) return Action::Close;

    if (sym == SDLK_w || sym == SDLK_UP)   { if (selected_ > 0) --selected_; return Action::None; }
    if (sym == SDLK_s || sym == SDLK_DOWN) { ++selected_; return Action::None; }

    // E and R handled in main.cpp which has access to the map and entity.
    return Action::None;
}

// ── Render ────────────────────────────────────────────────────────────────────

void ContainerUI::render(Renderer& rnd,
                          const EntityInstance& entity,
                          const std::vector<ItemDef>& defs) const {
    const int sw = rnd.screen_w(), sh = rnd.screen_h();
    const auto& contents = entity.container_contents;
    const int n = (int)contents.size();

    constexpr int PW = 380;
    const int row_area_h = std::min(k_vis_rows, std::max(1, n)) * k_row_h + 8;
    const int PH = 30 + row_area_h + 28; // header + rows + footer
    const int px = (sw - PW) / 2;
    const int py = (sh - PH) / 2;

    rnd.fill_rect(px, py, PW, PH, 14, 18, 26, 250);
    rnd.draw_rect_outline(px, py, PW, PH, 60, 70, 90);

    // Header.
    rnd.fill_rect(px, py, PW, 28, 26, 32, 48);
    const std::string title = (entity.label.empty() ? entity.type_id : entity.label)
                            + " (" + std::to_string(n) + " item" + (n==1?"":"s") + ")";
    rnd.draw_text(title, px + 10, py + 7, 200, 210, 225);

    // Item list.
    const int list_y = py + 30;
    if (n == 0) {
        rnd.draw_text("Empty.", px + 14, list_y + 6, 80, 85, 100);
    } else {
        const int vis = std::min(k_vis_rows, n);
        for (int i = 0; i < vis; ++i) {
            const int idx = scroll_ + i;
            if (idx >= n) break;
            const bool sel = (idx == selected_);
            const int ry = list_y + i * k_row_h + 4;

            if (sel) rnd.fill_rect(px + 1, ry - 2, PW - 2, k_row_h, 28, 38, 60, 220);

            rnd.draw_text(sel ? ">" : " ", px + 8, ry, 180, 200, 240);
            const ItemDef* d = find_item_def(defs, contents[idx].first);
            rnd.draw_text(d ? d->name : contents[idx].first, px + 22, ry,
                          sel ? 230 : 180, sel ? 235 : 185, sel ? 255 : 200);
            const std::string qty = "x" + std::to_string(contents[idx].second);
            rnd.draw_text(qty, px + PW - 10 - rnd.text_width(qty), ry,
                          sel ? 180 : 120, sel ? 190 : 130, sel ? 210 : 150);
        }
    }

    // Footer.
    const int fy = py + PH - 26;
    rnd.draw_line(px + 1, fy, px + PW - 2, fy, 45, 55, 70);
    rnd.draw_text(n > 0 ? "E: Take    R: Take All    Esc: Close"
                        : "Esc: Close",
                  px + 10, fy + 7, 70, 80, 100);
}
