// SPDX-License-Identifier: GPL-3.0-or-later
//
// TODO(branch hooks): delete this file once the SDK this mod builds against (sdk.lock) has these
// hook points, and use their types from openrac/games/rac1.hpp instead (mod.cpp names each once:
// UpdateMovement, DrawWorldQuad, DrawThrusterFlame, SpawnBurst). Also add their ids to
// openrac-mod.json's "hooks" then: package_mod.py refuses an id its SDK's table lacks.
//
// The hook points, declared in the same shape as the SDK's generated ones (OpenRAC PR #617, branch
// rac1-steer-hook, commits 58eb1fb1b and eb8da1602, sdk/bindings/rac1.json): the port finds each
// by its id; one without it refuses the mod at load with the reason.
#pragma once

#include <cstdint>

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

// Queues one effect quad (draw_world_quad, PAL func_L00_001FD1D8): the 0x90-byte record at quad
// holds four corners (+0x00), four RGBA words (+0x40, alpha 0x80 opaque), four ST pairs (+0x50) and
// the GS words (+0x70..+0x88); matrix, when not 0, places the corners. Most effects' draws call
// it, many times a frame. The record is the caller's, which may draw it again.
struct FX_DrawWorldQuad {
    static constexpr const char* id = "rac1.fx.world_quad.draw";
    static constexpr const char* name = "FX_DrawWorldQuad";
    static constexpr const char* signature = "void(u32,u32,i32)";
    using fn = void (*)(std::uint32_t, std::uint32_t, std::int32_t);
};

// Draws one of the Thruster-Pack's two flames (draw_joint_aligned_beam_effect, the draw callback of
// a class 0xA7 moby): its quads through FX_DrawWorldQuad, then its ribbon.
struct FX_ThrusterFlame_Draw {
    static constexpr const char* id = "rac1.fx.thruster_flame.draw";
    static constexpr const char* name = "FX_ThrusterFlame_Draw";
    static constexpr const char* signature = "void(u32)";
    using fn = void (*)(std::uint32_t);
};

// Spawns a spark (spawn_burst) at pos moving by dir for ticks ticks, its RGBA colour fading from
// colour to fade as it shrinks from size; children: it throws off sparks of its current colour.
struct FX_SpawnBurst {
    static constexpr const char* id = "rac1.fx.burst.spawn";
    static constexpr const char* name = "FX_SpawnBurst";
    static constexpr const char* signature = "u32(u32,u32,i32,i32,i32,i32,f32)";
    using fn = std::uint32_t (*)(std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, std::int32_t,
                                 std::int32_t, float);
};

}  // namespace thruster_charge::hooks
