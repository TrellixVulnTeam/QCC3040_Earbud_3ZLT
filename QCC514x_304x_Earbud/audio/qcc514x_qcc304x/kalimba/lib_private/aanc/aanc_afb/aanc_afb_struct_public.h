/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  aanc_afb_struct_public.h
 * \ingroup lib_private\aanc
 *
 * AANC AFB library header file providing public data structures.
 *
 */
#ifndef _AANC_AFB_LIB_STRUCT_PUBLIC_H_
#define _AANC_AFB_LIB_STRUCT_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"
#include "filter_bank_c.h"

/******************************************************************************
Public Constant Definitions
*/

/******************************************************************************
Public Variable Definitions
*/

/* Type definition for the AANC AFB library. Note that this must be allocated
 * with enough space as determined by `aanc_afb_bytes` to ensure the private
 * fields are allocated.
 */
typedef struct _AANC_AFB
{
    bool licensed;                        /* Flag to indicate license status */

    /* The analysis filterbank has a field fft_object_ptr of type t_fft_object
     * that has pointers to scratch memory that must be set before processing
     * data as follows:
     * - real_scratch_ptr (DM1)
     * - imag_scratch_ptr (DM2)
     * - fft_scratch_ptr (DM2)
     */
    t_analysis_filter_bank_object afb;    /* Filter bank object */

    /* Private fields follow */
} AANC_AFB;

#endif /* _AANC_AFB_LIB_STRUCT_PUBLIC_H_ */