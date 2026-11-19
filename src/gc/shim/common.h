/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The translation layer's C side: what the copied Going Commando files (src/gc/*.c) include as
 * "common.h" (and "names.h") in place of rac2-decomp's. It gives them rac2-decomp's types and
 * markers, and renames every global and function they use to the translation layer's
 * (gc/rac1_shim.cpp), which puts Going Commando's hero, states and functions in Ratchet & Clank's
 * terms. Nothing of rac2-decomp's own headers is copied.
 *
 * Two conventions of the copied code need the host's help:
 *  - The hero is Going Commando's, at its offsets: gc_shim_hero is a copy of it the translation
 *    layer fills from Ratchet & Clank's hero before the copied code runs and writes back after
 *    (rac1_shim.hpp: kHeroFields says which field is which).
 *  - hero_l00_update_state_machine passes addresses as s32 (a PS2 address is 32 bits). On a 64-bit
 *    host that keeps only an address's low half: the translation layer finds the address again
 *    among its own buffers (gc_shim_hero, gc_shim_lean, the scratch vectors), so that file's calls
 *    go to variants that take such handles (GC_SHIM_HANDLES, set for it by CMakeLists.txt). The
 *    other files pass real pointers.
 */
#ifndef CHARGE_BOOTS_GC_SHIM_COMMON_H
#define CHARGE_BOOTS_GC_SHIM_COMMON_H

/* rac2-decomp's types (include/common.h). */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef float f32;
typedef double f64;

/* Placement markers for the PS2's small-data area: nothing on the host. */
#define MACRO_ADDR
#define NOT_SDA
#define sda unused

/* A quadword copy (the EE's lq/sq pair in rac2-decomp). */
static __inline__ void qcopy(void *dst, const void *src) { __builtin_memcpy(dst, src, 16); }

/* --- Globals: Going Commando's, as the translation layer keeps them --- */

/* The hero (D_00189E20, g_hero_state): the translation layer's copy, at Going Commando's offsets.
   ProcessHeroEquippedItemInput.c has a view of its own (GC_SHIM_UNIT_ITEM_INPUT: see there). */
#if defined(GC_SHIM_UNIT_ITEM_INPUT)
#define g_hero_state gc_shim_item_input_hero
extern unsigned char gc_shim_hero[];
#else
#define g_hero_state gc_shim_hero
#endif
#define D_00189E20 gc_shim_hero
/* The pad's state (g_pad_state): held buttons at +0x1A0, pressed at +0x1A4, as Ratchet & Clank's. */
#define g_pad_state gc_shim_pad
/* The hero's body-tilt records (D_L20_00267210 in level 20's build): Ratchet & Clank's own table
   (hero_eases, PAL D_L00_0017A780), copied in and out. */
#define D_L20_00267210 gc_shim_lean
/* Nonzero when gravity pulls along the ground's normal (magnetic ground): Ratchet & Clank's
   gravity mode (+0x20B3). */
#define D_L20_001A8FF0 gc_shim_gravity_flag
/* What coll_process_hit found: moby slots. */
#define D_L20_001C4480 gc_shim_hits
/* One owned byte per item; 0x26 is the Charge Boots'. */
#define D_001A7B08 gc_shim_items_owned
/* What the hero wears and holds; slot 3 holds 54 when it is the Charge Boots. */
#define D_001A7390 gc_shim_item_slots
/* The double tap's window, in Going Commando's frames. */
#define D_L00_001A7CC4 gc_shim_double_tap_timer
/* The flames' two particle settings. */
#define D_L00_001A7CA0 gc_shim_flame_a
#define D_L00_001A7CB0 gc_shim_flame_b

/* --- Functions: the translation layer's (gc/rac1_shim.cpp says what each one does here) --- */

#if defined(GC_SHIM_HANDLES)
#define GC_SHIM_ADDRESSED(name) gc_shim_h_##name
#else
#define GC_SHIM_ADDRESSED(name) gc_shim_##name
#endif

/* Functions that take addresses: a variant per convention. */
#define ApproachValue GC_SHIM_ADDRESSED(ApproachValue)
#define FastVecAdd GC_SHIM_ADDRESSED(FastVecAdd)
#define FastVecLength GC_SHIM_ADDRESSED(FastVecLength)
#define FastVecScale GC_SHIM_ADDRESSED(FastVecScale)
#define cam_set_view_space_height GC_SHIM_ADDRESSED(cam_set_view_space_height)
#define coll_process_hit GC_SHIM_ADDRESSED(coll_process_hit)
#define hero_apply_mode_transform GC_SHIM_ADDRESSED(hero_apply_mode_transform)
#define hero_apply_mode_transform_reversed GC_SHIM_ADDRESSED(hero_apply_mode_transform_reversed)
#define hero_test_reach_blend GC_SHIM_ADDRESSED(hero_test_reach_blend)
#define math_fast_float_d GC_SHIM_ADDRESSED(math_fast_float_d)
#define math_spring_float_to_goal GC_SHIM_ADDRESSED(math_spring_float_to_goal)
#define moby_transform_joint_to_world GC_SHIM_ADDRESSED(moby_transform_joint_to_world)
#define pad_clamp_stick_to_unit_range GC_SHIM_ADDRESSED(pad_clamp_stick_to_unit_range)
#define part_queue_impact_effect GC_SHIM_ADDRESSED(part_queue_impact_effect)
#define part_spawn_object_relative_particle GC_SHIM_ADDRESSED(part_spawn_object_relative_particle)
#define vec_scale_to_length GC_SHIM_ADDRESSED(vec_scale_to_length)
#define vec_spring_to_target GC_SHIM_ADDRESSED(vec_spring_to_target)

/* Functions of plain values: one each. */
#define FastAddRotations gc_shim_FastAddRotations
#define cam_set_limits_first gc_shim_cam_set_limits_first
#define cam_set_limits_third gc_shim_cam_set_limits_third
#define cam_set_limits_fourth gc_shim_cam_set_limits_fourth
#define hero_approach_speed gc_shim_hero_approach_speed
#define hero_compute_push_direction gc_shim_hero_compute_push_direction
#define hero_play_sound gc_shim_hero_play_sound
#define hero_resolve_stick_direction gc_shim_hero_resolve_stick_direction
#define hero_rotate_matrix gc_shim_hero_rotate_matrix
#define hero_steer_heading gc_shim_hero_steer_heading
#define math_fast_sin_or_cos_a gc_shim_math_fast_sin_or_cos_a
#define math_fast_sin_or_cos_b gc_shim_math_fast_sin_or_cos_b
#define moby_is_in_special_class_range gc_shim_moby_is_in_special_class_range
#define set_hero_animation gc_shim_set_hero_animation
#define snd_update_slot_object gc_shim_snd_update_slot_object

/* --- The translation layer's own, for the lines the mod adds to the copied files --- */

/* Ratchet & Clank's change of state for Going Commando's `action` (hero_l00_set_action): 1 when
   the game made it. */
s32 gc_shim_enter_state(s32 action, s32 animate);
/* Scratch vector `index` (0-2), 16 bytes, aligned: hero_l00_update_state_machine's vec_a..vec_c. */
f32 *gc_shim_scratch(s32 index);
/* Whether the hero may use his items' input now (ProcessHeroEquippedItemInput's early returns). */
s32 gc_shim_item_input_allowed(void);

#endif
