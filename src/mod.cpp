// SPDX-License-Identifier: MIT
//
// Charge Boots: Going Commando's Charge Boots in Ratchet & Clank (charge_boots.hpp says what they
// do there and how the rules map onto this game).
//
// Four hook points:
//   rac1.player.update                   each tick, around the hero's update: before it, the double
//                                        tap starts a charge (the hover, state 0x81, entered as the
//                                        game enters it) and R1 let go stops it; after it, the bonk
//                                        at a wall and the hits ahead of him
//   rac1.player.enter_move_state         keeps the hover the charge's: the game's own way into it
//                                        (the Thruster-Pack's double tap) refused, its ways out
//                                        refused while he charges (charge_boots::judge)
//   rac1.player.steer_planar_velocity    in a charge, the hover's steering: along his facing, at
//                                        Going Commando's speed, turned by the stick across; the
//                                        game's function then sets the velocity, keeping its
//                                        vertical part, and its collision clips it after
//   rac1.player.turn_to_heading          in a charge, off: the stick's direction does not turn him
// The steering one is new in OpenRAC (branch rac1-steer-hook), declared in steer_hook.hpp until the
// SDK has it; a game without it refuses the mod with the reason.
//
// The boots are his once the game in progress owns the Persuader (layout.hpp says why that item),
// kept in the mod's save data beside the save, or from the start with the setting always_on.
//
// TODO(flames): Going Commando's boots spout four flame particles a frame from the feet (joints
// 0x16 and 0x17 of its Ratchet, GC:src/levels/shared/L00_002CB3A8-decider.c:892-937) and loop
// sound 0xD. Props need a .glb of the mod's (prop_create has no built-in shapes) and this game's
// Ratchet has other joints: a flame model and texture and the sound, exported from the disc, are a
// later step. The hover's own body lean (forward from his speed, a roll from yaw_error) stands in
// for Going Commando's anim 0x8B, so no hero_set_joints either.
#include <cmath>
#include <cstdint>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

#include "charge_boots.hpp"
#include "layout.hpp"
#include "steer_hook.hpp"

