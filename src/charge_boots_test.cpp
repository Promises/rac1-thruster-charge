// SPDX-License-Identifier: MIT
//
// The Charge Boots' rules (charge_boots.hpp) on plain values.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "charge_boots.hpp"

namespace {

using namespace charge_boots;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) <= tolerance; }

constexpr float kNtsc = 1.0f / 60.0f;
constexpr float kPal = 1.0f / 50.0f;

// Ticks of nothing pressed.
void idle(DoubleTap& tap, int ticks, float dt = kNtsc) {
    for (int i = 0; i < ticks; ++i) {
        tap.update(false, false, dt);
    }
}

void the_double_tap_needs_the_second_press_within_30_frames() {
    DoubleTap tap;
    check(!tap.update(true, true, kNtsc), "the first press: nothing yet");
    idle(tap, 28);
    check(tap.update(true, true, kNtsc), "the second press 29 frames on: a charge");

    DoubleTap late;
    late.update(true, true, kNtsc);
    idle(late, 30);
    check(!late.update(true, true, kNtsc), "31 frames on: too late, a first press again");
    check(late.update(true, true, kNtsc), "and a press right after it counts as the second");

    DoubleTap released;
    released.update(true, true, kNtsc);
    idle(released, 3);
    check(!released.update(true, false, kNtsc), "pressed and let go in the same poll: not held");

    DoubleTap pal;
    pal.update(true, true, kPal);
    idle(pal, 23, kPal);
    check(pal.update(true, true, kPal), "PAL: 24 ticks of a 50th (0.48 s) still in the window");
}

void a_triple_tap_is_one_charge() {
    DoubleTap tap;
    tap.update(true, true, kNtsc);
    tap.update(false, false, kNtsc);
    check(tap.update(true, true, kNtsc), "second press: a charge");
    tap.update(false, false, kNtsc);
    check(!tap.update(true, true, kNtsc), "third press: the window was used, a first press again");
}

void he_starts_on_his_feet_only() {
    check(can_start_from({kIdle, kModeIdle, 10, false, false}), "idle");
    check(can_start_from({0x2, kModeRun, 10, false, false}), "running");
    check(can_start_from({0x4, kModeCrouch, 10, false, false}), "crouched (the first R1)");
    check(can_start_from({0x7, kModeJump, 1, false, false}), "a jump's landing tick");
    check(!can_start_from({0x7, kModeJump, 0, false, false}), "in the air");
    check(!can_start_from({kIdle, kModeIdle, 10, true, false}), "in a vehicle");
    check(!can_start_from({kIdle, kModeIdle, 10, false, true}), "held by the Magneboots");
    check(!can_start_from({kHover, kModeRun, 10, false, false}), "already hovering");
    check(!can_start_from({0x16, kModeHurt, 10, false, false}), "hurt");
}

// Runs a charge for `seconds` at 60 a second, the stick still; returns the speed then.
float run(Charge& c, float seconds, float dt = kNtsc) {
    float speed = 0.0f;
    const int ticks = static_cast<int>(std::lround(seconds / dt));
    for (int i = 0; i < ticks; ++i) {
        speed = c.step(dt, 0.0f, 0.0f).speed;
        c.advance(dt);
    }
    return speed;
}

void the_charge_bursts_then_cruises() {
    Charge c;
    c.start(5.7f);
    check(c.active() && !c.ready(), "on");
    check(near(c.wanted(), 30.0f), "the burst: 30 units a second");
    const Charge::Step first = c.step(kNtsc, 0.0f, 0.0f);
    check(near(first.speed, 5.7f + 0.027777778f * 60.0f), "up by 0.0278 a frame (in units a second)");
    c.advance(kNtsc);
    check(near(run(c, 0.4f), 30.0f), "at the burst's speed within its first half second");
    run(c, 35 * kNtsc);  // 60 frames in all
    check(near(c.wanted(), 11.5f, 1e-2f), "after 60 frames: the cruise, 11.5");
    const float s = c.step(kNtsc, 0.0f, 0.0f).speed;
    check(near(s, 30.0f - 0.004166667f * 60.0f, 2e-2f), "down by 0.00417 a frame");
    c.advance(kNtsc);
    check(near(run(c, 2.0f), 11.5f, 1e-2f), "settled at the cruise");
}

void the_charge_is_the_same_on_pal() {
    Charge ntsc;
    Charge pal;
    ntsc.start(0.0f);
    pal.start(0.0f);
    check(near(run(ntsc, 0.3f, kNtsc), run(pal, 0.3f, kPal), 0.2f), "the same speed 0.3 s in, at 60 or 50 a second");
}

void the_speed_setting_scales_the_targets() {
    Charge c;
    c.set_speed_scale(0.5f);
    c.start(0.0f);
    check(near(c.wanted(), 15.0f), "half: a 15 unit burst");
    c.set_speed_scale(NAN);
    check(near(c.wanted(), 30.0f), "nonsense: as Going Commando");
}

void the_stick_across_turns_him() {
    Charge c;
    c.start(0.0f);
    check(c.step(kNtsc, 1.0f, 0.1f).turn == 0.0f, "inside the dead zone: no turn");
    const float burst = c.step(kNtsc, 1.0f, 1.0f).turn;
    check(near(burst, -0.020362178f * 0.1f, 1e-5f), "in the burst: a tenth, to the right is clockwise");
    run(c, 1.0f);
    const float cruise = c.step(kNtsc, -1.0f, 1.0f).turn;
    check(near(cruise, 0.020362178f, 1e-5f), "cruising, full left: 0.0204 a frame");
    const float half = c.step(kNtsc, 0.7f, 0.7f).turn;
    check(near(half, -0.020362178f * 0.49f, 1e-5f), "across times how far it is pushed");
    check(near(c.step(kPal, -1.0f, 1.0f).turn, 0.020362178f * 1.2f, 1e-5f), "PAL: a 50th turns 1.2 frames' worth");
}

