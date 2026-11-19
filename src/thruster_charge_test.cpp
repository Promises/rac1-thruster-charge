// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mod's rules (thruster_charge.hpp), and the translation layer (gc/rac1_shim.hpp) running Going
// Commando's copied charge code (src/gc/*.c) on a fake Ratchet & Clank: a buffer of game memory and
// recording stand-ins for the game's functions. What the game's own functions would do is the
// fake's guess (the simplest), so these test the copied code's path through the layer: which game
// calls it makes with which numbers, and what lands in Ratchet & Clank's hero. They do not show the
// game behaves so.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "thruster_charge.hpp"
#include "gc/rac1_shim.hpp"

namespace {

using namespace thruster_charge;
namespace gc = thruster_charge::gc;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b, float tolerance = 1e-4f) { return std::fabs(a - b) <= tolerance; }

constexpr float kNtsc = 1.0f / 60.0f;
constexpr float kPal = 1.0f / 50.0f;
constexpr float kPi = 3.14159265f;

// --- The fake game ---

// Ratchet & Clank's hero fields the tests set and read (include/structs/hero.h).
constexpr std::uint32_t kPos = 0x80;
constexpr std::uint32_t kBodyCenter = 0xD0;
constexpr std::uint32_t kGroundSpeed = 0x194;
constexpr std::uint32_t kStateTimer = 0x198;
constexpr std::uint32_t kCapsuleRadius = 0x234;
constexpr std::uint32_t kGroundZ = 0x2D8;
constexpr std::uint32_t kItemAnim = 0x20A8;
constexpr std::int32_t kSelf = 0;
constexpr std::int32_t kHeliPack = 2;
constexpr std::uint32_t kFakeMoby = 0x1A0000;
constexpr std::uint32_t kFakeClass = 0x1A1000;
constexpr std::uint32_t kFakeLongJump = 0x1A2000;
constexpr std::uint32_t kFakeHover = 0x1A2100;

struct Fake {
    std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(0x200000);
    Disc disc = disc_for(false);
    std::vector<std::pair<float, float>> approaches;
    std::vector<float> planars;
    std::vector<std::array<float, 4>> turns;
    std::vector<std::pair<std::int32_t, std::int32_t>> enters;
    std::vector<gc::Hit> hits;
    std::vector<std::pair<std::int32_t, bool>> sounds;
    struct Animation {
        std::int32_t id;
        std::int32_t start;
        float frames;
    };
    std::vector<Animation> animations;
    std::vector<std::string> infos;
    std::vector<std::pair<std::int32_t, std::int32_t>> loops;
    int stops = 0;
    std::int32_t moby_ahead = -1;
    bool wall_ahead = false;

    gc::Memory mem() { return gc::Memory{memory.data()}; }

    template <class T>
    T get(std::uint32_t offset) {
        return mem().read<T>(disc.hero + offset);
    }
    template <class T>
    void set(std::uint32_t offset, T v) {
        mem().write(disc.hero + offset, v);
    }
    float lean(std::uint32_t offset) { return mem().read<float>(disc.lean_l00 + offset); }

    void pad(bool r1_held, bool r1_pressed) {
        mem().write(disc.pad + kPadHeld, r1_held ? static_cast<std::int32_t>(kPadR1) : 0);
        mem().write(disc.pad + kPadPressed, r1_pressed ? static_cast<std::int32_t>(kPadR1) : 0);
    }
};

Fake* g_fake = nullptr;

