/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup lib_private\aanc
 * \file  aanc_afb_public.h
 * \ingroup AANC
 *
 * AANC AFB library public header file.
 *
 * The AANC AFB library provides an analysis filterbank for AANC modules.
 * Example usage is in the Earbud Fit Test (eft) capability.
 *
 * The library requires the following memory allocated in the capability:
 * - aanc_afb_bytes() in any DM
 * - AANC_AFB_SCRATCH_MEMORY scratch buffers - 1x in DM1 and 2x in DM2
 *
 * The library also requires an fft twiddle allocation in the math library.
 * Scratch and FFT allocations are required at the capability level as they are
 * only required once & can be re-used between instances of the library.
 */
#ifndef _AANC_AFB_LIB_PUBLIC_H_
#define _AANC_AFB_LIB_PUBLIC_H_

/* Imports AFB structures */
#include "aanc_afb_defs_public.h"
#include "aanc_afb_struct_public.h"

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Determine how much memory to allocate for AANC_AFB (bytes).
 *
 * \return  size value that will be populated with the memory required for
 *          AANC_AFB (bytes).
 */
extern uint16 aanc_afb_bytes(void);

/**
 * \brief  Create the AANC AFB data object.
 *
 * \param  p_afb  Pointer to AANC AFB data object.
 *
 * \return  boolean indicating success or failure.
 *
 * The memory for AANC_AFB must be allocated based on the return
 * value of aanc_afb_bytes rather than sizeof(AANC_AFB).
 */
extern bool aanc_afb_create(AANC_AFB *p_afb);

/**
 * \brief  Initialize the AANC AFB data object.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_afb  Pointer to AANC AFB data object.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_afb_initialize(void *p_asf, AANC_AFB *p_afb);

/**
 * \brief  Process data with AANC AFB.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_afb  Pointer to AANC AFB data object.
 * \param  p_input  Pointer to cbuffer with input data that will be processed.
 *
 * \return  boolean indicating success or failure.
 *
 * It is important that before calling aanc_afb_process_data the scratch
 * buffers are committed and set, and then unset and freed. The AANC AFB library
 * requires three scratch buffers that are AANC_AFB_SCRATCH_MEMORY bytes.
 * These are set in the fft_object_ptr: real_scratch_ptr (DM1),
 * imag_scratch_ptr (DM2), and fft_scratch_ptr (DM2).
 */
extern bool aanc_afb_process_data(void *p_asf, AANC_AFB *p_afb,
                                  tCbuffer *p_input);

/**
 * \brief  Destroy the AANC AFB data object.
 *
 * \param  pp_ed  Pointer to AANC AFB data object.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_afb_destroy(AANC_AFB *p_afb);

#endif /* _AANC_AFB_LIB_PUBLIC_H_ */