// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where Ratchet & Clank keeps what this mod reads and writes, on both discs. The hero's field
// offsets are the same on PAL and NTSC; the bases differ (NTSC is not a flat 0x100 below PAL:
// port/game/rac1-ntsc/host/rac1_version.h maps each PAL address to its NTSC one).
//   games/rac1/decomp/include/structs/hero.h (PAL), games/rac1/ntsc-decomp/include/rnc/hero/hero.h
#pragma once

#include <cstdint>

namespace charge_boots {

// --- The hero (offsets from g_hero) ---

// Hero.motion.rot.z: his facing, the yaw his moby is drawn at.
constexpr std::uint32_t kFacing = 0x98u;
// Hero.motion.velocity: his velocity this tick, units a tick (x, y, z).
constexpr std::uint32_t kVelocity = 0xE0u;
// Hero.motion.displacement: how far he actually moved this tick, after collision (x, y, z).
constexpr std::uint32_t kDisplacement = 0x100u;
// Hero.motion.move_yaw: where the stick asks him to go; steer_hero_planar_velocity moves him along it.
constexpr std::uint32_t kMoveYaw = 0x180u;
// Hero.motion.yaw_error: how far turn_to still has to turn him; the hover's body lean rolls him by it
// (adjust_hero_pose_targets_from_motion, hero_steering_and_tilt.c:352-362).
constexpr std::uint32_t kYawError = 0x188u;
// Hero.target_speed: the planar speed steer_hero_planar_velocity steers him to, units a tick.
constexpr std::uint32_t kTargetSpeed = 0x190u;
// Hero.move_lock_timer: ticks before the stop and idle states let him move or jump again.
constexpr std::uint32_t kMoveLockTimer = 0x1C4u;
// Hero.wall_hit: the wall probe found a wall in his way this tick.
constexpr std::uint32_t kWallHit = 0x257u;
// Hero.ground_distance: from him to the ground under him.
constexpr std::uint32_t kGroundDistance = 0x2DCu;
// Hero.grounded_frames: ticks on the ground.
constexpr std::uint32_t kGroundedFrames = 0x300u;
// Hero.unk308: 1 while the Magneboots hold him to a magnetic surface.
constexpr std::uint32_t kMagnetFlag = 0x308u;
// Hero.stick_x: the stick across, -1 left to 1 right (the d-pad when the stick is idle).
constexpr std::uint32_t kStickX = 0x1D20u;
// Hero.moby: the hero's moby (a game address).
constexpr std::uint32_t kMoby = 0x2080u;
// Hero.state.current and Hero.state.control_mode.
constexpr std::uint32_t kHeroState = 0x2084u;
constexpr std::uint32_t kControlMode = 0x208Cu;
// Hero.vehicle_mode: nonzero in a vehicle, as Clank, on a board.
constexpr std::uint32_t kVehicleMode = 0x20A4u;
// Hero.gravity_mode: 0 the world's down, 1 and 2 a magnetic surface's.
constexpr std::uint32_t kGravityMode = 0x20B3u;
// Hero.stick_magnitude: how far the stick is pushed, 0 to 1.
constexpr std::uint32_t kStickMagnitude = 0x229Cu;
// Hero.unk22CA (NTSC: hover_latch): set as the hover (state 0x81) starts, cleared as it ends; while
// it is set, idle puts him back in the hover (hero_item_input_and_hud.c:180-186).
constexpr std::uint32_t kHoverLatch = 0x22CAu;

// --- The pad (g_controller_state, PadState: include/structs/pad.h) ---
constexpr std::uint32_t kPadHeld = 0x1A0u;
constexpr std::uint32_t kPadPressed = 0x1A4u;
constexpr std::uint32_t kPadR1 = 0x0008u;

// --- Hero states (games/rac1/decomp/include/enums/hero_state.h) ---
constexpr std::int32_t kIdle = 0x0;
constexpr std::int32_t kStop = 0x3;
constexpr std::int32_t kFall = 0x6;
// The Thruster-Pack's hover (HERO_STATE_DELAYED_JUMP_OR_R1_REPEAT_EXIT_WAIT): anim 0x13, control
// mode run, ground spring at +0.17 (hero_vehicle_and_collision.c:1498-1512,
// hero_water_and_ground_motion.c:2275-2318). The charge runs in it.
constexpr std::int32_t kHover = 0x81;
// The wall bonk: turned to face away, pushed back at 9 units a second easing to 0 (anim 0x29;
// hero_vehicle_and_collision.c:2296-2304, hero_water_and_ground_motion.c:1351-1362). Numerically the
// same as Going Commando's charge bonk (its state 0x77).
constexpr std::int32_t kWallBonk = 0x7A;

// Control modes.
constexpr std::int32_t kModeIdle = 0x0;
constexpr std::int32_t kModeRun = 0x1;
constexpr std::int32_t kModeFall = 0x2;
constexpr std::int32_t kModeJump = 0x4;
constexpr std::int32_t kModeHurt = 0x7;
constexpr std::int32_t kModeCrouch = 0xC;

// --- Items ---
// What makes the boots his: the Persuader (item 0x23, the vendor discount). Ratchet & Clank has no
// Hypnomatic (that is Going Commando's item 55); the gadget an NPC on Pokitaru hands over is this
// one: Fred, the resort owner, takes the Raritanium for it (PAL
// src/overlays/l11_pokitaru/resort_owner_and_water.c:1219-1224, NTSC
// src/overlays/l11/mobys/fred_pokitaru.c:129-135). He sets the owned byte himself, not through
// GiveItem (rac1.item.give), so the mod reads the byte rather than hooking the give.
constexpr std::uint32_t kUnlockItem = 0x23u;

struct Disc {
    // g_hero.
    std::uint32_t hero;
    // g_controller_state: pad 0's state.
    std::uint32_t pad;
    // item_available: one owned byte per item id (0x25 of them).
    std::uint32_t items_owned;
    // g_frame_time: seconds a tick (1/60 at 60 ticks a second).
    std::uint32_t frame_time;
    // The hero's body-tilt records (hero_eases: 0xB0-byte HeroEase rows that ease his joints toward
    // their rot_target, +0x60; Hero_SetTiltSpringParams sets their spring, +0xA4 and +0xA8). Level
    // 0's address (PAL D_L00_0017A780, NTSC D_L00_0017A680, the NTSC decomp's hero_eases): every
    // level has its own copy, which relocate_level(0, ...) finds.
    std::uint32_t lean_l00;
};

inline Disc disc_for(bool ntsc) {
    if (ntsc) {
        return {0x0013F350u, 0x0013C940u, 0x0013D4C0u, 0x0015ED6Cu, 0x0017A680u};
    }
    return {0x0013F450u, 0x0013CA40u, 0x0013D5C8u, 0x0015EE6Cu, 0x0017A780u};
}

}  // namespace charge_boots
