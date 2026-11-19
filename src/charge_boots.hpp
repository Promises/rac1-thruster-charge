// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Charge Boots' rules that are this mod's, not Going Commando's: what owns the boots, which
// of the game's own changes of state a charge refuses, how often a moby takes the charge's hit,
// and the tick's length. The charge itself is Going Commando's code (src/gc/), run on Ratchet &
// Clank's hero by the translation layer (src/gc/rac1_shim.hpp).
//
// Nothing here reads the game: mod.cpp does, and the tests test the rules on plain values.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "layout.hpp"

namespace charge_boots {

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

// The boots, owned: with the game in progress, in the mod's save data beside its save.
class Ownership {
public:
    // The record: "CB", its version, and a byte of flags (1: owned).
    static std::vector<std::uint8_t> encode(bool owned) {
        return {'C', 'B', kVersion, static_cast<std::uint8_t>(owned ? kOwned : 0)};
    }

    static bool decode(const std::vector<std::uint8_t>& bytes) {
        return bytes.size() >= 4 && bytes[0] == 'C' && bytes[1] == 'B' && bytes[2] == kVersion &&
               (bytes[3] & kOwned) != 0;
    }

    // A game loaded or started: what its save data says.
    void restore(const std::vector<std::uint8_t>& bytes) { m_saved = decode(bytes); }

    // Each tick, whether the game's save owns the item that brings the boots. True when that just
    // made them his: the record is then to be written.
    bool observe(bool item_owned) {
        if (item_owned && !m_saved) {
            m_saved = true;
            return true;
        }
        return false;
    }

    bool owned(bool always_on) const { return always_on || m_saved; }

private:
    static constexpr std::uint8_t kVersion = 1;
    static constexpr std::uint8_t kOwned = 1;
    bool m_saved = false;
};

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

}  // namespace charge_boots
