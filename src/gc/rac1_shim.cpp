// SPDX-License-Identifier: GPL-3.0-or-later
//
// The translation layer (rac1_shim.hpp says what it is): Going Commando's hero kept as a copy, and
// the polyfills the copied charge code calls in place of Going Commando's functions. Each says what
// it is in Going Commando (rac2-decomp at 005bfc4d, its file), what stands in for it here, and what
// differs.
#include "rac1_shim.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../layout.hpp"

namespace tc = thruster_charge;
using thruster_charge::gc::Shim;
using thruster_charge::gc::shim;

// --- The buffers the copied code works on (gc/shim/common.h names them) ---

extern "C" {
alignas(16) unsigned char gc_shim_hero[thruster_charge::gc::kHeroBytes];
alignas(16) unsigned char gc_shim_lean[thruster_charge::gc::kLeanBytes];
alignas(16) unsigned char gc_shim_pad[thruster_charge::gc::kPadBytes];
std::int32_t gc_shim_gravity_flag;
std::int32_t gc_shim_hits[8];
unsigned char gc_shim_items_owned[0x40];
std::uint32_t gc_shim_item_slots[8];
std::int32_t gc_shim_double_tap_timer;
unsigned char gc_shim_flame_a[16];
unsigned char gc_shim_flame_b[16];
}

namespace {

namespace gc = thruster_charge::gc;

alignas(16) float g_scratch[3][4];

// What load last saw of the copy (and the tilt records), so flush writes back only what changed.
alignas(16) unsigned char g_hero_seen[gc::kHeroBytes];
alignas(16) unsigned char g_lean_seen[gc::kLeanBytes];

// The buffers the copied code's 32-bit handles may point into.
struct Region {
    unsigned char* base;
    std::size_t size;
};

const std::array<Region, 3> kRegions{{
    {gc_shim_hero, sizeof gc_shim_hero},
    {gc_shim_lean, sizeof gc_shim_lean},
    {reinterpret_cast<unsigned char*>(g_scratch), sizeof g_scratch},
}};

constexpr float kGcFps = 60.0f;
// Going Commando's Charge Boots: owned (D_001A7B08[0x26]) and worn (D_001A7390[3] == 54). Both
// read "the Thruster-Pack is on" here (Shim::thruster_on).
constexpr std::int32_t kChargeBootsItem = 0x26;
constexpr std::uint32_t kChargeBootsWorn = 54;

double read_kind(const gc::Memory& m, std::uint32_t address, gc::Kind kind) {
    switch (kind) {
    case gc::Kind::F32:
        return m.read<float>(address);
    case gc::Kind::S32:
        return m.read<std::int32_t>(address);
    case gc::Kind::S16:
        return m.read<std::int16_t>(address);
    case gc::Kind::U8:
        return m.read<std::uint8_t>(address);
    }
    return 0.0;
}

void write_kind(const gc::Memory& m, std::uint32_t address, gc::Kind kind, double v) {
    switch (kind) {
    case gc::Kind::F32:
        m.write(address, static_cast<float>(v));
        return;
    case gc::Kind::S32:
        m.write(address, static_cast<std::int32_t>(std::lround(v)));
        return;
    case gc::Kind::S16:
        m.write(address, static_cast<std::int16_t>(std::lround(v)));
        return;
    case gc::Kind::U8:
        m.write(address, static_cast<std::uint8_t>(std::lround(v)));
        return;
    }
}

// The copy, as a Memory at offset 0.
gc::Memory hero_copy() { return gc::Memory{gc_shim_hero}; }

float copy_f32(std::uint32_t offset) { return hero_copy().read<float>(offset); }

void approach(float& value, float goal, float step) {
    const float diff = std::clamp(goal - value, -step, step);
    value += diff;
}

float* resolve_f32(std::int32_t handle) { return static_cast<float*>(shim().resolve(handle)); }

}  // namespace

