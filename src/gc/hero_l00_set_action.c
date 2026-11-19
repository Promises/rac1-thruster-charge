/* SPDX-License-Identifier: GPL-3.0-only
 *
 * COPIED from Going Commando's decompilation: OpenRAC/rac2-decomp
 * (https://github.com/OpenRAC/rac2-decomp, GNU GPL version 3) at commit
 * 005bfc4dd3b0d715dcbffabdb8835e356e34474a, file src/levels/00_aranos_tutorial/L00_002C42E0-state-g8.c,
 * lines 1-147 (the file's head), 149-150, 167-168 (locals), 202 (the switch),
 * 382-388 (case 0x7B) and 1012-1017 (case 0x77). Code of that project from before its relicensing (d46a3539, 2026-10-11) is also
 * under its MIT notice: THIRD_PARTY_NOTICES.md.
 *
 * hero_l00_set_action, the hero's change of state: what entering the charge (0x7B) and the bonk
 * (0x77) set up.
 * USA v1.01 (rac2-decomp's disc): func_L00_002C42E0. USA v2.00 (the disc this mod's author plays; his own
 * decomp, Promises/RaC-Going-Decompiled, has it as asm only): func_L00_002C4560 (hero_l00_decide_state's jal for the stop and the bonk).
 *
 * The copied lines are unchanged. The lines marked (charge-boots) are this mod's: the function's
 * head cut down to the cases the charge needs. Every name the copied lines use is the translation
 * layer's (gc/shim/common.h, gc/rac1_shim.cpp): Going Commando's hero, states, globals and
 * functions in Ratchet & Clank's terms.
 */
/* func_L00_002C42E0 (00_aranos_tutorial): the hero's state entry, family F3. It changes the action at 0x2294, initialises the
 * movement parameters of the new state and picks its animation: a jump table on the new state id.
 * Ported from func_L19_002C44F8 (src/levels/19_grelbin/L19_002C44F8.c, whose header explains how the parameter
 * blocks were read off retail) and its other ports. Flags add -frac2-zero-ti-por (the 0x28 arm's zeroed vector).
 * Differences from the L19 routine: the L13 set plus 0x71; one function-scope pos[4] shared by 0x27 and 0x71 keeps retail's frame.
 *   - 0x7A calls func_002E5CF8(7, 0, moby) with moby = PTR(0xC18), whose position it sets first.
 *   - The 13-group's inner 0x4D case reads its 5.0f from a local set at the case's top: gcse then propagates the
 *     constant into both calls and retail loads it twice instead of keeping it in $f20.
 *   - The 0x45 arm animates with blend 5 (func_L00_002D2168(7, 5, 5.0f)).
 */
#include "common.h"

typedef int TI128 __attribute__((mode(TI)));
typedef TI128 V4_F6a70b950;
typedef struct Rac2Native_5b4b17178a13f443_SmallSlotReset128Owner Rac2Native_5b4b17178a13f443_SmallSlotReset128Owner;
typedef struct {
    char pad0[35];        /* 0   */
    unsigned char f35;    /* 35  */
    char pad36[8];        /* 36  */
    float f44;            /* 44  */
    unsigned char f48;    /* 48  */
    unsigned char f49;    /* 49  */
    unsigned short f50;   /* 50  */
    unsigned short f52;   /* 52  */
    char pad54[2];        /* 54  */
    long long f56;        /* 56  */
    char pad64[34];       /* 64  */
    unsigned char f98;    /* 98  */
    char pad99[21];       /* 99  */
    int f120;             /* 120 */
    unsigned short f124;  /* 124 */
    char pad126[26];      /* 126 */
    int f152;             /* 152 */
    char pad156[14];      /* 156 */
    short f170;           /* 170 */
} Src_F66766f2b;

