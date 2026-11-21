// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thruster Charge: Going Commando's Charge Boots charge on Clank's Thruster-Pack in Ratchet &
// Clank, running Going Commando's own code.
//
// The charge is Going Commando's Charge Boots code, copied (src/gc/*.c, from rac2-decomp): the double tap that
// starts it (ProcessHeroEquippedItemInput), what entering it sets up (hero_l00_set_action), the
// charge's and the bonk's movement (hero_l00_update_state_machine) and its ends (hero_l00_decide_state).
// The translation layer (src/gc/rac1_shim.hpp) runs it on Ratchet & Clank's hero: Going Commando's
// state 0x7B is Ratchet & Clank's Thruster-Pack hover (0x81) while he charges in it.
//
// Three hook points:
//   rac1.player.update                   each tick, around the hero's update: before it, the copied
//                                        double tap (it may start a charge); after it, the copied
//                                        decider (R1 let go stops the charge, a wall bonks it)
//   rac1.player.update_movement          while he charges, or bonks off a wall after a charge, the
//                                        copied state machine moves him in place of Ratchet & Clank's
//                                        hover and wall bonk
//   rac1.player.enter_move_state         keeps the hover the charge's: the game's own way into it
//                                        (the Thruster-Pack's double tap) refused, its ways out
//                                        refused while he charges (thruster_charge::judge)
// and the entries of rac1.player.approach_speed, set_planar_velocity, turn_to_heading and
// enter_move_state, which the translation layer calls in place of Going Commando's functions.
// And the blue flames: while he charges, the Thruster-Pack flame's own code runs recoloured
// (tc::FlameTint) through
//   rac1.moby.moby167.update             the flame moby's update (class 0xA7), which spawns its sparks
//   rac1.fx.thruster_flame.draw          the flame's draw, which draws its quads
//   rac1.fx.world_quad.draw              each effect quad: the flame's drawn in blue
//   rac1.fx.burst.spawn                  each spark: the flame's spawned in blue
// and the translation layer tints its ribbon's colour table.
// The movement hook point and the three fx ones need mod interface 1.23 (OpenRAC's dev
// builds from 1.1.1-dev.1063); a game without one refuses the mod with the reason.
//
// The charge is his whenever the Thruster-Pack is on his back (the hover's own gate): its double tap
// of R1 charges instead of hovering. With the Heli-Pack or no pack, R1 does what the game does. The
// Thruster-Pack's long jump and its boost in the air are the game's.
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

#include "thruster_charge.hpp"
#include "gc/rac1_shim.hpp"
#include "layout.hpp"