namespace thruster_charge::gc {

Shim& shim() {
    static Shim s;
    return s;
}

void Shim::set_tick(float frame_time) {
    const float dt = tick_seconds(frame_time);
    m_k = dt * kGcFps;
    m_clock += dt;
}

void Shim::set_speed_scale(float scale) {
    m_speed_scale = std::isfinite(scale) ? std::clamp(scale, 0.25f, 2.0f) : 1.0f;
}

std::int32_t Shim::rac1_state_now() const { return game.memory.read<std::int32_t>(game.hero + kHeroState); }

void Shim::load() {
    const Memory& m = game.memory;
    if (m.base == nullptr || game.hero == 0) {
        return;
    }
    const Memory copy = hero_copy();
    for (const Field& f : kHeroFields) {
        for (std::uint32_t i = 0; i < f.count; ++i) {
            double v = read_kind(m, game.hero + f.rac1 + i * size_of(f.rac1_kind), f.rac1_kind);
            if (f.unit == Unit::PerFrame) {
                v /= m_k;
            } else if (f.unit == Unit::Frames) {
                v = std::floor(v * m_k + 1e-3);
            }
            // The `speed` setting undone (flush applies it), so the copied code sees its own speeds.
            if (f.gc == kGcTargetSpeed && m_charging) {
                v /= m_speed_scale;
            }
            write_kind(copy, f.gc + i * size_of(f.gc_kind), f.gc_kind, v);
        }
    }
    const std::int32_t state = gc_state(rac1_state_now(), m_charging, m_bonking);
    copy.write(kGcState, state);
    copy.write(kGcControlMode, gc_control_mode(m.read<std::int32_t>(game.hero + kControlMode), state));
    // The hero's moby, as a game address: at +0x2290 as a word, and as the Hero union's pointer.
    const std::uint32_t moby = m.read<std::uint32_t>(game.hero + kMoby);
    copy.write(kGcMoby, moby);
    copy.write(kGcMobyPointer, static_cast<std::uintptr_t>(moby));

    gc_shim_gravity_flag = m.read<std::uint8_t>(game.hero + kGravityMode);
    std::memcpy(gc_shim_pad + kPadHeld, m.base + game.pad + kPadHeld, 8);
    // A double tap is one charge of charge_time: the copied decider sees R1 held for that long, then
    // let go, whatever the pad does (it stops the charge on R1 let go after 61 frames).
    if (m_charging && state == kGcCharge) {
        std::uint32_t held = 0;
        std::memcpy(&held, gc_shim_pad + kPadHeld, 4);
        held = charge_holds_r1(copy.read<std::int32_t>(0x1B8), m_charge_frames) ? (held | kPadR1) : (held & ~kPadR1);
        std::memcpy(gc_shim_pad + kPadHeld, &held, 4);
    }
    const bool thruster = thruster_on();
    gc_shim_items_owned[kChargeBootsItem] = thruster ? 1 : 0;
    gc_shim_item_slots[3] = thruster ? kChargeBootsWorn : 0;
    if (game.lean != 0) {
        std::memcpy(gc_shim_lean, m.base + game.lean, kLeanBytes);
    }
    std::memcpy(g_hero_seen, gc_shim_hero, kHeroBytes);
    std::memcpy(g_lean_seen, gc_shim_lean, kLeanBytes);
    m_loaded = true;
}

void Shim::flush() {
    const Memory& m = game.memory;
    if (!m_loaded || m.base == nullptr || game.hero == 0) {
        return;
    }
    const Memory copy = hero_copy();
    for (const Field& f : kHeroFields) {
        if (f.access != Access::ReadWrite) {
            continue;
        }
        for (std::uint32_t i = 0; i < f.count; ++i) {
            const std::uint32_t at = f.gc + i * size_of(f.gc_kind);
            if (std::memcmp(gc_shim_hero + at, g_hero_seen + at, size_of(f.gc_kind)) == 0) {
                continue;
            }
            double v = read_kind(copy, at, f.gc_kind);
            if (f.unit == Unit::PerFrame) {
                v *= m_k;
            } else if (f.unit == Unit::Frames) {
                v /= m_k;
            }
            // The `speed` setting: the charge's speeds, as the charge asks for them.
            if (f.gc == kGcTargetSpeed && m_charging) {
                v *= m_speed_scale;
            }
            write_kind(m, game.hero + f.rac1 + i * size_of(f.rac1_kind), f.rac1_kind, v);
        }
    }
    if (game.lean != 0) {
        for (std::size_t at = 0; at < kLeanBytes; at += 4) {
            if (std::memcmp(gc_shim_lean + at, g_lean_seen + at, 4) != 0) {
                std::memcpy(m.base + game.lean + at, gc_shim_lean + at, 4);
            }
        }
    }
    std::memcpy(g_hero_seen, gc_shim_hero, kHeroBytes);
    std::memcpy(g_lean_seen, gc_shim_lean, kLeanBytes);
}

void Shim::observe() {
    if (game.memory.base == nullptr || game.hero == 0) {
        return;
    }
    const std::int32_t state = rac1_state_now();
    if (m_charging && state != kHover) {
        m_charging = false;
        end_charge();
    }
    if (m_bonking && state != kWallBonk) {
        m_bonking = false;
    }
}

bool Shim::drives_movement() const {
    if (!m_charging && !m_bonking) {
        return false;
    }
    const std::int32_t state = rac1_state_now();
    return (m_charging && state == kHover) || (m_bonking && state == kWallBonk);
}

void Shim::end_tick() {
    if (!m_sound_kept && m_voice >= 0) {
        if (game.sound_stop) {
            game.sound_stop(m_voice);
        }
        m_voice = -1;
    }
    m_sound_kept = false;
}

void Shim::reset() {
    if (m_charging) {
        end_charge();
    }
    m_charging = false;
    m_bonking = false;
    m_loaded = false;
    m_timer_carry = 0.0f;
    gc_shim_double_tap_timer = 0;
    m_hits.clear();
    // Each level's Ratchet has its own animations: their frames logged again.
    m_pose_frames_logged = -1;
}

void Shim::end_charge() {
    // Idle puts him back in the hover while its latch is set (hero_item_input_and_hud.c:180-186).
    if (game.memory.base != nullptr && game.hero != 0) {
        game.memory.write(game.hero + kHoverLatch, std::uint8_t{0});
    }
    if (m_voice >= 0 && game.sound_stop) {
        game.sound_stop(m_voice);
    }
    m_voice = -1;
    untint_flames();
    // The pose let go: the next state's animation sets the speed again; one that does not finds
    // the game's own 1.
    if (m_pose && m_pose_frozen && game.memory.base != nullptr &&
        game.memory.read<float>(game.hero + kHeroAnimSpeed) == 0.0f) {
        game.memory.write(game.hero + kHeroAnimSpeed, 1.0f);
    }
    m_pose = false;
    m_pose_frozen = false;
}

std::int32_t Shim::enter_state(std::int32_t gc_action) {
    const std::int32_t to = rac1_state(gc_action);
    if (to < 0 || !game.enter_move_state) {
        unreachable("hero_l00_set_action: a state the layer does not know");
        return 0;
    }
    flush();
    const bool was_charging = m_charging;
    // Ratchet & Clank starts the state's own animation when force is non-zero; Going Commando's
    // `animate` 0 (the stop) then sets its skid itself, which has no counterpart here.
    const bool entered = game.enter_move_state(to, 1) != 0 && rac1_state_now() == to;
    if (entered) {
        m_charging = gc_action == kGcCharge;
        m_bonking = gc_action == kGcBonk;
        if (m_charging) {
            m_hits.clear();
            tint_flames();
        }
    }
    if (was_charging && !m_charging) {
        end_charge();
    }
    load();
    return entered ? 1 : 0;
}

void* Shim::resolve(std::int32_t handle) const {
    const auto low = static_cast<std::uint32_t>(handle);
    for (const Region& r : kRegions) {
        const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(r.base));
        const std::uint32_t offset = low - base;
        if (offset < r.size) {
            return r.base + offset;
        }
    }
    return nullptr;
}

