#include "VoiceProfile.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

VoiceProfile default_voice_profile() {
    VoiceProfile p;
    p.id           = "station_ambient";
    p.display_name = "Station Ambient";
    p.r = 200; p.g = 200; p.b = 215;
    p.name_r = 150; p.name_g = 150; p.name_b = 180;
    p.text_speed   = 0.03f;
    return p;
}

void load_voice_profiles(const std::string& path, std::vector<VoiceProfile>& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "VoiceProfile: " << path << " not found, using default\n";
        out.push_back(default_voice_profile());
        return;
    }
    json data;
    try { f >> data; }
    catch (const json::exception& e) {
        std::cerr << "VoiceProfile: JSON error: " << e.what() << "\n";
        out.push_back(default_voice_profile());
        return;
    }
    for (const auto& p : data.value("profiles", json::array())) {
        VoiceProfile vp;
        vp.id           = p.value("id",           "");
        vp.display_name = p.value("display_name", "");
        vp.blip_sound   = p.value("blip_sound",   "");
        vp.portrait     = p.value("portrait",     "");
        vp.text_speed   = p.value("text_speed",   0.03f);
        if (p.contains("text_colour") && p["text_colour"].is_array()
                && p["text_colour"].size() >= 3) {
            vp.r = (Uint8)p["text_colour"][0].get<int>();
            vp.g = (Uint8)p["text_colour"][1].get<int>();
            vp.b = (Uint8)p["text_colour"][2].get<int>();
        }
        if (p.contains("name_colour") && p["name_colour"].is_array()
                && p["name_colour"].size() >= 3) {
            vp.name_r = (Uint8)p["name_colour"][0].get<int>();
            vp.name_g = (Uint8)p["name_colour"][1].get<int>();
            vp.name_b = (Uint8)p["name_colour"][2].get<int>();
        }
        if (p.contains("special_effects") && p["special_effects"].is_array())
            for (const auto& fx : p["special_effects"])
                vp.special_effects.push_back(fx.get<std::string>());
        if (!vp.id.empty()) out.push_back(vp);
    }
    if (out.empty()) out.push_back(default_voice_profile());
    std::cout << "VoiceProfile: loaded " << out.size() << " profiles\n";
}

const VoiceProfile* find_voice_profile(const std::vector<VoiceProfile>& profiles,
                                        const std::string& id) {
    for (const auto& p : profiles)
        if (p.id == id) return &p;
    return profiles.empty() ? nullptr : &profiles[0];
}
