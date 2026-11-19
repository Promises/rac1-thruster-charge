// SPDX-License-Identifier: MIT
//
// Going Commando's Charge Boots in Ratchet & Clank: the rules, apart from the game.
//
// In Going Commando (games/rac2/decomp, "GC" below) a double tap of R1 on the ground, the second
// press within 30 frames of the first and held, puts Ratchet in his charge (state 0x7B, anim 0x8B;
// GC:src/levels/shared/L00_002C9448-g8.c:368-378). The charge (GC:src/levels/shared/L00_002BA7D8.c:
// 1969-2015) ignores the stick's direction: he goes where he faces, at 0.1917 units a frame
// (11.5 a second), and for its first 60 frames at 0.5 (30 a second) with a tenth of the turn. His
// speed rises toward that by 0.0278 a frame and falls by 0.00417. The stick across turns him,
// 0.02036 radians a frame times how far it is pushed, and leans him into the turn (27 times the
// turn, at most 1.7). He hovers 0.17 over the ground. Each moby in a sphere of 0.57 ahead of him
// takes a hit of mask 0x10000, the wrench's. R1 let go after 61 frames stops him (state 3, anim 6);
// running into a wall with less than three quarters of the speed he wants bonks him back
// (state 0x77). There is no jump out of a charge.
//
// Ratchet & Clank has a state that is nearly that already: the Thruster-Pack's hover (0x81), also a
// double tap of R1, with the same ground spring at +0.17 and the same lean from his velocity. The
// mod puts him in it, keeps him there while he charges, and steers him by Going Commando's numbers
// (mod.cpp says through which hook points). Going Commando counts frames at 60 a second; the game
// here gives each tick's length (g_frame_time, 1/50 a second on PAL), so the rules below are in
// seconds.
//
// Nothing here reads the game: mod.cpp does, and tests the rules on plain values.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "layout.hpp"

namespace charge_boots {

// Going Commando's numbers, a frame at 60 frames a second, in seconds.
constexpr float kGcFrame = 1.0f / 60.0f;
// The second press of the double tap: within 30 frames of the first.
constexpr float kDoubleTapWindow = 30 * kGcFrame;
// Speeds, units a second: the cruise (0.19166668 a frame) and the burst (0.5 a frame).
constexpr float kCruiseSpeed = 0.19166668f / kGcFrame;
constexpr float kBurstSpeed = 0.5f / kGcFrame;
// The burst: the first 60 frames.
constexpr float kBurstTime = 60 * kGcFrame;
// How fast his speed comes up to the one he wants (0.027777778 a frame, each frame) and down to it
// (0.004166667), units a second each second.
constexpr float kSpeedRise = 0.027777778f / (kGcFrame * kGcFrame);
constexpr float kSpeedFall = 0.004166667f / (kGcFrame * kGcFrame);
// The turn: 0.020362178 radians a frame at the stick pushed fully across (70 degrees a second),
// a tenth of it in the burst, none with the stick within 0.2 of the middle.
constexpr float kTurnRate = 0.020362178f / kGcFrame;
constexpr float kBurstTurn = 0.1f;
constexpr float kStickDeadZone = 0.2f;
// The lean into the turn: 27 times the turn of a frame, at most 1.7.
constexpr float kLeanPerTurn = 27.0f;
constexpr float kLeanMax = 1.7f;
// R1 let go ends the charge once it has lasted 61 frames; another may start 30 frames after.
constexpr float kMinChargeTime = 61 * kGcFrame;
constexpr float kLockout = 30 * kGcFrame;
// Ticks of a 60th or a 50th summed as floats come out a crumb short or over: the times above are
// compared with this much slack.
constexpr float kTimeSlack = 1e-4f;
// The bonk: at a wall, moving at less than three quarters of the speed he wants. Not below a walk
// (the pack jumps' bonk asks 7 units a second, hero_item_input_and_hud.c:1989): a charge only
// starting up has nothing to bonk with.
constexpr float kBonkRatio = 0.75f;
constexpr float kBonkMinSpeed = 5.0f;
// Off an edge: more than this over the ground and the charge is over (a fall). The hover sinks by
// itself above 0.7 (hero_water_and_ground_motion.c:2282); 2 is where the game's own falls count.
constexpr float kLeaveGround = 2.0f;
// The hit ahead of him: the wrench's mask and damage (bolts_and_wrench.c:2084), from hip height,
// over a reach of the sphere's 0.57 and the three frames' travel Going Commando looks ahead.
constexpr std::uint32_t kHitMask = 0x10000u;
constexpr float kHitDamage = 1.0f;
constexpr float kHitHeight = 0.5f;
constexpr float kHitRadius = 0.57f;
constexpr float kHitLookAhead = 3 * kGcFrame;
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

// The double tap of R1: a press while held within kDoubleTapWindow of the one before, as the
// game's own hover asks it (hero_gadget_use.c:1065-1079: the window counted down first, then the
// press looked at, then a press opens the window again).
class DoubleTap {
public:
    bool update(bool pressed, bool held, float dt) {
        m_left = std::max(0.0f, m_left - dt);
        if (m_left > kTimeSlack && pressed && held) {
            m_left = 0.0f;
            return true;
        }
        if (pressed) {
            m_left = kDoubleTapWindow;
        }
        return false;
    }