// A fresh fake, Ratchet idle on flat ground at z 0, facing +x, and the layer wired to it.
Fake& fresh(float frame_time = kNtsc) {
    static Fake fake;
    gc::Shim& s = gc::shim();
    // The last test's charge ended on the last test's fake.
    s.reset();
    fake = Fake{};
    g_fake = &fake;
    s.set_speed_scale(1.0f);
    s.set_pose(gc::kChargePoseFrom, gc::kChargePoseTo);
    s.set_tick(frame_time);
    std::memset(gc_shim_hero, 0, gc::kHeroBytes);
    std::memset(gc_shim_lean, 0, gc::kLeanBytes);
    gc::Game& g = s.game;
    g = gc::Game{};
    g.memory = fake.mem();
    g.hero = fake.disc.hero;
    g.pad = fake.disc.pad;
    g.lean = fake.disc.lean_l00;
    // speed_step: ground speed toward target speed (hero_animation_slots.c:2880).
    g.approach_speed = [](float up, float down) {
        Fake& f = *g_fake;
        f.approaches.emplace_back(up, down);
        const float target = f.get<float>(kTargetSpeed);
        float speed = f.get<float>(kGroundSpeed);
        speed = speed < target ? std::min(target, speed + up) : std::max(target, speed - down);
        f.set(kGroundSpeed, speed);
    };
    // set_planar_vel: velocity at the ground speed along the angle, his facing above pi.
    g.set_planar_velocity = [](float angle) {
        Fake& f = *g_fake;
        f.planars.push_back(angle);
        const float yaw = angle <= kPi ? angle : f.get<float>(kFacing);
        const float speed = f.get<float>(kGroundSpeed);
        f.set(kVelocity, speed * std::cos(yaw));
        f.set(kVelocity + 4, speed * std::sin(yaw));
        f.set(kVelocity + 8, 0.0f);
    };
    g.turn_to_heading = [](std::int32_t mode, float a, float b, float c) {
        g_fake->turns.push_back({static_cast<float>(mode), a, b, c});
    };
    // HeroEnterMoveState: the state, its timer from 0.
    g.enter_move_state = [](std::int32_t to, std::int32_t force) {
        Fake& f = *g_fake;
        f.enters.emplace_back(to, force);
        f.set(kHeroState, to);
        f.set(kStateTimer, 0);
        return std::int32_t{1};
    };
    g.moby_on_line = [](const float*, const float*) { return g_fake->moby_ahead; };
    g.level_on_line = [](const float*, const float*) { return g_fake->wall_ahead; };
    g.hit = [](const gc::Hit& h) { g_fake->hits.push_back(h); };
    g.self_slot = [] { return kSelf; };
    g.sound_play = [](std::int32_t sound, bool loop) {
        g_fake->sounds.emplace_back(sound, loop);
        return std::int32_t{3};
    };
    g.sound_stop = [](std::int32_t) { ++g_fake->stops; };
    g.log = [](const char* what) { std::printf("layer: %s\n", what); };
    g.set_animation = [](std::int32_t animation, std::int32_t start, float frames) {
        g_fake->animations.push_back({animation, start, frames});
    };
    g.loop_animation = [](std::int32_t from, std::int32_t to) { g_fake->loops.emplace_back(from, to); };
    g.info = [](const char* what) { g_fake->infos.emplace_back(what); };
    g.ribbon = fake.disc.ribbon_l00;
    for (std::size_t i = 0; i < gc::kRibbonColours; ++i) {
        fake.mem().write(fake.disc.ribbon_l00 + static_cast<std::uint32_t>(i * 4), i % 7 == 0 ? 0x20808080u : 0x80808080u);
    }
    fake.set(kHeroState, kIdle);
    fake.set(kControlMode, kModeIdle);
    fake.set(kGroundedFrames, 10);
    fake.set(kCapsuleRadius, 0.5f);
    // His moby, its class, and two of its animations: the long jump's (14 frames, its own looping
    // sound 0x55) and the hover's (sound 0x33).
    fake.set(kMoby, kFakeMoby);
    fake.mem().write(kFakeMoby + gc::kMobyClass, kFakeClass);
    fake.mem().write(kFakeClass + gc::kClassSequences + gc::kChargePose * 4, kFakeLongJump);
    fake.mem().write(kFakeClass + gc::kClassSequences + gc::kHoverAnimation * 4, kFakeHover);
    fake.mem().write(kFakeLongJump + gc::kSequenceFrameCount, std::uint8_t{14});
    fake.mem().write(kFakeLongJump + gc::kSequenceSound, std::uint8_t{0x55});
    fake.mem().write(kFakeHover + gc::kSequenceSound, std::uint8_t{0x33});
    fake.set(gc::kHeroAnimSpeed, 1.0f);
    // The Thruster-Pack on his back.
    fake.set(kPackSlotState, kSlotReady);
    fake.set(kPackSlotItem, kThrusterPack);
    return fake;
}

