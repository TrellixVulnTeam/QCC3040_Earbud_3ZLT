/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 *
 * \file  fit100_public.h
 * \ingroup lib_private\aanc
 *
 * FIT100 library public header file.
 *
 * The FIT100 library provides an analysis of the fit quality under certain
 * listening conditions by comparing the power in the input signal with the
 * power in the reference signal.
 *
 * The library requires the following memory allocated in the capability:
 * - aanc_fit100_bytes() in any DM
 *
 * The memory for FIT100 must be allocated based on the return value of
 * aanc_fit100_bytes rather than sizeof(FIT100).
 */
#ifndef _FIT100_LIB_PUBLIC_H_
#define _FIT100_LIB_PUBLIC_H_

/* Import AFB types */
#include "aanc_afb_public.h"

/* Imports FIT100 structures */
#include "fit100_struct_public.h"
#include "fit100_defs_public.h"

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Determine how much memory to allocate for FIT100 (bytes).
 *
 * \return  size value that will be populated with the memory required for
 *          FIT100 (bytes).
 */
extern uint16 aanc_fit100_bytes(void);

/**
 * \brief  Populate the FIT100 data object.
 *
 * \param  p_fit  Pointer to FIT100 data object.
 *
 * \return  boolean indicating success or failure.
 *
 * The memory for FIT100 must be allocated based on the return
 * value of aanc_fit100_bytes rather than sizeof(FIT100).
 */
bool aanc_fit100_create(FIT100 *p_fit);

/**
 * \brief  Initialize the FIT100 data object.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_fit  Pointer to the FIT100 data object.
 * \param  p_afb  Pointer to AANC AFB data object for the internal microphone.
 * \param  p_afb  Pointer to AANC AFB data object for the reference.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fit100_initialize(void *p_asf,
                                   FIT100 *p_fit,
                                   AANC_AFB *p_afb_mic,
                                   AANC_AFB *p_afb_ref);

/**
 * \brief  Process data with FIT100.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_fit  Pointer to FIT100 data object.
 *
 * \return  boolean indicating success or failure.
 */
extern unsigned int aanc_fit100_process_data(void *p_asf, FIT100 *p_fit);

/**
 * \brief  Destroy the FIT100 data object.
 *
 * \param  pp_ed  Pointer to FIT100 data object.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_fit100_destroy(FIT100 *p_fit);

#endif /* _FIT100_LIB_PUBLIC_H_ */