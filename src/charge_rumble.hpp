// SPDX-License-Identifier: GPL-3.0-or-later
//
// The charge's rumble: a short burst as a charge starts, then a very low, steady hold for as long
// as it lasts, nothing after. Going Commando's Charge Boots do not rumble; this is the mod's own.
//
// It plays only through the port's set_motors (mod interface 1.37, motors.hpp) and only with the
// Rumble mod (manifest id "rumble") loaded, scaled by that mod's strength, as the player's
// controller rumble is that mod's to set; and only while the mod's own "rumble" setting is on.
// Otherwise the mod sends nothing at all, as before it had rumble.
//
// Nothing here reads the game or the SDK: mod.cpp does, and the tests test the rules on plain values.
#pragma once

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace thruster_charge {

// The Rumble mod's manifest id, and its strength setting's key (percent, 0 to 100).
constexpr const char* kRumbleModId = "rumble";
constexpr const char* kRumbleStrengthKey = "strength";

// The burst: the large motor from kBurstLarge eased down to the hold over kBurstSeconds, the small
// motor on for its first kBurstSmallSeconds (the first three ticks at 50 or 60 a second).
constexpr int kBurstLarge = 200;
constexpr float kBurstSeconds = 0.15f;
constexpr float kBurstSmallSeconds = 0.05f;
// The hold: the large motor alone, very low.
constexpr int kHoldLarge = 30;
// The small motor is only on or off: on while 255 times the strength is at least this, times 100
// (about 38% and up), as the Rumble mod has it (its kSmallOn), so the two drop it out alike.
constexpr int kSmallOn = 96;

// What to send to the motors after a tick: nothing (send false), or small 0/1 and large 0..255.
struct Motors {
    bool send = false;
    int small = 0;
    int large = 0;
};

// The Rumble mod's strength from its setting's text ("100"): 0 to 100, 100 for anything that is
// not a number.
inline int rumble_strength(const char* value) {
    if (value == nullptr || *value == '\0') {
        return 100;
    }
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || errno != 0) {
        return 100;
    }
    return static_cast<int>(std::clamp(v, 0L, 100L));
}

// Whether the charge rumbles at all: the port has set_motors, the Rumble mod is loaded and the mod's
// own "rumble" setting is on.
inline bool rumble_able(bool has_set_motors, bool rumble_mod_loaded, bool setting) {
    return has_set_motors && rumble_mod_loaded && setting;
}

// What the charge sends to the motors each tick.
class ChargeRumble {
public:
    // After each tick. able: the port has set_motors, the Rumble mod is loaded and the mod's own
    // setting is on; playing: a level in play with the player in control (not paused, in a menu,
    // a scene or a load); charging: a charge on; tick: seconds a tick; strength: the Rumble mod's
    // (percent).
    //
    // While all of them hold it sends every tick (the port holds a command 0.2 s); the tick one of
    // them stops holding, one 0, 0; then nothing. A pause holds the charge's time: after it the
    // burst goes on where it was, or the hold. Turned on in the middle of a charge, it starts
    // where the charge is (past the burst, the hold).
    Motors tick(bool able, bool playing, bool charging, float tick, int strength) {
        if (!charging) {
            m_elapsed = 0.0f;
        }
        if (!(playing && charging)) {
            return stop();
        }
        // The charge's time counts in play, rumbling or not.
        const float elapsed = m_elapsed;
        m_elapsed += std::isfinite(tick) && tick > 0.0f ? tick : 0.0f;
        if (!able) {
            return stop();
        }
        Motors out = scaled(elapsed, strength);
        out.send = true;
        m_sent = out.small != 0 || out.large != 0;
        if (!m_sent) {
            // Strength 0: the motors stay as they are, off.
            out.send = false;
        }
        return out;
    }

    // One 0, 0 when the last tick sent something (the charge's end, a pause, the mod's unload);
    // else nothing.
    Motors stop() {
        Motors out;
        out.send = m_sent;
        m_sent = false;
        return out;
    }

    // The burst and the hold, `elapsed` seconds into the charge, at full strength.
    static Motors at(float elapsed) {
        Motors out;
        if (elapsed < kBurstSeconds) {
            const float left = 1.0f - elapsed / kBurstSeconds;
            out.large = kHoldLarge + static_cast<int>(std::lround((kBurstLarge - kHoldLarge) * left * left));
            out.small = elapsed < kBurstSmallSeconds ? 1 : 0;
        } else {
            out.large = kHoldLarge;
        }
        return out;
    }

    // at(elapsed) at `strength` percent: the large motor scaled, the small one dropped below kSmallOn.
    static Motors scaled(float elapsed, int strength) {
        const int s = std::clamp(strength, 0, 100);
        Motors out = at(elapsed);
        out.large = (out.large * s + 50) / 100;
        out.small = out.small != 0 && 255 * s >= kSmallOn * 100 ? 1 : 0;
        return out;
    }

private:
    // Seconds into the charge, in play.
    float m_elapsed = 0.0f;
    // Whether the last tick sent the motors something other than 0, 0.
    bool m_sent = false;
};

}  // namespace thruster_charge