// One tick as mod.cpp runs it: the double tap before the hero's update, the movement in it, the
// decider after it. The game's own update in between is the fake's: the state timer counts.
void tick(Fake& f) {
    gc::Shim& s = gc::shim();
    // The game counts the stop's lockout down.
    if (f.get<std::int32_t>(kMoveLockTimer) > 0) {
        f.set(kMoveLockTimer, f.get<std::int32_t>(kMoveLockTimer) - 1);
    }
    s.observe();
    s.load();
    ProcessHeroEquippedItemInput();
    s.flush();
    if (s.drives_movement()) {
        s.load();
        hero_l00_update_state_machine();
        s.flush();
        if (s.charging()) {
            s.hold_pose();
        }
    }
    f.set(kStateTimer, f.get<std::int32_t>(kStateTimer) + 1);
    s.observe();
    if (s.charging() && f.get<std::int32_t>(kHeroState) == kHover) {
        s.load();
        hero_l00_decide_state();
        s.flush();
    }
    s.end_tick();
}

// Ratchet in a charge: the double tap.
void start_charge(Fake& f) {
    f.pad(true, true);
    tick(f);
    f.pad(false, false);
    tick(f);
    f.pad(true, true);
    tick(f);
    f.pad(true, false);
}

// Only the copied state machine's movement, one tick (no decider, no trigger).
void move(Fake& f) {
    gc::Shim& s = gc::shim();
    s.load();
    hero_l00_update_state_machine();
    s.flush();
    (void)f;
}

// --- The mod's rules ---

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

// --- The translation layer's tables ---

void states_and_animations_map() {
    check(gc::rac1_state(gc::kGcCharge) == kHover, "the charge runs in the hover");
    check(gc::rac1_state(gc::kGcBonk) == kWallBonk, "the bonk in the wall bonk");
    check(gc::rac1_state(gc::kGcStop) == kStop, "the stop in the stop");
    check(gc::rac1_state(0x16) == -1, "no other");
    check(gc::gc_state(kHover, true, false) == gc::kGcCharge, "the hover is the charge while charging");
    check(gc::gc_state(kHover, false, false) == kHover, "else the hover");
    check(gc::gc_state(kWallBonk, false, true) == gc::kGcBonk, "the bonk after a charge");
    check(gc::gc_state(kWallBonk, false, false) == kWallBonk, "else the game's own bonk");
    check(gc::gc_control_mode(kModeRun, gc::kGcCharge) == gc::kGcChargeControlMode, "control mode 28 in a charge");
    check(gc::gc_control_mode(kModeRun, gc::kGcBonk) == gc::kGcBonkControlMode, "10 in its bonk");
    check(gc::gc_control_mode(kModeCrouch, kIdle) == kModeCrouch, "else the same number");
    check(gc::rac1_animation(0x8B) == 0x26 && gc::rac1_animation(0x29) == -1 && gc::rac1_animation(6) == -1,
          "animations: the charge the long jump's, the bonk and the stop their states' own");
}

void the_field_table_is_sound() {
    for (const gc::Field& f : gc::kHeroFields) {
        const std::size_t gc_end = f.gc + f.count * gc::size_of(f.gc_kind);
        const std::size_t rac1_end = f.rac1 + f.count * gc::size_of(f.rac1_kind);
        check(gc_end <= 0x2600 && rac1_end <= 0x2600, "fields inside both heroes");
        check(f.gc % gc::size_of(f.gc_kind) == 0 && f.rac1 % gc::size_of(f.rac1_kind) == 0, "fields aligned");
        check(!(f.gc < gc::kGcControlMode + 4 && gc_end > gc::kGcMoby), "the table leaves the translated words alone");
        for (const gc::Field& g : gc::kHeroFields) {
            if (&g == &f) {
                continue;
            }
            const std::size_t g_end = g.gc + g.count * gc::size_of(g.gc_kind);
            check(gc_end <= g.gc || g_end <= f.gc, "no two fields overlap in the copy");
        }
    }
    // The fields mod.cpp's layout already had, the same offsets.
    const auto rac1_of = [](std::uint32_t gc_offset) {
        for (const gc::Field& f : gc::kHeroFields) {
            if (f.gc == gc_offset) {
                return f.rac1;
            }
        }
        return 0xFFFFFFFFu;
    };
    check(rac1_of(0x0F0) == kVelocity && rac1_of(0x110) == kDisplacement && rac1_of(0x1A0) == kMoveYaw &&
              rac1_of(0x1A8) == kYawError && rac1_of(0x1B0) == kTargetSpeed && rac1_of(0x1E0) == kMoveLockTimer &&
              rac1_of(0x287) == kWallHit && rac1_of(0x31C) == kGroundDistance && rac1_of(0x340) == kGroundedFrames &&
              rac1_of(0x348) == kMagnetFlag && rac1_of(gc::kGcStick) == kStickX &&
              rac1_of(0x22B4) == kVehicleMode && rac1_of(0x22C3) == kGravityMode &&
              rac1_of(0x24B8) == kStickMagnitude,
          "the table agrees with layout.hpp");
    check(gc::shim().handles_unambiguous(), "handles resolve one way");
    check(gc::shim().resolve(static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(gc_shim_hero + 0x88))) ==
              gc_shim_hero + 0x88,
          "a handle into the copy");
    check(gc::shim().resolve(static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(gc_shim_lean + 0x60))) ==
              gc_shim_lean + 0x60,
          "a handle into the tilt records");
}