    void reset() { m_left = 0.0f; }

private:
    float m_left = 0.0f;
};

// Where he may start a charge from: on his feet, on the ground (the game's own gate for the
// hover, hero_gadget_use.c:1068-1071: idle, running, crouched, or just landed from a fall or a
// jump), not in a vehicle, not held by the Magneboots, not already hovering.
struct Footing {
    std::int32_t state;
    std::int32_t control_mode;
    std::int32_t grounded_frames;
    bool in_vehicle;
    bool magnetic;
};

inline bool can_start_from(const Footing& f) {
    if (f.in_vehicle || f.magnetic || f.state == kHover) {
        return false;
    }
    const std::int32_t m = f.control_mode;
    return m == kModeIdle || m == kModeRun || m == kModeCrouch ||
           ((m == kModeFall || m == kModeJump) && f.grounded_frames != 0);
}

// One charge: how long it has lasted, how fast he goes, and the lockout after it.
class Charge {
public:
    struct Step {
        // Units a second along his facing.
        float speed;
        // Radians to turn him this tick (positive to the left, as the game's yaw goes).
        float turn;
    };

    // speed_scale: the player's setting, times Going Commando's speeds.
    void set_speed_scale(float scale) { m_scale = std::isfinite(scale) ? std::clamp(scale, 0.25f, 2.0f) : 1.0f; }

    bool active() const { return m_active; }
    bool ready() const { return !m_active && m_lockout <= 0.0f; }
    float age() const { return m_age; }
    float speed() const { return m_speed; }

    // Starts at the planar speed he has (units a second).
    void start(float speed) {
        m_active = true;
        m_age = 0.0f;
        m_speed = std::isfinite(speed) ? std::max(0.0f, speed) : 0.0f;
    }

    // The speed he wants now.
    float wanted() const { return (in_burst() ? kBurstSpeed : kCruiseSpeed) * m_scale; }

    // The first 60 frames.
    bool in_burst() const { return m_age < kBurstTime - kTimeSlack; }

    // The tick's steering: his speed eased toward the one he wants, the turn from the stick across
    // (stick_x -1 left to 1 right) by how far it is pushed. Does not age the charge (advance does).
    Step step(float dt, float stick_x, float stick_magnitude) {
        const float want = wanted();
        if (m_speed < want) {
            m_speed = std::min(want, m_speed + kSpeedRise * dt);
        } else {
            m_speed = std::max(want, m_speed - kSpeedFall * dt);
        }
        float turn = 0.0f;
        if (stick_magnitude > kStickDeadZone) {
            const float across = std::clamp(stick_x, -1.0f, 1.0f);
            const float magnitude = std::min(stick_magnitude, 1.0f);
            // The stick to the right turns him clockwise seen from above: his yaw goes down.
            turn = -across * magnitude * kTurnRate * dt;
            if (in_burst()) {
                turn *= kBurstTurn;
            }
        }
        return {m_speed, turn};
    }

    // Once a tick, whether or not he was steered.
    void advance(float dt) {
        if (m_active) {
            m_age += dt;
        } else if (m_lockout > 0.0f) {
            m_lockout = std::max(0.0f, m_lockout - dt);
        }
    }

    // R1 let go, and the charge long enough to stop.
    bool released(bool r1_held) const { return m_active && !r1_held && m_age >= kMinChargeTime - kTimeSlack; }

    void end() {
        m_active = false;
        m_lockout = kLockout;
    }

private:
    bool m_active = false;
    float m_age = 0.0f;
    float m_speed = 0.0f;
    float m_lockout = 0.0f;
    float m_scale = 1.0f;
};

// The roll into a turn, for the hover's lean (Hero.motion.yaw_error): Going Commando's 27 times the
// turn of a 60th of a second, at most 1.7 either way.
inline float lean_for_turn(float turn, float dt) {
    if (!(dt > 0.0f)) {
        return 0.0f;
    }
    const float per_frame = turn * (kGcFrame / dt);
    return std::clamp(per_frame * kLeanPerTurn, -kLeanMax, kLeanMax);
}

// The bonk: a wall in his way, and he moved (units a second, after collision) at less than three
// quarters of the speed he was steered at.
inline bool bonks(bool wall_hit, float moved, float wanted) {
    return wall_hit && wanted >= kBonkMinSpeed && moved < kBonkRatio * wanted;
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
// own call.
//
// - The mod's own calls go through.
// - The hover is the charge's: the game asking for it by itself (the Thruster-Pack's double tap,
//   hero_gadget_use.c:1061-1079 and each level family's copy; idle with the hover latch set,
//   hero_item_input_and_hud.c:180-186) is refused. But the end of a hurt asks for it when the hurt
//   came in the hover (hero_item_input_and_hud.c:2241-2244) and asks again each tick it is
//   refused: that one is idle instead.
// - During a charge, the hover's own ways out to idle are refused: without the Thruster-Pack it
//   lets go after 20 ticks, and X or a double tap ends it with one (hero_item_input_and_hud.c:
//   1761-1800). A cutscene's call (force 2) is not.
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
