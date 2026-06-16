#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct VoiceProfile {
    std::string id;
    std::string display_name;
    Uint8  r = 200, g = 200, b = 215;
    Uint8  name_r = 150, name_g = 150, name_b = 180;
    float  text_speed = 0.03f;  // seconds per character
    std::string blip_sound;     // path relative to project root, empty = silent
    std::string portrait;       // path, empty = no portrait
    std::vector<std::string> special_effects;  // "glitch", "shake", "slow"
};

VoiceProfile               default_voice_profile();
void                       load_voice_profiles(const std::string& path,
                                               std::vector<VoiceProfile>& out);
const VoiceProfile*        find_voice_profile(const std::vector<VoiceProfile>& profiles,
                                              const std::string& id);