void load_translates_ratchets_hero() {
    Fake& f = fresh(kPal);
    f.set(kVelocity, 0.24f);
    f.set(kDisplacement + 4, 0.12f);
    f.set(kStateTimer, 51);
    f.set(kMoveLockTimer, 25);
    f.set(kFacing, 1.25f);
    f.set(kStickX, -0.5f);
    f.set(kStickMagnitude, 0.75f);
    f.set(kWallHit, std::uint8_t{1});
    f.set(kMoby, 0x001A2B30u);
    f.set(kGravityMode, std::uint8_t{0});
    f.pad(true, false);
    gc::shim().load();
    const gc::Memory copy{gc_shim_hero};
    check(near(copy.read<float>(0xF0), 0.2f), "velocity: a tick's over k (1.2 on PAL)");
    check(near(copy.read<float>(0x114), 0.1f), "displacement too");
    check(copy.read<std::int32_t>(0x1B8) == 61, "51 ticks are 61 frames");
    check(copy.read<std::int16_t>(0x1E0) == 30, "the lock: 25 ticks are 30 frames");
    check(copy.read<float>(0x98) == 1.25f, "facing as it is");
    check(copy.read<float>(0x1FB0) == -0.5f && copy.read<float>(0x24B8) == 0.75f, "the stick");
    check(copy.read<std::uint8_t>(0x287) == 1, "the wall");
    check(copy.read<std::int32_t>(gc::kGcState) == kIdle && copy.read<std::int32_t>(gc::kGcControlMode) == kModeIdle,
          "state and control mode");
    check(copy.read<std::uint32_t>(gc::kGcMoby) == 0x001A2B30u &&
              copy.read<std::uintptr_t>(gc::kGcMobyPointer) == 0x001A2B30u,
          "the moby, as a word and as the Hero union's pointer");
    check((gc_shim_pad[kPadHeld] & kPadR1) != 0, "the pad");
    check(gc_shim_items_owned[0x26] == 1 && gc_shim_item_slots[3] == 54,
          "the Thruster-Pack on: Going Commando's boots owned and worn");
    f.set(kPackSlotItem, kHeliPack);
    gc::shim().load();
    check(gc_shim_items_owned[0x26] == 0 && gc_shim_item_slots[3] != 54, "the Heli-Pack: neither");
}

void flush_writes_back_only_what_changed() {
    Fake& f = fresh(kPal);
    f.set(kVelocity, 0.24f);
    f.set(kMoveYaw, 0.5f);
    f.set(kDisplacement, 0.3f);
    gc::Shim& s = gc::shim();
    s.load();
    const gc::Memory copy{gc_shim_hero};
    copy.write(0xF4, 0.1f);
    copy.write(0x110, 9.0f);
    f.set(kMoveYaw, 0.75f);
    s.flush();
    check(near(f.get<float>(kVelocity + 4), 0.12f), "a changed velocity: k times it");
    check(near(f.get<float>(kVelocity), 0.24f), "the unchanged x as it was");
    check(f.get<float>(kMoveYaw) == 0.75f, "a field the copied code did not touch: the game's own");
    check(f.get<float>(kDisplacement) == 0.3f, "a read-only field: never written");
}

// --- The copied code, through the layer ---

void entering_the_charge() {
    Fake& f = fresh(kPal);
    f.set(kDisplacement, 0.06f);
    gc::Shim& s = gc::shim();
    s.load();
    check(hero_l00_set_action(gc::kGcCharge, 1) == 1, "entered");
    s.flush();
    check(s.charging() && f.get<std::int32_t>(kHeroState) == kHover, "the charge, in the hover");
    check(near(f.get<float>(kVelocity), 0.06f), "his velocity is what he moved (COPY_VECTOR)");
    check(gc::shim().drives_movement(), "the copied code moves him");
}

