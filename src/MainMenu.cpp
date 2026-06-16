#include "MainMenu.h"
#include "Renderer.h"
#include "SettingsPanel.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

void MainMenu::rescan_saves(const std::string& dir) {
    save_sys_.scan(dir);
    save_scroll_ = 0; save_hovered_ = -1;
}

// ── Events ────────────────────────────────────────────────────────────────────

MainMenuResult MainMenu::handle_event(const SDL_Event& e, Renderer& renderer,
                                       SettingsPanel& settings) {
    const int sw = renderer.screen_w(), sh = renderer.screen_h();

    // Settings panel takes priority
    if (settings.is_open()) {
        const auto r = settings.handle_event(e, renderer);
        if (r == SettingsPanel::Result::Apply || r == SettingsPanel::Result::Close)
            settings.close();
        return MainMenuResult::None;
    }

    // ── New game panel ────────────────────────────────────────────────────────
    if (state_ == State::NewGame) {
        if (name_editing_) {
            if (e.type == SDL_TEXTINPUT) {
                if (player_name_.size() < 24) player_name_ += e.text.text;
                return MainMenuResult::None;
            }
            if (e.type == SDL_KEYDOWN) {
                switch(e.key.keysym.sym) {
                    case SDLK_BACKSPACE:
                        if (!player_name_.empty()) player_name_.pop_back(); break;
                    case SDLK_RETURN: case SDLK_ESCAPE: name_editing_ = false;
                        SDL_StopTextInput(); break;
                }
                return MainMenuResult::None;
            }
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            state_ = State::Main; return MainMenuResult::None;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;
            constexpr int PW = 360, PH = 260;
            const int px = (sw-PW)/2, py = (sh-PH)/2;
            // Name field click
            if (mx>=px+10 && mx<px+PW-10 && my>=py+72 && my<py+96) {
                name_editing_ = true; SDL_StartTextInput(); return MainMenuResult::None;
            }
            // Normal / Hard toggles
            if (mx>=px+10 && mx<px+10+120 && my>=py+110 && my<py+134) {
                difficulty_hard_ = false; return MainMenuResult::None;
            }
            if (mx>=px+140 && mx<px+260 && my>=py+110 && my<py+134) {
                difficulty_hard_ = true;  return MainMenuResult::None;
            }
            // Start button
            if (mx>=px+10 && mx<px+PW-10 && my>=py+PH-52 && my<py+PH-20) {
                state_ = State::Main; return MainMenuResult::NewGame;
            }
            // Back
            if (mx>=px+10 && mx<px+80 && my>=py+8 && my<py+28) {
                state_ = State::Main; return MainMenuResult::None;
            }
        }
        return MainMenuResult::None;
    }

    // ── Load game panel ───────────────────────────────────────────────────────
    if (state_ == State::LoadGame) {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            state_ = State::Main; return MainMenuResult::None;
        }
        if (e.type == SDL_MOUSEWHEEL) {
            save_scroll_ -= e.wheel.y * 60;
            save_scroll_  = std::max(0, save_scroll_);
            return MainMenuResult::None;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            const int mx = e.button.x, my = e.button.y;
            constexpr int PW = 480, PH = 380, ENTRY_H = 62;
            const int px = (sw-PW)/2, py = (sh-PH)/2;
            const int list_y = py + 38;
            const int list_h = PH - 38 - 44;

            const auto& slots = save_sys_.slots();
            int y = list_y - save_scroll_;
            for (int i = 0; i < (int)slots.size(); ++i) {
                if (y >= list_y && y < list_y+list_h) {
                    if (mx>=px+6 && mx<px+PW-6 && my>=y && my<y+ENTRY_H) {
                        // Delete button?
                        if (mx>=px+PW-66 && mx<px+PW-10 && my>=y+4 && my<y+28) {
                            save_sys_.delete_save(slots[i].path);
                            return MainMenuResult::None;
                        }
                        sel_save_path_ = slots[i].path;
                        state_ = State::Main;
                        return MainMenuResult::LoadGame;
                    }
                }
                y += ENTRY_H + 4;
            }
            // Back
            if (mx>=px+10 && mx<px+80 && my>=py+8 && my<py+28) {
                state_ = State::Main; return MainMenuResult::None;
            }
        }
        return MainMenuResult::None;
    }

    // ── Main menu ─────────────────────────────────────────────────────────────
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        return MainMenuResult::Quit;

    if (e.type == SDL_MOUSEMOTION) {
        const int mx = e.motion.x, my = e.motion.y;
        const int px = (sw - PANEL_W) / 2;
        const int btn_x = (sw - BTN_W) / 2;
        const int start_y = sh/2 - 10;
        hovered_ = -1;
        for (int i = 0; i < 5; ++i) {
            const int by = start_y + i*(BTN_H+8);
            if (mx>=btn_x && mx<btn_x+BTN_W && my>=by && my<by+BTN_H) { hovered_=i; break; }
        }
        (void)px;
        return MainMenuResult::None;
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        const int mx = e.button.x, my = e.button.y;
        const int btn_x = (sw - BTN_W) / 2;
        const int start_y = sh/2 - 10;
        const char* labels[] = {"New Game","Load Game","Map Editor","Settings","Quit"};
        const MainMenuResult acts[] = {
            MainMenuResult::NewGame, MainMenuResult::LoadGame,
            MainMenuResult::OpenEditor, MainMenuResult::OpenSettings,
            MainMenuResult::Quit
        };
        for (int i = 0; i < 5; ++i) {
            const int by = start_y + i*(BTN_H+8);
            if (mx>=btn_x && mx<btn_x+BTN_W && my>=by && my<by+BTN_H) {
                if (acts[i] == MainMenuResult::NewGame) {
                    state_ = State::NewGame; SDL_StartTextInput(); return MainMenuResult::None;
                }
                if (acts[i] == MainMenuResult::LoadGame) {
                    state_ = State::LoadGame; return MainMenuResult::None;
                }
                if (acts[i] == MainMenuResult::OpenSettings) {
                    return MainMenuResult::OpenSettings;
                }
                return acts[i];
            }
        }
        (void)labels;
    }
    return MainMenuResult::None;
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void MainMenu::render_main(Renderer& r) const {
    const int sw = r.screen_w(), sh = r.screen_h();

    // Background — dark with subtle vignette
    // Placeholder: load Assets/ui/main_menu_bg.png here when file exists.
    r.fill_rect(0, 0, sw, sh, 8, 10, 14);
    // Subtle grain: draw sparse dots (simple noise approximation)
    for (int i = 0; i < sw*sh/800; ++i) {
        const int gx = (i*7919+13) % sw;
        const int gy = (i*6271+37) % sh;
        r.fill_rect(gx, gy, 1, 1, 18,20,24);
    }

    // Title
    const char* title = "OUTPOST 13-C";
    const int tw = r.text_width(title);
    r.draw_text(title, (sw-tw)/2, sh/4, 200, 220, 240);

    const char* sub = "Tales from the Spiral";
    const int sw2 = r.text_width(sub);
    r.draw_text(sub, (sw-sw2)/2, sh/4+24, 120, 130, 150);

    // Separator
    r.draw_line(sw/2-120, sh/2-18, sw/2+120, sh/2-18, 55,65,85);

    const char* labels[] = {"New Game","Load Game","Map Editor","Settings","Quit"};
    const int btn_x = (sw - BTN_W) / 2;
    const int start_y = sh/2 - 10;

    for (int i = 0; i < 5; ++i) {
        const int by = start_y + i*(BTN_H+8);
        const bool hov = (hovered_ == i);
        r.fill_rect(btn_x, by, BTN_W, BTN_H, hov?48:28, hov?62:34, hov?90:50);
        r.draw_rect_outline(btn_x, by, BTN_W, BTN_H, hov?110:50, hov?150:65, hov?210:95);
        const int lw = r.text_width(labels[i]);
        r.draw_text(labels[i], btn_x+(BTN_W-lw)/2, by+11, hov?230:180, hov?235:185, hov?245:200);
    }

    r.draw_text("v0.1 \xe2\x80\x94 Outpost 13-C", 6, sh-16, 45,50,60);
}

