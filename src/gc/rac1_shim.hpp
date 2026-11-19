// SPDX-License-Identifier: GPL-3.0-or-later
//
// The translation layer: Going Commando's charge code (src/gc/*.c, copied from rac2-decomp) run on
// Ratchet & Clank's hero.
//
// The copied code reads and writes Going Commando's hero at Going Commando's offsets, and calls
// Going Commando's functions. Here its hero is a copy (gc_shim_hero) that Shim::load fills from
// Ratchet & Clank's hero by kHeroFields, and Shim::flush writes back, field by field, only what the
// copied code changed. Its functions are polyfills (rac1_shim.cpp): each one is either Ratchet &
// Clank's own function for the job, called through a hook point's entry (Hero_ApproachSpeed,
// Hero_SetPlanarVelocity, Hero_TurnToHeading, Hero_EnterMoveState), or a stand-in built on the mod
// SDK (collision_line, moby_hit, the hero's sounds), or nothing; its comment says which and what
// differs from Going Commando.
//
// Time: Going Commando counts frames at 60 a second; Ratchet & Clank's tick is g_frame_time long
// (a 50th on PAL, a 60th on NTSC). k is a tick in Going Commando's frames (frame_time * 60, 1.2 on
// PAL), and it scales as Ratchet & Clank's own code does with frame_scale: a value per frame (a
// speed, a turn) is k times as much a tick, an acceleration k * k times, a count of frames 1 / k
// times as many ticks. The copy holds per-frame values in Going Commando's units (Unit::PerFrame,
// Unit::Frames); the polyfills scale the rates they are given. Arithmetic the copied code does
// itself on those units is not rescaled: where it integrates (the bonk's fall), it is off by k.
//
// Nothing here includes the SDK: mod.cpp fills Game from it, and the tests from a fake.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include "../thruster_charge.hpp"