void the_double_tap_starts_a_charge() {
    Fake& f = fresh();
    start_charge(f);
    check(gc::shim().charging(), "charging");
    check(f.enters.size() == 1 && f.enters[0].first == kHover && f.enters[0].second == 1, "into the hover, forced");

    f = fresh();
    f.pad(true, true);
    tick(f);
    f.pad(false, false);
    for (int i = 0; i < 30; ++i) {
        tick(f);
    }
    f.pad(true, true);
    tick(f);
    check(!gc::shim().charging(), "30 frames late: no charge");

    f = fresh();
    f.set(kPackSlotItem, kHeliPack);
    start_charge(f);
    check(!gc::shim().charging() && f.enters.empty(), "the Heli-Pack: no charge, R1 is the game's");

    f = fresh();
    f.set(kPackSlotState, 0);
    start_charge(f);
    check(!gc::shim().charging() && f.enters.empty(), "no pack: no charge");

    f = fresh();
    f.set(kPackDisabled, std::int16_t{1});
    start_charge(f);
    check(!gc::shim().charging(), "the pack switched off: no charge");

    f = fresh();
    f.set(kControlMode, 0x7);
    start_charge(f);
    check(!gc::shim().charging(), "hurt: no charge");

    f = fresh();
    f.set(kVehicleMode, std::uint8_t{1});
    start_charge(f);
    check(!gc::shim().charging(), "in a vehicle: no charge");
}

void the_double_tap_window_is_half_a_second_on_pal() {
    for (const int gap : {23, 26}) {
        Fake& f = fresh(kPal);
        f.pad(true, true);
        tick(f);
        f.pad(false, false);
        for (int i = 0; i < gap; ++i) {
            tick(f);
        }
        f.pad(true, true);
        tick(f);
        check(gc::shim().charging() == (gap == 23), "30 frames are 25 ticks at 50 a second");
    }
}

void a_charge_tick_cruising() {
    Fake& f = fresh();
    start_charge(f);
    f.set(kStateTimer, 100);
    f.set(kStickX, 1.0f);
    f.set(kStickMagnitude, 1.0f);
    f.set(kGroundZ, 0.0f);
    f.approaches.clear();
    f.planars.clear();
    move(f);
    check(f.approaches.size() == 1 && near(f.approaches[0].first, 0.027777778f) &&
              near(f.approaches[0].second, 0.004166667f),
          "speed_step at Going Commando's rates");
    check(f.planars.size() == 1 && f.planars[0] == 99999.0f, "set_planar_vel along his facing");
    check(near(f.get<float>(kTargetSpeed), 0.19166668f), "the cruise's target speed");
    const float turn = 0.020362178f * -0.17f;
    check(near(f.get<float>(kFacing), turn, 1e-6f), "the stick right turns him right, eased in by 0.17");
    check(near(f.lean(0x60), -turn * 27.0f, 1e-5f) && near(f.lean(0x278), turn * 13.5f, 1e-5f) &&
              near(f.lean(0x1C8), turn * 13.5f, 1e-5f),
          "the lean in the tilt records");
    check(near(f.lean(0xA4), 0.017f) && near(f.lean(0xA8), 0.3f) && near(f.lean(2 * 0xB0 + 0xA4), 0.025f) &&
              near(f.lean(3 * 0xB0 + 0xA8), 0.27f),
          "the tilt springs (rac2-decomp's cam_set_limits_*)");
    check(f.get<float>(kPos + 8) > 0.0f, "the ground spring lifts him toward 0.17");
    check(f.sounds.size() == 1 && f.sounds[0].first == gc::kChargeSound && f.sounds[0].second, "the hum, looped");
}

void a_charge_tick_bursting_on_pal_at_a_faster_setting() {
    Fake& f = fresh(kPal);
    start_charge(f);
    gc::shim().set_speed_scale(1.5f);
    f.set(kStateTimer, 10);
    f.set(kStickX, 1.0f);
    f.set(kStickMagnitude, 1.0f);
    f.approaches.clear();
    move(f);
    check(near(f.approaches.back().first, 0.027777778f * 1.44f), "rates times k squared");
    check(near(f.get<float>(kTargetSpeed), 0.5f * 1.2f * 1.5f), "the burst's target: k times, and the setting's");
    const float turn = 0.020362178f * -(0.17f * 1.2f) * 0.1f * 1.2f;
    check(near(f.get<float>(kFacing), turn, 1e-6f), "a tenth of the turn in the burst, a tick's");
}

