// SPDX-License-Identifier: GPL-3.0-or-later
//
// Charge Boots: Going Commando's Charge Boots in Ratchet & Clank, as Going Commando's own code.
//
// The charge is Going Commando's code, copied (src/gc/*.c, from rac2-decomp): the double tap that
// starts it (ProcessHeroEquippedItemInput), what entering it sets up (hero_l00_set_action), the
// charge's and the bonk's movement (hero_l00_update_state_machine) and its ends (hero_l00_decide_state).
// The translation layer (src/gc/rac1_shim.hpp) runs it on Ratchet & Clank's hero: Going Commando's
// state 0x7B is Ratchet & Clank's Thruster-Pack hover (0x81) while the boots charge in it.
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
//                                        refused while he charges (charge_boots::judge)
// and the entries of rac1.player.approach_speed, set_planar_velocity, turn_to_heading and
// enter_move_state, which the translation layer calls in place of Going Commando's functions.
// The movement one is new in OpenRAC (branch rac1-steer-hook), declared in movement_hook.hpp until
// the SDK has it; a game without it refuses the mod with the reason.
//
// The boots are his once the game in progress owns the Persuader (layout.hpp says why that item),
// kept in the mod's save data beside the save, or from the start with the setting always_on.
#include <cstdint>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

#include "charge_boots.hpp"
#include "gc/rac1_shim.hpp"
#include "layout.hpp"
#include "movement_hook.hpp"

namespace {

namespace cb = charge_boots;
namespace gc = charge_boots::gc;
namespace rac1 = openrac::rac1;

// TODO(movement hook): openrac::rac1::Hero_UpdateMovement once the SDK has it (movement_hook.hpp).
using UpdateMovement = cb::hooks::Hero_UpdateMovement;

struct State {
    openrac::Mod* mod = nullptr;
    cb::Disc disc{};
    // Whether the change of state going on is the mod's own (the translation layer's).
    bool mine = false;
    bool always_on = false;
    cb::Ownership ownership;
    // The level connect() last saw (-2: none yet).
    int level = -2;
};

State& state() {
    static State s;
    return s;
}

gc::Shim& layer() { return gc::shim(); }

std::int32_t hero_state(const openrac::Memory& m) { return m.read<std::int32_t>(state().disc.hero + cb::kHeroState); }

void read_settings(openrac::Mod& mod) {
    state().always_on = mod.setting<bool>("always_on");
    layer().set_speed_scale(mod.setting<float>("speed"));
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
    layer().reset();
    gc::Game& g = layer().game;
    g.memory = gc::Memory{mod.memory().base()};
    g.hero = s.disc.hero;
    g.pad = s.disc.pad;
    g.lean = level >= 0 ? mod.relocate_level(0, s.disc.lean_l00) : 0;

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
    gc::Game& g = layer().game;
    openrac::Mod* m = &mod;
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
}

void before_update() {
    State& s = state();
    openrac::Mod& mod = *s.mod;
    const openrac::Memory m = mod.memory();
    connect(mod);
    gc::Shim& l = layer();
    l.set_tick(m.read<float>(s.disc.frame_time));

    if (s.ownership.observe(m.read<std::uint8_t>(s.disc.items_owned + cb::kUnlockItem) != 0)) {
        mod.save_data_set(cb::Ownership::encode(true));
        mod.info("Charge Boots: his, with the Persuader");
    }
    l.set_owned(s.ownership.owned(s.always_on));

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
    if (l.charging() && hero_state(state().mod->memory()) == cb::kHover) {
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
    if (!mod.has_save_data()) {
        throw openrac::Error("needs OpenRAC with mod interface 1.16 (save data)");
    }
    if (!mod.has_hook_entry()) {
        throw openrac::Error("needs OpenRAC with mod interface 1.21 (hook_entry)");
    }
    if (!layer().handles_unambiguous()) {
        throw openrac::Error("the translation layer's buffers share their low 32 bits in this process");
    }
    State& s = state();
    s.mod = &mod;
    s.disc = cb::disc_for(ntsc);
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
    });

    mod.hook<rac1::Hero_EnterMoveState>([](auto original, std::int32_t asked, std::int32_t force) -> std::int32_t {
        State& st = state();
        const openrac::Memory m = st.mod->memory();
        const std::uint32_t h = st.disc.hero;
        switch (cb::judge(asked, force, hero_state(m), m.read<std::int32_t>(h + cb::kControlMode),
                          layer().charging(), st.mine)) {
        case cb::Request::Pass:
            return original(asked, force);
        case cb::Request::Refuse:
            if (asked == cb::kHover && !layer().charging()) {
                m.write(h + cb::kHoverLatch, std::uint8_t{0});
            }
            return 0;
        case cb::Request::AsIdle:
            m.write(h + cb::kHoverLatch, std::uint8_t{0});
            return original(cb::kIdle, 0);
        }
        return original(asked, force);
    });

    s.ownership.restore(mod.save_data());
    mod.on_save_data([] {
        State& st = state();
        st.ownership.restore(st.mod->save_data());
        layer().reset();
    });
    if (mod.has_settings_set()) {
        mod.on_settings([] { read_settings(*state().mod); });
    }

    mod.info("Charge Boots: double-tap R1 and hold to charge (Going Commando's code)");
}

}  // namespace

OPENRAC_MOD_CPP("charge-boots", "0.2.0", load)
