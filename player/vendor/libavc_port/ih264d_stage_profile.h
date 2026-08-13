#ifndef MR_IH264D_STAGE_PROFILE_H
#define MR_IH264D_STAGE_PROFILE_H

/*
 * Diagnostic-only timing breakdown of libavc decode, added to find out
 * where a real-hardware Pistorm frame's ~150-300ms libavc-core cost
 * actually goes: motion compensation, deblocking, and IDCT/reconstruction
 * each sit behind their own function-pointer table in dec_struct_t, so each
 * can be wrapped with a clock()-based timer without touching a single line
 * of the vendored libavc source - the same pattern this port already uses
 * to swap in m68k asm (ih264d_function_selector_port.c), just measuring
 * instead of replacing.
 *
 * There is no function pointer for bitstream/CABAC/CAVLC parsing, MV
 * prediction, or intra prediction, so those are not broken out here; treat
 * core_us minus (mc_us+deblock_us+recon_us) as that combined remainder.
 */

typedef struct mr_h264_stage_us {
    unsigned long mc_us;
    unsigned long deblock_us;
    unsigned long recon_us;
} mr_h264_stage_us;

/* Wrap this codec instance's apf_inter_pred_luma[]/deblock/recon function
 * pointers with timing instrumentation. Call once, after
 * ih264d_init_function_ptr_generic() and any MR_M68K_ASM overrides have set
 * up the real implementations to measure - this captures whatever is
 * sitting in each slot at that point (generic C or our asm) and installs a
 * thin pass-through wrapper in its place.
 *
 * Takes a bare dec_struct_t* as void* so this header - included from
 * mr_h264.c, which is portable core code - never has to pull in libavc's
 * ih264d_structs.h just to spell the parameter type. */
void mr_h264_stage_profile_install(void *codec);

/* Zero the accumulators before a libavc decode sub-call. */
void mr_h264_stage_profile_reset(void);

/* Read the accumulated per-stage microseconds since the last reset. */
void mr_h264_stage_profile_get(mr_h264_stage_us *out);

#endif
