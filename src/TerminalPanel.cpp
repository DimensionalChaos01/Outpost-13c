#include "TerminalPanel.h"
#include "Renderer.h"
#include <string>
#include <cstdio>

void TerminalPanel::open()  { open_ = true; }
void TerminalPanel::close() { open_ = false; }

bool TerminalPanel::handle_event(const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        close();
        return true;
    }
    return false;
}

void TerminalPanel::render(Renderer& rnd, float sim_minutes) const {
    if (!open_) return;

    const int sw = rnd.screen_w(), sh = rnd.screen_h();

    // Derive time values from sim_minutes
    const int total_min = (int)sim_minutes;
    const int day       = total_min / 1440 + 1;
    const int hour      = (total_min / 60) % 24;
    const int minute    = total_min % 60;

    const bool is_day   = (hour >= 6 && hour < 18);
    const char* shift   = is_day ? "Day" : "Night";

    // Next shift change: 06:00 if night, 18:00 if day
    const int next_h    = is_day ? 18 : 6;
    const int mins_to_next = ((next_h * 60) - (hour * 60 + minute) + 1440) % 1440;
    const int next_hour = next_h;
    const int next_min  = 0;

    // Panel dimensions
    constexpr int PW = 360, PH = 200;
    const int px = (sw - PW) / 2;
    const int py = (sh - PH) / 2;

    // Background — near black with green tint
    rnd.fill_rect(px, py, PW, PH, 4, 12, 4, 252);
    rnd.draw_rect_outline(px, py, PW, PH, 0, 160, 40);

    // Inner border (double-line effect)
    rnd.draw_rect_outline(px + 3, py + 3, PW - 6, PH - 6, 0, 90, 20);

    // Terminal green colour
    constexpr Uint8 TG_R = 0, TG_G = 220, TG_B = 50;
    constexpr Uint8 TG_DIM_R = 0, TG_DIM_G = 120, TG_DIM_B = 28;

    // Scanlines (horizontal lines at every other row for atmosphere)
    for (int sy = py + 1; sy < py + PH - 1; sy += 2)
        rnd.fill_rect(px + 1, sy, PW - 2, 1, 0, 0, 0, 40);

    int ty = py + 16;
    constexpr int TX = 20;

    rnd.draw_text("OUTPOST 13-C STATION CLOCK", px + TX, ty, TG_R, TG_G, TG_B);
    ty += 18;
    rnd.draw_text("===========================", px + TX, ty, TG_DIM_R, TG_DIM_G, TG_DIM_B);
    ty += 22;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Day:   %d", day);
    rnd.draw_text(buf, px + TX, ty, TG_R, TG_G, TG_B); ty += 18;

    std::snprintf(buf, sizeof(buf), "Shift: %s", shift);
    rnd.draw_text(buf, px + TX, ty, TG_R, TG_G, TG_B); ty += 22;

    std::snprintf(buf, sizeof(buf), "Local time:        %02d:%02d", hour, minute);
    rnd.draw_text(buf, px + TX, ty, TG_R, TG_G, TG_B); ty += 18;

    std::snprintf(buf, sizeof(buf), "Next shift change: %02d:%02d", next_hour, next_min);
    rnd.draw_text(buf, px + TX, ty, TG_DIM_R, TG_DIM_G, TG_DIM_B);
    (void)mins_to_next;
    ty += 22;

    rnd.draw_text("===========================", px + TX, ty, TG_DIM_R, TG_DIM_G, TG_DIM_B);
    ty += 18;
    rnd.draw_text("[ESC] Close", px + TX, ty, TG_DIM_R, TG_DIM_G, TG_DIM_B);
}
