/* SPDX-License-Identifier: GPL-3.0-only
 *
 * COPIED from Going Commando's decompilation: OpenRAC/rac2-decomp
 * (https://github.com/OpenRAC/rac2-decomp, GNU GPL version 3) at commit
 * 005bfc4dd3b0d715dcbffabdb8835e356e34474a, file src/levels/shared/L00_002CB3A8-decider.c,
 * lines 1-143 (the file's head, the function's head and its vectors), 214 (the switch)
 * and 892-937 (case 0x7B). Code of that project from before its relicensing (d46a3539, 2026-10-11) is also
 * under its MIT notice: THIRD_PARTY_NOTICES.md.
 *
 * hero_l00_decide_state, the hero's state decider, cut to the charge (0x7B): R1 let go stops it,
 * a wall bonks it, else the flames at his feet.
 * USA v1.01 (rac2-decomp's disc): func_L00_002CB3A8. USA v2.00 (the disc this mod's author plays; his own
 * decomp, Promises/RaC-Going-Decompiled, has it as asm only): func_L00_002CB650, its case 0x7B at 0x002CD5C4.
 *
 * The copied lines are unchanged. The lines marked (charge-boots) are this mod's: the function's
 * head cut down to the cases the charge needs. Every name the copied lines use is the translation
 * layer's (gc/shim/common.h, gc/rac1_shim.cpp): Going Commando's hero, states, globals and
 * functions in Ratchet & Clank's terms.
 */
#include "names.h"
/* func_L00_002CB3A8 (26432 bytes, 11 places: levels 0, 2, 5, 6, 10, 14, 15, 24, 25, 26, 30), the hero
 * state decider of the shared level code.  Its own file for the decider profile in
 * config/cflags.txt (as func_L19_002CAAB0).
 *
 * The probe loop of states 0x28/0x29: `pos` is computed at the top of the loop body, so
 * loop.c hoists it after gcse's PRE insertions of the &sp48..&sp5C call arguments; that gives
 * retail's preheader order `addiu $fp,$sp,0x5C; sw $v0,0x94($sp); addiu $s5,$s1,0x80`.  A `pos`
 * assigned before the loop is a user insn ahead of those insertions and is scheduled first.
 * The success block's quadword copy 0x5D0 -> 0x610 is written through pointer arithmetic:
 * its addresses then carry constant REG_EQUAL notes and cse bases them on the loop's hero
 * register, as retail does; the FLD struct form folds them onto `pos` (pos + 0x590/0x550).
 * The `0x1A0 & 4` tests call an 8-byte stub that returns 0: in levels 5, 10, 15 and 25 retail
 * calls another catalogue row's copy than in the other seven, so the call is ref_L00_002CB3A8_00,
 * whose value the build reads from the retail bytes.
 * nonmatching/levels/func_L00_002CB3A8.c's history (git log) has the earlier rounds' notes. */
