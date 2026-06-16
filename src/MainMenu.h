#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "SaveSystem.h"

class Renderer;
class SettingsPanel;

enum class MainMenuResult { None, NewGame, LoadGame, OpenEditor, OpenSettings, Quit };

class MainMenu {
public:
    MainMenuResult handle_event(const SDL_Event& e, Renderer& renderer,
                                SettingsPanel& settings);
    void render(Renderer& renderer, const SettingsPanel& settings) const;

    void rescan_saves(const std::string& dir);

    const std::string& selected_save_path()  const { return sel_save_path_; }
    const std::string& new_game_player_name()const { return player_name_; }
    bool               new_game_hard()       const { return difficulty_hard_; }

private:
    enum class State { Main, NewGame, LoadGame };

    State  state_   = State::Main;
    int    hovered_ = -1;

    // New game
    std::string player_name_     = "Player";
    bool        difficulty_hard_ = false;
    bool        name_editing_    = false;

    // Load game
    SaveSystem  save_sys_;
    int         save_scroll_  = 0;
    int         save_hovered_ = -1;
    std::string sel_save_path_;

    void render_main    (Renderer& r) const;
    void render_new_game(Renderer& r) const;
    void render_load    (Renderer& r) const;

    static constexpr int BTN_W = 260, BTN_H = 38;
    static constexpr int PANEL_W = 320;
};