void he_leans_into_the_turn() {
    check(near(lean_for_turn(0.020362178f, kNtsc), 0.5498f), "full turn: 27 times it");
    check(near(lean_for_turn(-1.0f, kNtsc), -1.7f), "at most 1.7");
    check(near(lean_for_turn(0.020362178f * 1.2f, kPal), 0.5498f), "the same on PAL");
    check(lean_for_turn(0.1f, 0.0f) == 0.0f, "no tick: no lean");
}

void r1_ends_it_after_61_frames() {
    Charge c;
    c.start(0.0f);
    run(c, 60 * kNtsc);
    check(!c.released(false), "let go at 60 frames: it goes on");
    check(!c.released(true), "held: goes on");
    run(c, kNtsc);
    check(c.released(false), "let go at 61: it ends");
    c.end();
    check(!c.active() && !c.ready(), "over, locked out");
    c.advance(29 * kNtsc);
    check(!c.ready(), "29 frames on: still locked out");
    c.advance(kNtsc);
    check(c.ready(), "30 frames on: another may start");
}

void the_wall_bonk() {
    check(bonks(true, 5.0f, 11.5f), "at a wall, held to under three quarters");
    check(!bonks(true, 10.0f, 11.5f), "at a wall but sliding along it");
    check(!bonks(false, 0.0f, 11.5f), "no wall (a slope, a moby that gave way)");
    check(!bonks(true, 0.0f, 3.0f), "barely moving yet");
}

void requests_for_the_hover() {
    check(judge(kHover, 1, 0x2, kModeRun, false, false) == Request::Refuse, "the Thruster's double tap: refused");
    check(judge(kHover, 1, kIdle, kModeIdle, false, false) == Request::Refuse, "idle with the latch: refused");
    check(judge(kHover, 1, 0x16, kModeHurt, false, false) == Request::AsIdle, "after a hurt in the hover: idle");
    check(judge(kHover, 1, 0x2, kModeRun, true, true) == Request::Pass, "the mod's own: through");
    check(judge(kHover, 1, kHover, kModeRun, true, false) == Request::Refuse, "asked again in a charge: refused");
}

void the_hover_does_not_end_a_charge_by_itself() {
    check(judge(kIdle, 0, kHover, kModeRun, true, false) == Request::Refuse, "its exit to idle: refused");
    check(judge(kIdle, 2, kHover, kModeRun, true, false) == Request::Pass, "a cutscene: through");
    check(judge(0x16, 1, kHover, kModeRun, true, false) == Request::Pass, "hurt: through (it ends the charge)");
    check(judge(kStop, 1, kHover, kModeRun, true, true) == Request::Pass, "the mod's stop: through");
    check(judge(kIdle, 0, kHover, kModeRun, false, false) == Request::Pass, "no charge: the game's");
    check(judge(0x7, 1, 0x2, kModeRun, false, false) == Request::Pass, "anything else: the game's");
}

void ownership_comes_with_the_persuader_and_is_kept() {
    Ownership o;
    check(!o.owned(false) && o.owned(true), "none yet; always_on owns them");
    check(!o.observe(false), "the save without the item: nothing to write");
    check(o.observe(true), "the item owned: the record to write");
    check(o.owned(false), "owned");
    check(!o.observe(true), "written once");
    const std::vector<std::uint8_t> record = Ownership::encode(true);
    Ownership loaded;
    loaded.restore(record);
    check(loaded.owned(false), "a save with the record: owned");
    loaded.restore({});
    check(!loaded.owned(false), "a new game: not owned");
    loaded.restore({'C', 'B', 2, 1});
    check(!loaded.owned(false), "a record of another version: not read");
    loaded.restore(Ownership::encode(false));
    check(!loaded.owned(false), "a record saying not owned");
}

void a_moby_is_hit_once_a_half_second() {
    HitCooldown h;
    check(h.ready(7, 0.0f), "first touch");
    h.mark(7, 0.0f);
    check(!h.ready(7, 0.2f) && h.ready(8, 0.2f), "the same moby waits, another does not");
    check(h.ready(7, 0.5f), "half a second on: again");
    for (int slot = 0; slot < 20; ++slot) {
        h.mark(slot, 1.0f);
    }
    check(!h.ready(19, 1.1f), "the latest kept when the table is full");
}

void angles_and_ticks() {
    check(near(wrap_angle(3.5f), 3.5f - 6.2831853f), "past pi");
    check(near(wrap_angle(-3.5f), -3.5f + 6.2831853f), "past -pi");
    check(near(wrap_angle(1.0f), 1.0f), "inside");
    check(wrap_angle(NAN) == 0.0f, "not a number");
    check(tick_seconds(kPal) == kPal && tick_seconds(0.0f) == kGcFrame && tick_seconds(NAN) == kGcFrame,
          "the tick's length, or a 60th");
}

}  // namespace

int main() {
    the_double_tap_needs_the_second_press_within_30_frames();
    a_triple_tap_is_one_charge();
    he_starts_on_his_feet_only();
    the_charge_bursts_then_cruises();
    the_charge_is_the_same_on_pal();
    the_speed_setting_scales_the_targets();
    the_stick_across_turns_him();
    he_leans_into_the_turn();
    r1_ends_it_after_61_frames();
    the_wall_bonk();
    requests_for_the_hover();
    the_hover_does_not_end_a_charge_by_itself();
    ownership_comes_with_the_persuader_and_is_kept();
    a_moby_is_hit_once_a_half_second();
    angles_and_ticks();
    if (failures == 0) {
        std::printf("charge_boots: all passed\n");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
