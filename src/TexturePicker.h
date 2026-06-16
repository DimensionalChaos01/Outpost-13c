#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "TextureAtlas.h"

class Renderer;

class TexturePicker {
public:
    enum class Result { None, Close, Selected };

    void   open(const std::vector<TextureAtlas>* atlases, int preferred_tile_type = -1);
    void   close();
    bool   is_open() const { return open_; }

    const std::string& selected_id() const { return selected_id_; }
    void  set_selection(const std::string& id) { selected_id_ = id; }

    Result handle_event(const SDL_Event& e, const Renderer& renderer);
    void   render(Renderer& renderer) const;

private:
    static constexpr int W     = 400;
    static constexpr int H     = 360;
    static constexpr int HDR_H = 28;
    static constexpr int SB_H  = 30;
    static constexpr int CELL  = 44; // thumbnail cell size
    static constexpr int BOT_H = 30;

    bool open_               = false;
    int  preferred_tile_type_= -1;
    int  scroll_             = 0;

    const std::vector<TextureAtlas>* atlases_ = nullptr;
    std::string selected_id_;
    std::string search_buf_;
    bool        search_active_ = false;

    struct PickEntry { const TextureAtlas* atlas; const AtlasEntry* entry; };
    std::vector<PickEntry> filtered(int tile_type) const;

    int panel_x(const Renderer& r) const;
    int panel_y(const Renderer& r) const;
};
