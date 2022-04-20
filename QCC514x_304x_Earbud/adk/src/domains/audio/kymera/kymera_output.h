/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Kymera private header for output chain APIs.
*/

#ifndef KYMERA_OUTPUT_H
#define KYMERA_OUTPUT_H

#include "kymera_output_chain_config.h"
#include <chain.h>

/*! \brief Load downloadable capabilities for the output chain in advance.
    \param chain_type Chain type to use when loading the capabilities.
*/
void KymeraOutput_LoadDownloadableCaps(output_chain_t chain_type);

/*! \brief Undo KymeraOutput_LoadDownloadableCaps.
    \param chain_type Should be the same type used in KymeraOutput_LoadDownloadableCaps
*/
void KymeraOutput_UnloadDownloadableCaps(output_chain_t chain_type);

/*! \brief Start output chain
*/
void  KymeraOutput_ChainStart(void);

/*! \brief Get output chain handle
 */
kymera_chain_handle_t KymeraOutput_GetOutputHandle(void);

/*! \brief Initialize an output chain config with default parameters
 *  \param config[out] Pointer to the chain config to populate
 *  \param rate Sample rate to consider for calculated parameters
 *  \param kick_period The kick period value to consider for calculated parameters
 *  \param buffer_size The input buffer size
*/
void KymeraOutput_SetDefaultOutputChainConfig(kymera_output_chain_config *config,
                                              uint32 rate, unsigned kick_period,
                                              unsigned buffer_size);

/*! \brief Set the main volume for audio output chain
    \param volume_in_db The volume to set.
*/
void KymeraOutput_SetMainVolume(int16 volume_in_db);

/*! \brief Set the auxiliary volume for audio output chain
    \param volume_in_db The volume to set.
*/
void KymeraOutput_SetAuxVolume(int16 volume_in_db);

/*! \brief Set Time-To-Play for the auxiliary output
    \param time_to_play The TTP to set
    \return TRUE on success, FALSE otherwise
*/
bool KymeraOutput_SetAuxTtp(uint32 time_to_play);

/*! \brief Get the sample rate used for the main output
    \return The sample rate
*/
uint32 KymeraOutput_GetMainSampleRate(void);

/*! \brief Get the sample rate used for the auxiliary output
    \return The sample rate
*/
uint32 KymeraOutput_GetAuxSampleRate(void);

/*! \brief Set the mute state for the main output channel
    \param mute_enable TRUE to enable mute, FALSE to disable mute
*/
void KymeraOutput_MuteMainChannel(bool mute_enable);


#endif /* KYMERA_OUTPUT_H */