void the_charge_hits_what_is_ahead_once() {
    Fake& f = fresh();
    start_charge(f);
    f.moby_ahead = 5;
    f.set(kBodyCenter + 8, 0.5f);
    move(f);
    check(f.hits.size() == 1, "one hit for the moby the three lines meet");
    if (!f.hits.empty()) {
        check(f.hits[0].victim == 5 && f.hits[0].attacker == kSelf && f.hits[0].mask == 0x10000u &&
                  f.hits[0].damage == 1.0f,
              "the wrench's mask, damage 1, from him");
    }
    gc::shim().set_tick(kNtsc);
    move(f);
    check(f.hits.size() == 1, "not again within half a second");
}

void holding_r1_keeps_it_going_and_letting_go_stops_it_after_61_frames() {
    Fake& f = fresh(kPal);
    start_charge(f);
    f.set(kStateTimer, 45);
    f.pad(false, false);
    tick(f);
    check(gc::shim().charging(), "before 61 frames, R1 let go does nothing");
    f.pad(true, false);
    f.set(kStateTimer, 51);
    tick(f);
    check(gc::shim().charging(), "R1 held at 61 frames: on");
    f.set(kStateTimer, 1000);
    tick(f);
    check(gc::shim().charging(), "held: on as long as he holds it");
    f.set(kHoverLatch, std::uint8_t{1});
    f.pad(false, false);
    tick(f);
    check(!gc::shim().charging() && f.enters.back().first == kStop, "let go after 61 frames: the stop");
    check(f.get<std::int32_t>(kMoveLockTimer) == 25, "30 frames before he moves: 25 ticks");
    check(f.get<std::uint8_t>(kHoverLatch) == 0, "the hover's latch cleared");
    check(f.stops == 1, "the hum stopped");
}

void another_charge_after_the_lockout() {
    Fake& f = fresh();
    start_charge(f);
    f.set(kStateTimer, 70);
    f.pad(false, false);
    tick(f);
    check(!gc::shim().charging() && f.get<std::int32_t>(kMoveLockTimer) == 30, "stopped, 30 frames' lockout");
    f.set(kHeroState, kStop);
    f.enters.clear();
    start_charge(f);
    check(!gc::shim().charging() && f.enters.empty(), "a double tap in the lockout: nothing");
    f.pad(false, false);
    for (int i = 0; i < 30; ++i) {
        tick(f);
    }
    f.set(kHeroState, kIdle);
    start_charge(f);
    check(gc::shim().charging(), "after it: a charge again");
}

void other_ends_of_a_charge() {
    Fake& f = fresh();
    start_charge(f);
    f.set(kMagnetFlag, std::int16_t{1});
    tick(f);
    check(!gc::shim().charging() && f.enters.back().first == kStop, "the Magneboots hold him: the stop");

    f = fresh();
    start_charge(f);
    f.set(kItemAnim, std::uint8_t{1});
    tick(f);
    check(!gc::shim().charging() && f.enters.back().first == kStop, "changing weapon: the stop");

    f = fresh();
    start_charge(f);
    f.set(kHeroState, 0x16);
    tick(f);
    check(!gc::shim().charging() && !gc::shim().drives_movement(), "taken out of the hover: over");
}

