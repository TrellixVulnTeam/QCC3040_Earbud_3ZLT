/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 *
 * \file  fxlms100_public.h
 * \ingroup lib_private\aanc
 *
 * FXLMS100 library public header file.
 *
 * The FxLMS100 library can be used to calculate an estimate of the feedforward
 * fine gain required to optimize ANC performance. Example usage is in the
 * Adaptive ANC (aanc) capability.
 *
 * The library requires the following memory allocated in the capability:
 * - aanc_fxlms100_dmx_bytes() bytes in any DM
 * - output of macro FXLMS100_DM_BYTES bytes in each of DM1 and DM2 (based on
 *   the size of the filters used)
 * - two buffers of FXLMS100_SCRATCH_MEMORY bytes in scratch memory in any DM
 *
 */
#ifndef _FXLMS100_LIB_PUBLIC_H_
#define _FXLMS100_LIB_PUBLIC_H_

/* Imports FXLMS100 structures */
#include "fxlms100_struct_public.h"

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Determine how much memory to allocate for FXLMS100_DMX (bytes).
 *
 * \return  size value that will be populated with the memory required for
 *          FXLMS100_DMX (bytes).
 */
extern uint16 aanc_fxlms100_dmx_bytes(void);

/**
 * \brief  Create the FXLMS100 data object.
 *
 * \param  p_dmx  Pointer to memory allocated for FXLMS100_DMX. This should be
 *                allocated using the value returned from
 *                aanc_fxlms100_dmx_bytes.
 * \param  p_dm1  Pointer to DM1 memory allocated for the FXLMS100 filters. This
 *                requires FXLMS100_DM_BYTES bytes.
 * \param  p_dm2  Pointer to DM2 memory allocated for the FXLMS100 filters. This
 *                requires FXLMS100_DM_BYTES bytes.
 *
 * \return  boolean indicating success or failure.
 *
 * The memory for FXLMS100_DMX must be allocated based on the return
 * value of aanc_fxlms100_dmx_bytes rather than sizeof(FXLMS100_DMX).
 *
 * It is important that before calling aanc_fxlms100_create the number of
 * coefficients in each filter is assigned (num_coeffs and full_num_coeffs).
 * This ensures correct alignment within the library.
 */
extern bool aanc_fxlms100_create(FXLMS100_DMX *p_dmx, uint8 *p_dm1,
                                 uint8 *p_dm2);

/**
 * \brief  Initialize the FXLMS100 data object.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_dmx  Pointer to memory allocated for FXLMS100_DMX.
 * \param  reset_gain  Boolean indicating whether to reset the gain calculation.
 *
 * \return  boolean indicating success or failure.
 *
 * It is important that before calling aanc_fxlms100_initialize the input buffer
 * pointers are assigned, input parameters are set, and bandpass filter
 * coefficients (e.g. p_bp_int.coeffs.p_num, p_bp_int.coeffs.p_den) are
 * populated.
 */
extern bool aanc_fxlms100_initialize(void *p_asf,
                                     FXLMS100_DMX *p_dmx,
                                     bool reset_gain);

/**
 * \brief  Process data with FXLMS100.
 *
 * \param  p_asf  Pointer to AANC feature handle.
 * \param  p_dmx  Pointer to memory allocated for FXLMS100_DMX.
 *
 * \return  boolean indicating success or failure.
 *
 * It is important that before calling aanc_fxlms100_process_data the scratch
 * buffers are committed and set, and then unset and freed. The FxLMS library
 * requires two scratch buffers that are FXLMS100_SCRATCH_MEMORY bytes. There
 * is no requirement for a specific memory bank.
 */
extern bool aanc_fxlms100_process_data(void *p_asf, FXLMS100_DMX *p_dmx);

/**
 * \brief  Update FxLMS algorithm gain
 *
 * \param  p_dmx  Pointer to memory allocated for FXLMS100_DMX.
 * \param  new_gain  Gain value used to update the FxLMS algorithm gain
 *
 * Note that the gain update will only be made if the new value is within the
 * parameter bounds for the algorithm.
 *
 * \return boolean indicating success or failure.
 */
extern bool aanc_fxlms100_update_gain(FXLMS100_DMX *p_dmx, uint16 new_gain);

/**
 * \brief  Set the plant model coefficients for FXLMS100.
 *
 * \param  p_dmx  Pointer to memory allocated for FXLMS100_DMX.
 * \param  p_msg  Pointer to the OPMSG data received from the framework.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_set_plant_model(FXLMS100_DMX *p_dmx,
                                          OPMSG_AANC_SET_MODEL_MSG *p_msg);

/**
 * \brief  Set the control model coefficients for FXLMS100.
 *
 * \param  p_dmx  Pointer to memory allocated for FXLMS100_DMX.
 * \param  p_msg  Pointer to the OPMSG data received from the framework.
 * \param  p_destination  Pointer to integer that is populated with the
 *                        destination of the control model message.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_set_control_model(FXLMS100_DMX *p_dmx,
                                            OPMSG_AANC_SET_MODEL_MSG *p_msg,
                                            int *p_destination);

/**
 * \brief  Calculate the actual number of coefficients being used in a filter.
 *
 * \param  p_filter  Pointer to `FXLMS100_FILTER` object for calculation.
 * \param  max_taps  Maximum number of coefficients in the filter.
 *
 * \return  unsigned value indicating the number of coefficients.
 */
extern uint16 aanc_fxlms100_calculate_num_coeffs(FXLMS100_FILTER *p_filter,
                                                 uint16 max_coeffs);

#endif /* _FXLMS100_LIB_PUBLIC_H_ */