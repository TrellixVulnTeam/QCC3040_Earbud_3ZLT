/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \file  fxlms100_defs_public.h
 * \ingroup lib_private\aanc
 *
 * FXLMS100 library header file providing public definitions common to C and
 * ASM code.
 */
#ifndef _FXLMS100_LIB_DEFS_PUBLIC_H_
#define _FXLMS100_LIB_DEFS_PUBLIC_H_

/******************************************************************************
Public Constant Definitions
*/

/* Enable saturation detection in the build */
#define DETECT_SATURATION

/* Flag definitions that are set during processing */
#define FXLMS100_FLAGS_SATURATION_INT_SHIFT      12
#define FXLMS100_FLAGS_SATURATION_EXT_SHIFT      13
#define FXLMS100_FLAGS_SATURATION_PLANT_SHIFT    14
#define FXLMS100_FLAGS_SATURATION_CONTROL_SHIFT  15

#define FXLMS100_BANDPASS_SHIFT                   3

/* FxLMS model definitions differ based on platform */
#ifdef AANC_MAOR_V20
#define FXLMS100_PLANT_SHIFT                      7
#define FXLMS100_CONTROL_SHIFT                    7
#define FXLMS100_COEFF_SHIFT                      24
#else
#define FXLMS100_PLANT_SHIFT                      2
#define FXLMS100_CONTROL_SHIFT                    2
#define FXLMS100_COEFF_SHIFT                      29
#endif

#define FXLMS100_MODEL_COEFF0                     1 << FXLMS100_COEFF_SHIFT

#define FXLMS100_GAIN_SHIFT                      23

/* Frame size used to allocate temporary buffer. */
#define FXLMS100_FRAME_SIZE                      64
#define FXLMS100_SCRATCH_MEMORY                  FXLMS100_FRAME_SIZE * sizeof(int)

/* FxLMS filters require history buffers split between DM banks. Each DM bank
 * requires 2 * (plant + 2*control + 2*bandpass) integers. Returns the number
 * of integers (not bytes).
 */
#define FXLMS100_BUFFER_SIZE(num_plant, num_control, num_bp) \
    ((2 * (num_control) + (num_plant) + 2 * (num_bp)))
#define FXLMS100_DM_BYTES(num_plant, num_control, num_bp) \
    (sizeof(int) * 2 * FXLMS100_BUFFER_SIZE(num_control, num_plant, num_bp))

/* Indicate whether to run the FxLMS calculation based on a single filter or
 * parallel filter configuration.
 */
#define FXLMS100_CONFIG_SINGLE                    0x0000
#define FXLMS100_CONFIG_PARALLEL                  0x0001
#define FLXMS100_CONFIG_LAYOUT_MASK               0x000F
#define FXLMS100_CONFIG_LAYOUT_MASK_INV           (FLXMS100_CONFIG_LAYOUT_MASK ^ \
                                                   0xFFFF)

#endif /* _FXLMS100_LIB_DEFS_PUBLIC_H_ */
