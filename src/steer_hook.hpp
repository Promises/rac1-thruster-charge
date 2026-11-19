// SPDX-License-Identifier: MIT
//
// TODO(steer hook): delete this file once the SDK this mod builds against (sdk.lock) has the hook
// point, and use openrac::rac1::Hero_SteerPlanarVelocity from openrac/games/rac1.hpp instead (mod.cpp
// names it once, as SteerPlanarVelocity). Also add "rac1.player.steer_planar_velocity" to
// openrac-mod.json's "hooks" then: package_mod.py refuses an id its SDK's table lacks.
//
// The hook point, declared in the same shape as the SDK's generated ones (OpenRAC branch
// rac1-steer-hook, sdk/bindings/rac1.json): the port finds it by its id; one without it refuses the
// mod at load with the reason.
#pragma once

#include <cstdint>

namespace charge_boots::hooks {

// Steers the hero's planar velocity (+0xE0) toward his target speed (+0x190) along his move yaw
// (+0x180), by at most step a tick, up to three times that as it turns him (1.7 times in state
// 0x81); in gravity modes 1 and 2 it eases the velocity's length along the ground toward his target
// speed by step instead. steer_hero_planar_velocity (PAL func_L00_00214520) and its level copies,
// one hook point. The move states call it after setting the target speed and the step, the
// Thruster-Pack's hover (state 0x81) among them.
struct Hero_SteerPlanarVelocity {
    static constexpr const char* id = "rac1.player.steer_planar_velocity";
    static constexpr const char* name = "Hero_SteerPlanarVelocity";
    static constexpr const char* signature = "void(f32)";
    using fn = void (*)(float);
};

}  // namespace charge_boots::hooks
