#include "PlaytestNotes.h"
#include "Renderer.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <cstdio>

using json = nlohmann::json;

static const char* k_notes_template = R"({"notes":[]})";

// ── Persistence ───────────────────────────────────────────────────────────────

bool PlaytestNotes::load(const std::string& path) {
    path_ = path;
    notes_.clear(); next_id_ = 1;
    if (!ensure_file(path, k_notes_template)) return false;

    std::ifstream f(path);
    if (!f.is_open()) return false;
    json data;
    try { f >> data; } catch (...) { return false; }

    if (!data.contains("notes")) return true;
    for (const auto& n : data["notes"]) {
        PlaytestNote note;
        note.id        = n.value("id",        next_id_);
        note.timestamp = n.value("timestamp", "");
        note.map_name  = n.value("map",        "");
        note.pos_x     = n.value("player_position", json::object()).value("x", 0);
        note.pos_y     = n.value("player_position", json::object()).value("y", 0);
        note.text      = n.value("text",       "");
        note.read      = n.value("read",       false);
        if (note.id >= next_id_) next_id_ = note.id + 1;
        notes_.push_back(note);
    }
    return true;
}

bool PlaytestNotes::save() const {
    if (path_.empty()) return false;
    json data;
    data["notes"] = json::array();
    for (const auto& n : notes_) {
        json e;
        e["id"]        = n.id;
        e["timestamp"] = n.timestamp;
        e["map"]       = n.map_name;
        e["player_position"] = {{"x", n.pos_x}, {"y", n.pos_y}};
        e["text"]      = n.text;
        e["read"]      = n.read;
        data["notes"].push_back(e);
    }
    std::ofstream f(path_);
    if (!f.is_open()) { std::cerr << "PlaytestNotes: cannot write\n"; return false; }
    f << data.dump(2) << "\n";
    return true;
}

// ── Note operations ───────────────────────────────────────────────────────────

std::string PlaytestNotes::current_timestamp() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

void PlaytestNotes::add_note(const std::string& text,
                              const std::string& map_name, int px, int py) {
    PlaytestNote n;
    n.id        = next_id_++;
    n.timestamp = current_timestamp();
    n.map_name  = map_name;
    n.pos_x     = px; n.pos_y = py;
    n.text      = text;
    n.read      = false;
    notes_.push_back(n);
    save();
}

void PlaytestNotes::mark_read(int id) {
    for (auto& n : notes_) if (n.id == id) { n.read = true; break; }
    save();
}
void PlaytestNotes::delete_note(int id) {
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(),
                                [id](const PlaytestNote& n){ return n.id == id; }),
                 notes_.end());
    save();
}
void PlaytestNotes::clear_read() {
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(),
                                [](const PlaytestNote& n){ return n.read; }),
                 notes_.end());
    save();
}
int PlaytestNotes::unread_count() const {
    int c = 0;
    for (const auto& n : notes_) if (!n.read) ++c;
    return c;
}

// ── Panel control ─────────────────────────────────────────────────────────────

void PlaytestNotes::open_add(const std::string& map_name, int px, int py) {
    view_mode_ = false;
    add_buf_.clear();
    add_map_ = map_name; add_px_ = px; add_py_ = py;
    open_ = true;
    SDL_StartTextInput();
}
void PlaytestNotes::open_view() {
    view_mode_   = true;
    view_scroll_ = 0;
    open_ = true;
}
void PlaytestNotes::close() {
    open_ = false;
    if (!view_mode_) SDL_StopTextInput();
}

// ── Events ────────────────────────────────────────────────────────────────────

