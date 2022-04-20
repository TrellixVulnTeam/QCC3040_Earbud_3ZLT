/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \ingroup capabilities
 * \file  aanc_proc.h
 * \ingroup AANC
 *
 * AANC processing library header file.
 *
 */

#ifndef _AANC_PROC_H_
#define _AANC_PROC_H_

/* Imports Kalimba type definitions */
#include "types.h"

/* Imports macros for working with fractional numbers */
#include "platform/pl_fractional.h"

/* Imports logging functions */
#include "audio_log/audio_log.h"

/* Imports memory management functions */
#include "pmalloc/pl_malloc.h"
/* Imports cbuffer management functions */
#include "buffer/cbuffer_c.h"
/* Imports memory functions */
#include "mem_utils/scratch_memory.h"
#include "mem_utils/memory_table.h"

/* Imports ADDR_PER_WORD definition */
#include "portability_macros.h"

/* Imports AANC private library functions */
#include "aanc_defs.h"
/* Imports AANC parameter/statistic definitions. */
#include "aanc_gen_c.h"

/******************************************************************************
Public Constant Definitions
*/
#define AANC_PROC_MEM_TABLE_SIZE            9 /* Memory table size (entries) */

/* Number of taps in the FxLMS filters */
#define AANC_PROC_NUM_TAPS_BP               5

/* Number of model coefficients depends on platform */
#ifdef AANC_MAOR_V20
#define AANC_PROC_NUM_TAPS_PLANT            9
#define AANC_PROC_NUM_TAPS_CONTROL          9
#else
#define AANC_PROC_NUM_TAPS_PLANT            8
#define AANC_PROC_NUM_TAPS_CONTROL          8
#endif

#define AANC_PROC_FXLMS_DM_BYTES            FXLMS100_DM_BYTES(\
                                                AANC_PROC_NUM_TAPS_PLANT, \
                                                AANC_PROC_NUM_TAPS_CONTROL, \
                                                AANC_PROC_NUM_TAPS_BP)

#define AANC_PROC_CLIPPING_THRESHOLD        0x3FFFFFFF
#define AANC_PROC_RESET_INT_MIC_CLIP_FLAG   0x7FFFFEFF
#define AANC_PROC_RESET_EXT_MIC_CLIP_FLAG   0x7FFFFDFF
#define AANC_PROC_RESET_PLAYBACK_CLIP_FLAG  0x7FFFFBFF

#define AANC_PROC_QUIET_MODE_RESET_FLAG     0x7FEFFFFF

#define AANC_ED_FLAG_MASK           (AANC_FLAGS_ED_INT | \
                                     AANC_FLAGS_ED_EXT | \
                                     AANC_FLAGS_ED_PLAYBACK)

#define AANC_CLIPPING_FLAG_MASK      (AANC_FLAGS_CLIPPING_INT | \
                                      AANC_FLAGS_CLIPPING_EXT | \
                                      AANC_FLAGS_CLIPPING_PLAYBACK)

#define AANC_SATURATION_FLAG_MASK    (AANC_FLAGS_SATURATION_INT | \
                                      AANC_FLAGS_SATURATION_EXT | \
                                      AANC_FLAGS_SATURATION_PLANT | \
                                      AANC_FLAGS_SATURATION_CONTROL)

/* Model loading depends on having at least control 0 along with gains
 * and the plant model.
 */
#define AANC_FLAGS_STATIC_GAIN_LOADED     0x00010000
#define AANC_FLAGS_PLANT_MODEL_LOADED     0x00020000
#define AANC_FLAGS_CONTROL_0_MODEL_LOADED 0x00040000
#define AANC_FLAGS_CONTROL_1_MODEL_LOADED 0x00080000

#define AANC_MODEL_LOADED            (AANC_FLAGS_STATIC_GAIN_LOADED | \
                                      AANC_FLAGS_PLANT_MODEL_LOADED | \
                                      AANC_FLAGS_CONTROL_0_MODEL_LOADED)
#define AANC_MODEL_MASK              (AANC_FLAGS_STATIC_GAIN_LOADED | \
                                      AANC_FLAGS_PLANT_MODEL_LOADED | \
                                      AANC_FLAGS_CONTROL_0_MODEL_LOADED | \
                                      AANC_FLAGS_CONTROL_1_MODEL_LOADED)

/******************************************************************************
Public Type Definitions
*/

/* Clipping detection and signal peak calculation */
typedef struct _AANC_CLIP_DETECT
{
    unsigned peak_value;  /* Current peak value of the signal */
    uint16 duration;      /* Duration of the clip detect counter */
    uint16 counter;       /* Clip detect counter value */
    bool frame_detect:8;  /* Clip detected in a given frame */
    bool disabled:8;      /* Clip detection enable/disable */
    bool detected:8;      /* Clip detected flag (held by the counter) */
} AANC_CLIP_DETECT;

