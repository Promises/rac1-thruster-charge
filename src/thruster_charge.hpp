// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thruster Charge's rules that are this mod's, not Going Commando's: which of the game's own
// changes of state a charge refuses, how often a moby takes the charge's hit, the blue of its
// flames, and the tick's length. The charge itself is Going Commando's Charge Boots code (src/gc/), run on Ratchet &
// Clank's hero and his Thruster-Pack by the translation layer (src/gc/rac1_shim.hpp).
//
// Nothing here reads the game: mod.cpp does, and the tests test the rules on plain values.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "layout.hpp"

namespace thruster_charge {

// Going Commando's frame: a 60th of a second.
constexpr float kGcFrame = 1.0f / 60.0f;
// One hit a moby a half second: the charge goes through it in a few frames, and an enemy whose
// update reads a hit each tick should not take one each tick.
constexpr float kHitCooldown = 0.5f;

// The tick's length as the game gives it, or a 60th when it gives nothing sensible.
inline float tick_seconds(float frame_time) {
    return std::isfinite(frame_time) && frame_time > 0.001f && frame_time < 0.1f ? frame_time : kGcFrame;
}

// An angle in -pi..pi.
inline float wrap_angle(float a) {
    constexpr float kPi = 3.14159265f;
    if (!std::isfinite(a)) {
        return 0.0f;
    }
    a = std::fmod(a + kPi, 2.0f * kPi);
    if (a < 0.0f) {
        a += 2.0f * kPi;
    }
    return a - kPi;
}

// What to do with a change of state the game asks for (rac1.player.enter_move_state).
enum class Request {
    // Let it through.
    Pass,
    // Refuse it (the game's call answers 0: no change).
    Refuse,
    // Idle instead.
    AsIdle,
};

// asked, force: the call's; current, control_mode: his now; charging: a charge on; mine: the mod's
// own call (the translation layer's changes of state).
//
// - The mod's own calls go through.
// - The hover is the charge's: the game asking for it by itself (the Thruster-Pack's double tap,
//   hero_gadget_use.c:1061-1079 and each level family's copy; idle with the hover latch set,
//   hero_item_input_and_hud.c:180-186) is refused. But the end of a hurt asks for it when the hurt
//   came in the hover (hero_item_input_and_hud.c:2241-2244) and asks again each tick it is
//   refused: that one is idle instead.
// - During a charge, the hover's own ways out to idle are refused: without the Thruster-Pack it
//   lets go after 20 ticks, and X or a double tap ends it with one (hero_item_input_and_hud.c:
//   1761-1800). A cutscene's call (force 2) is not. Going Commando's charge has no way out but its
//   own (the stop, the bonk), which its decider takes.
inline Request judge(std::int32_t asked, std::int32_t force, std::int32_t current, std::int32_t control_mode,
                     bool charging, bool mine) {
    if (mine) {
        return Request::Pass;
    }
    if (asked == kHover) {
        if (!charging && control_mode == kModeHurt) {
            return Request::AsIdle;
        }
        return Request::Refuse;
    }
    if (charging && current == kHover && asked == kIdle && force != 2) {
        return Request::Refuse;
    }
    return Request::Pass;
}

// One hit a moby each kHitCooldown, by its slot.
class HitCooldown {
public:
    bool ready(std::int32_t slot, float now) const {
        for (const Entry& e : m_entries) {
            if (e.slot == slot && now < e.until) {
                return false;
            }
        }
        return true;
    }

    void mark(std::int32_t slot, float now) {
        // Its own entry, else one run out, else the one that runs out first.
        Entry* pick = &m_entries[0];
        for (Entry& e : m_entries) {
            if (e.slot == slot) {
                pick = &e;
                break;
            }
            if (e.until < pick->until) {
                pick = &e;
            }
        }
        *pick = {slot, now + kHitCooldown};
    }

    void clear() { m_entries.fill({}); }

private:
    struct Entry {
        std::int32_t slot = -1;
        float until = -1.0f;
    };
    std::array<Entry, 8> m_entries{};
};

// --- The blue flames ---
//
// Every colour the Thruster-Pack's flames are drawn in is a PS2 RGBA word: red in the low byte,
// then green, blue, and alpha in the high byte, where 0x80 is full (the GS modulates a texture by
// colour / 0x80, and blends by alpha / 0x80): the flame quads' 0x80808080 (texture 0xC as it is),
// the glow's 0x7F2020FF (texture 0xB doubled in red), the ribbon table's 0x80808080 and 0x20808080
// (texture 0xA as it is), the sparks' 0x4F007FFF fading to 0x1FFFFFFF (orange to white).
//
// One blue for all of them: the colour's brightest channel kept as the brightness, the hue set to
// red 0x18 : green 0x50 : blue 0x80 of it, blue then doubled (to at most 0xFF) so a texture of the
// fire's own warm colours, modulated, still comes out a bright blue rather than a dark one. The
// alpha is kept, so each part fades and blends as the game made it.
constexpr std::uint32_t kBlueRed = 0x18;
constexpr std::uint32_t kBlueGreen = 0x50;
constexpr std::uint32_t kBlueBlue = 0x100;

inline std::uint32_t blue_flame(std::uint32_t rgba) {
    const std::uint32_t r = rgba & 0xFFu;
    const std::uint32_t g = (rgba >> 8) & 0xFFu;
    const std::uint32_t b = (rgba >> 16) & 0xFFu;
    const std::uint32_t i = std::max(r, std::max(g, b));
    const std::uint32_t red = i * kBlueRed / 0x80u;
    const std::uint32_t green = i * kBlueGreen / 0x80u;
    const std::uint32_t blue = std::min(0xFFu, i * kBlueBlue / 0x80u);
    return (rgba & 0xFF000000u) | (blue << 16) | (green << 8) | red;
}

// The Thruster-Pack flames' own code, recoloured while he charges: on around the flame's update
// (its sparks) and its draw (its quads), only during a charge, so every other effect drawn with the
// same routines keeps its colours.
class FlameTint {
public:
    // Around a call of the flame's code: on for it when he charges. Returns what to restore after.
    bool enter(bool charging) {
        const bool was = m_on;
        m_on = charging;
        return was;
    }
    void leave(bool was) { m_on = was; }
    bool on() const { return m_on; }

    // An effect quad's four colours (its record's +0x40..+0x4F), blue; what they were, for after.
    static std::array<std::uint32_t, 4> tint_quad(std::uint32_t (&colours)[4]) {
        const std::array<std::uint32_t, 4> was{colours[0], colours[1], colours[2], colours[3]};
        for (std::uint32_t& c : colours) {
            c = blue_flame(c);
        }
        return was;
    }
    static void restore_quad(std::uint32_t (&colours)[4], const std::array<std::uint32_t, 4>& was) {
        for (std::size_t i = 0; i < 4; ++i) {
            colours[i] = was[i];
        }
    }

private:
    bool m_on = false;
};

}  // namespace thruster_charge
