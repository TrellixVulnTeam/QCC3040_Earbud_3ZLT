/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  ed100_struct_public.h
 * \ingroup lib_private\aanc
 *
 * ED100 library header file providing public data structures.
 *
 */
#ifndef _ED100_LIB_STRUCT_PUBLIC_H_
#define _ED100_LIB_STRUCT_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"
#include "opmgr/opmgr_for_ops.h"

/* Include ED100 definitions */
#include "ed100_defs_public.h"

/******************************************************************************
Public Constant Definitions
*/

/******************************************************************************
Public Variable Definitions.
*/

/* Type definition for the ED100 library. Note that this must be allocated
 * with enough space as determined by `aanc_ed100_dmx_bytes` to ensure the
 * private fields are allocated.
 */
typedef struct _ED100_DMX
{
    /* Working cbuffers need to be set prior to initialization */
    tCbuffer *p_input;                /* Input buffer pointer */
    tCbuffer *p_tmp;                  /* Temporary buffer pointer */

    /* Input parameters need to be set prior to initialization */
    int frame_size;                   /* No. samples to process per frame */
    int attack_time;                  /* Attach time (Q7.N, s) */
    int decay_time;                   /* Decay time (Q7.N, s) */
    int envelope_time;                /* Envelope time (Q7.N, s) */
    int init_frame_time;              /* Initial frame time (Q7.N, s) */
    int ratio;                        /* Ratio */
    int min_signal;                   /* Min signal (Q12.N, dB) */
    int min_max_envelope;             /* Min-Max envelope (Q12.N, dB) */
    int delta_th;                     /* Delta threshold (Q12.B, dB) */
    int count_th;                     /* Count threshold (Q7.N) */
    unsigned hold_frames;             /* No. frames to hold a trigger */
    unsigned e_min_threshold;         /* e_min threshold (12.N, dB) */
    unsigned e_min_counter_threshold; /* e_min counter threshold (frames) */
    bool e_min_check_disabled;        /* Control e_min check */

	/* Statistics reported by the ED100 library */
    int spl;                         /* SPL value (dBFS) */
    int spl_max;                     /* Maximum SPL value (dBFS) */
    int spl_mid;                     /* Mid-point SPL value (dBFS) */
    bool detection;                  /* Detection flag */
    bool licensed;                   /* License status flag */

    /* Private fields follow */
} ED100_DMX;

#endif /* _ED100_LIB_STRUCT_PUBLIC_H_ */