bool Shim::handles_unambiguous() const {
    for (std::size_t i = 0; i < kRegions.size(); ++i) {
        for (std::size_t j = i + 1; j < kRegions.size(); ++j) {
            const auto a = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(kRegions[i].base));
            const auto b = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(kRegions[j].base));
            if (b - a < kRegions[i].size || a - b < kRegions[j].size) {
                return false;
            }
        }
    }
    return true;
}

void Shim::play_animation(std::int32_t gc_animation, std::int32_t blend, float frames) {
    const std::int32_t animation = rac1_animation(gc_animation);
    if (animation < 0 || !game.set_animation) {
        return;
    }
    // Going Commando's frames are Ratchet & Clank's: below 0 a blend curve (the hover's entry passes
    // -2 too), above it a time in frames.
    if (animation != kChargePose) {
        game.set_animation(animation, blend, frames);
        return;
    }
    const Memory& m = game.memory;
    const std::uint32_t moby = m.read<std::uint32_t>(game.hero + kMoby);
    if (moby == 0) {
        return;
    }
    const std::uint32_t sequences = m.read<std::uint32_t>(moby + kMobyClass) + kClassSequences;
    const std::uint32_t pose = m.read<std::uint32_t>(sequences + static_cast<std::uint32_t>(kChargePose) * 4);
    const std::uint32_t hover = m.read<std::uint32_t>(sequences + static_cast<std::uint32_t>(kHoverAnimation) * 4);
    const auto count = pose != 0 ? static_cast<std::int32_t>(m.read<std::uint8_t>(pose + kSequenceFrameCount)) : 0;
    const std::pair<std::int32_t, std::int32_t> held = pose_frames(m_pose_from, m_pose_to, count);
    if (count != m_pose_frames_logged && game.info) {
        m_pose_frames_logged = count;
        char line[160];
        std::snprintf(line, sizeof line,
                      "Thruster Charge: the long jump's animation (0x26) has %d frames; the charge plays from %d "
                      "and loops %d..%d",
                      count, pose_start(held.first, held.second), held.first, held.second);
        game.info(line);
    }
    // Going Commando's `blend` (0) is the start frame here: the first after the take-off.
    game.set_animation(animation, pose_start(held.first, held.second), frames);
    // The hover's looping sound, not the long jump's.
    m.write(moby + kMobySound, hover != 0 ? m.read<std::uint8_t>(hover + kSequenceSound) : std::uint8_t{0xFF});
    m_pose_frozen = held.first == held.second;
    if (!m_pose_frozen && game.loop_animation) {
        game.loop_animation(held.first, held.second);
    }
    m_pose = true;
    hold_pose();
}

