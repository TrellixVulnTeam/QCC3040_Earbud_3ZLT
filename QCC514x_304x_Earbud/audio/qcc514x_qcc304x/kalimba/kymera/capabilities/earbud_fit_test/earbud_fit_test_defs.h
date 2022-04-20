/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \ingroup capabilities
 * \file  earbud_fit_test_defs.h
 * \ingroup Earbud Fit Test
 *
 * Earbud Fit Test operator shared definitions and include files
 *
 */


#ifndef _EARBUD_FIT_TEST_DEFS_H_
#define _EARBUD_FIT_TEST_DEFS_H_

#include "aanc_afb_public.h"
#include "fit100_public.h"
#include "aanc_security_public.h"

#define EFT_DEFAULT_FRAME_SIZE   64   /* 4 ms at 16k */
#define EFT_DEFAULT_BLOCK_SIZE  0.5 * EFT_DEFAULT_FRAME_SIZE
#define EFT_DEFAULT_BUFFER_SIZE   2 * EFT_DEFAULT_FRAME_SIZE
#define EFT_INTERNAL_BUFFER_SIZE  EFT_DEFAULT_FRAME_SIZE + 1
#define EFT_FRAME_RATE          250   /* Fs = 16kHz, frame size = 64 */

#endif /* _EARBUD_FIT_TEST_DEFS_H_ */