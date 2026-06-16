#include "Settings.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

static const char* k_settings_template = R"({
  "display": {
    "resolution_w": 1280, "resolution_h": 720,
    "fullscreen": false, "vsync": true, "show_grid": false, "ui_scale": 1
  },
  "camera": { "default_zoom": 1.0, "follow_speed": 12.0, "smooth_follow": true },
  "audio": { "master": 80, "music": 70, "sfx": 80, "ambient": 60 },
  "keybinds": {
    "move_up":"W","move_down":"S","move_left":"A","move_right":"D",
    "pause":"Escape","interact":"F","inventory":"I",
    "editor_paint":"B","editor_erase":"X","editor_eyedropper":"Q",
    "editor_fill":"F","editor_select":"S","editor_entity":"E",
    "editor_save":"Ctrl+S","editor_undo":"Ctrl+Z","editor_redo":"Ctrl+Y",
    "editor_toggle_grid":"G","editor_toggle_path":"P","editor_toggle_entities":"V",
    "editor_play_test":"F5","editor_texture_picker":"Backslash","editor_texture_mode":"T",
    "playtest_return_to_editor":"Escape"
  }
}
)";

std::map<std::string, std::string> AppSettings::default_keybinds() {
    json data = json::parse(k_settings_template);
    std::map<std::string, std::string> kb;
    for (const auto& [k, v] : data["keybinds"].items())
        kb[k] = v.get<std::string>();
    return kb;
}

AppSettings AppSettings::load(const std::string& path) {
    AppSettings s;
    s.keybinds = default_keybinds();
    if (!ensure_file(path, k_settings_template)) return s;

    std::ifstream f(path);
    if (!f.is_open()) return s;
    json data;
    try { f >> data; } catch (...) { return s; }

    if (data.contains("display")) {
        const auto& d = data["display"];
        s.res_w      = d.value("resolution_w", s.res_w);
        s.res_h      = d.value("resolution_h", s.res_h);
        s.fullscreen = d.value("fullscreen",    s.fullscreen);
        s.vsync      = d.value("vsync",         s.vsync);
        s.show_grid  = d.value("show_grid",     s.show_grid);
        s.ui_scale   = d.value("ui_scale",      s.ui_scale);
    }
    if (data.contains("camera")) {
        const auto& c = data["camera"];
        s.cam_default_zoom  = c.value("default_zoom",  s.cam_default_zoom);
        s.cam_follow_speed  = c.value("follow_speed",  s.cam_follow_speed);
        s.cam_smooth_follow = c.value("smooth_follow", s.cam_smooth_follow);
    }
    if (data.contains("audio")) {
        const auto& a = data["audio"];
        s.vol_master  = a.value("master",  s.vol_master);
        s.vol_music   = a.value("music",   s.vol_music);
        s.vol_sfx     = a.value("sfx",     s.vol_sfx);
        s.vol_ambient = a.value("ambient", s.vol_ambient);
    }
    if (data.contains("keybinds"))
        for (const auto& [k, v] : data["keybinds"].items())
            s.keybinds[k] = v.get<std::string>();

    // Migrate: old default had editor_paint:"P" conflicting with editor_toggle_path:"P"
    if (s.keybinds["editor_paint"] == "P" && s.keybinds["editor_toggle_path"] == "P")
        s.keybinds["editor_paint"] = "B";

    return s;
}

bool AppSettings::save(const std::string& path) const {
    json data;
    data["display"]["resolution_w"] = res_w;
    data["display"]["resolution_h"] = res_h;
    data["display"]["fullscreen"]   = fullscreen;
    data["display"]["vsync"]        = vsync;
    data["display"]["show_grid"]    = show_grid;
    data["display"]["ui_scale"]     = ui_scale;
    data["camera"]["default_zoom"]  = cam_default_zoom;
    data["camera"]["follow_speed"]  = cam_follow_speed;
    data["camera"]["smooth_follow"] = cam_smooth_follow;
    data["audio"]["master"]         = vol_master;
    data["audio"]["music"]          = vol_music;
    data["audio"]["sfx"]            = vol_sfx;
    data["audio"]["ambient"]        = vol_ambient;
    for (const auto& [k, v] : keybinds)
        data["keybinds"][k] = v;

    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "Settings: cannot write " << path << "\n"; return false; }
    f << data.dump(2) << "\n";
    return true;
}