void Shim::set_pose(std::int32_t from, std::int32_t to) {
    m_pose_from = from;
    m_pose_to = to;
}

void Shim::hold_pose() {
    if (!m_pose || game.memory.base == nullptr) {
        return;
    }
    const Memory& m = game.memory;
    const std::uint32_t moby = m.read<std::uint32_t>(game.hero + kMoby);
    if (moby != 0) {
        m.write(moby + kMobyTriggers, std::uint8_t{0});
    }
    if (m_pose_frozen) {
        m.write(game.hero + kHeroAnimSpeed, 0.0f);
    }
}

void Shim::tint_flames() {
    if (game.ribbon == 0 || m_ribbon_tinted != 0 || game.memory.base == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < kRibbonColours; ++i) {
        const std::uint32_t at = game.ribbon + static_cast<std::uint32_t>(i * 4);
        m_ribbon_saved[i] = game.memory.read<std::uint32_t>(at);
        game.memory.write(at, blue_flame(m_ribbon_saved[i]));
    }
    m_ribbon_tinted = game.ribbon;
}

void Shim::untint_flames() {
    // Only where it was tinted, and only while that level is loaded: a level's program brings its
    // own table, untinted.
    if (m_ribbon_tinted != 0 && m_ribbon_tinted == game.ribbon && game.memory.base != nullptr) {
        for (std::size_t i = 0; i < kRibbonColours; ++i) {
            game.memory.write(m_ribbon_tinted + static_cast<std::uint32_t>(i * 4), m_ribbon_saved[i]);
        }
    }
    m_ribbon_tinted = 0;
}

void Shim::keep_sound() {
    m_sound_kept = true;
    if (m_voice < 0 && game.sound_play) {
        m_voice = game.sound_play(kChargeSound, true);
    }
}

std::int32_t Shim::step_frame_timer(std::int32_t* timer) {
    if (*timer <= 0) {
        m_timer_carry = 0.0f;
        return 0;
    }
    m_timer_carry += m_k;
    const auto frames = static_cast<std::int32_t>(m_timer_carry);
    m_timer_carry -= static_cast<float>(frames);
    *timer = std::max(0, *timer - frames);
    return *timer == 0 ? 1 : 0;
}

bool Shim::thruster_on() const {
    const Memory& m = game.memory;
    if (m.base == nullptr || game.hero == 0) {
        return false;
    }
    return m.read<std::int32_t>(game.hero + kPackSlotState) == kSlotReady &&
           m.read<std::int32_t>(game.hero + kPackSlotItem) == kThrusterPack &&
           m.read<std::int16_t>(game.hero + kPackDisabled) == 0;
}

bool Shim::item_input_allowed() const {
    const Memory& m = game.memory;
    if (m.base == nullptr || game.hero == 0) {
        return false;
    }
    // The stop's lockout (Going Commando's 30 frames, +0x1E0 here Ratchet & Clank's move_lock_timer):
    // no new charge until it has run out.
    return thruster_on() && m.read<std::int32_t>(game.hero + kMoveLockTimer) == 0 &&
           m.read<std::uint8_t>(game.hero + kVehicleMode) == 0 &&
           m.read<std::uint8_t>(game.hero + kGravityMode) == 0 && m.read<std::int16_t>(game.hero + kMagnetFlag) == 0;
}