void MainMenu::render_new_game(Renderer& r) const {
    const int sw = r.screen_w(), sh = r.screen_h();
    r.fill_rect(0, 0, sw, sh, 8,10,14);

    constexpr int PW = 360, PH = 260;
    const int px = (sw-PW)/2, py = (sh-PH)/2;
    r.fill_rect(px, py, PW, PH, 22,28,40);
    r.draw_rect_outline(px, py, PW, PH, 70,90,130);
    r.fill_rect(px, py, PW, 32, 30,40,62);
    r.draw_text("\xe2\x97\x80 Back", px+10, py+10, 130,140,160);
    r.draw_text("New Game", px+PW/2-35, py+9, 200,210,230);

    r.draw_text("Player Name:", px+10, py+52, 160,165,180);
    r.fill_rect(px+10, py+70, PW-20, 28, 18,22,32);
    r.draw_rect_outline(px+10, py+70, PW-20, 28, name_editing_?90:55, name_editing_?120:65, name_editing_?180:90);
    std::string disp = player_name_ + (name_editing_ && (SDL_GetTicks()/500)%2==0 ? "|" : "");
    r.draw_text(disp, px+14, py+76, 210,215,225);

    r.draw_text("Difficulty:", px+10, py+108, 160,165,180);
    const bool nh = !difficulty_hard_, hh = difficulty_hard_;
    r.fill_rect(px+10,  py+108+24, 120, 26, nh?40:22, nh?70:28, nh?55:40);
    r.draw_rect_outline(px+10, py+108+24, 120, 26, nh?80:40, nh?140:55, nh?100:70);
    r.draw_text("Normal", px+30, py+108+30, nh?200:140, nh?210:145, nh?185:155);

    r.fill_rect(px+140, py+108+24, 120, 26, hh?40:22, hh?28:28, hh?30:40);
    r.draw_rect_outline(px+140, py+108+24, 120, 26, hh?120:40, hh?50:55, hh?50:70);
    r.draw_text("Hard", px+175, py+108+30, hh?220:140, hh?160:145, hh?160:155);

    r.fill_rect(px+10, py+PH-52, PW-20, 32, 40,80,55);
    r.draw_rect_outline(px+10, py+PH-52, PW-20, 32, 60,130,80);
    const int sw2 = r.text_width("Start Game");
    r.draw_text("Start Game", px+(PW-sw2)/2, py+PH-41, 160,225,170);
}

