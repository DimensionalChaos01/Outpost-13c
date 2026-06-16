#pragma once
#include <string>
#include <algorithm>

class Map;

enum class Direction { NORTH, SOUTH, EAST, WEST };

struct PlayerStats {
    int hp_current     = 6;
    int hp_max         = 6;
    int sanity_current = 100;
    int sanity_max     = 100;
    int value          = 100;
    int strength       = 1;
    int agility        = 1;
    int defense        = 1;   // Armor Rating
    int perception     = 1;
    int endurance      = 1;
    int fortitude      = 1;
    int discipline     = 1;

    void recalc_hp_max() {
        hp_max = std::max(6, (endurance + fortitude) * 3);
        if (hp_current > hp_max) hp_current = hp_max;
    }
};

class Player {
public:
    bool load(const std::string& path);

    int tile_x()       const { return tile_x_; }
    int tile_y()       const { return tile_y_; }
    int sight_radius() const { return sight_radius_; }

    int color_r() const { return color_r_; }
    int color_g() const { return color_g_; }
    int color_b() const { return color_b_; }

    Direction   facing()     const { return facing_; }
    std::string facing_str() const;

    void set_position(int x, int y) {
        tile_x_ = x; tile_y_ = y;
        vis_tx_ = (float)x; vis_ty_ = (float)y;
    }
    void set_facing(Direction d)         { facing_ = d; }
    void set_facing_from_str(const std::string& s);

    bool try_move(int dx, int dy, const Map& map);
    void update_visual(float dt, float speed);

    float vis_tx() const { return vis_tx_; }
    float vis_ty() const { return vis_ty_; }

    // ── Stats ─────────────────────────────────────────────────────────────────
    PlayerStats&       stats()       { return stats_; }
    const PlayerStats& stats() const { return stats_; }
    void               set_stats(const PlayerStats& s) { stats_ = s; }

    // ── Name ──────────────────────────────────────────────────────────────────
    const std::string& name() const    { return name_; }
    void               set_name(const std::string& n) { name_ = n; }

private:
    int         tile_x_       = 5;
    int         tile_y_       = 5;
    float       vis_tx_       = 5.0f;
    float       vis_ty_       = 5.0f;
    int         sight_radius_ = 5;
    int         color_r_      = 220;
    int         color_g_      = 240;
    int         color_b_      = 255;
    Direction   facing_       = Direction::SOUTH;
    PlayerStats stats_;
    std::string name_         = "SMITH";
};
