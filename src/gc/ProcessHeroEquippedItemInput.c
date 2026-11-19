/* SPDX-License-Identifier: GPL-3.0-only
 *
 * COPIED from Going Commando's decompilation: OpenRAC/rac2-decomp
 * (https://github.com/OpenRAC/rac2-decomp, GNU GPL version 3) at commit
 * 005bfc4dd3b0d715dcbffabdb8835e356e34474a, file src/levels/shared/L00_002C9448-g8.c,
 * lines 1-121 (the file's head), 123-126, 131, 146 and 368-380 (the charge's double tap). Code of that project from before its relicensing (d46a3539, 2026-10-11) is also
 * under its MIT notice: THIRD_PARTY_NOTICES.md.
 *
 * ProcessHeroEquippedItemInput, the hero's item input, cut to its last block: the Charge Boots'
 * double tap of R1 that starts the charge.
 * USA v1.01 (rac2-decomp's disc): func_L00_002C9448. USA v2.00 (the disc this mod's author plays; his own
 * decomp, Promises/RaC-Going-Decompiled, has it as asm only): func_L00_002C96D0, its call to hero_l00_set_action(0x7B, 1) at 0x002CA220.
 *
 * The copied lines are unchanged. The lines marked (thruster-charge) are this mod's: the function's
 * head cut down to the cases the charge needs. Every name the copied lines use is the translation
 * layer's (gc/shim/common.h, gc/rac1_shim.cpp): Going Commando's hero, states, globals and
 * functions in Ratchet & Clank's terms.
 */
#include "common.h"

typedef int Quadword __attribute__((mode(TI)));
/* Fields used here are a prefix of the resident hero state, not its total size. */
typedef struct {
    unsigned char pad_000[128];
    float position[4];
    unsigned char pad_090[8];
    float heading;
    unsigned char pad_09C[284];
    int frame;
    unsigned char pad_1BC[74];
    short control_lock;
    unsigned char pad_208[8];
    u64 motion_flags;
    unsigned char pad_218[260];
    float movement;
    unsigned char pad_320[32];
    int action_ready;
    unsigned char pad_344[4];
    short special_ready;
    unsigned char pad_34A[4];
    short extended_range;
    unsigned char pad_350[332];
    unsigned char grounded;
    unsigned char pad_49D[1];
    unsigned char fast_attack;
    unsigned char pad_49F[53];
    int attack_mode;
    unsigned char pad_4D8[84];
    unsigned char alternate_ready;
    unsigned char pad_52D[103];
    int attack_pending;
    unsigned char pad_598[40];
    char *aim_target;
    float target_angle;
    unsigned char pad_5C8[292];
    int attack_blocked;
    unsigned char pad_6F0[952];
    int nearby_ready;
    unsigned char pad_AAC[24];
    float nearby_distance;
    unsigned char pad_AC8[16];
    float alternate_distance;
    unsigned char pad_ADC[2];
    short alternate_target_ready;
    unsigned char pad_AE0[52];
    int second_target_ready;
    unsigned char pad_B18[228];
    short animation_index;
    unsigned char pad_BFE[46];
    int attack_delay;
    unsigned char pad_C30[8];
    float animation_frame;
    unsigned char pad_C3C[1508];
    char *object;
    unsigned char pad_1224[12];
    u32 buttons;
    unsigned char pad_1234[6];
    unsigned char input_locked;
    unsigned char pad_123B[5];
    int input_window;
    int control_mode;
    int action;
    unsigned char pad_124C[590];
    unsigned char action_locked;
    unsigned char pad_149B[3573];
    char *owner;
    int mode;
    unsigned char pad_2298[4];
    u32 state;
    unsigned char pad_22A0[20];
    unsigned char disabled;
    unsigned char pad_22B5[3];
    unsigned char changing_weapon;
    unsigned char pad_22B9[3];
    unsigned char special_control;
    unsigned char pad_22BD[39];
    int pending_action;
    unsigned char pad_22E8[464];
    float transition_progress;
} HeroActionState;
extern HeroActionState g_hero_state;
typedef union {
    struct { u32 held, pressed; } word;
    u64 both;
} ButtonPair;
typedef struct {
    unsigned char prefix[0x1A0];
    ButtonPair buttons;
} InputState;
extern InputState g_pad_state;
extern unsigned char g_item_slot_to_record[];
extern u8 D_001A7B08[0x40] __attribute__((sda));
extern unsigned int D_001A7390[] __attribute__((sda));
extern int D_L00_001A7CC4 __attribute__((sda));
typedef struct { char pad[12]; int first_frame, trigger_frame; char rest[24]; } AnimationRange;
extern AnimationRange D_L00_0025D990[];
typedef struct { char pad[20]; int sound; char rest[200]; } EffectDefinition;
extern EffectDefinition g_weapon_l00_table[];

