#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

class Renderer;

struct MenuItem {
    std::string label;
    std::string shortcut;
    std::string action;
    bool        enabled = true;
    bool        is_sep  = false;
};

struct Menu {
    std::string           label;
    std::vector<MenuItem> items;
    int bar_x = 0;   // pixel x of this menu's label in the bar
    int bar_w = 0;   // pixel width of the label area
};

struct MenuEvent {
    bool        consumed = false;
    std::string action;  // non-empty = item was activated
};

class MenuBar {
public:
    static constexpr int k_bar_h    = 24;
    static constexpr int k_item_h   = 22;
    static constexpr int k_drop_w   = 200;
    static constexpr int k_sep_h    = 8;

    MenuBar();

    // Returns a MenuEvent. consumed=true means the event should not be
    // forwarded to the editor (e.g. a click in the menu area).
    MenuEvent handle_event(const SDL_Event& e, int screen_w);

    void draw(Renderer& r, int screen_w) const;

    bool is_open()  const { return open_ >= 0; }
    void close()          { open_ = -1; hover_ = -1; }

private:
    int  bar_menu_at(int x, int y) const;
    int  drop_item_at(int menu_idx, int x, int y) const;
    int  drop_x(int menu_idx, int screen_w) const;
    int  drop_h(int menu_idx) const;

    std::vector<Menu> menus_;
    int open_  = -1;
    int hover_ = -1;  // hovered item index in open dropdown
};
