/* SPDX-License-Identifier: GPL-3.0-only
 *
 * COPIED from Going Commando's decompilation: OpenRAC/rac2-decomp
 * (https://github.com/OpenRAC/rac2-decomp, GNU GPL version 3) at commit
 * 005bfc4dd3b0d715dcbffabdb8835e356e34474a, file src/levels/shared/L00_002BA7D8.c,
 * lines 1-184 (the file's head), 258, 392, 401, 428, 463 (locals), 563 (the switch),
 * 1699-1720 (case 0x77) and 1969-2017 (case 0x7B). Code of that project from before its relicensing (d46a3539, 2026-10-11) is also
 * under its MIT notice: THIRD_PARTY_NOTICES.md.
 *
 * hero_l00_update_state_machine, the hero's per-state movement, cut to the charge (state 0x7B)
 * and the bonk off a wall at the end of one (0x77).
 * USA v1.01 (rac2-decomp's disc): func_L00_002BA7D8. USA v2.00 (the disc this mod's author plays; his own
 * decomp, Promises/RaC-Going-Decompiled, has it as asm only): func_L00_002BA8E0, its case 0x7B at 0x002BF18C.
 *
 * The copied lines are unchanged. The lines marked (charge-boots) are this mod's: the function's
 * head cut down to the cases the charge needs. Every name the copied lines use is the translation
 * layer's (gc/shim/common.h, gc/rac1_shim.cpp): Going Commando's hero, states, globals and
 * functions in Ratchet & Clank's terms.
 */
/* func_L00_002BA7D8: the hero's per-frame state handler, the twin of func_L20_002CD928 (see
   src/levels/20_yeedil/L20_002CD928.c for what each flag in this unit's config/cflags.txt line is for).
   One function: one prologue, one return, a switch on the state at 0x2294. */

