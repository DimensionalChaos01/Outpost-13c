#include "TextQueue.h"
#include "Renderer.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>

// ── Helpers ───────────────────────────────────────────────────────────────────

const VoiceProfile& TextQueue::current_profile() const {
    static const VoiceProfile s_default = default_voice_profile();
    if (!profiles_ || profiles_->empty()) return s_default;
    if (!queue_.empty()) {
        const VoiceProfile* p = find_voice_profile(*profiles_,
                                    queue_.front().voice_profile_id);
        if (p) return *p;
    }
    return (*profiles_)[0];
}

float TextQueue::timed_duration() const {
    if (queue_.empty()) return 0;
    const std::string& d = queue_.front().dismiss;
    if (d.rfind("timed:", 0) == 0) {
        try { return std::stof(d.substr(6)); }
        catch (...) {}
    }
    return 0;
}

bool TextQueue::awaiting_input() const {
    if (queue_.empty()) return false;
    const std::string& d = queue_.front().dismiss;
    return d.empty() || d == "input";
}

// ── Public interface ──────────────────────────────────────────────────────────

void TextQueue::push(const TextEntry& e) {
    queue_.push_back(e);
    // If this is the first entry, reset state
    if (queue_.size() == 1) {
        chars_shown_   = 0;
        fully_shown_   = false;
        char_timer_    = 0;
        dismiss_timer_ = 0;
    }
}

void TextQueue::tick(float dt) {
    if (queue_.empty()) return;

    const VoiceProfile& vp = current_profile();
    const TextEntry& e = queue_.front();
    const int text_len = (int)e.text.size();

    // Advance typewriter
    if (!fully_shown_) {
        float speed = vp.text_speed;
        for (const auto& fx : vp.special_effects)
            if (fx == "slow") speed *= 2.f;

        char_timer_ += dt;
        while (char_timer_ >= speed && chars_shown_ < text_len) {
            char_timer_ -= speed;
            ++chars_shown_;
        }
        if (chars_shown_ >= text_len) {
            chars_shown_ = text_len;
            fully_shown_ = true;
            dismiss_timer_ = 0;
        }
    }

    // Auto-dismiss
    if (fully_shown_) {
        const std::string& d = e.dismiss;
        float dur = 0;
        if (d == "auto") dur = 2.f;
        else              dur = timed_duration();

        if (dur > 0) {
            dismiss_timer_ += dt;
            if (dismiss_timer_ >= dur) {
                queue_.pop_front();
                if (!queue_.empty()) {
                    chars_shown_   = 0;
                    fully_shown_   = false;
                    char_timer_    = 0;
                    dismiss_timer_ = 0;
                }
            }
        }
    }
}

void TextQueue::advance() {
    if (queue_.empty()) return;
    if (!fully_shown_) {
        // Skip typewriter — show full text now
        chars_shown_ = (int)queue_.front().text.size();
        fully_shown_ = true;
        dismiss_timer_ = 0;
    } else {
        // Advance to next entry
        queue_.pop_front();
        if (!queue_.empty()) {
            chars_shown_   = 0;
            fully_shown_   = false;
            char_timer_    = 0;
            dismiss_timer_ = 0;
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void TextQueue::render(Renderer& rnd, int sw, int sh, bool bottom_overlay) const {
    if (queue_.empty()) return;

    const TextEntry& e     = queue_.front();
    const VoiceProfile& vp = current_profile();

    static constexpr int BAR_H = 80;
    static constexpr int PAD   = 14;
    const int bx = 0;
    const int by = sh - BAR_H;
    const Uint8 bg_a = bottom_overlay ? 180 : 220;

    // Background
    rnd.fill_rect(bx, by, sw, BAR_H, 14, 16, 24, bg_a);
    rnd.fill_rect(bx, by, sw, 1, 45, 50, 70, bg_a);  // top border

    // Shake effect
    int sx = 0, sy_off = 0;
    for (const auto& fx : vp.special_effects) {
        if (fx == "shake") {
            const Uint32 t = SDL_GetTicks();
            sx     = (int)((t / 50) % 3) - 1;
            sy_off = (int)((t / 37) % 3) - 1;
        }
    }

    // Build visible text (with optional glitch scramble on last chars while typing)
    std::string visible = e.text.substr(0, (size_t)chars_shown_);
    if (!fully_shown_) {
        bool glitch = false;
        for (const auto& fx : vp.special_effects)
            if (fx == "glitch") glitch = true;
        if (glitch && !visible.empty()) {
            static const char k_noise[] = "@#$%&?!~|><";
            const Uint32 t = SDL_GetTicks();
            const int scramble = std::min((int)visible.size(), 4);
            for (int i = (int)visible.size() - scramble; i < (int)visible.size(); ++i) {
                if ((t / 80 + i) % 3 == 0)
                    visible[i] = k_noise[(t / 40 + i) % (sizeof(k_noise) - 1)];
            }
        }
    }

    // Main text
    const int text_y = by + PAD;
    rnd.draw_text(visible, bx + PAD + sx, text_y + sy_off, vp.r, vp.g, vp.b);

    // Narrator line (shown once text is fully displayed, or immediately if it's a standalone narrator)
    if (!e.narrator.empty() && fully_shown_) {
        rnd.draw_text(e.narrator, bx + PAD, text_y + 20, vp.name_r, vp.name_g, vp.name_b);
    }

    // Dismiss hint
    if (awaiting_input() && fully_shown_) {
        const std::string hint = "[ Press E to continue ]";
        const int hw = rnd.text_width(hint);
        rnd.draw_text(hint, sw - hw - PAD, by + BAR_H - 16, 75, 80, 100);
    }
}
