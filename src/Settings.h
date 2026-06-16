#pragma once
#include <string>
#include <map>

struct AppSettings {
    // Display
    int  res_w      = 1280, res_h = 720;
    bool fullscreen = false;
    bool vsync      = true;
    bool show_grid  = false;
    int  ui_scale   = 1;
    // Camera
    float cam_default_zoom  = 1.0f;   // saved zoom (applied on game start)
    float cam_follow_speed  = 12.0f;  // tiles per second recentre speed
    bool  cam_smooth_follow = true;   // lerp vs instant snap
    // Audio (stored; applied when audio system arrives)
    int vol_master = 80, vol_music = 70, vol_sfx = 80, vol_ambient = 60;
    // Keybinds: action_id -> key string
    std::map<std::string, std::string> keybinds;

    bool save(const std::string& path) const;
    static AppSettings load(const std::string& path);
    static std::map<std::string, std::string> default_keybinds();
};
