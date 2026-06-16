#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

enum class MenuMode { GAMEPLAY, EDITOR, PLAYTEST };

enum class PauseResult {
    None,
    Close,          // Resume / Close Menu
    SaveGame,
    LoadGame,
    Settings,
    GoMainMenu,
    QuitDesktop,
    SaveAndQuit,    // save map/game then quit
    ReturnEditor,   // playtest -> editor
    AddNote,
    ViewNotes,
    Controls,       // show game controls overlay
    DevChanged,     // dev tool toggle changed; re-read dev_* fields
};

class Renderer;

class PauseMenu {
public:
    void open(MenuMode mode, bool dirty = false);
    void close();
    bool is_open() const { return open_; }

    // Returns the result of the user's action; None = still open.
    PauseResult handle_event(const SDL_Event& e, Renderer& renderer);
    void        render(Renderer& renderer) const;

    // Dev tools state (playtest only) — read by caller after DevChanged
    bool dev_fog_enabled     = true;
    bool dev_god_mode        = false;
    bool dev_show_collision  = false;
    bool dev_show_entity_ids = false;

private:
    struct Button {
        std::string label;
        PauseResult action;
        bool        is_toggle    = false;
        const bool* toggle_state = nullptr;
        bool        sep_before   = false;
    };

    enum class Sub { Main, DevTools, Confirm };

    bool    open_  = false;
    MenuMode mode_ = MenuMode::GAMEPLAY;
    bool    dirty_ = false;
    Sub     sub_   = Sub::Main;
    PauseResult confirm_pending_ = PauseResult::None;

    std::vector<Button> buttons_;
    std::vector<Button> dev_buttons_;
    int hovered_ = -1;

    void build_buttons();
    void build_dev_buttons();

    static constexpr int k_panel_w   = 300;
    static constexpr int k_btn_h     = 36;
    static constexpr int k_title_h   = 44;
    static constexpr int k_pad       = 12;

    int  panel_h(const std::vector<Button>& btns) const;
    void render_overlay(Renderer& r, int sw, int sh) const;
    void render_buttons(Renderer& r, const std::vector<Button>& btns,
                        int px, int py) const;
    void render_confirm(Renderer& r, int sw, int sh) const;
};