void Shim::unreachable(const char* what) {
    // Each message once (by its address's low bits: there are few).
    const std::uint32_t bit = 1u << (reinterpret_cast<std::uintptr_t>(what) % 31);
    if ((m_unreachable_logged & bit) == 0) {
        m_unreachable_logged |= bit;
        if (game.log) {
            game.log(what);
        }
    }
}

}  // namespace thruster_charge::gc

// --- The polyfills, by Going Commando's names (gc/shim/common.h renames the copied code's calls) ---

extern "C" {

// --- The translation layer's own (the copied files' marked lines) ---

std::int32_t gc_shim_enter_state(std::int32_t action, std::int32_t /*animate*/) { return shim().enter_state(action); }

float* gc_shim_scratch(std::int32_t index) { return g_scratch[std::clamp(index, 0, 2)]; }

std::int32_t gc_shim_item_input_allowed(void) { return shim().item_input_allowed() ? 1 : 0; }

// --- Math: Going Commando's own, the same on the host ---

// FastAddRotations: a + b, wrapped to -pi..pi.
float gc_shim_FastAddRotations(float a, float b) { return tc::wrap_angle(a + b); }

// math_fast_sin_or_cos_a / _b: cos and sin (hero_compute_push_direction builds x from _a, y from _b).
float gc_shim_math_fast_sin_or_cos_a(float a) { return std::cos(a); }
float gc_shim_math_fast_sin_or_cos_b(float a) { return std::sin(a); }

// FastVecScale, FastVecAdd: the EE's four-lane vector ops.
void gc_shim_FastVecScale(void* dst, void* src, float s) {
    auto* d = static_cast<float*>(dst);
    const auto* v = static_cast<const float*>(src);
    for (int i = 0; i < 4; ++i) {
        d[i] = v[i] * s;
    }
}

void gc_shim_h_FastVecScale(std::int32_t dst, std::int32_t src, float s) {
    float* d = resolve_f32(dst);
    float* v = resolve_f32(src);
    if (d != nullptr && v != nullptr) {
        gc_shim_FastVecScale(d, v, s);
    }
}

void gc_shim_h_FastVecAdd(std::int32_t dst, std::int32_t a, std::int32_t b) {
    float* d = resolve_f32(dst);
    float* x = resolve_f32(a);
    float* y = resolve_f32(b);
    if (d != nullptr && x != nullptr && y != nullptr) {
        for (int i = 0; i < 4; ++i) {
            d[i] = x[i] + y[i];
        }
    }
}

// FastVecLength: x, y and z.
float gc_shim_FastVecLength(void* v) {
    const auto* f = static_cast<const float*>(v);
    return std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
}

// ApproachValue (rac2-decomp src/game/002A7750.c): *value toward target by at most step; returns
// what is left. The step is a frame's: k times it a tick.
float gc_shim_h_ApproachValue(std::int32_t value, float target, float step) {
    float* v = resolve_f32(value);
    if (v == nullptr) {
        return 0.0f;
    }
    approach(*v, target, step * shim().k());
    return target - *v;
}

// math_spring_float_to_goal (rac2-decomp src/game/002A7750.c): *value sprung toward goal, its
// velocity by stiffness and damping, at most limit a frame; snapped to the goal within a hundredth
// of the limit. Ratchet & Clank's Spring is the same spring (moby_ground_collision.c:1643-1675) and
// scales it to its tick as here: stiffness and damping by k * k, the limit and the velocity by k.
// The velocity is kept in Going Commando's units (a frame's).
float gc_shim_h_math_spring_float_to_goal(std::int32_t value, std::int32_t velocity, float goal, float stiffness,
                                          float damping, float limit) {
    float* p = resolve_f32(value);
    float* v = resolve_f32(velocity);
    if (p == nullptr || v == nullptr) {
        return 0.0f;
    }
    const float k = shim().k();
    const float gap = goal - *p;
    const float lim = limit * k;
    float vt = *v * k;
    vt += stiffness * k * k * gap - damping * k * k * vt;
    if (lim > 0.0f) {
        vt = std::clamp(vt, -lim, lim);
    }
    vt = std::clamp(vt, -std::fabs(gap), std::fabs(gap));
    *p += vt;
    const float error = goal - *p;
    if (std::fabs(error) < lim * 0.01f) {
        *p = goal;
        vt = 0.0f;
    }
    *v = vt / k;
    return error;
}

// --- The hero ---

// hero_resolve_stick_direction (rac2-decomp src/levels/shared/L00_002AFBC0-input.c): the target
// speed and move yaw from the stick. Nothing here: the charge overwrites the target speed on the next
// line, and goes along his facing (hero_compute_push_direction(99999)), not the move yaw.
void gc_shim_hero_resolve_stick_direction(float /*scale*/, std::int32_t /*mode*/) {}

// pad_clamp_stick_to_unit_range (rac2-decomp src/levels/shared/L00_002A6420.c:289-304): the
// stick's x (negated) and y, each eased toward it by rate a frame within -1..1, into out; the
// eased values kept at stick + 8 and + 4. As there, on the stick Ratchet & Clank read (+0x1D20);
// the rate is a frame's: k times it a tick.
void gc_shim_h_pad_clamp_stick_to_unit_range(std::int32_t out, std::int32_t stick, float rate) {
    float* o = resolve_f32(out);
    float* s = resolve_f32(stick);
    if (o == nullptr || s == nullptr) {
        return;
    }
    const float step = rate * shim().k();
    approach(s[2], -copy_f32(thruster_charge::gc::kGcStick), step);
    s[2] = std::clamp(s[2], -1.0f, 1.0f);
    o[0] = s[2];
    approach(s[1], copy_f32(thruster_charge::gc::kGcStick + 4), step);
    s[1] = std::clamp(s[1], -1.0f, 1.0f);
    o[1] = s[1];
}

// hero_rotate_matrix (L00_002AFBC0.c:822-834): his matrix turned by x, y, z. Here his facing
// (motion.rot.z) turned by z, a frame's turn (k times it a tick); Ratchet & Clank builds his matrix
// from it. x and y are 0 in the charge.
void gc_shim_hero_rotate_matrix(float /*x*/, float /*y*/, float z) {
    const gc::Memory copy = hero_copy();
    copy.write(0x98u, tc::wrap_angle(copy.read<float>(0x98u) + z * shim().k()));
}

// hero_approach_speed (L00_002AFBC0.c:863-868): ground speed toward target speed, up by rise and
// down by fall a frame. Ratchet & Clank's speed_step is the same (hero_animation_slots.c:2880),
// called through Hero_ApproachSpeed's entry with the rates scaled to its tick (k * k).
void gc_shim_hero_approach_speed(float rise, float fall) {
    Shim& s = shim();
    if (!s.game.approach_speed) {
        return;
    }
    const float k2 = s.k() * s.k();
    s.call_game([&] { s.game.approach_speed(rise * k2, fall * k2); });
}

// hero_compute_push_direction (L00_002B4208.c): velocity at his ground speed along angle (his facing
// above pi), on the ground's pitch, or along the magnetic ground's frame. Ratchet & Clank's
// set_planar_vel is the same (hero_animation_slots.c:2899), called through
// Hero_SetPlanarVelocity's entry.
void gc_shim_hero_compute_push_direction(float angle) {
    Shim& s = shim();
    if (s.game.set_planar_velocity) {
        s.call_game([&] { s.game.set_planar_velocity(angle); });
    }
}

// hero_steer_heading (L00_002AFBC0.c:852-858): his facing toward his move yaw, by an acceleration,
// damping and a limit a frame. Ratchet & Clank's turn_to is the same, called through
// Hero_TurnToHeading's entry with them scaled to its tick (k * k, k * k, k). Going Commando's other
// way (on magnetic ground) is not reached: the charge stops there.
void gc_shim_hero_steer_heading(std::int32_t mode, float accel, float damping, float limit) {
    Shim& s = shim();
    if (!s.game.turn_to_heading) {
        return;
    }
    const float k = s.k();
    s.call_game([&] { s.game.turn_to_heading(mode, accel * k * k, damping * k * k, limit * k); });
}

// Going Commando's ways on magnetic ground (D_L20_001A8FF0 set): the charge stops on magnetic ground
// (hero_l00_decide_state, +0x348) and does not start there (gc_shim_item_input_allowed), so they
// are not reached. Each does nothing here and says so once.
void gc_shim_h_hero_apply_mode_transform(std::int32_t, std::int32_t, float) {
    shim().unreachable("Thruster Charge: hero_apply_mode_transform reached (magnetic ground): not translated");
}
void gc_shim_h_hero_apply_mode_transform_reversed(std::int32_t, std::int32_t, float) {
    shim().unreachable("Thruster Charge: hero_apply_mode_transform_reversed reached (magnetic ground): not translated");
}
float gc_shim_h_vec_spring_to_target(std::int32_t, std::int32_t, std::int32_t, float, float, float) {
    shim().unreachable("Thruster Charge: vec_spring_to_target reached (magnetic ground): not translated");
    return 0.0f;
}
void gc_shim_h_vec_scale_to_length(std::int32_t, std::int32_t, float) {
    shim().unreachable("Thruster Charge: vec_scale_to_length reached (magnetic ground): not translated");
}
void gc_shim_h_cam_set_view_space_height(std::int32_t, std::int32_t, float) {
    shim().unreachable("Thruster Charge: cam_set_view_space_height reached (magnetic ground): not translated");
}

// set_hero_animation(animation, blend, frames): his animation. Ratchet & Clank's own
// set_hero_animation (hero_mapped_animations.c:342-404) with kAnimations' id: the charge's 0x8B is
// the Thruster-Pack long jump's 0x26, held in its flight (Shim::play_animation); the bonk's and the
// stop's are their states' own, which Ratchet & Clank's change of state started.
void gc_shim_set_hero_animation(std::int32_t animation, std::int32_t blend, float frames) {
    shim().play_animation(animation, blend, frames);
}

// cam_set_limits_first, _third, _fourth (rac2-decomp L00_002A6420.c:442-464): despite rac2-decomp's
// names, they set the spring of body-tilt records 0, 2 and 3 (their rows are 0xB0 bytes, the pair at
// +0xA4, +0xA8): Ratchet & Clank's Hero_SetTiltSpringParams on its hero_eases (accel, max speed), the
// same table D_L20_00267210 is. Here written to those records, scaled to its tick (k * k, k).
namespace {
void set_tilt_spring(std::size_t record, float accel, float max_speed) {
    const float k = shim().k();
    const gc::Memory lean{gc_shim_lean};
    lean.write(static_cast<std::uint32_t>(record * gc::kLeanRecord + 0xA4), accel * k * k);
    lean.write(static_cast<std::uint32_t>(record * gc::kLeanRecord + 0xA8), max_speed * k);
}
}  // namespace

void gc_shim_cam_set_limits_first(float a, float b) { set_tilt_spring(0, a, b); }
void gc_shim_cam_set_limits_third(float a, float b) { set_tilt_spring(2, a, b); }
void gc_shim_cam_set_limits_fourth(float a, float b) { set_tilt_spring(3, a, b); }

// --- Sound ---

// snd_update_slot_object(slot, moby, sound): sound looped on the moby in voice slot `slot`, kept
// while called each frame. Going Commando's charge sound (0xD) is not in Ratchet & Clank: Ratchet's
// hover hum (kChargeSound) loops in its place, through the SDK's hero sounds, stopped when a tick
// passes without this call or the charge ends.
void gc_shim_snd_update_slot_object(std::int32_t /*slot*/, std::int32_t /*moby*/, std::int32_t /*sound*/) {
    shim().keep_sound();
}

// hero_play_sound(9, 0): the stop's sound. No sound of Ratchet's is known to match it: none.
std::int32_t gc_shim_hero_play_sound(std::int32_t /*sound*/, std::int32_t /*flags*/) { return -1; }

// --- Collision and hits ---

// coll_process_hit(radius, centre, ...): every moby in the sphere (radius at centre) but the hero's,
// into D_L20_001C4480, and their count. Here three lines through the sphere along his facing (its
// middle and its sides at 0.6 of the radius), through the SDK's collision_line on the mobys: the
// first moby on each, so at most three a frame where Going Commando takes all.
std::int32_t gc_shim_h_coll_process_hit(float radius, std::int32_t centre, std::int32_t, std::int32_t, std::int32_t) {
    Shim& s = shim();
    const float* c = resolve_f32(centre);
    if (c == nullptr || !s.game.moby_on_line) {
        return 0;
    }
    const float facing = copy_f32(0x98u);
    const float fx = std::cos(facing);
    const float fy = std::sin(facing);
    std::int32_t count = 0;
    for (const float side : {0.0f, -0.6f, 0.6f}) {
        const float sx = -fy * side * radius;
        const float sy = fx * side * radius;
        const float from[3] = {c[0] + sx - fx * radius, c[1] + sy - fy * radius, c[2]};
        const float to[3] = {c[0] + sx + fx * radius, c[1] + sy + fy * radius, c[2]};
        const std::int32_t slot = s.game.moby_on_line(from, to);
        if (slot < 0 || std::find(gc_shim_hits, gc_shim_hits + count, slot) != gc_shim_hits + count) {
            continue;
        }
        gc_shim_hits[count++] = slot;
    }
    return count;
}

// moby_is_in_special_class_range: Going Commando's filter on what the charge hits. Every moby the
// lines meet passes here; each one's own update decides what a hit does to it.
std::int32_t gc_shim_moby_is_in_special_class_range(std::int32_t /*moby*/) { return 1; }

// part_queue_impact_effect(victim, attacker, mask, position, velocity, damage): the hit posted to
// the victim. Here the SDK's moby_hit, the attacker the hero's slot, the velocity in Ratchet &
// Clank's units (k times). Going Commando posts one each frame the moby is in the sphere; here one a
// moby each half second (HitCooldown), since Ratchet & Clank's mobys read a hit each tick.
void gc_shim_h_part_queue_impact_effect(std::int32_t victim, std::int32_t, std::int32_t mask, std::int32_t position,
                                        std::int32_t velocity, float damage) {
    Shim& s = shim();
    const float* p = resolve_f32(position);
    const float* v = resolve_f32(velocity);
    if (p == nullptr || v == nullptr || !s.game.hit || !s.hits().ready(victim, s.clock())) {
        return;
    }
    const float k = s.k();
    gc::Hit hit{};
    hit.victim = victim;
    hit.attacker = s.game.self_slot ? s.game.self_slot() : -1;
    hit.mask = static_cast<std::uint32_t>(mask);
    hit.damage = damage;
    for (int i = 0; i < 3; ++i) {
        hit.position[i] = p[i];
        hit.velocity[i] = v[i] * k;
    }
    s.game.hit(hit);
    s.hits().mark(victim, s.clock());
}

// hero_test_reach_blend(out, height, reach) (L00_002A6420.c:466-478): a line from his capsule's
// edge (D_0018A084 less 0.02) out to reach ahead of him, at height, in his frame
// (hero_build_relative_offset); 1 when it meets the level. Here the same line through the SDK's
// collision_line, ahead along his facing on the world's up (the charge runs on flat ground only).
std::int32_t gc_shim_hero_test_reach_blend(float* out, float height, float reach) {
    Shim& s = shim();
    if (!s.game.level_on_line) {
        return 0;
    }
    const float facing = copy_f32(0x98u);
    const float fx = std::cos(facing);
    const float fy = std::sin(facing);
    const float near = copy_f32(0x264u) - 0.02f;
    const float x = copy_f32(0x80u);
    const float y = copy_f32(0x84u);
    const float z = copy_f32(0x88u) + height;
    const float from[3] = {x + fx * near, y + fy * near, z};
    const float to[3] = {x + fx * reach, y + fy * reach, z};
    if (!s.game.level_on_line(from, to)) {
        return 0;
    }
    if (out != nullptr) {
        *out = 0.0f;
    }
    return 1;
}

// --- The flames ---

// moby_transform_joint_to_world(moby, joint, out): a joint of the moby, in the world. The flames
// come from joints 0x16 and 0x17 of Going Commando's Ratchet, whose skeleton is not Ratchet &
// Clank's: here his position.
void gc_shim_moby_transform_joint_to_world(std::int8_t* /*moby*/, std::int32_t /*joint*/, std::int8_t* out) {
    std::memcpy(out, gc_shim_hero + 0x80, 16);
}

// part_spawn_object_relative_particle: a flame particle at the feet. Nothing here: Ratchet & Clank
// has no such particle, and the SDK's props need a model of the mod's, which it does not have yet.
void* gc_shim_part_spawn_object_relative_particle(std::int8_t*, void*, float*, std::int32_t*, std::int32_t) {
    return nullptr;
}

// --- Timers ---

// math_fast_float_d(&timer): the frame timer down by one, to 0. Here down by a tick's frames (k,
// its fractions carried to the next tick).
std::int32_t gc_shim_math_fast_float_d(std::int32_t* timer) { return shim().step_frame_timer(timer); }

}  // extern "C"