void a_wall_bonks_him_back() {
    Fake& f = fresh();
    start_charge(f);
    f.set(kStateTimer, 40);
    f.set(kGroundSpeed, 0.19f);
    f.set(kFacing, 0.5f);
    f.set(kWallHit, std::uint8_t{1});
    f.set(kDisplacement, 0.01f);
    tick(f);
    check(!gc::shim().charging() && gc::shim().bonking() && f.enters.back().first == kWallBonk,
          "into the wall slower than three quarters: the bonk");
    check(near(f.get<float>(kGroundSpeed), 0.15f), "pushed back at 0.15 a frame");
    check(gc::shim().drives_movement(), "the copied code moves him in the bonk");

    f.turns.clear();
    f.set(kStateTimer, 1);
    f.set(kGroundDistance, 0.0f);
    move(f);
    const float away = 0.5f + kPi - 2.0f * kPi;
    const float speed = 0.15f - 0.006666667f;
    check(near(f.get<float>(kGroundSpeed), speed), "easing to a stop");
    check(near(f.get<float>(kVelocity), std::cos(away) * speed) && near(f.get<float>(kVelocity + 4), std::sin(away) * speed),
          "moving away from the wall");
    check(f.turns.size() == 1 && near(f.turns[0][1], 0.05f) && near(f.turns[0][3], 0.2821616f), "turn_to to face it");
    check(near(f.get<float>(kMoveYaw), 0.5f), "facing the wall he hit");

    f = fresh();
    start_charge(f);
    f.set(kStateTimer, 40);
    f.set(kWallHit, std::uint8_t{1});
    f.set(kVelocity, 0.19f);
    f.set(kDisplacement, 0.19f);
    tick(f);
    check(gc::shim().charging(), "a wall he slides along at speed: on");
    f.wall_ahead = true;
    tick(f);
    check(gc::shim().bonking(), "a wall in reach ahead: the bonk");
}

void pose_frames_are_clamped() {
    check(gc::pose_frames(4, 9, 14) == std::make_pair(4, 9), "the default loop");
    check(gc::pose_frames(12, 24, 14) == std::make_pair(12, 13), "past the end: to its last frame");
    check(gc::pose_frames(40, 50, 14) == std::make_pair(13, 13), "both past it: its last, held");
    check(gc::pose_frames(9, 4, 14) == std::make_pair(9, 9), "backwards: held at the first");
    check(gc::pose_frames(-3, 5, 0) == std::make_pair(0, 5), "frames unknown: as asked, from 0");
}

void the_charge_loops_the_long_jumps_flight() {
    Fake& f = fresh();
    start_charge(f);
    check(f.animations.size() == 1 && f.animations[0].id == gc::kChargePose && f.animations[0].start == 4 &&
              f.animations[0].frames == -2.0f,
          "the long jump's animation from the loop's first frame, on Going Commando's blend curve");
    check(f.loops.size() == 1 && f.loops[0].first == 4 && f.loops[0].second == 9, "its flight looped, 4..9");
    check(f.infos.size() == 1 && f.infos[0].find("has 14 frames") != std::string::npos, "its frames logged");
    const gc::Memory m = f.mem();
    check(m.read<std::uint8_t>(kFakeMoby + gc::kMobySound) == 0x33, "the hover's looping sound, not the long jump's");
    check(f.get<float>(gc::kHeroAnimSpeed) == 1.0f, "playing, not held");
    m.write(kFakeMoby + gc::kMobyTriggers, std::uint8_t{4});
    for (int i = 0; i < 600; ++i) {
        tick(f);
    }
    check(gc::shim().charging(), "ten seconds held: still charging");
    check(f.animations.size() == 1 && f.loops.size() == 1, "and never started again: the loop stays");
    check(m.read<std::uint8_t>(kFakeMoby + gc::kMobyTriggers) == 0, "no long jump sounds at its frames");
    check(f.sounds.size() == 1 && f.sounds[0].first == gc::kChargeSound && f.stops == 0, "one hum, for all of it");
    f.pad(false, false);
    tick(f);
    check(!gc::shim().charging() && f.stops == 1, "let go: stopped, the hum with it");
    f.set(kMoveLockTimer, 0);
    f.set(kHeroState, kIdle);
    start_charge(f);
    check(gc::shim().charging() && f.infos.size() == 1, "the frames logged once a level");

    f = fresh();
    gc::shim().set_pose(6, 6);
    start_charge(f);
    check(f.animations.size() == 1 && f.animations[0].start == 6 && f.loops.empty() &&
              f.get<float>(gc::kHeroAnimSpeed) == 0.0f,
          "one frame: held still there");
    f.set(kStateTimer, 70);
    f.pad(false, false);
    tick(f);
    check(!gc::shim().charging() && f.get<float>(gc::kHeroAnimSpeed) == 1.0f, "let go after it");

    f = fresh();
    gc::shim().set_pose(12, 24);
    start_charge(f);
    check(f.animations.size() == 1 && f.animations[0].start == 12 && f.loops.size() == 1 && f.loops[0].second == 13,
          "past its 14 frames: to its last, so the game takes the loop");

    f = fresh();
    start_charge(f);
    f.set(kStateTimer, 40);
    f.set(kWallHit, std::uint8_t{1});
    f.set(kDisplacement, 0.0f);
    tick(f);
    check(gc::shim().bonking() && f.animations.size() == 1, "the bonk: the wall bonk's own animation");
}