/* Adaptive gain calculation */
typedef struct _ADAPTIVE_GAIN
{
    unsigned *p_aanc_reinit_flag;

    malloc_t_entry *p_table;             /* Pointer to memory allocation table */
    tCbuffer *p_tmp_ed;                  /* Pointer to temp buffer used by EDs */

    tCbuffer *p_tmp_int_ip;              /* Pointer to temp int mic ip (DM1) */
    tCbuffer *p_tmp_int_op;              /* Pointer to temp int mic op (DM2) */
    ED100_DMX *p_ed_int;                 /* Pointer to int mic ED object */
    uint8 *p_ed_int_dm1;                 /* Pointer to int mic ED DM1 memory */

    /* Note that temp int/ext mic input buffers are in different memory banks
     * to facilitate efficient clipping and peak detection. Output buffers
     * are in DM2 to facilitate efficient FXLMS processing. */
    tCbuffer *p_tmp_ext_ip;              /* Pointer to temp ext mic ip (DM2) */
    tCbuffer *p_tmp_ext_op;              /* Pointer to temp ext mic op (DM2) */
    ED100_DMX *p_ed_ext;                 /* Pointer to ext mic ED object */
    uint8 *p_ed_ext_dm1;                 /* Pointer to ext mic ED DM1 memory */

    tCbuffer *p_tmp_pb_ip;               /* Pointer to temp playback buffer */
    ED100_DMX *p_ed_pb;                  /* Pointer to playback ED object */
    uint8 *p_ed_pb_dm1;                  /* Pointer to playback ED DM1 memory */

    FXLMS100_DMX *p_fxlms;               /* Pointer to FxLMS data */
    uint8 *p_fxlms_dm1;                  /* Pointer to FxLMS memory in DM1 */
    uint8 *p_fxlms_dm2;                  /* Pointer to FxLMS memory in DM2 */

    AANC_CLIP_DETECT clip_ext;           /* Clip detect struct for ext mic */
    AANC_CLIP_DETECT clip_int;           /* Clip detect struct for int mic */
    AANC_CLIP_DETECT clip_pb;            /* Clip detect struct for playback */

    unsigned clip_threshold;             /* Threshold for clipping detection */

    /* Pointers to cap data parameters */
    AANC_PARAMETERS *p_aanc_params;      /* Pointer to AANC parameters */
    unsigned *p_aanc_flags;              /* Pointer to AANC flags */

    /* Input/Output buffer pointers from terminals */
    tCbuffer *p_playback_op;
    tCbuffer *p_fbmon_ip;
    tCbuffer *p_mic_int_ip;
    tCbuffer *p_mic_ext_ip;

    tCbuffer *p_playback_ip;
    tCbuffer *p_fbmon_op;
    tCbuffer *p_mic_int_op;
    tCbuffer *p_mic_ext_op;

    /* Indicate whether scratch is registered */
    bool scratch_registered;

    void *f_handle;                      /* Pointer to AANC feature handle */

} ADAPTIVE_GAIN;

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Create the ADAPTIVE_GAIN data object.
 *
 * \param  pp_ag  Address of the pointer to the object to be created.
 * \param  sample_rate  The sample rate of the operator. Used for the AANC
 *                      EC module.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_create(ADAPTIVE_GAIN **pp_ag, unsigned sample_rate);

/**
 * \brief  Destroy the ADAPTIVE_GAIN data object.
 *
 * \param  pp_ag  Address of the pointer to the adaptive gain object created in
 *                `aanc_proc_create`.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_destroy(ADAPTIVE_GAIN **pp_ag);

/**
 * \brief  Initialize the ADAPTIVE_GAIN data object.
 *
 * \param  p_params  Pointer to the AANC parameters.
 * \param  p_ag  Pointer to the adaptive gain object created in
 *               `aanc_proc_create`.
 * \param  ag_start  Value to initialize the gain calculation to (0-255).
 * \param  p_flags  Pointer to aanc flags.
 * \param  hard_initialize  Boolean indicating whether to hard reset the
 *                          algorithm.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_initialize(AANC_PARAMETERS *p_params, ADAPTIVE_GAIN *p_ag,
                          unsigned ag_start, unsigned *p_flags,
                          bool hard_initialize);

/**
 * \brief  Process data with the adaptive gain algorithm.
 *
 * \param  p_ag  Pointer to the adaptive gain object created in
 *                  `aanc_proc_create`.
 * \param calculate_gain  Boolean indicating whether the gain calculation step
 *                        should be performed. All other adaptive gain
 *                        processing will continue.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_process_data(ADAPTIVE_GAIN *p_ag, bool calculate_gain);

/**
 * \brief  ASM function to do clipping and peak detection.
 *
 * \param  p_ag  Pointer to the adaptive gain object created in
 *                  `aanc_proc_create`.
 *
 * \return Boolean indicating success or failure
 */
extern bool aanc_proc_clipping_peak_detect(ADAPTIVE_GAIN *p_ag);

/**
 * \brief  ASM function to calculate dB representation of gain
 *
 * \param  fine_gain    Fine gain value
 * \param  coarse_gain  Coarse gain value
 *
 * \return int value of calculated gain in dB in Q12.20
 */
extern int aanc_proc_calc_gain_db(uint16 fine_gain, int16 coarse_gain);

#endif /* _AANC_PROC_H_ */
