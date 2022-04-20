/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \file  ed100_defs_public.h
 * \ingroup lib_private\aanc
 *
 * ED100 library header file providing public definitions common to C and
 * ASM code.
 */
#ifndef _ED100_LIB_DEFS_PUBLIC_H_
#define _ED100_LIB_DEFS_PUBLIC_H_

/******************************************************************************
Public Constant Definitions
*/

#define ED100_DEFAULT_FRAME_SIZE  64
#define ED100_DEFAULT_BUFFER_SIZE ED100_DEFAULT_FRAME_SIZE + 1
#define ED100_SCRATCH_MEMORY ED100_DEFAULT_BUFFER_SIZE * sizeof(int)

#endif /* _ED100_LIB_DEFS_PUBLIC_H_ */