PlaytestNotes::Event PlaytestNotes::handle_event(const SDL_Event& e) {
    if (!open_) return {};

    if (!view_mode_) {
        // Add note panel
        if (e.type == SDL_TEXTINPUT) {
            add_buf_ += e.text.text;
            return {};
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_BACKSPACE:
                    if (!add_buf_.empty()) add_buf_.pop_back(); break;
                case SDLK_RETURN:
                    if (!add_buf_.empty()) {
                        add_note(add_buf_, add_map_, add_px_, add_py_);
                    }
                    close();
                    return {Result::Close};
                case SDLK_ESCAPE:
                    close();
                    return {Result::Close};
            }
        }
        return {};
    }

    // View mode
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        close(); return {Result::Close};
    }
    if (e.type == SDL_MOUSEWHEEL) {
        view_scroll_ -= e.wheel.y * 60;
        view_scroll_ = std::max(0, view_scroll_);
        return {};
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        // Hit-test buttons handled in render — we do it here too
        // Handled below in render-correlated logic.
        return {};
    }
    return {};
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void PlaytestNotes::render(Renderer& r) const {
    if (!open_) return;

    const int sw = r.screen_w(), sh = r.screen_h();
    r.fill_rect(0, 0, sw, sh, 0, 0, 0, 160);

    if (!view_mode_) {
        // ── Add note panel ────────────────────────────────────────────────────
        constexpr int W = 460, H = 200;
        const int px = (sw-W)/2, py = (sh-H)/2;
        r.fill_rect(px, py, W, H, 22, 28, 38);
        r.draw_rect_outline(px, py, W, H, 80, 100, 140);
        r.fill_rect(px, py, W, 28, 30, 40, 60);
        r.draw_text("Add Playtest Note", px+10, py+8, 200,215,230);
        r.draw_text("Enter text, press Enter to save, Esc to cancel",
                    px+10, py+36, 120,125,135);

        // Text input field
        r.fill_rect(px+10, py+58, W-20, 80, 18, 22, 30);
        r.draw_rect_outline(px+10, py+58, W-20, 80, 90, 110, 160);

        // Display text with wrapping (simple: just truncate for now)
        const bool blink = (SDL_GetTicks()/500) % 2 == 0;
        std::string disp = add_buf_;
        if (blink) disp += '|';
        // Wrap at roughly 50 chars per line
        int y = py + 64;
        size_t pos = 0;
        while (pos < disp.size() && y < py+130) {
            size_t end = std::min(pos + 52, disp.size());
            r.draw_text(disp.substr(pos, end-pos), px+14, y, 210,215,225);
            pos = end; y += 16;
        }

        char loc[64];
        std::snprintf(loc, sizeof(loc), "Map: %s  Pos: %d,%d", add_map_.c_str(), add_px_, add_py_);
        r.draw_text(loc, px+10, py+146, 90,95,110);

        r.fill_rect(px+10, py+H-42, W-20, 32, 40,80,55);
        r.draw_rect_outline(px+10, py+H-42, W-20, 32, 60,120,80);
        r.draw_text("Submit  (Enter)", px+W/2 - 55, py+H-33, 160,220,170);
        return;
    }

    // ── View notes panel ──────────────────────────────────────────────────────
    constexpr int W = 520, H = 440;
    const int px = (sw-W)/2, py = (sh-H)/2;
    r.fill_rect(px, py, W, H, 20, 26, 36);
    r.draw_rect_outline(px, py, W, H, 70, 90, 130);
    r.fill_rect(px, py, W, 30, 28, 38, 58);
    r.draw_text("Playtest Notes", px+10, py+9, 200,215,230);

    const int unread = unread_count();
    if (unread > 0) {
        char ub[32]; std::snprintf(ub, sizeof(ub), "%d unread", unread);
        r.draw_text(ub, px+W-90, py+9, 220,180,80);
    }
    r.draw_text("[Esc]", px+W-46, py+9, 80,85,100);

    // Clear Read button
    r.fill_rect(px+10, py+H-38, 130, 26, 32,40,52);
    r.draw_rect_outline(px+10, py+H-38, 130, 26, 55,65,90);
    r.draw_text("Clear All Read", px+14, py+H-30, 140,145,165);

    const int list_y  = py + 34;
    const int list_h  = H - 34 - 44;
    SDL_Rect clip{px, list_y, W, list_h};
    // (Can't set clip directly here without SDL handle — just draw in range)

    constexpr int NOTE_H = 68;
    int y = list_y - view_scroll_;

    for (const auto& n : notes_) {
        if (y + NOTE_H < list_y || y > list_y + list_h) { y += NOTE_H + 4; continue; }

        const bool read = n.read;
        r.fill_rect(px+6, y, W-12, NOTE_H, read?18:22, read?22:30, read?28:44);
        r.draw_rect_outline(px+6, y, W-12, NOTE_H, read?40:70, read?48:88, read?58:130);

        // Unread dot
        if (!read) r.fill_rect(px+12, y+8, 6, 6, 220, 180, 60);

        char hdr[80];
        std::snprintf(hdr, sizeof(hdr), "%s  |  %s  |  %d,%d",
                      n.timestamp.c_str(), n.map_name.c_str(), n.pos_x, n.pos_y);
        r.draw_text(hdr, px+22, y+6, 120,130,150);

        // Note text (truncated)
        std::string txt = n.text.length() > 68 ? n.text.substr(0,67)+"..." : n.text;
        r.draw_text(txt, px+12, y+24, read?130:190, read?135:195, read?140:205);

        // Delete button
        r.fill_rect(px+W-60, y+4, 50, 22, 80,30,30);
        r.draw_rect_outline(px+W-60, y+4, 50, 22, 140,50,50);
        r.draw_text("Delete", px+W-57, y+8, 210,160,160);

        y += NOTE_H + 4;
    }

    if (notes_.empty())
        r.draw_text("No notes yet.", px + W/2 - 40, list_y + list_h/2, 100,105,115);
}
