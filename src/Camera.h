#pragma once

class Camera {
public:
    static constexpr int k_tile_size = 32;

    // Zoom level table: 50 / 75 / 100 / 150 / 200 / 300 %
    static constexpr float k_zoom_steps[] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f};
    static constexpr int   k_num_zoom_steps = 6;

    int   screen_w() const { return screen_w_; }
    int   screen_h() const { return screen_h_; }
    int   map_left() const { return map_left_; }
    int   map_top()  const { return map_top_;  }

    void set_screen(int w, int h)          { screen_w_ = w; screen_h_ = h; }
    void set_map_origin(int left, int top) { map_left_ = left; map_top_ = top; }

    int   tile_px() const { return (int)(k_tile_size * zoom_); }
    float zoom()    const { return zoom_; }

    void follow(int tile_x, int tile_y);   // sets follow target (immediate if !smooth)
    void snap_to_tile_f(float tx, float ty); // always snaps camera to fractional tile pos
    void pan(int dx, int dy);              // direct pan; detaches smooth follow
    void zoom_to(float new_zoom, int anchor_sx, int anchor_sy);
    void zoom_step(int dir);              // +1 = zoom in, -1 = zoom out

    // Smooth follow.  Call update() each frame with delta seconds.
    void set_smooth_follow(bool on)      { smooth_follow_ = on; }
    void set_follow_speed(float tiles_s) { follow_speed_ = tiles_s; }
    float follow_speed() const           { return follow_speed_; }
    void update(float dt);
    bool is_detached() const;            // camera lags more than 1 tile behind target

    int offset_x() const { return (int)(offset_tx_ * k_tile_size * zoom_); }
    int offset_y() const { return (int)(offset_ty_ * k_tile_size * zoom_); }

    // World tile -> absolute window pixel (includes map_left/map_top).
    int to_screen_x(int tx) const { return tx * tile_px() - offset_x() + map_left_; }
    int to_screen_y(int ty) const { return ty * tile_px() - offset_y() + map_top_;  }
    int to_screen_x_f(float tx) const { return (int)(tx * tile_px() - offset_x() + map_left_); }
    int to_screen_y_f(float ty) const { return (int)(ty * tile_px() - offset_y() + map_top_);  }

    // Absolute window pixel -> world tile.
    int to_tile_x(int sx) const { return (sx - map_left_ + offset_x()) / tile_px(); }
    int to_tile_y(int sy) const { return (sy - map_top_  + offset_y()) / tile_px(); }

private:
    float offset_tx_ = 0.0f, offset_ty_ = 0.0f;
    float target_tx_ = 0.0f, target_ty_ = 0.0f;
    float zoom_      = 1.0f;
    bool  smooth_follow_  = false;
    float follow_speed_   = 8.0f; // tiles per second

    // Map viewport area (excludes menu/status bars, sidebars).
    int screen_w_  = 1280;
    int screen_h_  = 656;
    int map_left_  = 0;
    int map_top_   = 0;
};
