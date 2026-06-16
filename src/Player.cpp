#include "Player.h"
#include "Map.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <cmath>

using json = nlohmann::json;

static const char* k_player_template = R"({
  "start_x": 5,
  "start_y": 5,
  "sight_radius": 5,
  "color_r": 220,
  "color_g": 240,
  "color_b": 255
}
)";

bool Player::load(const std::string& path) {
    if (!ensure_file(path, k_player_template))
        return false;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Player: cannot open " << path << "\n";
        return false;
    }

    json data;
    try {
        file >> data;
    } catch (const json::exception& e) {
        std::cerr << "Player: JSON error: " << e.what() << "\n";
        return false;
    }

    tile_x_       = data.value("start_x",      5);
    tile_y_       = data.value("start_y",      5);
    sight_radius_ = data.value("sight_radius", 5);
    color_r_      = data.value("color_r",    220);
    color_g_      = data.value("color_g",    240);
    color_b_      = data.value("color_b",    255);

    vis_tx_ = (float)tile_x_;
    vis_ty_ = (float)tile_y_;

    return true;
}

bool Player::try_move(int dx, int dy, const Map& map) {
    // Update facing before attempting the move (face the direction of intent).
    if      (dy < 0) facing_ = Direction::NORTH;
    else if (dy > 0) facing_ = Direction::SOUTH;
    else if (dx < 0) facing_ = Direction::WEST;
    else if (dx > 0) facing_ = Direction::EAST;

    int nx = tile_x_ + dx;
    int ny = tile_y_ + dy;
    if (!map.get_tile(nx, ny).walkable)
        return false;
    tile_x_ = nx;
    tile_y_ = ny;
    return true;
}

std::string Player::facing_str() const {
    switch (facing_) {
        case Direction::NORTH: return "north";
        case Direction::SOUTH: return "south";
        case Direction::EAST:  return "east";
        case Direction::WEST:  return "west";
    }
    return "south";
}

void Player::update_visual(float dt, float speed) {
    const float dx = tile_x_ - vis_tx_;
    const float dy = tile_y_ - vis_ty_;
    const float dist = std::sqrt(dx*dx + dy*dy);
    if (dist < 0.001f) { vis_tx_ = (float)tile_x_; vis_ty_ = (float)tile_y_; return; }
    const float step = speed * dt;
    if (step >= dist) { vis_tx_ = (float)tile_x_; vis_ty_ = (float)tile_y_; }
    else              { vis_tx_ += (dx/dist)*step; vis_ty_ += (dy/dist)*step; }
}

void Player::set_facing_from_str(const std::string& s) {
    if      (s == "north") facing_ = Direction::NORTH;
    else if (s == "east")  facing_ = Direction::EAST;
    else if (s == "west")  facing_ = Direction::WEST;
    else                   facing_ = Direction::SOUTH;
}
