#include "PauseMenu.h"
#include "Renderer.h"
#include <algorithm>
#include <cstring>

// ── Open / Close ──────────────────────────────────────────────────────────────

void PauseMenu::open(MenuMode mode, bool dirty) {
    mode_  = mode;
    dirty_ = dirty;
    sub_   = Sub::Main;
    confirm_pending_ = PauseResult::None;
    hovered_ = -1;
    open_  = true;
    build_buttons();
    build_dev_buttons();
}

void PauseMenu::close() { open_ = false; hovered_ = -1; sub_ = Sub::Main; }

// ── Button construction ───────────────────────────────────────────────────────

void PauseMenu::build_buttons() {
    buttons_.clear();
    switch (mode_) {
        case MenuMode::GAMEPLAY:
            buttons_ = {
                {"Resume",          PauseResult::Close},
                {"Controls",        PauseResult::Controls},
                {"Save Game",       PauseResult::SaveGame},
                {"Load Game",       PauseResult::LoadGame},
                {"Settings",        PauseResult::Settings},
                {"Main Menu",       PauseResult::GoMainMenu,   false,nullptr,true},
                {"Quit to Desktop", PauseResult::QuitDesktop},
            };
            break;
        case MenuMode::EDITOR:
            buttons_ = {
                {"Close Menu",      PauseResult::Close},
                {"Settings",        PauseResult::Settings},
                {"Quit to Desktop", PauseResult::QuitDesktop,  false,nullptr,true},
            };
            break;
        case MenuMode::PLAYTEST:
            buttons_ = {
                {"Resume Playtest",     PauseResult::Close},
                {"Controls",            PauseResult::Controls},
                {"Add Playtest Note",   PauseResult::AddNote},
                {"View Playtest Notes", PauseResult::ViewNotes},
                {"Dev Tools \xe2\x96\xb6", PauseResult::None,  false,nullptr,true}, // placeholder: opens sub
                {"Return to Editor",    PauseResult::ReturnEditor, false,nullptr,true},
                {"Quit to Desktop",     PauseResult::QuitDesktop},
            };
            break;
    }
}

void PauseMenu::build_dev_buttons() {
    dev_buttons_ = {
        {"Toggle Fog of War",       PauseResult::DevChanged, true, &dev_fog_enabled},
        {"God Mode",                PauseResult::DevChanged, true, &dev_god_mode},
        {"Show Collision Overlay",  PauseResult::DevChanged, true, &dev_show_collision},
        {"Show Entity IDs",         PauseResult::DevChanged, true, &dev_show_entity_ids},
        {"\xe2\x97\x80 Back",       PauseResult::None},   // go back to main sub
    };
}

// ── Panel height ──────────────────────────────────────────────────────────────

int PauseMenu::panel_h(const std::vector<Button>& btns) const {
    int h = k_title_h + k_pad;
    for (const auto& b : btns) {
        if (b.sep_before) h += 6;
        h += k_btn_h + 4;
    }
    return h + k_pad;
}

// ── Event handling ────────────────────────────────────────────────────────────