/* The retail helper consumes a button mask, a history window and optional age output. */
extern int PadPressedWithin(u32, int, int *);
extern int moby_is_class_71(unsigned char *);
/* Matched helper definition takes no parameters. */
extern void hero_setup_attached_object_transform(void);
extern int FloatToInt(float);
extern s32 hero_l00_set_action(int, int);
extern int get_ammo(int);
extern int snd_play_from_class_bank(int, int, char *, int);
extern char *SelectBestEligibleTarget(void *, int, int, float, float, float, float);
extern int CollLine(void *, void *, int, void *, void *);
extern float mat_fast_helper_a(float, float);
extern float fast_subtract_rotations(float, float);
extern void hero_find_swingshot_node_in_front(void);
extern int hero_find_active_swingshot_node(void);
extern int hero_find_blue_swingshot_target(void);
extern void start_equipped_item_animation(void);
extern int PlayMobySound(int, int, char *);
extern int math_fast_float_d(int *);

/* ==== (thruster-charge) The file's head ends here. ==== */

/* (thruster-charge) g_hero_state is this file's own view of the hero: the struct above has pointers,
   which make it longer on a 64-bit host than on the PS2, so it cannot lie over the translation
   layer's hero at Going Commando's offsets as the other files' views do. It is filled with the
   fields the block reads, from those offsets, as it starts. */
HeroActionState g_hero_state;

static void gc_shim_view_hero(void)
{
    __builtin_memcpy(&g_hero_state.frame, gc_shim_hero + 0x1B8, sizeof g_hero_state.frame);
    __builtin_memcpy(&g_hero_state.motion_flags, gc_shim_hero + 0x210, sizeof g_hero_state.motion_flags);
    __builtin_memcpy(&g_hero_state.action_ready, gc_shim_hero + 0x340, sizeof g_hero_state.action_ready);
    __builtin_memcpy(&g_hero_state.mode, gc_shim_hero + 0x2294, sizeof g_hero_state.mode);
    __builtin_memcpy(&g_hero_state.state, gc_shim_hero + 0x229C, sizeof g_hero_state.state);
}

/* ==== Below, ProcessHeroEquippedItemInput with only its last block; lines not marked are the
   source's. ==== */
/* Selects context-sensitive hero actions from buffered input and nearby targets.
 * RAC1's process_hero_equipped_item_input is the counterpart. */
int ProcessHeroEquippedItemInput(void)
{
    int previous_frame, state;
    /* (thruster-charge) In place of the source's lines 132-145 (the function's early returns: the hero
       disabled, holding nothing, his input locked, his hands busy) and 147-367 (the other
       items' input): the view filled, and the translation layer's stand-in for those returns. */
    gc_shim_view_hero();
    if (!gc_shim_item_input_allowed()) return 0;
    previous_frame = g_hero_state.frame;
    if (g_hero_state.frame == previous_frame && D_001A7B08[0x26] && D_001A7390[3] == 54 &&
        g_hero_state.mode != 29 && g_hero_state.mode != 1 &&
        (g_hero_state.motion_flags & 0xffffffff00000000ULL) == 0) {
        math_fast_float_d(&D_L00_001A7CC4);
        if (D_L00_001A7CC4 &&
            (g_hero_state.state < 2 || g_hero_state.state == 12 ||
             ((g_hero_state.state == 2 || g_hero_state.state == 4) && g_hero_state.action_ready)) &&
            (g_pad_state.buttons.both & 0x800000008ULL) == 0x800000008ULL)
            hero_l00_set_action(123, 1);
        if (g_pad_state.buttons.word.pressed & 8) D_L00_001A7CC4 = 30;
    }
    return g_hero_state.frame < previous_frame;
}
