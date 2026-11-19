// SPDX-License-Identifier: GPL-3.0-or-later
//
// TODO(movement hook): delete this file once the SDK this mod builds against (sdk.lock) has the
// hook point, and use openrac::rac1::Hero_UpdateMovement from openrac/games/rac1.hpp instead
// (mod.cpp names it once, as UpdateMovement). Also add "rac1.player.update_movement" to
// openrac-mod.json's "hooks" then: package_mod.py refuses an id its SDK's table lacks.
//
// The hook point, declared in the same shape as the SDK's generated ones (OpenRAC branch
// rac1-steer-hook, commit 58eb1fb1b, sdk/bindings/rac1.json): the port finds it by its id; one
// without it refuses the mod at load with the reason.
#pragma once

namespace thruster_charge::hooks {

// The hero's movement for the state he is in (+0x2084), once a tick from Hero_UpdateMotionInput
// after it reads the stick: a case per state that sets his target speed and turn, steers or sets his
// velocity (+0xE0) and applies gravity, the Thruster-Pack's hover (state 0x81) among them.
// hero_update_movement (PAL func_L00_002180F8) and each level's own build of it, one hook point.
struct Hero_UpdateMovement {
    static constexpr const char* id = "rac1.player.update_movement";
    static constexpr const char* name = "Hero_UpdateMovement";
    static constexpr const char* signature = "void()";
    using fn = void (*)();
};

}  // namespace thruster_charge::hooks