namespace {

namespace tc = thruster_charge;
namespace gc = thruster_charge::gc;
namespace rac1 = openrac::rac1;

using UpdateMovement = rac1::Hero_UpdateMovement;
using DrawWorldQuad = rac1::FX_DrawWorldQuad;
using DrawThrusterFlame = rac1::FX_ThrusterFlame_Draw;
using SpawnBurst = rac1::FX_SpawnBurst;

// An effect quad record's four RGBA words (FX_DrawWorldQuad).
constexpr std::uint32_t kQuadColours = 0x40;

struct State {
    openrac::Mod* mod = nullptr;
    tc::Disc disc{};
    // Whether the change of state going on is the mod's own (the translation layer's).
    bool mine = false;
    // The level connect() last saw (-2: none yet).
    int level = -2;
    // On while the Thruster-Pack flame's own code runs in a charge.
    tc::FlameTint flames;
};

State& state() {
    static State s;
    return s;
}

gc::Shim& layer() { return gc::shim(); }

std::int32_t hero_state(const openrac::Memory& m) { return m.read<std::int32_t>(state().disc.hero + tc::kHeroState); }

void read_settings(openrac::Mod& mod) {
    layer().set_speed_scale(mod.setting<float>("speed"));
    layer().set_pose(mod.setting<std::int32_t>("pose_loop_from"), mod.setting<std::int32_t>("pose_loop_to"),
                     gc::pose_motion_named(mod.setting<std::string>("pose_motion").c_str()));
    layer().set_pose_speed(mod.setting<float>("pose_rate"));
}

// The game as the translation layer reaches it in the level loaded now: each level has its own
// build of the hero's functions (the hook points' entries) and its own tilt records.
void connect(openrac::Mod& mod) {
    State& s = state();
    const int level = mod.loaded_level();
    if (level == s.level) {
        return;
    }
    s.level = level;
    gc::Game& g = layer().game;
    g.memory = gc::Memory{mod.memory().base()};
    g.hero = s.disc.hero;
    g.pad = s.disc.pad;
    g.lean = level >= 0 ? mod.relocate_level(0, s.disc.lean_l00) : 0;
    g.ribbon = level >= 0 ? mod.relocate_level(0, s.disc.ribbon_l00) : 0;
    // After the new level's addresses: the last level's tint is not written into this one.
    layer().reset();

    const auto approach = mod.hook_entry<rac1::Hero_ApproachSpeed>();
    g.approach_speed = approach == nullptr ? nullptr : std::function<void(float, float)>(approach);
    const auto planar = mod.hook_entry<rac1::Hero_SetPlanarVelocity>();
    g.set_planar_velocity = planar == nullptr ? nullptr : std::function<void(float)>(planar);
    const auto turn = mod.hook_entry<rac1::Hero_TurnToHeading>();
    g.turn_to_heading =
        turn == nullptr ? nullptr : std::function<void(std::int32_t, float, float, float)>(turn);
    const auto enter = mod.hook_entry<rac1::Hero_EnterMoveState>();
    g.enter_move_state = enter == nullptr ? nullptr
                                          : std::function<std::int32_t(std::int32_t, std::int32_t)>(
                                                [enter](std::int32_t to, std::int32_t force) {
                                                    state().mine = true;
                                                    const std::int32_t entered = enter(to, force);
                                                    state().mine = false;
                                                    return entered;
                                                });
}

// The SDK's calls the translation layer needs, set once at load.
void connect_once(openrac::Mod& mod) {
    State& s = state();
    gc::Game& g = layer().game;
    openrac::Mod* m = &mod;
    // Ratchet's animation (set_hero_animation) and its loop (hero_queue_sequence): level 0's build,
    // which the port runs in any level as its own hero_grab does (port/game/rac1/host/hero_grab.c
    // calls func_L00_00232C10 whatever the level).
    using SetAnimation = void (*)(std::int32_t, std::int32_t, float);
    using LoopAnimation = void (*)(std::int32_t, std::int32_t);
    const auto set_animation = mod.function<SetAnimation>(s.disc.set_hero_animation);
    const auto loop_animation = mod.function<LoopAnimation>(s.disc.hero_queue_sequence);
    if (set_animation == nullptr || loop_animation == nullptr) {
        mod.warn("Thruster Charge: Ratchet's animation functions not found: the charge keeps the hover's pose");
    } else {
        g.set_animation = set_animation;
        g.loop_animation = loop_animation;
    }
    g.self_slot = [m] { return m->figure_slot(-1); };
    if (mod.has_collision_line()) {
        g.moby_on_line = [m](const float* from, const float* to) -> std::int32_t {
            openrac_line_hit line{};
            const int self = m->figure_slot(-1);
            if (!m->collision_line(from, to, line, OPENRAC_LINE_MOBYS, self) || line.hit == 0 || line.moby < 0 ||
                line.moby == self) {
                return -1;
            }
            return line.moby;
        };
        g.level_on_line = [m](const float* from, const float* to) {
            openrac_line_hit line{};
            return m->collision_line(from, to, line, OPENRAC_LINE_LEVEL) && line.hit != 0;
        };
    }
    g.hit = [m](const gc::Hit& h) {
        openrac_hit hit{};
        hit.victim = h.victim;
        hit.attacker = h.attacker;
        hit.mask = h.mask;
        hit.damage = h.damage;
        for (int i = 0; i < 3; ++i) {
            hit.position[i] = h.position[i];
            hit.velocity[i] = h.velocity[i];
        }
        m->moby_hit(hit);
    };
    g.sound_play = [m](std::int32_t sound, bool loop) { return m->hero_sound_play(sound, loop); };
    g.sound_stop = [m](std::int32_t voice) { m->hero_sound_stop(voice); };
    g.log = [m](const char* what) { m->warn(what); };
    g.info = [m](const char* what) { m->info(what); };
}

void before_update() {
    State& s = state();
    openrac::Mod& mod = *s.mod;
    const openrac::Memory m = mod.memory();
    connect(mod);
    gc::Shim& l = layer();
    l.set_tick(m.read<float>(s.disc.frame_time));

    // Hurt, a slip, a cutscene, his death: the game took him out of the charge.
    l.observe();
    // Going Commando's double tap, as its item input runs it each frame.
    l.load();
    ProcessHeroEquippedItemInput();
    l.flush();
}

void after_update() {
    gc::Shim& l = layer();
    l.observe();
    if (l.charging() && hero_state(state().mod->memory()) == tc::kHover) {
        l.load();
        hero_l00_decide_state();
        l.flush();
    }
    l.end_tick();
}

void load(openrac::Mod& mod) {
    if (mod.game() != "rac1") {
        throw openrac::Error("this mod is for Ratchet & Clank");
    }
    const bool ntsc = mod.version() == "ntsc";
    if (!ntsc && mod.version() != "pal") {
        throw openrac::Error("this mod knows the PAL and NTSC-U discs only");
    }
    if (!mod.has_hook_entry()) {
        throw openrac::Error("needs OpenRAC with mod interface 1.21 (hook_entry)");
    }
    if (!layer().handles_unambiguous()) {
        throw openrac::Error("the translation layer's buffers share their low 32 bits in this process");
    }
    State& s = state();
    s.mod = &mod;
    s.disc = tc::disc_for(ntsc);
    read_settings(mod);
    connect_once(mod);

    mod.hook<rac1::Hero_Update>([](auto original) {
        before_update();
        original();
        after_update();
    });

    // The copied state machine in place of Ratchet & Clank's movement while he charges or bonks.
    // What Ratchet & Clank's hero_update_movement does after its switch (two counters reset when
    // its state's step or animation changed) is not needed: the copied code changes neither.
    mod.hook<UpdateMovement>([](auto original) {
        gc::Shim& l = layer();
        if (!l.drives_movement()) {
            original();
            return;
        }
        l.load();
        hero_l00_update_state_machine();
        l.flush();
        if (l.charging()) {
            l.hold_pose();
        }
    });

    // The blue flames. The flame's update and draw: the tint on around them while he charges.
    mod.hook<rac1::Moby_Class167_Update>([](auto original, std::uint32_t moby) {
        State& st = state();
        const bool was = st.flames.enter(layer().charging());
        original(moby);
        st.flames.leave(was);
    });
    mod.hook<DrawThrusterFlame>([](auto original, std::uint32_t flame) {
        State& st = state();
        const bool was = st.flames.enter(layer().charging());
        original(flame);
        st.flames.leave(was);
    });
    // Each effect quad, many a frame: straight to the game unless it is the flame's in a charge;
    // then drawn blue, and its record (the caller's, which may draw it again) put back.
    mod.hook<DrawWorldQuad>([](auto original, std::uint32_t quad, std::uint32_t matrix, std::int32_t mode) {
        State& st = state();
        if (!st.flames.on()) {
            original(quad, matrix, mode);
            return;
        }
        std::uint8_t* record = st.mod->memory().base() + quad + kQuadColours;
        std::uint32_t colours[4];
        std::memcpy(colours, record, sizeof colours);
        const std::array<std::uint32_t, 4> was = tc::FlameTint::tint_quad(colours);
        std::memcpy(record, colours, sizeof colours);
        original(quad, matrix, mode);
        tc::FlameTint::restore_quad(colours, was);
        std::memcpy(record, colours, sizeof colours);
    });
    // Each spark: the flame's spawned blue, fading to a paler blue (its children take its colour).
    mod.hook<SpawnBurst>([](auto original, std::uint32_t pos, std::uint32_t dir, std::int32_t colour,
                            std::int32_t fade, std::int32_t ticks, std::int32_t children, float size) {
        if (state().flames.on()) {
            colour = static_cast<std::int32_t>(tc::blue_flame(static_cast<std::uint32_t>(colour)));
            fade = static_cast<std::int32_t>(tc::blue_flame(static_cast<std::uint32_t>(fade)));
        }
        return original(pos, dir, colour, fade, ticks, children, size);
    });

    mod.hook<rac1::Hero_EnterMoveState>([](auto original, std::int32_t asked, std::int32_t force) -> std::int32_t {
        State& st = state();
        const openrac::Memory m = st.mod->memory();
        const std::uint32_t h = st.disc.hero;
        switch (tc::judge(asked, force, hero_state(m), m.read<std::int32_t>(h + tc::kControlMode),
                          layer().charging(), st.mine)) {
        case tc::Request::Pass:
            return original(asked, force);
        case tc::Request::Refuse:
            if (asked == tc::kHover && !layer().charging()) {
                m.write(h + tc::kHoverLatch, std::uint8_t{0});
            }
            return 0;
        case tc::Request::AsIdle:
            m.write(h + tc::kHoverLatch, std::uint8_t{0});
            return original(tc::kIdle, 0);
        }
        return original(asked, force);
    });

    if (mod.has_settings_set()) {
        mod.on_settings([] { read_settings(*state().mod); });
    }

    mod.info("Thruster Charge: with the Thruster-Pack on, double-tap R1 and hold to charge (Going Commando's Charge Boots code)");
}

}  // namespace

OPENRAC_MOD_CPP("thruster-charge", "1.0.2", load)
