/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Kymera private header for output chain APIs only meant to be used by output manager
*/

#ifndef KYMERA_OUTPUT_PRIVATE_H
#define KYMERA_OUTPUT_PRIVATE_H

#include "kymera_output_chain_config.h"
#include "chain.h"

/*! \brief Create and configure the audio output chain operators.
    \param config The output chain configuration.
*/
void KymeraOutput_CreateOperators(const kymera_output_chain_config *config);

/*! \brief Check if AEC REF must always be part of the output chain
 */
bool KymeraOutput_MustAlwaysIncludeAec(void);

/*! \brief Connect the audio output chain.
*/
void KymeraOutput_ConnectChain(void);

/*! \brief Stop and destroy the audio output chain.
*/
void KymeraOutput_DestroyChain(void);

/*! \brief Set the output chains sample rate for the main input.
    \param rate The sample rate to set
 */
void KymeraOutput_SetMainSampleRate(uint32 rate);

/*! \brief Set the output chains sample rate for the auxiliary input.
    \param rate The sample rate to set
 */
void KymeraOutput_SetAuxSampleRate(uint32 rate);

/*! \brief Connect to main input (stereo).
    \param left Source to connect to the left channel.
    \param right Source to connect to the right channel.
 */
void KymeraOutput_ConnectToStereoMainInput(Source left, Source right);

/*! \brief Connect to main input (mono).
    \param mono Source to connect.
 */
void KymeraOutput_ConnectToMonoMainInput(Source mono);

/*! \brief Connect to auxiliary input.
    \param aux Source to connect.
 */
void KymeraOutput_ConnectToAuxInput(Source aux);

/*! \brief Disconnect main input (stereo).
 */
void KymeraOutput_DisconnectStereoMainInput(void);

/*! \brief Disconnect main input (mono).
 */
void KymeraOutput_DisconnectMonoMainInput(void);

/*! \brief Disconnect auxiliary input.
 */
void KymeraOutput_DisconnectAuxInput(void);

#endif /* KYMERA_OUTPUT_PRIVATE_H */