#include "common.h"
typedef int TI128 __attribute__((mode(TI)));
typedef union { u8 b[0x2600]; s8 c[0x2600]; s16 h[0x1300]; u16 uh[0x1300]; s32 i[0x980]; u32 u[0x980]; f32 f[0x980]; void *ptr[0x980]; s64 d[0x4C0]; TI128 q[0x260]; } Hero;
typedef struct { s32 x0[7]; s32 x1C; u8 x20[0xC0]; } LevelInfo;
typedef struct { s32 w[11]; } Rec2C;
#define M2C_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(expr))
#define M2C_ERROR(d) (0)
extern char g_pad_state[];
extern u8 D_00138320[];
extern f32 D_L20_00298970[];
extern u8 D_00138354[];
extern u8 g_item_slot_to_record[];
extern u8 g_sound_state[];
extern char g_hero_state[];
extern char D_00189F10[];
extern char g_progress_statistics[];
extern u8 D_0018CF10[];
extern u32 D_001A7390 MACRO_ADDR;
extern s32 g_current_map_index MACRO_ADDR;
extern u8 D_001A7B08[0x40] MACRO_ADDR;
extern u8 D_001A7B2A MACRO_ADDR;
extern f32 D_L20_001A7DAC[][4] MACRO_ADDR;
extern s32 D_L20_001A8FF0 MACRO_ADDR;
extern u8 D_L20_001ACEE8[];
extern s32 D_L20_001AFF00 MACRO_ADDR;
extern s32 D_L20_001B2AC8 MACRO_ADDR;
extern s32 D_L20_001B3730 MACRO_ADDR;
extern s32 D_L20_001B3738 MACRO_ADDR;
extern u8 D_L20_001B9E80[];
extern u8 D_L20_001B9FD8[];
extern u8 D_L20_001BA248[];
extern u8 D_L20_001C4480[];
extern u8 D_L20_00267210[];
extern u8 D_L20_00267270[];
extern u8 D_L20_00267274[];
extern Rec2C D_L20_00268D90[];
extern u8 D_L20_00269110[];
extern u8 D_L20_0026D970[];
extern s32 CollLine(s32, s32, s32, s32, s32);
extern s32 coll_process_hit(f32, s32, s32, s32, s32);
extern void FadeToBlack(s32);
extern s32 math_fast_float_d(s32);
extern s32 FastDecTimer(s32);
extern f32 square_root(f32);
extern f32 math_fabs(f32);
extern void vec_clear_128(s32);
extern void FastVecAdd(s32, s32, s32);
extern void FastVecSub(s32, s32, s32);
extern void FastVecScale(s32, s32, f32);
extern f32 FastVecDot(s32, s32);
extern void FastVecCross(s32, s32, s32);
extern f32 FastVecLength(s32);
extern f32 vec_length_xy(s32);
extern f32 fast_vec_dist(s32, s32);
extern f32 vec_distance_xy(s32, s32);
extern void vec_scale_to_length(s32, s32, f32);
extern void vec_fast_normalize_helper(s32, s32, f32);
extern void matrix_mul_vec3(s32, s32, s32);
extern void transform_vector(s32, s32, s32);
extern f32 math_fast_sin_or_cos_a(f32);
extern f32 math_fast_sin_or_cos_b(f32);
extern f32 mat_fast_helper_a(f32, f32);
extern void euler_to_matrix(s32, s32);
extern void matrix_from_basis_rows(s32, s32);
extern f32 FastAddRotations(f32, f32);
extern f32 fast_subtract_rotations(f32, f32);
extern f32 FastDiffRots(f32, f32);
extern s32 FloatToInt(f32);
extern void gauge_popup_show(s32, s32, s32, f32);
extern char *CreateMoby(s32);
extern void moby_transform_joint_to_world(s32, s32, s32);
extern void func_002A0F48(s32);
extern s32 math_random_int_below(s32);
extern s32 math_random_int_range(s32, s32);
extern f32 math_random_float_range(f32, f32);
extern f32 math_random_signed_float_range(f32, f32);
extern s32 math_solve_quadratic(s32, s32, f32, f32, f32);
extern void make_hit_descriptor(s32, s32, s32, f32, s32);
extern int coll_sweep_from_object(s32, s32, s32, f32, f32, f32, s32, s32, s32);
extern void part_queue_impact_effect(s32, s32, s32, s32, s32, f32);
extern f32 ApproachValue(s32, f32, f32);
extern f32 math_spring_float_to_goal(s32, s32, f32, f32, f32, f32);
extern f32 vec_spring_to_target(s32, s32, s32, f32, f32, f32);
extern f32 math_shortest_angle_delta(s32, f32, f32);
extern void StepAngleTowardTarget(s32, s32, f32, f32);
extern f32 math_approach_with_velocity(s32, f32, s32, f32, f32, f32, s32);
extern s32 moby_is_in_special_class_range(s32);
extern void vec_clamp_length(s32, f32);
extern void vec_build_spherical_offset(s32, f32, f32, f32);
extern void part_spawn_white_flash(s32, s32, s32, f32);
extern void *part_spawn_bubble(s32, s32, f32, f32);
extern void snd_stop_voice(s32);
extern s32 PlayMobySound(s32, s32, s32);
extern void RippleDisturb(s32, s32, void *, f32, f32, f32, f32);
extern void pad_clamp_stick_to_unit_range(s32, s32, f32);
extern float AdjustHeroAnimationSpeedForTargetFrame(f32, f32, f32, f32);
extern f32 math_blend_table_entries_by_clock(s32, s32);
extern void hero_start_pose_offset(f32, f32, f32);
extern void part_spawn_jittered_after_delay(s32, s32);
extern void part_emit_hero_side_particles(s32, s32);
extern void part_emit_hero_trail_spark(s32, s32);
extern void EmitHeroRandomizedMotionEffects(s32, s32, s32);
extern void part_emit_hero_dust_and_debris(s32, s32, s32);
extern void EmitHeroTimedEffects(void);
extern void cam_set_limits_first(f32, f32);
extern void cam_set_limits_second(f32, f32);
extern void cam_set_limits_third(f32, f32);
extern void cam_set_limits_fourth(f32, f32);
extern void ProbeHeroAirborneWallContact(void);
extern s32 hero_find_ledge_grab(s32, s32, s32);
extern s32 hero_slot_item_id(s32);
extern void hero_update_turn_from_anim(void);
extern s32 math_is_below_plane_within_depth(f32);
extern void hero_resolve_stick_direction(f32, s32);
extern void hero_update_target_speed(void);
extern void hero_rotate_matrix(f32, f32, f32);
extern void hero_steer_heading(s32, f32, f32, f32);
extern void hero_set_turn_rates(void);
extern void hero_approach_speed(f32, f32);
extern void hero_compute_push_direction(f32);
extern void hero_set_push_direction(s32);
extern void hero_cancel_velocity_into_wall(void);
extern void hero_build_move_impulse(f32, f32);
extern f32 hero_get_speed_along_forward(s32);
extern void ApproachHeroVelocity(f32);
extern void ApplyHeroGravityAndJumpMotion(void);
extern void hero_update_airborne_steering(void);
extern void hero_l00_select_aim_target(f32, f32, f32);
extern void hero_apply_mode_pose_offsets(void);
extern void UpdateHeroMotionResponseParameters(void);
extern void UpdateHeroLeanAndSwayParameters(void);
extern void hero_apply_reference_tilt_rotation(s32, s32, s32, f32, f32, f32);
extern void hero_shrink_vector_by_amount(f32);
extern void hero_steer_reference_point(void);
extern f32 hero_get_direction_angle_delta(s32, s32);
extern s32 hero_play_sound(s32, s32);
extern void snd_update_slot_object(s32, s32, s32);
extern void hero_try_surface_push_response(f32, f32, f32);
extern void part_emit_effects_at_nearby_mobys(s32, s32);
extern void part_emit_hero_radial_particles(void);
extern void hero_setup_attached_object_transform(void);
extern void hero_update_height_offset(void);
extern s32 hero_try_start_grab(void);
extern void set_hero_animation(s32, s32, f32);
extern void hero_apply_mode_transform(s32, s32, f32);
extern void hero_build_relative_point(s32, s32, f32, f32, f32);
extern void hero_apply_mode_transform_reversed(s32, s32, f32);
extern void cam_offset_point_by_side_direction(s32, s32, f32);
extern void cam_offset_point_in_view_space(s32, s32, f32);
extern void cam_project_vector_onto_view_plane(s32, s32);
extern f32 cam_get_view_space_vector_length(s32);
extern f32 cam_get_view_space_height(s32);
extern void cam_set_view_space_height(s32, s32, f32);
extern void cam_set_view_space_vector_length(s32, s32, f32);
extern s32 queue_copy_record_by_index(s32);
extern s32 func_L00_002EE4D8(s32, s32, f32);
extern s32 math_lerp_int(s32, s32, f32);
extern void level_init_pending_slot(s32, s32);
extern void level_append_record_to_table(s32, s32, s32, f32);
extern void moby_update_transform_history_followers(s32, s32);
extern void vec_add_offset(s32, s32, f32, f32, f32);
extern s32 math_update_angle_and_blend(s32, s32, s32, f32);
extern f32 ReadPadFloatHistory(s32);
extern void *part_spawn_two_point_effect(s32, s32, f32);
extern s32 path_advance_index(s32, s32, s32, s32);
extern s32 Spline_AdvanceByDistance(s32, s32, s32, s32, s32, f32);
extern s32 find_nearest_path_point(s32, s32, s32, s32, s32, s32, f32, f32, f32);
extern void UpdateHeroAimTarget(f32, f32, f32);
extern void hero_update_held_grab(void);
extern s32 shooter_update_ranged_attack(s32);
extern s32 SelectHeroStateAnimation(s32);
extern void hero_exit_vehicle_mode(s32);
extern void hero_slide_along_wall(s32);
extern s32 hero_l20_set_state(s32, s32);
extern s32 ref_L00_002BA7D8_00 MACRO_ADDR;
extern void hero_get_offset_vector(f32 *);
/* ==== (charge-boots) The file's head ends here. Below, hero_l00_update_state_machine with only
   the two cases the charge needs; lines not marked are the source's. ==== */