void MainMenu::render_load(Renderer& r) const {
    const int sw = r.screen_w(), sh = r.screen_h();
    r.fill_rect(0, 0, sw, sh, 8,10,14);

    constexpr int PW = 480, PH = 380, ENTRY_H = 62;
    const int px = (sw-PW)/2, py = (sh-PH)/2;
    r.fill_rect(px, py, PW, PH, 22,28,40);
    r.draw_rect_outline(px, py, PW, PH, 70,90,130);
    r.fill_rect(px, py, PW, 36, 30,40,62);
    r.draw_text("\xe2\x97\x80 Back", px+10, py+10, 130,140,160);
    r.draw_text("Load Game", px+PW/2-40, py+10, 200,210,230);

    const int list_y = py + 38;
    const int list_h = PH - 38 - 44;
    const auto& slots = save_sys_.slots();

    if (slots.empty()) {
        r.draw_text("No saves found.", px+PW/2-50, list_y+list_h/2, 100,105,115);
    } else {
        int y = list_y - save_scroll_;
        for (int i = 0; i < (int)slots.size(); ++i) {
            if (y + ENTRY_H < list_y || y > list_y+list_h) { y+=ENTRY_H+4; continue; }
            const bool hov = (save_hovered_ == i);
            r.fill_rect(px+6, y, PW-12, ENTRY_H, hov?28:20, hov?36:25, hov?54:36);
            r.draw_rect_outline(px+6, y, PW-12, ENTRY_H, hov?80:40, hov?100:55, hov?150:80);
            r.draw_text(slots[i].save_name.c_str(), px+14, y+8,  210,215,225);
            char info[80];
            std::snprintf(info, sizeof(info), "%s  |  %s",
                          slots[i].timestamp.c_str(), slots[i].map_name.c_str());
            r.draw_text(info, px+14, y+28, 120,125,140);
            // Delete button
            r.fill_rect(px+PW-66, y+4, 56, 24, 80,28,28);
            r.draw_rect_outline(px+PW-66, y+4, 56, 24, 140,48,48);
            r.draw_text("Delete", px+PW-62, y+10, 210,160,160);
            y += ENTRY_H + 4;
        }
    }
}

void MainMenu::render(Renderer& renderer, const SettingsPanel& settings) const {
    switch (state_) {
        case State::Main:    render_main(renderer);      break;
        case State::NewGame: render_new_game(renderer);  break;
        case State::LoadGame:render_load(renderer);      break;
    }
    if (settings.is_open())
        const_cast<SettingsPanel&>(settings).render(renderer);
}
