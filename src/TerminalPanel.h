#pragma once
#include <string>
#include <SDL2/SDL.h>

class Renderer;

// Reusable green-on-black terminal display panel.
// Other terminal types (supply, hire, etc.) will extend this same style.
class TerminalPanel {
public:
    void open();
    void close();
    bool is_open() const { return open_; }

    // Returns true if the panel should close (Esc pressed).
    bool handle_event(const SDL_Event& e);

    // sim_minutes: total in-game minutes elapsed since day 1, 00:00.
    // day_offset: if nonzero, added to derived day (unused for base terminal).
    void render(Renderer& rnd, float sim_minutes) const;

private:
    bool open_ = false;
};
