/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  fxlms100_struct_public.h
 * \ingroup lib_private\aanc
 *
 * FXLMS100 library header file providing public data structures.
 *
 */
#ifndef _FXLMS100_LIB_STRUCT_PUBLIC_H_
#define _FXLMS100_LIB_STRUCT_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"
#include "opmgr/opmgr_for_ops.h"
#include "fxlms100_defs_public.h"

/******************************************************************************
Public Constant Definitions
*/

/******************************************************************************
Public Variable Definitions.
*/

/* FXLMS100_FILTER_COEFFS provides clarity for identifying coefficient arrays.
 * Expected usage is to instantiate them in a parent structure.
 */
typedef struct _FXLMS100_FILTER_COEFFS
{
    int *p_num; /* Pointer to numerator coefficient array */
    int *p_den; /* Pointer to denominator coefficient array */
} FXLMS100_FILTER_COEFFS;

/* FXLMS100_FILTER represents the filters used by the FXLMS, with coefficients
 * and the input and output history buffers grouped together in one location.
 */
typedef struct _FXLMS100_FILTER
{
    FXLMS100_FILTER_COEFFS coeffs; /* Filter coefficients
                                    * (num: DM2, den: DM1)
                                    */
    int *p_input_history;          /* Input history array (DM1) */
    int *p_current_input_history;  /* Input history array index pointer */
    int *p_output_history;         /* Output history array (DM2) */
    int *p_current_output_history; /* Output history array index pointer */
    /* Number of coefficients used in the filter */
    uint16 num_coeffs;
    /* Number of coefficients stored in the filter */
    uint16 full_num_coeffs;
} FXLMS100_FILTER;

/* Type definition for the FxLMS100 library. Note that this must be allocated
 * with enough space as determined by `aanc_fxlms100_dmx_bytes` to ensure the
 * private fields are allocated.
 */
typedef struct _FXLMS100_DMX
{
	/* Pointers to memory that must be set by the capability prior to
     * aanc_fxlms100_process_data. The capability requires two scratch buffers
     * that are FXLMS100_SCRATCH_MEMORY bytes. There is no memory preference for
     * these.
     */
    unsigned *p_scratch_plant;   /* Pointer to pre-plant scratch buffer */
    unsigned *p_scratch_control; /* Pointer to pre-control scratch buffer */

    /* Pointers to I/O buffers for the FxLMS100 library that must be set
     * prior to initialization.
     */
    tCbuffer *p_int_ip;        /* Pointer to cbuffer for int mic input */
    tCbuffer *p_ext_ip;        /* Pointer to cbuffer for ext mic input */

    tCbuffer *p_int_op;        /* Pointer to cbuffer for int mic output */
    tCbuffer *p_ext_op;        /* Pointer to cbuffer for ext mic output */

	/* Parameters for the FxLMS library */
    int target_nr;             /* Target Noise Reduction */
    int mu;                    /* Mu (convergence rate control) */
    int gamma;                 /* Gamma (leak control) */
    int frame_size;            /* Number of samples to process per frame */
    int lambda;                /* Lambda (fine tuning)  */
    unsigned initial_gain;     /* Initial gain value for algorithm */
    unsigned read_ptr_upd;     /* Control read pointer update in process_data */
    unsigned min_bound;        /* Minimum bound for FxLMS gain */
    unsigned max_bound;        /* Maximum bound for FxLMS gain */
    unsigned max_delta;        /* Maximum gain step for FxLMS algorithm */
    unsigned configuration;    /* FxLMS operating configuration */

	/* Statistics from the FxLMS library */
    unsigned flags;            /* Flags set during data processing */
    unsigned adaptive_gain;    /* Calculated adaptive gain value for the ANC */
    bool licensed;             /* Flag to indicate license status */

    FXLMS100_FILTER p_plant;        /* Filter for the plant model */
    FXLMS100_FILTER p_control_0;    /* Filter for the control model 0 */
    FXLMS100_FILTER p_control_1;    /* Filter for the control model 1 */
    FXLMS100_FILTER p_bp_int;       /* Bandpass filter on the int mic input */
    FXLMS100_FILTER p_bp_ext;       /* Bandpass filter on the ext mic input */

    /* Private fields follow */
} FXLMS100_DMX;

/* OPMSG_AANC_SET_MODEL_MSG represents the data that is received from a
 * set_model message, whether plant or control.
 */
typedef struct _OPMSG_AANC_SET_MODEL_MSG
{
    OPMSG_HEADER header; /* OPMSG header */
    unsigned data[];     /* Model message data */
} OPMSG_AANC_SET_MODEL_MSG;

#endif /* _FXLMS100_LIB_STRUCT_PUBLIC_H_ */