/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup lib_private\aanc
 * \file  ed100_public.h
 * \ingroup AANC
 *
 * ED100 library public header file.
 *
 * The ED100 library can be used to monitor for non-stationary energy in the
 * environment. Example usage is in the Adaptive ANC (aanc) capability.
 *
 * The library requires the following memory allocated in the capability:
 * - aanc_ed100_dmx_bytes() bytes in any DM
 * - aanc_ed100_dm1_bytes() bytes in DM1
 * - temporary cbuffer of ED100_DEFAULT_BUFFER_SIZE bytes in any DM
 *
 */
#ifndef _ED100_LIB_PUBLIC_H_
#define _ED100_LIB_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"

/* Imports ED100 data types */
#include "ed100_struct_public.h"

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Determine how much memory to allocate for ED100_DMX (bytes).
 *
 * \return  size value that will be populated with the memory required for
 *          ED100_DMX (bytes).
 */
extern uint16 aanc_ed100_dmx_bytes(void);

/**
 * \brief  Determine how much memory to allocate for ED100 in DM1.
 *
 * \return  size value that will be populated with the memory required for
 *          ED100 in DM1 (bytes).
 */
extern uint16 aanc_ed100_dm1_bytes(void);

/**
 * \brief  Create the ED100 data object.
 *
 * \param  p_ed  Pointer to allocated ED100_DMX object. This should be allocated
 *               using the value returned from aanc_ed100_dmx_bytes.
 * \param  p_dm1  Pointer to memory space allocated for ED100 in DM1. This
 *                should be allocated using the value returned from
 *                aanc_ed100_dm1_bytes.
 * \param  sample_rate  Sample rate for the ED100 module (Hz).
 *
 * \return  boolean indicating success or failure.
 *
 */
extern bool aanc_ed100_create(ED100_DMX *p_ed, uint8 *p_dm1,
                              unsigned sample_rate);

/**
 * \brief  Initialize the ED100 data object.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_ed  Pointer to allocated ED100_DMX object.
 *
 * \return  boolean indicating success or failure.
 *
 * It is important that prior to calling aanc_ed100_initialize the input buffer
 * has been assigned, the temporary buffer has been assigned and all parameters
 * have been set.
 */
extern bool aanc_ed100_initialize(void *p_asf, ED100_DMX *p_ed);

/**
 * \brief  Process data with ED100.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 *  \param  p_ed  Pointer to allocated ED100_DMX object.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_ed100_process_data(void *p_asf, ED100_DMX *p_ed);

/**
 * \brief  Destroy the ED100 data object.
 *
 * \param  p_ed  Pointer to allocated ED100_DMX object.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_ed100_destroy(ED100_DMX *p_ed);

/**
 * \brief  Do self-speech detection.
 *
 * \param  p_ed_int  Pointer to the internal ED100_DMX object.
 * \param  p_ed_ext  Pointer to the external ED100_DMX object.
 * \param  threshold  Threshold for self-speech detection
 *
 * \return  boolean indicating self-speech detection.
 */
extern bool aanc_ed100_self_speech_detect(ED100_DMX *p_ed_int,
                                          ED100_DMX *p_ed_ext,
                                          int threshold);

#endif /* _ED100_LIB_PUBLIC_H_ */