typedef struct {
    char pad0[256];       /* 0   */
    int a[4];             /* 256 */
    int b[4];             /* 272 */
    Src_F66766f2b *c[4];            /* 288 */
    float d[4];           /* 304 */
    int pad320;           /* 320 */
    Src_F66766f2b *src;             /* 324 */
    int count;            /* 328 */
} List_F66766f2b;
extern s32 PlayMobySound(s32, s32, u8 *);
extern void mem_clear(unsigned int *, unsigned int, int);
extern u8 D_L00_001A7DE0[8] __attribute__((sda));
extern u8 D_L00_001A7E00[8] __attribute__((sda));
extern s32 D_L00_0025D990[][11];
extern s32 D_L00_001A9010;
extern s32 D_L00_001A8FF4 __attribute__((sda));
extern f32 D_L00_001A7CD8 __attribute__((sda));
extern f32 D_L00_001A7CDC __attribute__((sda));
extern f32 D_L00_0025DB20[][25];
extern s32 D_L00_0025D990[][11];
extern f32 math_fabs(f32);
extern int Spline_AdvanceByDistance(s32, f32 *, s32 *, f32 *, s32, f32);
extern f32 FastVecDot(void *, void *);
extern f32 FastVecLength(void *);
extern void vec_scale_to_length(void *, void *, f32);
extern void vec_clear_128(TI128 *);
extern s32 math_random_int_range(s32, s32);
extern f32 math_random_signed_float_range(f32, f32);
extern void *part_find_owned_effect_slot(void *, u32, s32);
extern void vec_clamp_length(float *, float);
extern s32 math_l00_return_argument(s32);
extern void hero_clear_slot_records(void);
extern u32 hero_get_slot_object(s32);
extern s32 hero_is_in_special_kind_or_mode(void);
extern f32 hero_get_recent_float(int);
extern void cancel_hero_item_animation(void);
extern void hero_reset_speeds_and_vectors(void);
extern s32 moby_is_class_71(u8 *);
extern void hero_scale_saved_vectors(f32);
extern f32 hero_get_direction_angle_delta(void *, int);
extern int hero_play_sound(int, int);
extern int hero_classify_input_direction(void);
extern void hero_select_anim_frame(s32, s32);
extern f32 cam_get_view_space_vector_length(char *);
extern void cam_set_view_space_height(V4_F6a70b950 *, V4_F6a70b950 *, float);
extern int queue_copy_record_by_index(unsigned int);
extern void slot_clear_active_bytes(void);
extern void level_init_pending_slot(void *, char *);
extern void level_append_record_to_table(List_F66766f2b *, int, int, float);
extern void level_release_slot_list(Rac2Native_5b4b17178a13f443_SmallSlotReset128Owner *);
extern int snd_start_in_slot(int, int, int);
extern void hero_start_loader_state(int);
extern s32 enemy_get_kind_index(u8 *);
extern void enemy_reset_timer_by_kind(u8 *);
extern void FastVecSub(void *, void *, void *);
extern void FastVecScale(void *, void *, f32);
extern f32 vec_length_xy(void *);
extern f32 fast_vec_dist(void *, void *);
extern f32 math_fast_sin_or_cos_a(f32);
extern f32 math_fast_sin_or_cos_b(f32);
extern f32 mat_fast_helper_a(f32, f32);
extern f32 FastAddRotations(f32, f32);
extern f32 FastDiffRots(f32, f32);
extern s32 FloatToInt(f32);
extern void anim_start_blended(void *, int, int, int);
/* These calls use side effects only; their original scalar return contracts remain unqualified. */
extern void SpawnExplosionEffects(void *, void *, void *, float, float, int, int, int, float, float, float, float, int, float, int, int, int, int, int);
extern void RippleDisturb(void *, s32, u32, f32, f32, f32, f32);
extern void AdjustPlayerHealth(int);
extern void hero_play_footstep_sound_dust(int, int, int, int);
extern void hero_clear_tracked_slots(void);
extern void part_emit_hero_dust_and_debris(int, int, int);
extern void hero_create_item_slot_objects(int, int);
extern void hero_resolve_stick_direction(int, f32);
extern void hero_l00_select_aim_target(f32, f32, f32);
extern void hero_setup_attached_object_transform(void);
extern int hero_try_start_grab(void);
extern void set_hero_animation(int, int, f32);
extern void UpdateHeroAimTarget(f32, f32, f32);
extern s32 hero_get_weapon_or_form_id(int);
extern u32 D_001A7390;
extern s32 g_is_pal_video;
extern u8 D_001A7A40[8];
extern u8 *D_L00_001B2630;
extern s32 D_L00_001B2638;
extern u8 g_hero_state[];
extern u32 g_current_map_index;
typedef struct { u16 count; u16 pad; u32 maximum; u32 levels; } Statistics;
extern Statistics D_0018C360[];
extern Statistics g_progress_statistics[];
extern s32 D_L00_001A8F00;
extern s32 D_L00_001A7DD8[2] __attribute__((sda));
extern f32 D_L00_0025DB20[][25];
extern u8 D_L00_0025DCB0[];
typedef struct { u8 pad0[0x148]; f32 f148; u8 pad14C[0x400 - 0x14C]; s32 i400; } L19State_1B9900;
extern L19State_1B9900 D_L00_001B8C80;

