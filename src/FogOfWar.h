#pragma once
#include <vector>

class Map;

enum class FogState { Unseen, Seen, Visible };

class FogOfWar {
public:
    void init(int width, int height);

    void update(int cx, int cy, int sight_radius, const Map& map);

    // Mark every tile Visible — used by the editor. Also makes out-of-bounds
    // coords return Visible so auto-expanded map areas render immediately.
    void reveal_all();

    // Returns Unseen for out-of-bounds unless reveal_all() was called.
    FogState get(int x, int y) const;

private:
    bool has_clear_los(int x0, int y0, int x1, int y1, const Map& map) const;

    std::vector<std::vector<FogState>> fog_;
    int  width_        = 0;
    int  height_       = 0;
    bool all_revealed_ = false;
};
