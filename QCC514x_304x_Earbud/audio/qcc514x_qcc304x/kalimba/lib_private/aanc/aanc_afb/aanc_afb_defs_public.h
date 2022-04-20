/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \ingroup lib_private\aanc
 * \file  aanc_afb_defs_public.h
 * \ingroup AANC
 *
 * aanc AFB library header file providing public definitions common to C and
 * ASM code.
 */
#ifndef _AANC_AFB_LIB_DEFS_PUBLIC_H_
#define _AANC_AFB_LIB_DEFS_PUBLIC_H_

/******************************************************************************
Public Constant Definitions
*/

/* Buffer sizes */
#define AANC_FILTER_BANK_FRAME_SIZE    64
#define AANC_FILTER_BANK_WINDOW_SIZE  128
#define AANC_FILTER_BANK_NUM_BINS      65
#define AANC_AFB_SCRATCH_MEMORY       AANC_FILTER_BANK_WINDOW_SIZE*sizeof(int)

/* Scale factor */
#define AANC_FILTER_BANK_SCALE          6

#endif /* _AANC_AFB_LIB_DEFS_PUBLIC_H_ */