namespace thruster_charge::gc {

// --- Going Commando's states, control modes and animations, and Ratchet & Clank's for them ---

// The charge (Going Commando's state 0x7B, control mode 28, anim 0x8B).
constexpr std::int32_t kGcCharge = 0x7B;
constexpr std::int32_t kGcChargeControlMode = 28;
// The bonk off a wall at the end of a charge (0x77, control mode 10, anim 0x29).
constexpr std::int32_t kGcBonk = 0x77;
constexpr std::int32_t kGcBonkControlMode = 10;
// The stop (3, anim 6).
constexpr std::int32_t kGcStop = 3;

// A state of Going Commando's and the one of Ratchet & Clank's that stands in for it.
struct StateMap {
    std::int32_t gc;
    std::int32_t rac1;
};

// The charge runs in the Thruster-Pack's hover (0x81): the same double tap of R1, the same ground
// spring at +0.17, a lean from his speed; the bonk in the game's own wall bonk (0x7A, anim 0x29,
// pushed back at 9 units a second as Going Commando's); the stop in the stop (3).
inline constexpr std::array<StateMap, 3> kStates{{
    {kGcCharge, kHover},
    {kGcBonk, kWallBonk},
    {kGcStop, kStop},
}};

// Ratchet & Clank's state for Going Commando's, -1 for one the layer does not know.
inline std::int32_t rac1_state(std::int32_t gc) {
    for (const StateMap& s : kStates) {
        if (s.gc == gc) {
            return s.rac1;
        }
    }
    return -1;
}

// Going Commando's state for Ratchet & Clank's, as the copied code is to see it: the hover is the
// charge only while it is one, the wall bonk the charge's bonk only after one; any other state is
// its own number (the copied code compares it with none but 1 and 29: the look, as in Going
// Commando, and a state the charge never starts from).
inline std::int32_t gc_state(std::int32_t rac1, bool charging, bool bonking) {
    if (charging && rac1 == kHover) {
        return kGcCharge;
    }
    if (bonking && rac1 == kWallBonk) {
        return kGcBonk;
    }
    return rac1;
}

// Going Commando's control mode for Ratchet & Clank's: the charge's and the bonk's own while he
// is in them (no control mode of Ratchet & Clank's says "charging"), else the same number (the
// trigger's gate, idle 0, run 1, fall 2, jump 4, crouch 12, is Ratchet & Clank's hover gate too:
// hero_gadget_use.c:1068-1071).
inline std::int32_t gc_control_mode(std::int32_t rac1_mode, std::int32_t gc_state_now) {
    if (gc_state_now == kGcCharge) {
        return kGcChargeControlMode;
    }
    if (gc_state_now == kGcBonk) {
        return kGcBonkControlMode;
    }
    return rac1_mode;
}

// An animation of Going Commando's and Ratchet & Clank's in its place; -1: the state's own, which
// Ratchet & Clank's change of state starts (nothing more to play).
struct AnimationMap {
    std::int32_t gc;
    std::int32_t rac1;
};

// The charge's pose: the Thruster-Pack long jump's (state 0x10, R1 or R2 and X), animation 0x26,
// which its entry starts (HeroEnterMoveState, hero_vehicle_and_collision.c:1997-2004:
// set_hero_animation(0x26, 0, scale_ticks(5 or 8))). The hover's entry starts its own (0x13,
// hero_vehicle_and_collision.c:1509-1511) first; the charge's entry then plays this one over it.
constexpr std::int32_t kChargePose = 0x26;

inline constexpr std::array<AnimationMap, 3> kAnimations{{
    {0x8B, kChargePose},  // the charge: the long jump's
    {0x29, -1},           // the bonk: the wall bonk's own (0x29 in both games)
    {0x06, -1},           // the stop's skid: the stop state's own
}};

inline std::int32_t rac1_animation(std::int32_t gc) {
    for (const AnimationMap& a : kAnimations) {
        if (a.gc == gc) {
            return a.rac1;
        }
    }
    return -1;
}

// The long jump is one jump: its animation plays once, then wraps to its first frame
// (advance_hero_animation, hero_mapped_animations.c:474-...). Its state's own numbers place the
// parts of it (HeroEnterMoveState, hero_vehicle_and_collision.c:1795-1806, and its movement,
// hero_water_and_ground_motion.c:2520-2540): the take-off until about frame 8 (its hits start 8
// ticks in, once he is off the ground), the apex at 18, its hits until 28, the landing's target at
// 30. A charge lasts longer than that, so it plays the middle only:
//  - set_hero_animation starts it at kChargePoseStart, the first frame after the take-off (its
//    second argument is the start frame: it writes Moby.prev_frame, +0x51);
//  - it then loops pose_loop_from..pose_loop_to (default 12..24, about the apex, short of the hits'
//    end at 28 and the landing) by Ratchet & Clank's own hero_queue_sequence
//    (hero_mapped_animations.c:428-437). Past `to`, advance_hero_animation blends back to `from`
//    over three ticks (it sets the frame rate to a third for that frame): the smoothest jump back
//    the game has. Wrapping the frame by hand would snap in one tick, and the advance cannot play
//    a sequence backwards, so no ping-pong.
//  - hero_queue_sequence refuses a `to` the animation does not have (it compares with its
//    AnimSeq.frame_count, +0x10), and the animation then plays to its end and starts over: so both
//    are clamped to its frames, which the mod logs once for tuning.
// from == to holds that one frame still instead (Hero.anim_speed, +0xA90, at 0 while he charges).
// These numbers are the long jump state's, in its anim_frame; not seen in this animation yet.
constexpr std::int32_t kChargePoseStart = 8;
constexpr std::int32_t kChargePoseFrom = 12;
constexpr std::int32_t kChargePoseTo = 24;

// The frames to hold for the settings, in an animation of `frame_count` frames: both within it,
// to no earlier than from. frame_count 0 (unknown): as asked.
inline std::pair<std::int32_t, std::int32_t> pose_frames(std::int32_t from, std::int32_t to, std::int32_t frame_count) {
    if (frame_count > 0) {
        from = std::clamp(from, 0, frame_count - 1);
        to = std::clamp(to, 0, frame_count - 1);
    } else {
        from = std::max(from, 0);
        to = std::max(to, 0);
    }
    return {from, std::max(from, to)};
}

// The frame the charge's animation starts at: kChargePoseStart, but not past the loop's start; a
// frame held still, that frame.
inline std::int32_t pose_start(std::int32_t from, std::int32_t to) {
    return from == to ? from : std::min(kChargePoseStart, from);
}

// The charge's length. Going Commando's charge stops once R1 is let go and it has lasted 61 frames
// (hero_l00_decide_state: `!(R1 held) && frames >= 0x3D`): a double tap let go at once gives
// exactly 61 frames, its burst (0.5 a frame for 60 frames) and one frame of the cruise. Here a
// double tap is always that tap: the copied decider sees R1 held for charge_time, then let go, so
// the charge lasts charge_time, and never less than those 61 frames, which Going Commando cannot
// stop sooner.
constexpr std::int32_t kGcMinChargeFrames = 0x3D;
constexpr float kChargeTimeDefault = kGcMinChargeFrames / 60.0f;

// The charge's length in Going Commando's frames for a charge_time in seconds.
inline std::int32_t charge_frames(float seconds) {
    if (!std::isfinite(seconds)) {
        return kGcMinChargeFrames;
    }
    return std::max(kGcMinChargeFrames, static_cast<std::int32_t>(std::lround(seconds * 60.0f)));
}

// Whether the copied decider is to see R1 held: while the charge is shorter than its length.
inline bool charge_holds_r1(std::int32_t frames, std::int32_t length) { return frames < length; }

// What an animation brings besides its pose (advance_hero_animation): a looping sound of its own
// (its AnimSeq +0x11, which set_hero_animation puts in Moby +0x7C and the advance keeps playing)
// and sounds at frames (its AnimSeq +0x12 triggers, which the advance copies to Moby +0x7E when it
// takes the animation up and plays as the frames pass them). The long jump's are a jump's. The
// charge keeps the hover's instead, as up to 0.3.0: the hover animation's (0x13) looping sound in
// +0x7C, no triggers (+0x7E held at 0), and the hover's hum (kChargeSound, Shim::keep_sound).
constexpr std::int32_t kHoverAnimation = 0x13;
constexpr std::uint32_t kMobyClass = 0x24;
constexpr std::uint32_t kClassSequences = 0x48;
constexpr std::uint32_t kSequenceFrameCount = 0x10;
constexpr std::uint32_t kSequenceSound = 0x11;
constexpr std::uint32_t kMobyStartFrame = 0x51;
constexpr std::uint32_t kMobySound = 0x7C;
constexpr std::uint32_t kMobyTriggers = 0x7E;
constexpr std::uint32_t kHeroAnimSpeed = 0xA90;

// The Thruster-Pack's flames (class 0xA7 mobys, thruster_pack_flame_update, visibomb_and_pyrocitor.c:
// 897-1000) show in the hover, so during the charge. Each draws its flame in
// draw_joint_aligned_beam_effect (visibomb_and_pyrocitor.c:621-757): two quads of effect texture 0xC
// in a neutral colour (0x80808080), a glow quad of texture 0xB tinted 0x7F2020FF, and a ribbon of 196
// points of texture 0xA in the colours of the table joint_beam_ribbon_colors (PAL D_L00_001DA9D0,
// NTSC D_L00_001DA6D0), which nothing else draws with. The table is data: during a charge it is
// tinted blue here (blue_flame, thruster_charge.hpp). The quads, the glow and the sparks
// (emit_beam_trail_particles' spawn_burst, 0x4F007FFF fading to 0x1FFFFFFF) are recoloured in
// mod.cpp's effect hooks, by FlameTint.
constexpr std::size_t kRibbonColours = 196;

// Ratchet's hover hum (his class's sound 0x12, which the hover loops in voice slot 4): Going
// Commando's charge loops its own sound 0xD there, which Ratchet & Clank does not have.
constexpr std::int32_t kChargeSound = 0x12;

// --- The hero's fields ---

enum class Kind : std::uint8_t { F32, S32, S16, U8 };

inline constexpr std::size_t size_of(Kind k) {
    return k == Kind::F32 || k == Kind::S32 ? 4 : k == Kind::S16 ? 2 : 1;
}

enum class Unit : std::uint8_t {
    // The same in both games: positions, angles, flags.
    None,
    // A value per frame (a speed, a turn rate): Ratchet & Clank's per tick is k times it.
    PerFrame,
    // A count of frames: Ratchet & Clank counts 1 / k times as many ticks.
    Frames,
};

enum class Access : std::uint8_t {
    // Copied in only: what the copied code writes there stays in the copy.
    Read,
    // Copied in, and back out when the copied code changed it.
    ReadWrite,
};

struct Field {
    // Going Commando's offset and type.
    std::uint32_t gc;
    Kind gc_kind;
    // Ratchet & Clank's (include/structs/hero.h).
    std::uint32_t rac1;
    Kind rac1_kind;
    // Elements (a vector's four floats).
    std::uint8_t count;
    Unit unit;
    Access access;
};

// The copy's size: rac2-decomp's Hero union is 0x980 words, and its ptr[] member is 0x980
// pointers, 8 bytes each on a 64-bit host.
constexpr std::size_t kHeroBytes = 0x980 * sizeof(void*);
// Going Commando's state and control mode, and the hero's moby (translated, not by the table).
constexpr std::uint32_t kGcMoby = 0x2290;
constexpr std::uint32_t kGcState = 0x2294;
constexpr std::uint32_t kGcControlMode = 0x229C;
// The moby as the copied code's Hero union reads it: ptr[0x2290 / 4], a host pointer.
constexpr std::uint32_t kGcMobyPointer = kGcMoby / 4 * sizeof(void*);
// The target speed: the `speed` setting scales what the charge writes there.
constexpr std::uint32_t kGcTargetSpeed = 0x1B0;
// The stick, smoothed (pad_clamp_stick_to_unit_range: +4 the y, +8 the x, -1 to 1); kept in
// the copy only.
constexpr std::uint32_t kGcStickSmooth = 0x9F0;
// The stick as read (x, y; Ratchet & Clank's +0x1D20).
constexpr std::uint32_t kGcStick = 0x1FB0;

// Going Commando's hero field by field, and where Ratchet & Clank keeps the same thing. Matched by
// what the code does with them in both games: rac2-decomp names them in hero_compute_push_direction
// (yaw +0x98, speed +0x1B4, pitch +0x324, flag +0x22C3), hero_approach_speed (+0x1B0, +0x1B4),
// hero_steer_heading (+0x98, +0x1A0..+0x1A8) and ProcessHeroEquippedItemInput (its struct); the
// rest by the same shift as their neighbours (+0x10 for the motion vectors, +0x20 from +0x190,
// +0x30/+0x40 from +0x250, +0x210 for the state block) and by use. "Inferred" marks a field matched
// by the shift and its use only.
inline constexpr std::array<Field, 28> kHeroFields{{
    // The matrix: VEC(0) is his forward row (the bonk turns away from it).
    {0x000, Kind::F32, 0x000, Kind::F32, 16, Unit::None, Access::Read},
    // motion.pos (the ground spring moves z, +0x88).
    {0x080, Kind::F32, 0x080, Kind::F32, 4, Unit::None, Access::ReadWrite},
    // motion.rot (z, +0x98: his facing, which the charge turns).
    {0x090, Kind::F32, 0x090, Kind::F32, 4, Unit::None, Access::ReadWrite},
    // motion.body_center: the hit sphere's start (inferred).
    {0x0E0, Kind::F32, 0x0D0, Kind::F32, 4, Unit::None, Access::Read},
    // motion.velocity (hero_compute_push_direction writes D_00189F10, the hero + 0xF0).
    {0x0F0, Kind::F32, 0x0E0, Kind::F32, 4, Unit::PerFrame, Access::ReadWrite},
    // motion.displacement: how far he moved, after collision.
    {0x110, Kind::F32, 0x100, Kind::F32, 4, Unit::PerFrame, Access::Read},
    // motion.resolved_velocity (the bonk's fall reads its z, +0x128).
    {0x120, Kind::F32, 0x110, Kind::F32, 4, Unit::PerFrame, Access::Read},
    // motion.move_dir (the bonk on magnetic ground writes it).
    {0x190, Kind::F32, 0x170, Kind::F32, 4, Unit::None, Access::ReadWrite},
    // motion.move_yaw, yaw_rate, yaw_error (hero_steer_heading).
    {0x1A0, Kind::F32, 0x180, Kind::F32, 1, Unit::None, Access::ReadWrite},
    {0x1A4, Kind::F32, 0x184, Kind::F32, 1, Unit::PerFrame, Access::ReadWrite},
    {0x1A8, Kind::F32, 0x188, Kind::F32, 1, Unit::None, Access::ReadWrite},
    // target_speed and ground_speed (hero_approach_speed).
    {0x1B0, Kind::F32, 0x190, Kind::F32, 1, Unit::PerFrame, Access::ReadWrite},
    {0x1B4, Kind::F32, 0x194, Kind::F32, 1, Unit::PerFrame, Access::ReadWrite},
    // state_timer: frames in the state (the burst's 60, the stop's 61; ProcessHeroEquippedItemInput's
    // `frame`).
    {0x1B8, Kind::S32, 0x198, Kind::S32, 1, Unit::Frames, Access::Read},
    // move_lock_timer: the stop's 30 frames before he moves again (inferred).
    {0x1E0, Kind::S16, 0x1C4, Kind::S32, 1, Unit::Frames, Access::ReadWrite},
    // capsule_radius: where the reach probe starts (hero_test_reach_blend's D_0018A084, the hero +
    // 0x264; inferred).
    {0x264, Kind::F32, 0x234, Kind::F32, 1, Unit::None, Access::Read},
    // wall_hit: the wall probe found a wall in his way.
    {0x287, Kind::U8, 0x257, Kind::U8, 1, Unit::None, Access::Read},
    // ground_point (the spring on magnetic ground; inferred).
    {0x2D0, Kind::F32, 0x2A0, Kind::F32, 4, Unit::None, Access::Read},
    // ground_z and ground_distance (the spring's goal; the bonk's "in the air").
    {0x318, Kind::F32, 0x2D8, Kind::F32, 1, Unit::None, Access::Read},
    {0x31C, Kind::F32, 0x2DC, Kind::F32, 1, Unit::None, Access::Read},
    // grounded_frames (ProcessHeroEquippedItemInput's `action_ready`: landed).
    {0x340, Kind::S32, 0x300, Kind::S32, 1, Unit::None, Access::Read},
    // unk308: the Magneboots hold him (the charge stops; inferred).
    {0x348, Kind::S16, 0x308, Kind::S16, 1, Unit::None, Access::Read},
    // ground_spring_velocity (the ground spring's).
    {0xA00, Kind::F32, 0x304, Kind::F32, 1, Unit::PerFrame, Access::ReadWrite},
    // stick_x, stick_y.
    {kGcStick, Kind::F32, 0x1D20, Kind::F32, 2, Unit::None, Access::Read},
    // vehicle_mode.
    {0x22B4, Kind::U8, 0x20A4, Kind::U8, 1, Unit::None, Access::Read},
    // item_anim_active (ProcessHeroEquippedItemInput's `changing_weapon`: the charge stops).
    {0x22B8, Kind::U8, 0x20A8, Kind::U8, 1, Unit::None, Access::Read},
    // gravity_mode (hero_compute_push_direction's flag).
    {0x22C3, Kind::U8, 0x20B3, Kind::U8, 1, Unit::None, Access::Read},
    // stick_magnitude.
    {0x24B8, Kind::F32, 0x229C, Kind::F32, 1, Unit::None, Access::Read},
}};

// Going Commando's body-tilt records (D_L20_00267210, and the cam_set_limits_* table of rac2-decomp,
// whose 0xB0-byte rows hold their spring at +0xA4, +0xA8) are Ratchet & Clank's hero_eases, same
// layout (HeroEase: rot_target at +0x60, accel +0xA4, max_speed +0xA8): four of them.
constexpr std::size_t kLeanRecord = 0xB0;
constexpr std::size_t kLeanBytes = 4 * kLeanRecord;
constexpr std::size_t kPadBytes = 0x200;

// --- The game, as the translation layer reaches it ---

// Game memory: game address a is base + a.
struct Memory {
    std::uint8_t* base = nullptr;

