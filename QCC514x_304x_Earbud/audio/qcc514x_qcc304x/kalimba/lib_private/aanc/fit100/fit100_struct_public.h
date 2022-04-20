/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 * \file  fit100_struct_public.h
 * \ingroup lib_private\aanc
 *
 * FIT100 library header file providing public data structures.
 *
 */
#ifndef _FIT100_LIB_STRUCT_PUBLIC_H_
#define _FIT100_LIB_STRUCT_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"
#include "opmgr/opmgr_for_ops.h"

/******************************************************************************
Public Constant Definitions
*/

/******************************************************************************
Public Variable Definitions.
*/

/* Type definition for the FIT100 library. Note that this must be allocated
 * with enough space as determined by `aanc_fit100_bytes` to ensure the private
 * fields are allocated.
 */
typedef struct _FIT100
{
    unsigned int time_constant;     /* Smoothing time constant (s, Q1.N) */
    unsigned int threshold;         /* Good/bad fit threshold (dB, Q8.N) */
    unsigned int bexp_offset;       /* Bin power scaling factor */

    unsigned int fit_flag;          /* Denotes the quality of the fit */
    /* Frequency bin power values calculated based on the first bin */
    unsigned int pwr_internal;      /* Internal mic power (dB, Q8.N) */
    unsigned int pwr_reference;     /* Reference mic power (dB, Q8.N) */
    unsigned int pwr_ratio;         /* Power Ratio (dB, Q8.N) */

    bool licensed;                  /* Flag to indicate license status */
    /* Private fields follow */
} FIT100;


#endif /* _FIT100_LIB_STRUCT_PUBLIC_H_ */