#include "common.h"
typedef int TI128 __attribute__((mode(TI)));
typedef TI128 s128;
#define NULL ((void *)0)
#define FLD(base, T, off) (((struct { u8 pad[off]; T v; } *)(base))->v)
#define M2C_ERROR(d) (0)
#define M2C_UNK s32
extern char g_pad_state[];
extern char g_item_slot_to_record[];
extern char g_sound_state[];
extern char g_hero_state[];
extern char D_00189F10[];
extern u8 D_001A7BB2[] MACRO_ADDR;
extern u8 g_item_owned_flags[] MACRO_ADDR;
extern s32 g_current_map_index;
extern f32 D_L00_001A7DAC[][4] MACRO_ADDR;
extern s32 D_L00_001A7DDC;
extern s32 D_L00_001A8FF4;
extern f32 D_L00_001B1AE8;
extern s32 D_L00_001A8FB4;
extern s32 D_L00_001B19C4;
extern s32 D_L00_001A8A48;
extern u8 D_L00_001A7CA0[16] __attribute__((sda));
extern u8 D_L00_001A7CB0[16] __attribute__((sda));
extern s32 D_L00_001A7DD8[2] __attribute__((sda));
extern char D_L00_0025BC00[];
extern char D_L00_0025BE10[];
typedef struct { s32 w[11]; } Rec2C;
extern Rec2C D_L00_0025D990[];
extern char g_weapon_l00_table[];
extern char D_L00_001B8C80[];
s32 CollLine(void *, void *, s32, void *, s32);
s32 coll_test_point(void *, s32, void *, f32);
s32 math_fast_float_d(void *);
s32 FastDecTimer(s16 *);
f32 math_fabs(f32);
void FastVecAdd(void *, void *, void *);
void FastVecScale(void *, void *, f32);
f32 FastVecLength(void *);
f32 vec_length_xy(void *);
f32 fast_vec_dist(void *, void *);
f32 vec_distance_xy(void *, void *);
void vec_scale_to_length(void *, void *, f32);
f32 math_fast_sin_or_cos_a(f32);
f32 math_fast_sin_or_cos_b(f32);
f32 FastAddRotations(f32 a0, f32 a1);
f32 FastDiffRots(f32 a0, f32 a1);
s32 FloatToInt(f32);
int hud_set_slot_contents(s32, s32, void *, void *, void *, s32, s32);
void menu_init_panel_layout(u8 *object);
void weapon_wheel_update_select(s8 *);
s32 weapon_wheel_draw(s8 *);
void weapon_wheel_init(void *a0);
void UpdateRadialMenuSelection(u8 *object);
int DrawRadialSelectorPanel();
void moby_transform_joint_to_world(s8 *a0, s32 a1, s8 *a2);
void anim_start_blended(u8 *object, s32 animation, s32 frame, s32 rate);
s32 math_random_int_range(s32 a0, s32 a1);
f32 math_random_float_range(f32 a0, f32 a1);
f32 ApproachValue(f32 *value, f32 target, f32 step);
void vec_clamp_length(f32 *, f32);
void snd_stop_voice(s32 a0);
s32 hero_get_weapon_or_form_id(s32 arg);
u32 math_l00_return_argument(u32 param_1);
void hero_play_footstep_effect(void);
void hero_play_footstep_sound_dust(s32 kind, s32 side, s32 play_sound, s32 duration);
void hero_clear_tracked_slots(void);
float AdjustHeroAnimationSpeedForTargetFrame(f32 arg0, f32 arg1, f32 arg2, f32 arg3);
void part_emit_hero_dust_and_debris(s32 dust, s32 debris, s32 model);
s32 hero_test_reach_blend(f32 *out, f32 scale, f32 amount);
s32 hero_find_ledge_grab(s8 *, s8 *, s8 *);
s32 hero_slot_item_id(s32 index);
void cancel_hero_item_animation(void);
void hero_record_play_counters(void);
s32 moby_is_class_71(u8 *object);
s32 hero_is_turned_past_angle(f32 x);
s32 math_is_below_plane_within_depth(f32 value);
void hero_resolve_stick_direction(s32, f32);
void hero_update_target_speed(void);
s32 hero_play_sound(s32 index, s32 flags);
s32 hero_get_step_offset_from_direction(void);
s32 hero_l00_set_action(s32, s32);
void hero_start_loader_state(s32 a0);
s32 hero_try_start_transition(void);
s32 hero_try_start_ledge_move(void);
s32 hero_check_fall_transition(void);
s32 ProcessHeroEquippedItemInput(void);
s32 hero_try_start_move_from_buffer(s32 arg0, s32 arg1);
void hero_update_anim_scale(void);
void hero_clear_target_flags(void);
s32 hero_choose_idle_anim(s32 *selected);
void hero_choose_next_move_after_landing(void);
s32 hero_choose_jump_or_roll_move(void);
s32 hero_check_step_ahead(void);
s32 hero_try_start_grab(void);
void UpdateHeroHeldTarget(void);
void hero_update_button_hold_timers(void);
void hero_apply_pending_change(void);
void hero_set_linked_objects_anim(void);
void set_hero_animation(s32 a0, s32 a1, f32 f12);
void hero_clear_queued_anim(void);
void hero_set_anim_mode(s32 value);
f32 hero_find_collision_time(f32 time, f32 acceleration, f32 height);
void hero_set_level_clock_and_bytes(s16, u8, u8, u8, f32, f32);
void hero_build_relative_point(f32 *p, s32 flag, f32 x, f32 y, f32 z);
f32 cam_get_view_space_height(s8 *in);
void cam_set_view_space_height(void *dst, void *src, f32 k);
f32 EvaluateQuantizedHeight(f32 *a0, f32 *a1, s32 *out, f32 scale);
void func_L00_0030C710(s32 argument);
void module_l00_call_handler_with_arg_4(s32 argument);
f32 math_compute_falling_step_offset(f32 *a0, f32 *a1, s32 *a2, f32 factor);
f32 weapon_compute_shot_launch_speed(f32 *a, f32 *b, f32 *out, f32 d, f32 e);
void moby_set_flag_bit(u8 *entity, s32 enabled);
s32 PadPressedWithin(u32 mask, s32 arg1, s32 *out);
void *part_spawn_object_relative_particle(s8 *object,void *position,f32 *velocity,s32 *config,s32 flag);
s32 Spline_AdvanceByDistance(void *path, void *out, s32 *index, f32 *offset, s32 wrap, f32 step);
s32 find_nearest_path_point(void *, void *, void *, s32 *, f32 *, s32, f32, f32, f32);
void part_l00_spawn_spark_pairs(void *position,s32 count);
s32 ref_L00_002CB3A8_00(void);
s32 hero_try_grab_object(s32);
s32 nearest_rail_point(s8 *, void **, s128 *, void *, void *, s32 *, void *, s32, s32);