PauseResult PauseMenu::handle_event(const SDL_Event& e, Renderer& renderer) {
    if (!open_) return PauseResult::None;

    const int sw = renderer.screen_w(), sh = renderer.screen_h();

    // ── Confirm dialog ────────────────────────────────────────────────────────
    if (sub_ == Sub::Confirm) {
        constexpr int CW = 360, CH = 140;
        const int cx = (sw-CW)/2, cy = (sh-CH)/2;
        constexpr int BTN_W = 100, BTN_H = 32;
        const int by = cy + CH - BTN_H - 14;

        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            sub_ = Sub::Main; return PauseResult::None;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;
            // [Save and Exit]
            if (mx>=cx+10 && mx<cx+10+BTN_W && my>=by && my<by+BTN_H)
                { close(); return PauseResult::SaveAndQuit; }
            // [Discard and Exit]
            if (mx>=cx+10+BTN_W+10 && mx<cx+10+BTN_W+10+BTN_W && my>=by && my<by+BTN_H)
                { close(); return confirm_pending_; }
            // [Cancel]
            if (mx>=cx+CW-BTN_W-10 && mx<cx+CW-10 && my>=by && my<by+BTN_H)
                { sub_ = Sub::Main; return PauseResult::None; }
        }
        return PauseResult::None;
    }

    // ── Select button list ────────────────────────────────────────────────────
    const std::vector<Button>& btns = (sub_ == Sub::DevTools) ? dev_buttons_ : buttons_;
    const int ph = panel_h(btns);
    const int px = (sw - k_panel_w) / 2;
    const int py = (sh - ph) / 2;

    if (e.type == SDL_KEYDOWN) {
        const SDL_Keycode sym = e.key.keysym.sym;
        if (sym == SDLK_ESCAPE) {
            if (sub_ == Sub::DevTools) { sub_ = Sub::Main; hovered_ = -1; return PauseResult::None; }
            close(); return PauseResult::Close;
        }
        if (sym == SDLK_UP)   { if (hovered_ > 0) hovered_--; }
        if (sym == SDLK_DOWN) { hovered_ = std::min(hovered_+1, (int)btns.size()-1); }
        if ((sym == SDLK_RETURN || sym == SDLK_SPACE) && hovered_ >= 0)
            goto activate; // fall through to activation
        return PauseResult::None;
    }

    if (e.type == SDL_MOUSEMOTION) {
        const int mx = e.motion.x, my = e.motion.y;
        hovered_ = -1;
        int y = py + k_title_h + k_pad;
        for (int i = 0; i < (int)btns.size(); ++i) {
            if (btns[i].sep_before) y += 6;
            if (mx>=px && mx<px+k_panel_w && my>=y && my<y+k_btn_h) { hovered_=i; break; }
            y += k_btn_h + 4;
        }
        return PauseResult::None;
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        const int mx = e.button.x, my = e.button.y;
        int y = py + k_title_h + k_pad;
        for (int i = 0; i < (int)btns.size(); ++i) {
            if (btns[i].sep_before) y += 6;
            if (mx>=px && mx<px+k_panel_w && my>=y && my<y+k_btn_h) {
                hovered_ = i; goto activate;
            }
            y += k_btn_h + 4;
        }
        return PauseResult::None;
    }
    return PauseResult::None;

    activate: {
        if (hovered_ < 0 || hovered_ >= (int)btns.size()) return PauseResult::None;
        const Button& btn = btns[hovered_];

        // Dev Tools open button
        if (sub_ == Sub::Main && mode_ == MenuMode::PLAYTEST && btn.label.rfind("Dev Tools",0)==0) {
            sub_ = Sub::DevTools; hovered_ = -1; return PauseResult::None;
        }
        // Back from dev tools
        if (sub_ == Sub::DevTools && btn.action == PauseResult::None) {
            sub_ = Sub::Main; hovered_ = -1; return PauseResult::None;
        }
        // Toggle buttons
        if (btn.is_toggle && btn.toggle_state) {
            *const_cast<bool*>(btn.toggle_state) = !*btn.toggle_state;
            return PauseResult::DevChanged;
        }
        // Destructive actions with unsaved → confirm
        if ((btn.action == PauseResult::QuitDesktop || btn.action == PauseResult::GoMainMenu) && dirty_) {
            confirm_pending_ = btn.action;
            sub_ = Sub::Confirm;
            return PauseResult::None;
        }
        // Close
        if (btn.action == PauseResult::Close) { close(); return PauseResult::Close; }

        const PauseResult r = btn.action;
        if (r != PauseResult::None) close();
        return r;
    }
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void PauseMenu::render_overlay(Renderer& r, int sw, int sh) const {
    switch (mode_) {
        case MenuMode::GAMEPLAY:  r.fill_rect(0,0,sw,sh,  0,  0,  0,185); break;
        case MenuMode::EDITOR:    r.fill_rect(0,0,sw,sh, 10, 20, 40,170); break;
        case MenuMode::PLAYTEST:  r.fill_rect(0,0,sw,sh, 30, 15,  0,170); break;
    }
}