    template <class T>
    T read(std::uint32_t address) const {
        T value;
        std::memcpy(&value, base + address, sizeof value);
        return value;
    }

    template <class T>
    void write(std::uint32_t address, const T& value) const {
        std::memcpy(base + address, &value, sizeof value);
    }
};

// A hit, as the SDK's moby_hit takes it.
struct Hit {
    std::int32_t victim;
    std::int32_t attacker;
    std::uint32_t mask;
    float damage;
    float position[3];
    float velocity[3];
};

// What the polyfills call in the game. mod.cpp fills it from the SDK; a test from a fake.
struct Game {
    Memory memory;
    // g_hero, the pad's state, and the tilt records in the level loaded now (0: none).
    std::uint32_t hero = 0;
    std::uint32_t pad = 0;
    std::uint32_t lean = 0;
    // Ratchet & Clank's speed_step (Hero_ApproachSpeed): ground speed toward target speed.
    std::function<void(float up, float down)> approach_speed;
    // set_planar_vel (Hero_SetPlanarVelocity): velocity at his ground speed; above pi his facing.
    std::function<void(float angle)> set_planar_velocity;
    // turn_to (Hero_TurnToHeading): facing toward move yaw.
    std::function<void(std::int32_t mode, float accel, float damping, float limit)> turn_to_heading;
    // HeroEnterMoveState (Hero_EnterMoveState), as the mod's own call.
    std::function<std::int32_t(std::int32_t state, std::int32_t force)> enter_move_state;
    // The moby slot the line from..to meets first (not the hero), -1 for none.
    std::function<std::int32_t(const float* from, const float* to)> moby_on_line;
    // Whether the line from..to meets the level.
    std::function<bool(const float* from, const float* to)> level_on_line;
    std::function<void(const Hit&)> hit;
    // The hero's moby slot.
    std::function<std::int32_t()> self_slot;
    // One of the hero's own sounds (looped): its voice, -1 for none; and its stop.
    std::function<std::int32_t(std::int32_t sound, bool loop)> sound_play;
    std::function<void(std::int32_t voice)> sound_stop;
    std::function<void(const char*)> log;
    // A note for the player's log (not a warning).
    std::function<void(const char*)> info;
    // set_hero_animation(animation, blend, frames) and hero_queue_sequence(from, to), Ratchet &
    // Clank's own (mod.cpp finds them by name).
    std::function<void(std::int32_t animation, std::int32_t blend, float frames)> set_animation;
    std::function<void(std::int32_t from, std::int32_t to)> loop_animation;
    // The flames' ribbon colours in the level loaded now (0: none).
    std::uint32_t ribbon = 0;
};

class Shim {
public:
    Game game;