/* ==== (charge-boots) The file's head ends here. Below, hero_l00_decide_state with only the case
   the charge needs; lines not marked are the source's. ==== */
/* The main per-frame decision routine for Ratchet: it reads the pad and the current state, then picks the next move (jump, swing, glide, grind, attack and more) and starts the matching animation. */
void hero_l00_decide_state(void) {
    f32 vec_a[4] __attribute__((aligned(16)));
    f32 vec_b[4] __attribute__((aligned(16)));
    f32 vec_c[4] __attribute__((aligned(16)));
    /* (charge-boots) The source's switch on the state (line 214), with one of its cases: */
    switch (FLD(g_hero_state, u32, 0x2294)) {
    case 0x7B:
    {
        s32 one = 1;
        s32 flag = 0;
        if (FLD(g_hero_state, s16, 0x348) != 0) {
            flag = 1;
        }
        if (!(FLD(g_pad_state, s32, 0x1A0) & 8)) {
            if (FLD(g_hero_state, s32, 0x1B8) >= 0x3D) {
                flag = one;
            }
        }
        if (FLD(g_hero_state, u8, 0x22B8) != 0) {
            flag = one;
        }
        if (flag != 0) {
            if (FLD(g_hero_state, s32, 0x2304) != 0) {
                FLD(g_hero_state, s32, 0x2320) = one;
            }
            hero_play_sound(9, 0);
            FLD(g_hero_state, s16, 0x1E0) = 0x1E;
            hero_l00_set_action(3, 0);
            set_hero_animation(6, 4, 11.0f);
        } else {
            if (FLD(g_hero_state, u8, 0x287) != 0) {
                f32 a = FastVecLength(g_hero_state + 0x110);
                if (a < FastVecLength(g_hero_state + 0xF0) * 0.75f) {
                    hero_l00_set_action(0x77, 1);
                    break;
                }
                if (hero_test_reach_blend(NULL, 1.0f, 1.25f) != 0) {
                    hero_l00_set_action(0x77, 1);
                    break;
                }
            }
            *(s128 *)vec_c = 0;
            vec_c[0] = -0.083333336f;
            moby_transform_joint_to_world(FLD(g_hero_state, s8 *, 0x2290), 0x16, (s8 *)vec_a);
            moby_transform_joint_to_world(FLD(g_hero_state, s8 *, 0x2290), 0x17, (s8 *)vec_b);
            part_spawn_object_relative_particle(FLD(g_hero_state, s8 *, 0x2290), vec_a, vec_c, (s32 *)D_L00_001A7CA0, 1);
            part_spawn_object_relative_particle(FLD(g_hero_state, s8 *, 0x2290), vec_a, vec_c, (s32 *)D_L00_001A7CB0, 1);
            part_spawn_object_relative_particle(FLD(g_hero_state, s8 *, 0x2290), vec_b, vec_c, (s32 *)D_L00_001A7CA0, 1);
            part_spawn_object_relative_particle(FLD(g_hero_state, s8 *, 0x2290), vec_b, vec_c, (s32 *)D_L00_001A7CB0, 1);
        }
        break;
    }
    /* (charge-boots) the switch's and the function's ends */
    }
}