void PauseMenu::render_buttons(Renderer& r,
                                const std::vector<Button>& btns,
                                int px, int py) const {
    int y = py + k_title_h + k_pad;
    for (int i = 0; i < (int)btns.size(); ++i) {
        const Button& b = btns[i];
        if (b.sep_before) {
            r.draw_line(px+16, y+2, px+k_panel_w-16, y+2, 55,62,78);
            y += 6;
        }
        const bool hov = (hovered_ == i);
        if (hov)  r.fill_rect(px+6, y, k_panel_w-12, k_btn_h, 55,75,110);
        else      r.fill_rect(px+6, y, k_panel_w-12, k_btn_h, 28,33,44);
        r.draw_rect_outline(px+6, y, k_panel_w-12, k_btn_h, hov?100:50, hov?140:58, hov?200:78);

        // Toggle indicator
        if (b.is_toggle && b.toggle_state) {
            const bool on = *b.toggle_state;
            r.fill_rect(px+k_panel_w-44, y+10, 14, 14, on?20:25, on?140:35, on?60:45);
            r.draw_rect_outline(px+k_panel_w-44, y+10, 14, 14, 70,85,100);
            if (on) {
                r.draw_line(px+k_panel_w-42, y+17, px+k_panel_w-38, y+21, 80,220,80);
                r.draw_line(px+k_panel_w-38, y+21, px+k_panel_w-32, y+13, 80,220,80);
            }
        }

        const int tx = px + k_panel_w/2 - r.text_width(b.label)/2;
        r.draw_text(b.label, tx, y+10, hov?230:190, hov?235:195, hov?240:205);
        y += k_btn_h + 4;
    }
}

void PauseMenu::render_confirm(Renderer& r, int sw, int sh) const {
    constexpr int CW = 360, CH = 140;
    const int cx = (sw-CW)/2, cy = (sh-CH)/2;

    r.fill_rect(cx, cy, CW, CH, 28, 32, 42);
    r.draw_rect_outline(cx, cy, CW, CH, 100, 80, 60);
    r.fill_rect(cx, cy, CW, 28, 50, 38, 22);
    r.draw_text("Unsaved Changes", cx+10, cy+7, 230, 190, 120);

    r.draw_text("You have unsaved changes.", cx+14, cy+38, 180, 185, 195);

    constexpr int BTN_W = 106, BTN_H = 32;
    const int by = cy + CH - BTN_H - 14;

    r.fill_rect(cx+10,          by, BTN_W, BTN_H, 40,90,50);
    r.draw_rect_outline(cx+10,  by, BTN_W, BTN_H, 60,140,80);
    r.draw_text("Save and Exit", cx+18, by+9, 180,230,180);

    r.fill_rect(cx+10+BTN_W+10, by, BTN_W, BTN_H, 80,40,40);
    r.draw_rect_outline(cx+10+BTN_W+10, by, BTN_W, BTN_H, 140,60,60);
    r.draw_text("Discard and Exit", cx+10+BTN_W+13, by+9, 220,170,170);

    r.fill_rect(cx+CW-BTN_W-10, by, BTN_W, BTN_H, 32,38,52);
    r.draw_rect_outline(cx+CW-BTN_W-10, by, BTN_W, BTN_H, 60,68,85);
    r.draw_text("Cancel", cx+CW-BTN_W-10+34, by+9, 180,185,200);
}

void PauseMenu::render(Renderer& renderer) const {
    if (!open_) return;

    const int sw = renderer.screen_w(), sh = renderer.screen_h();
    render_overlay(renderer, sw, sh);

    const std::vector<Button>& btns = (sub_ == Sub::DevTools) ? dev_buttons_ : buttons_;
    const int ph = panel_h(btns);
    const int px = (sw - k_panel_w) / 2;
    const int py = (sh - ph) / 2;

    // Panel background
    renderer.fill_rect(px, py, k_panel_w, ph, 22, 28, 38);
    renderer.draw_rect_outline(px, py, k_panel_w, ph, 70, 85, 120);
    renderer.fill_rect(px, py, k_panel_w, k_title_h, 30, 38, 55);

    // Title
    const char* title = nullptr;
    switch (mode_) {
        case MenuMode::GAMEPLAY:  title = "PAUSED";   break;
        case MenuMode::EDITOR:    title = "EDITOR";   break;
        case MenuMode::PLAYTEST:  title = (sub_==Sub::DevTools)?"DEV TOOLS":"PLAYTEST"; break;
    }
    const int tx = px + k_panel_w/2 - renderer.text_width(title)/2;
    renderer.draw_text(title, tx, py+14, 200,210,230);

    render_buttons(renderer, btns, px, py);

    if (sub_ == Sub::Confirm)
        render_confirm(renderer, sw, sh);
}
