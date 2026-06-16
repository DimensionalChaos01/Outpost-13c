#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct PlaytestNote {
    int         id = 0;
    std::string timestamp;
    std::string map_name;
    int         pos_x = 0, pos_y = 0;
    std::string text;
    bool        read = false;
};

class Renderer;

class PlaytestNotes {
public:
    bool load(const std::string& path);
    bool save() const;

    void add_note(const std::string& text,
                  const std::string& map_name, int px, int py);
    void mark_read(int id);
    void delete_note(int id);
    void clear_read();
    int  unread_count() const;

    const std::vector<PlaytestNote>& get_notes() const { return notes_; }

    // Panel control
    void open_add(const std::string& map_name, int px, int py);
    void open_view();
    void close();
    bool is_open() const { return open_; }

    enum class Result { None, Close, JumpTo };
    struct Event { Result result = Result::None; int jump_x = 0, jump_y = 0; };

    Event handle_event(const SDL_Event& e);
    void  render(Renderer& renderer) const;

private:
    std::string             path_;
    std::vector<PlaytestNote> notes_;
    int next_id_ = 1;

    bool open_      = false;
    bool view_mode_ = false;   // false=add, true=view

    // Add mode
    std::string add_buf_;
    std::string add_map_;
    int         add_px_ = 0, add_py_ = 0;

    // View mode
    int view_scroll_ = 0;

    static std::string current_timestamp();
};
