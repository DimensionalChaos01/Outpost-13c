#include "FogOfWar.h"
#include "Map.h"

void FogOfWar::init(int width, int height) {
    width_        = width;
    height_       = height;
    all_revealed_ = false;
    fog_.assign(height, std::vector<FogState>(width, FogState::Unseen));
}

void FogOfWar::update(int cx, int cy, int sight_radius, const Map& map) {
    for (auto& row : fog_)
        for (auto& cell : row)
            if (cell == FogState::Visible)
                cell = FogState::Seen;

    if (sight_radius <= 0) {
        for (int ty = 0; ty < height_; ++ty)
            for (int tx = 0; tx < width_; ++tx)
                if (has_clear_los(cx, cy, tx, ty, map))
                    fog_[ty][tx] = FogState::Visible;
        return;
    }

    const int r2 = sight_radius * sight_radius;
    for (int dy = -sight_radius; dy <= sight_radius; ++dy) {
        for (int dx = -sight_radius; dx <= sight_radius; ++dx) {
            if (dx * dx + dy * dy > r2) continue;
            int tx = cx + dx, ty = cy + dy;
            if (tx < 0 || tx >= width_ || ty < 0 || ty >= height_) continue;
            if (has_clear_los(cx, cy, tx, ty, map))
                fog_[ty][tx] = FogState::Visible;
        }
    }
}

void FogOfWar::reveal_all() {
    all_revealed_ = true;
    for (auto& row : fog_)
        for (auto& cell : row)
            cell = FogState::Visible;
}

FogState FogOfWar::get(int x, int y) const {
    // In reveal-all mode (editor), any coord is Visible — includes auto-expanded tiles.
    if (all_revealed_) return FogState::Visible;
    if (x < 0 || x >= width_ || y < 0 || y >= height_)
        return FogState::Unseen;
    return fog_[y][x];
}

bool FogOfWar::has_clear_los(int x0, int y0, int x1, int y1, const Map& map) const {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int cx = x0, cy = y0;

    while (!(cx == x1 && cy == y1)) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
        if (cx == x1 && cy == y1) break;
        if (map.blocks_vision(cx, cy)) return false;
    }
    return true;
}