    // Each tick, before the copied code runs: the tick's length in seconds.
    void set_tick(float frame_time);
    // A tick in Going Commando's frames.
    float k() const { return m_k; }

    // The player's setting.
    void set_speed_scale(float scale);

    // Going Commando's hero from Ratchet & Clank's (and the pad, the tilt records, the globals).
    void load();
    // What the copied code changed, back to Ratchet & Clank's hero.
    void flush();

    // The game's state each tick: a charge he was taken out of (hurt, a cutscene) is over.
    void observe();
    bool charging() const { return m_charging; }
    bool bonking() const { return m_bonking; }
    // Whether the copied state machine moves him this tick, in place of Ratchet & Clank's.
    bool drives_movement() const;
    // After his update: the charge's sound stopped when nothing kept it this tick.
    void end_tick();
    // A game loaded or a level left: no charge.
    void reset();

    // --- For the polyfills ---

    // Ratchet & Clank's change of state for Going Commando's (gc_shim_enter_state).
    std::int32_t enter_state(std::int32_t gc_action);
    // An address the copied code passed as a 32-bit handle, in the layer's buffers; nullptr when it
    // is none of them.
    void* resolve(std::int32_t handle) const;
    // Whether no two of those buffers share their low 32 bits (else handles would be ambiguous).
    bool handles_unambiguous() const;
    // A call into the game: the copy flushed before it and loaded after.
    template <class F>
    void call_game(F&& f) {
        flush();
        f();
        load();
    }
    // The charge's looping sound, kept this tick.
    void keep_sound();
    // Going Commando's animation in Ratchet & Clank's terms (gc_shim_set_hero_animation).
    void play_animation(std::int32_t gc_animation, std::int32_t blend, float frames);
    // The charge's pose settings (frames of the long jump's animation; from == to: frozen).
    void set_pose(std::int32_t from, std::int32_t to);
    // The charge's length (charge_time, seconds).
    void set_charge_time(float seconds) { m_charge_frames = charge_frames(seconds); }
    // Each tick of a charge, after the copied movement: the pose held, the long jump's sounds off.
    void hold_pose();
    // Going Commando's frame timer decremented by a tick (math_fast_float_d).
    std::int32_t step_frame_timer(std::int32_t* timer);
    // Whether the hero's items' input runs (ProcessHeroEquippedItemInput's early returns): his feet
    // on plain ground, the Thruster-Pack on.
    bool item_input_allowed() const;
    // Whether the Thruster-Pack is on his back and not switched off: what makes the charge his. In
    // Going Commando the Charge Boots owned (D_001A7B08[0x26]) and worn (D_001A7390[3] == 54);
    // here the hover's own gate (hero_slot_item_id(3) == 3 and pack_disabled 0,
    // hero_gadget_use.c:1061-1064).
    bool thruster_on() const;
    // Once for each polyfill reached that should not be.
    void unreachable(const char* what);
    HitCooldown& hits() { return m_hits; }
    float clock() const { return m_clock; }

private:
    void end_charge();
    // The flames' ribbon blue while he charges, and back.
    void tint_flames();
    void untint_flames();
    std::int32_t rac1_state_now() const;