void one_blue_for_every_part_of_the_flame() {
    check(blue_flame(0x80808080u) == 0x80FF5018u, "the quads' and the ribbon's neutral grey: full blue, doubled");
    check(blue_flame(0x20808080u) == 0x20FF5018u, "the ribbon's faint ends: their alpha kept");
    check(blue_flame(0x7F2020FFu) == 0x7FFF9F2Fu, "the red glow: the same blue, as bright");
    check(blue_flame(0x4F007FFFu) == 0x4FFF9F2Fu, "the orange sparks");
    check(blue_flame(0x1FFFFFFFu) == 0x1FFF9F2Fu, "fading to a faint blue, not white");
    check(blue_flame(0u) == 0u, "black stays black");
    check(blue_flame(0x80404040u) == 0x8080280Cu, "a dimmer colour: a dimmer blue of the same hue");
}

void the_tint_is_only_the_flames_in_a_charge() {
    FlameTint tint;
    check(!tint.on(), "off at first");
    bool was = tint.enter(false);
    check(!tint.on(), "the flame's code, no charge: off");
    tint.leave(was);
    was = tint.enter(true);
    check(tint.on(), "the flame's code in a charge: on");
    tint.leave(was);
    check(!tint.on(), "off again after it");

    std::uint32_t colours[4] = {0x80808080u, 0x80808080u, 0x7F2020FFu, 0x20808080u};
    const std::array<std::uint32_t, 4> was_colours = FlameTint::tint_quad(colours);
    check(colours[0] == 0x80FF5018u && colours[2] == 0x7FFF9F2Fu && colours[3] == 0x20FF5018u, "a quad drawn blue");
    FlameTint::restore_quad(colours, was_colours);
    check(colours[0] == 0x80808080u && colours[2] == 0x7F2020FFu && colours[3] == 0x20808080u,
          "its record as the caller left it");
}

void the_ribbon_is_blue_in_a_charge() {
    Fake& f = fresh();
    const std::uint32_t first = f.disc.ribbon_l00;
    start_charge(f);
    check(f.mem().read<std::uint32_t>(first) == blue_flame(0x20808080u) &&
              f.mem().read<std::uint32_t>(first + 4) == blue_flame(0x80808080u),
          "the ribbon blue in a charge");
    f.set(kStateTimer, 100);
    f.pad(false, false);
    tick(f);
    check(!gc::shim().charging(), "stopped");
    check(f.mem().read<std::uint32_t>(first) == 0x20808080u && f.mem().read<std::uint32_t>(first + 4) == 0x80808080u,
          "orange again after it");

    f = fresh();
    start_charge(f);
    gc::shim().game.ribbon = first + 0x1000;
    f.mem().write(first + 0x1000, 0x12345678u);
    gc::shim().reset();
    check(f.mem().read<std::uint32_t>(first + 0x1000) == 0x12345678u,
          "another level loaded: its memory is not written with the last one's colours");
}

}  // namespace

int main() {
    requests_for_the_hover();
    the_hover_does_not_end_a_charge_by_itself();
    a_moby_is_hit_once_a_half_second();
    angles_and_ticks();
    states_and_animations_map();
    the_field_table_is_sound();
    load_translates_ratchets_hero();
    flush_writes_back_only_what_changed();
    entering_the_charge();
    the_double_tap_starts_a_charge();
    the_double_tap_window_is_half_a_second_on_pal();
    a_charge_tick_cruising();
    a_charge_tick_bursting_on_pal_at_a_faster_setting();
    the_charge_hits_what_is_ahead_once();
    holding_r1_keeps_it_going_and_letting_go_stops_it_after_61_frames();
    another_charge_after_the_lockout();
    other_ends_of_a_charge();
    a_wall_bonks_him_back();
    pose_frames_are_clamped();
    the_charge_loops_the_long_jumps_flight();
    one_blue_for_every_part_of_the_flame();
    the_tint_is_only_the_flames_in_a_charge();
    the_ribbon_is_blue_in_a_charge();
    if (failures != 0) {
        std::printf("%d failed\n", failures);
        return 1;
    }
    std::printf("all passed\n");
    return 0;
}
