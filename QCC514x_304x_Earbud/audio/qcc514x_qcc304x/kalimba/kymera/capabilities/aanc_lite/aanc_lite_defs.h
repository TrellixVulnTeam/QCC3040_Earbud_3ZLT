/****************************************************************************
 * Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC_LITE
 * \ingroup capabilities
 * \file  aanc_lite_defs.h
 * \ingroup AANC_LITE
 *
 * Adaptive ANC (AANC) Lite operator shared definitions and include files
 *
 */

#ifndef _AANC_LITE_DEFS_H_
#define _AANC_LITE_DEFS_H_

#define AANC_LITE_DEFAULT_FRAME_SIZE   64   /* 4 ms at 16k */
#define AANC_LITE_DEFAULT_BLOCK_SIZE  0.5 * AANC_LITE_DEFAULT_FRAME_SIZE
#define AANC_LITE_DEFAULT_BUFFER_SIZE   2 * AANC_LITE_DEFAULT_FRAME_SIZE
#define AANC_LITE_FRAME_RATE          250   /* Fs = 16kHz, frame size = 64 */ 

#endif /* _AANC_LITE_DEFS_H_ */