/* Runs the hero's state machine once per frame. It picks the code for the current state (walk, jump,
 * swing, swim, hurt and so on) and moves, animates and steers the hero accordingly. */
void hero_l00_update_state_machine(void) {
    /* (charge-boots) The source's vec_a, vec_b and vec_c are f32[4] on its stack (lines 196-198): the
       cases pass their addresses as 32-bit handles, which the translation layer resolves only in
       its own memory (gc/shim/common.h), so they are its scratch vectors here. */
    f32 *vec_a = gc_shim_scratch(0);
    f32 *vec_b = gc_shim_scratch(1);
    f32 *vec_c = gc_shim_scratch(2);
    f32 scalar27;
    f32 scalar161;
    f32 scalar170;
    s32 value29;
    s32 value64;
    /* (charge-boots) The source's switch on the state (line 563), with two of its cases: */
    switch (((Hero *)g_hero_state)->u[0x2294/4]) {                              /* switch 1 */
    case 0x77:                                      /* switch 1 */
        ApproachValue((s32)(g_hero_state + 0x1B4), 0.0f, 0.006666667f);
        if ((*(s32*)&D_L20_001A8FF0) != 0) {
            vec_scale_to_length((s32)(g_hero_state + 0xF0), (s32)(g_hero_state + 0xBD0), ((Hero *)g_hero_state)->f[0x1B4/4]);
            if (((Hero *)g_hero_state)->f[0x31C/4] > 0.0f) {
                hero_apply_mode_transform((s32)(g_hero_state + 0xF0), (s32)(g_hero_state + 0x130), 0.0069444445f);
            } else {
                cam_set_view_space_height((s32)(g_hero_state + 0xF0), (s32)(g_hero_state + 0xF0), 0.0f);
            }
            FastVecScale((s32)(g_hero_state + 0x190), (s32)(g_hero_state + 0xBD0), -1.0f);
        } else {
            ((Hero *)g_hero_state)->f[0xF0/4] = (f32) (math_fast_sin_or_cos_a(((Hero *)g_hero_state)->f[0xBF0/4]) * ((Hero *)g_hero_state)->f[0x1B4/4]);
            ((Hero *)g_hero_state)->f[0xF4/4] = (f32) (math_fast_sin_or_cos_b(((Hero *)g_hero_state)->f[0xBF0/4]) * ((Hero *)g_hero_state)->f[0x1B4/4]);
            if (((Hero *)g_hero_state)->f[0x31C/4] > 0.0f) {
                ((Hero *)g_hero_state)->f[0xF8/4] = (f32) (((Hero *)g_hero_state)->f[0x128/4] - 0.0069444445f);
            } else {
                ((Hero *)g_hero_state)->f[0xF8/4] = 0.0f;
            }
            ((Hero *)g_hero_state)->f[0x1A0/4] = FastAddRotations(((Hero *)g_hero_state)->f[0xBF0/4], 3.1415927f);
        }
        hero_steer_heading(0, 0.05f, 0.2f, 0.2821616f);
        break;
    case 0x7B:                                      /* switch 1 */
        snd_update_slot_object(4, (s32) ((Hero *)g_hero_state)->ptr[0x2290/4], 0xD);
        hero_resolve_stick_direction(1.0f, 0);
        ((Hero *)g_hero_state)->f[0x1B0/4] = 0.19166668f;
        pad_clamp_stick_to_unit_range((s32)((TI128*)vec_a), (s32) (((void*)g_hero_state) + 0x9F0), 0.17f);
        scalar161 = (((Hero *)g_hero_state)->f[0x24B8/4] * 0.020362178f) * vec_a[0];
        if ((*(s32*)&D_L20_001A8FF0) == 0) {
            if (((Hero *)g_hero_state)->i[0x1B8/4] < 0x3C) {
                scalar161 *= 0.1f;
                ((Hero *)g_hero_state)->f[0x1B0/4] = 0.5f;
            }
        }
        if (((Hero *)g_hero_state)->f[0x24B8/4] > 0.2f) {
            hero_rotate_matrix(0.0f, 0.0f, scalar161);
        }
        hero_approach_speed(0.027777778f, 0.004166667f);
        hero_compute_push_direction(99999.0f);
        if ((*(s32*)&D_L20_001A8FF0) != 0) {
            qcopy(vec_b, &((Hero *)g_hero_state)->q[0x2D0/16]);
            hero_apply_mode_transform_reversed((s32) ((TI128*)vec_b), (s32) ((TI128*)vec_b), 0.17f);
            vec_spring_to_target((s32) (((void*)g_hero_state) + 0x80), (s32) ((TI128*)vec_b), (s32) (((void*)g_hero_state) + 0xA00), 0.04f, 0.3f, 0.125f);
        } else {
            math_spring_float_to_goal((s32) (((void*)g_hero_state) + 0x88), (s32) (((void*)g_hero_state) + 0xA00), ((Hero *)g_hero_state)->f[0x318/4] + 0.17f, 0.04f, 0.3f, 0.125f);
        }
        FastVecScale((s32) ((TI128*)vec_b), (s32)(g_hero_state + 0x110), 3.0f);
        FastVecAdd((s32) ((TI128*)vec_b), (s32) ((TI128*)vec_b), (s32)(g_hero_state + 0xE0));
        value29 = coll_process_hit(0.57f, (s32) ((TI128*)vec_b), 0, (s32) ((Hero *)g_hero_state)->ptr[0x2290/4], 0);
        if (value29 != 0) {
            FastVecScale((s32) ((TI128*)vec_c), (s32)(g_hero_state + 0x110), 7.0f);
            for (value64 = 0; value64 < value29; value64++) {
                if (moby_is_in_special_class_range(((s32 *)D_L20_001C4480)[value64]) != 0) {
                    part_queue_impact_effect(((s32 *)D_L20_001C4480)[value64], (s32) ((Hero *)g_hero_state)->ptr[0x2290/4], 0x10000, (s32) (((void*)g_hero_state) + 0x80), (s32) ((TI128*)vec_c), 1.0f);
                }
            }
        }
        cam_set_limits_first(0.017f, 0.3f);
        cam_set_limits_third(0.025f, 0.3f);
        cam_set_limits_fourth(0.03f, 0.27f);
        scalar170 = scalar161 * 27.0f;
        if (scalar170 > 1.7f) {
            scalar170 = 1.7f;
        } else if (scalar170 < -1.7f) {
            scalar170 = -1.7f;
        }
        scalar27 = scalar170 * 0.5f;
        ((Hero *)D_L20_00267210)->f[0x60/4] = (f32) -scalar170;
        ((Hero *)D_L20_00267210)->f[0x278/4] = scalar27;
        ((Hero *)D_L20_00267210)->f[0x1C8/4] = scalar27;
        break;
    /* (charge-boots) the switch's and the function's ends */
    }
}
