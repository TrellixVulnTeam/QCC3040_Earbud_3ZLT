/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file cbops_howling_limiter_c.h
 * \ingroup cbops
 *
 */

#ifndef _CBOPS_HOWLING_LIMITER_C_H_
#define _CBOPS_HOWLING_LIMITER_C_H_

#ifdef CAPABILITY_DOWNLOAD_BUILD
#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER

#include "hl_limiter.h"
#include "cbops_c.h"
/****************************************************************************
Public Type Declarations
*/

/** Structure of the Howling Limiter cbops operator specific data */
typedef HL_LIMITER_DATA cbops_howling_limiter;

/** The address of the function vector table. This is aliased in ASM */
extern unsigned cbops_howling_limiter_table[];

/**
 * create operator to minimise/prevent "howling"
 *
 * \param input_idx             Pointer to the input channel index.
 * \param mic_sampling_rate     Microphone sampling rate.
 * \param hl_ui                 Howling Limiter user interface parameter structure
 *
 * \return pointer to operator
 */
cbops_op *create_howling_limiter_op(
    unsigned input_idx,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui);

/**
 * Initialize the howling limiter operator
 *
 * \param op                    Pointer to the operator structure.
 * \param mic_sampling_rate     Microphone sampling rate.
 * \param hl_ui                 Howling Limiter user interface parameter structure
 *
 * \return none
 */
void initialize_howling_limiter_op(
    cbops_op *op,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui);

/**
 * Configure the howling limiter operator
 *
 * \param op                    Pointer to the operator structure.
 * \param mic_sampling_rate     Microphone sampling rate.
 * \param hl_ui                 Howling Limiter user interface parameter structure
 *
 * \return none
 */
void configure_howling_limiter_op(
    cbops_op *op,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui);

#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif /* _CBOPS_HOWLING_LIMITER_C_H_ */
