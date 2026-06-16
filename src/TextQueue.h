#pragma once
#include <string>
#include <deque>
#include <vector>
#include "VoiceProfile.h"

struct TextEntry {
    std::string text;
    std::string narrator;           // secondary line shown below main text
    std::string voice_profile_id;   // matches a VoiceProfile id
    std::string dismiss;            // "input", "auto", "timed:N"
};

class Renderer;

class TextQueue {
public:
    ~TextQueue() = default;

    void set_profiles(const std::vector<VoiceProfile>* p) { profiles_ = p; }

    void push(const TextEntry& e);
    void tick(float dt);
    void advance();            // E/Space pressed: skip typewriter or advance entry
    bool is_active()       const { return !queue_.empty(); }
    bool awaiting_input()  const;

    // bottom_overlay: true draws bar with reduced alpha (combat/text over game world)
    void render(Renderer& rnd, int sw, int sh, bool bottom_overlay = false) const;

private:
    const VoiceProfile& current_profile() const;
    float timed_duration() const;  // parse "timed:N" → N, or 0

    std::deque<TextEntry>            queue_;
    float                            char_timer_    = 0;
    float                            dismiss_timer_ = 0;
    int                              chars_shown_   = 0;
    bool                             fully_shown_   = false;
    const std::vector<VoiceProfile>* profiles_      = nullptr;
};
