#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "Settings.h"

class Renderer;

class SettingsPanel {
public:
    enum class Result { None, Apply, Close };

    void   open(const AppSettings& current);
    void   close();
    bool   is_open() const { return open_; }

    Result handle_event(const SDL_Event& e, Renderer& renderer);
    void   render(Renderer& renderer) const;

    const AppSettings& working() const { return working_; }

    static constexpr int W = 600, H = 480;

private:
    enum class Tab { Display, Audio, Keybinds };

    bool       open_    = false;
    AppSettings working_;
    Tab         tab_    = Tab::Display;

    // Slider dragging
    int drag_slider_   = -1;
    int drag_base_x_   = 0;
    int drag_base_val_ = 0;

    // Keybinds
    int  kb_scroll_  = 0;
    bool rebinding_  = false;
    int  rebind_idx_ = -1;

    // Conflict dialog
    bool        conflict_open_  = false;
    std::string conflict_key_;
    std::string conflict_other_;
    int         conflict_target_ = -1;

    int px(Renderer& r) const;
    int py(Renderer& r) const;

    void render_display (Renderer& r, int bx, int by, int bw, int bh) const;
    void render_audio   (Renderer& r, int bx, int by, int bw, int bh) const;
    void render_keybinds(Renderer& r, int bx, int by, int bw, int bh) const;
    void render_conflict(Renderer& r) const;

    static constexpr int TAB_H   = 32;
    static constexpr int HDR_H   = 28;
    static constexpr int ROW_H   = 28;

    // Ordered list of keybind action IDs + display names
    struct KbRow { std::string id; std::string name; };
    static const std::vector<KbRow>& kb_rows();
};