namespace {

namespace cb = charge_boots;
namespace rac1 = openrac::rac1;

// TODO(steer hook): openrac::rac1::Hero_SteerPlanarVelocity once the SDK has it (steer_hook.hpp).
using SteerPlanarVelocity = cb::hooks::Hero_SteerPlanarVelocity;

// The step the game's steering is given in a charge: it moves his velocity at most this far toward
// the one wanted, so this puts it there; the speed's easing is the mod's (Charge::step).
constexpr float kSnapStep = 1000.0f;

struct State {
    openrac::Mod* mod = nullptr;
    cb::Disc disc{};
    // The chain of enter_move_state as the game calls it, and whether the call is the mod's own.
    rac1::Hero_EnterMoveState::fn enter = nullptr;
    bool mine = false;
    // Settings.
    bool always_on = false;
    // The tick's length, and the time since load (for the hit cooldowns).
    float dt = cb::kGcFrame;
    float clock = 0.0f;
    cb::DoubleTap tap;
    cb::Charge charge;
    cb::Ownership ownership;
    cb::HitCooldown hits;
    // Whether the steering ran in a charge this tick (the bonk compares with it).
    bool steered = false;
};

State& state() {
    static State s;
    return s;
}

openrac::Memory memory() { return state().mod->memory(); }

std::uint32_t hero() { return state().disc.hero; }

std::int32_t hero_state(const openrac::Memory& m) { return m.read<std::int32_t>(hero() + cb::kHeroState); }

float planar_length(const openrac::Memory& m, std::uint32_t vector) {
    const float x = m.read<float>(vector);
    const float y = m.read<float>(vector + 4);
    return std::sqrt(x * x + y * y);
}

// A change of state of the mod's own, through every mod's enter_move_state as the game makes one.
std::int32_t enter(std::int32_t move_state, std::int32_t force) {
    State& s = state();
    s.mine = true;
    const std::int32_t entered = s.enter(move_state, force);
    s.mine = false;
    return entered;
}

void clear_hover_latch(const openrac::Memory& m) { m.write(hero() + cb::kHoverLatch, std::uint8_t{0}); }

// The charge over: the hover's latch cleared so idle does not put him back in it.
void finish(const openrac::Memory& m, bool stopped) {
    State& s = state();
    s.charge.end();
    s.tap.reset();
    clear_hover_latch(m);
    // Going Commando's 30 frames before he moves again after a stop.
    if (stopped) {
        m.write(hero() + cb::kMoveLockTimer, static_cast<std::int32_t>(std::lround(cb::kLockout / s.dt)));
    }
}

void read_settings(openrac::Mod& mod) {
    state().always_on = mod.setting<bool>("always_on");
    const float speed = mod.setting<float>("speed");
    state().charge.set_speed_scale(speed > 0.0f ? speed : 1.0f);
}

void before_update() {
    State& s = state();
    const openrac::Memory m = memory();
    const std::uint32_t h = hero();
    s.dt = cb::tick_seconds(m.read<float>(s.disc.frame_time));

    if (s.ownership.observe(m.read<std::uint8_t>(s.disc.items_owned + cb::kUnlockItem) != 0)) {
        s.mod->save_data_set(cb::Ownership::encode(true));
        s.mod->info("Charge Boots: his, with the Persuader");
    }

    const std::int32_t pad_held = m.read<std::int32_t>(s.disc.pad + cb::kPadHeld);
    const std::int32_t pad_pressed = m.read<std::int32_t>(s.disc.pad + cb::kPadPressed);
    const bool held = (pad_held & cb::kPadR1) != 0;
    const bool pressed = (pad_pressed & cb::kPadR1) != 0;
    const std::int32_t current = hero_state(m);

    if (s.charge.active()) {
        if (current != cb::kHover) {
            // Hurt, a slip, a cutscene, his death: the game took him out of it.
            finish(m, false);
        } else if (s.charge.released(held)) {
            enter(cb::kStop, 1);
            finish(m, true);
        } else if (m.read<float>(h + cb::kGroundDistance) > cb::kLeaveGround) {
            enter(cb::kFall, 1);
            finish(m, false);
        }
        return;
    }

    if (!s.tap.update(pressed, held, s.dt) || !s.ownership.owned(s.always_on) || !s.charge.ready()) {
        return;
    }
    const cb::Footing footing{
        current,
        m.read<std::int32_t>(h + cb::kControlMode),
        m.read<std::int32_t>(h + cb::kGroundedFrames),
        m.read<std::uint8_t>(h + cb::kVehicleMode) != 0,
        m.read<std::uint8_t>(h + cb::kGravityMode) != 0 || m.read<std::int16_t>(h + cb::kMagnetFlag) != 0,
    };
    if (!cb::can_start_from(footing)) {
        return;
    }
    const float speed = planar_length(m, h + cb::kVelocity) / s.dt;
    if (enter(cb::kHover, 1) != 0 && hero_state(m) == cb::kHover) {
        s.charge.start(speed);
        s.hits.clear();
    }
}

// Each moby a little ahead of him at hip height takes the wrench's hit: three lines along his
// facing, through the middle and the sides of Going Commando's 0.57 sphere.
void hit_ahead(const openrac::Memory& m) {
    State& s = state();
    if (!s.mod->has_collision_line()) {
        return;
    }
    const std::uint32_t h = hero();
    const int self = s.mod->figure_slot(-1);
    const float facing = m.read<float>(h + cb::kFacing);
    const float fx = std::cos(facing);
    const float fy = std::sin(facing);
    const float reach = cb::kHitRadius + s.charge.speed() * cb::kHitLookAhead;
    const float base[3] = {m.read<float>(h + 0x80u), m.read<float>(h + 0x84u),
                           m.read<float>(h + 0x88u) + cb::kHitHeight};
    for (const float side : {0.0f, -0.6f, 0.6f}) {
        const float from[3] = {base[0] - fy * side * cb::kHitRadius, base[1] + fx * side * cb::kHitRadius, base[2]};
        const float to[3] = {from[0] + fx * reach, from[1] + fy * reach, from[2]};
        openrac_line_hit line{};
        if (!s.mod->collision_line(from, to, line, OPENRAC_LINE_MOBYS, self) || line.hit == 0 || line.moby < 0 ||
            line.moby == self || !s.hits.ready(line.moby, s.clock)) {
            continue;
        }
        openrac_hit hit{};
        hit.victim = line.moby;
        hit.attacker = self;
        hit.mask = cb::kHitMask;
        hit.damage = cb::kHitDamage;
        hit.position[0] = line.point[0];
        hit.position[1] = line.point[1];
        hit.position[2] = line.point[2];
        hit.velocity[0] = fx;
        hit.velocity[1] = fy;
        s.mod->moby_hit(hit);
        s.hits.mark(line.moby, s.clock);
    }
}

void after_update() {
    State& s = state();
    const openrac::Memory m = memory();
    if (s.charge.active() && hero_state(m) == cb::kHover) {
        const std::uint32_t h = hero();
        const float moved = planar_length(m, h + cb::kDisplacement) / s.dt;
        if (s.steered && cb::bonks(m.read<std::uint8_t>(h + cb::kWallHit) != 0, moved, s.charge.speed())) {
            enter(cb::kWallBonk, 1);
            finish(m, false);
        } else {
            hit_ahead(m);
        }
    }
    s.charge.advance(s.dt);
    s.clock += s.dt;
    s.steered = false;
}

bool charging_in_hover() { return state().charge.active() && hero_state(memory()) == cb::kHover; }

void load(openrac::Mod& mod) {
    if (mod.game() != "rac1") {
        throw openrac::Error("this mod is for Ratchet & Clank");
    }
    const bool ntsc = mod.version() == "ntsc";
    if (!ntsc && mod.version() != "pal") {
        throw openrac::Error("this mod knows the PAL and NTSC-U discs only");
    }
    if (!mod.has_save_data()) {
        throw openrac::Error("needs OpenRAC with mod interface 1.16 (save data)");
    }
    State& s = state();
    s.mod = &mod;
    s.disc = cb::disc_for(ntsc);
    read_settings(mod);

    mod.hook<rac1::Hero_Update>([](auto original) {
        before_update();
        original();
        after_update();
    });

    mod.hook<rac1::Hero_EnterMoveState>([](auto original, std::int32_t asked, std::int32_t force) -> std::int32_t {
        State& st = state();
        const openrac::Memory m = memory();
        const std::uint32_t h = hero();
        switch (cb::judge(asked, force, hero_state(m), m.read<std::int32_t>(h + cb::kControlMode),
                          st.charge.active(), st.mine)) {
        case cb::Request::Pass:
            return original(asked, force);
        case cb::Request::Refuse:
            if (asked == cb::kHover && !st.charge.active()) {
                clear_hover_latch(m);
            }
            return 0;
        case cb::Request::AsIdle:
            clear_hover_latch(m);
            return original(cb::kIdle, 0);
        }
        return original(asked, force);
    });

    mod.hook<SteerPlanarVelocity>([](auto original, float step) {
        if (!charging_in_hover()) {
            original(step);
            return;
        }
        State& st = state();
        const openrac::Memory m = memory();
        const std::uint32_t h = hero();
        const cb::Charge::Step tick =
            st.charge.step(st.dt, m.read<float>(h + cb::kStickX), m.read<float>(h + cb::kStickMagnitude));
        const float heading = cb::wrap_angle(m.read<float>(h + cb::kFacing) + tick.turn);
        m.write(h + cb::kFacing, heading);
        m.write(h + cb::kMoveYaw, heading);
        m.write(h + cb::kYawError, cb::lean_for_turn(tick.turn, st.dt));
        m.write(h + cb::kTargetSpeed, tick.speed * st.dt);
        st.steered = true;
        original(kSnapStep);
    });

    mod.hook<rac1::Hero_TurnToHeading>([](auto original, std::int32_t mode, float accel, float damping, float limit) {
        if (!charging_in_hover()) {
            original(mode, accel, damping, limit);
        }
    });

    s.enter = mod.hook_entry<rac1::Hero_EnterMoveState>();
    if (s.enter == nullptr) {
        throw openrac::Error("needs OpenRAC with mod interface 1.21 (hook_entry)");
    }

    s.ownership.restore(mod.save_data());
    mod.on_save_data([] {
        State& st = state();
        st.ownership.restore(st.mod->save_data());
        st.tap.reset();
    });
    if (mod.has_settings_set()) {
        mod.on_settings([] { read_settings(*state().mod); });
    }

    mod.info("Charge Boots: double-tap R1 and hold to charge");
}

}  // namespace

OPENRAC_MOD_CPP("charge-boots", "0.1.0", load)
