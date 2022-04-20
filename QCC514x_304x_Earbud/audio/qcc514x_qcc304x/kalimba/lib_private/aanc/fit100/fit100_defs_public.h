/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  fit100_defs_public.h
 * \ingroup lib_private\aanc
 *
 * FIT100 library header file providing public definitions common to C and
 * ASM code.
 */
#ifndef _FIT100_LIB_DEFS_PUBLIC_H_
#define _FIT100_LIB_DEFS_PUBLIC_H_

/******************************************************************************
Public Constant Definitions
*/

/* Frame size  */
#define FIT100_FRAME_SIZE   64
#define FIT100_BINX         1
#define FIT100_FFT_SIZE     128

#define FIT_BAD             0
#define FIT_OK              1
#define FIT_FAIL            2

#define LOG2_TO_DB_CONV_FACTOR           Qfmt_(6.020599913279624, 8)

#endif /* _FIT100_LIB_DEFS_PUBLIC_H_ */