/* All resident fields below are views of the single player-state object. */
typedef union { u8 b[0x2600]; s16 h[0x1300]; u16 uh[0x1300]; s32 i[0x980]; u32 u[0x980]; f32 f[0x980]; u8 *ptr[0x980]; TI128 q[0x260]; } Hero;
#define HP ((Hero *)D_00189E20)
#define BYTE(o) (HP->b[(o)])
#define HALF(o) (HP->uh[(o)/2])
#define SHORT(o) (HP->h[(o)/2])
#define WORD(o) (HP->i[(o)/4])
#define BITS(o) (HP->u[(o)/4])
#define REAL(o) (HP->f[(o)/4])
#define PTR(o) (HP->ptr[(o)/4])
#define VEC(o) (D_00189E20 + (o))
#define COPY_VECTOR(dst,src) (*(TI128 *)VEC(dst) = *(TI128 *)VEC(src))
/* ==== (charge-boots) The file's head ends here. Below, hero_l00_set_action with only the two
   cases the charge needs; lines not marked are the source's. ==== */
/* Changes the player's action, initializes its movement parameters and selects animation. */
s32 hero_l00_set_action(s32 action, s32 animate) {
    /* (charge-boots) of the source's locals (lines 151-168), the lines declaring the ones the two cases use: */
    u32 animation_16, animation_21, animation_22, animation_41, animation_43, animation_53;
    f32 frames;
    /* (charge-boots) In place of the source's lines 169-201 (its refusals, and its bookkeeping of the
       state it leaves): Ratchet & Clank's own change of state, to its state for this one
       (gc_shim_enter_state); 0 when the game refuses it. */
    if (!gc_shim_enter_state(action, animate)) return 0;
    /* (charge-boots) The source's switch on the new state (line 202), with two of its cases: */
    switch (action) {
    case 0x7B: {
        s32 n = -2;
        WORD(0x229C) = 28; WORD(0x1188) = 0x68; BITS(0x24A0) = 0;
        COPY_VECTOR(0xF0, 0x110);
        if (WORD(0x1298) != 0x36) { WORD(0x2304) = WORD(0x1298); WORD(0x22CC) = 0x36; }
        if (!animate) goto finish;
        set_hero_animation(0x8B, 0, n); goto finish; }
    case 0x77:
        WORD(0x229C) = 10; BITS(0x24A0) = 0;
        REAL(0xBF0) = FastAddRotations(REAL(0x98), 3.14159297943f);
        FastVecScale(VEC(0xBD0), VEC(0), -1.0f); REAL(0x1B4) = 0.15f;
        if (!animate) goto finish;
        frames = 8.0f; animation_43 = 0x29; { set_hero_animation(animation_43, 5, frames); goto finish; }
    /* (charge-boots) the rest of the switch's cases go straight to finish, as its default does */
    default:
        goto finish;
    }
    /* (charge-boots) In place of the source's finish (lines 1198-1211, its bookkeeping after the
       change), which Ratchet & Clank's change of state did: */
finish:
    return 1;
}