    float m_k = 1.0f;
    float m_clock = 0.0f;
    float m_speed_scale = 1.0f;
    bool m_charging = false;
    bool m_bonking = false;
    bool m_loaded = false;
    bool m_sound_kept = false;
    std::int32_t m_voice = -1;
    float m_timer_carry = 0.0f;
    HitCooldown m_hits;
    std::uint32_t m_unreachable_logged = 0;
    // The ribbon's colours as they were, and where (0: not tinted).
    std::array<std::uint32_t, kRibbonColours> m_ribbon_saved{};
    std::uint32_t m_ribbon_tinted = 0;
    std::int32_t m_pose_from = kChargePoseFrom;
    std::int32_t m_pose_to = kChargePoseTo;
    // The pose being held (and frozen), since the charge's animation started.
    bool m_pose = false;
    bool m_pose_frozen = false;
    // The long jump animation's frame count last logged (-1: none yet).
    std::int32_t m_pose_frames_logged = -1;
    std::int32_t m_charge_frames = kGcMinChargeFrames;
};

Shim& shim();

}  // namespace thruster_charge::gc

// The copied functions, and the layer's buffers they work on (src/gc/*.c, gc/shim/common.h).
extern "C" {
void hero_l00_update_state_machine(void);
std::int32_t hero_l00_set_action(std::int32_t action, std::int32_t animate);
void hero_l00_decide_state(void);
int ProcessHeroEquippedItemInput(void);

extern unsigned char gc_shim_hero[];
extern unsigned char gc_shim_lean[];
extern unsigned char gc_shim_pad[];
extern std::int32_t gc_shim_gravity_flag;
extern std::int32_t gc_shim_hits[];
extern unsigned char gc_shim_items_owned[];
extern std::uint32_t gc_shim_item_slots[];
extern std::int32_t gc_shim_double_tap_timer;
}
