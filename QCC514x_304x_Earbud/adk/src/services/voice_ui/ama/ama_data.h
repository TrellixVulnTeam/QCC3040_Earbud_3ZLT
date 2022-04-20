/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_data.h
\brief  File consists of function to get/set data for ama
*/

#ifndef AMA_DATA_H
#define AMA_DATA_H

#include "ama_protocol.h"
#include <csrtypes.h>
#include <sink.h>

#define AMA_OPUS_16KBPS (16)
#define AMA_OPUS_32KBPS (32)

#define MSBC_ENCODER_BITPOOL_VALUE 26


/* Defines different state of ama */
typedef enum {
    ama_state_initialized,
    ama_state_idle,
    ama_state_sending,
    ama_state_last
}ama_state_t;

/* \brief Defines different supported codec of ama */
typedef enum {
    ama_codec_sbc = 0,
    ama_codec_msbc,
    ama_codec_opus,
    ama_codec_last
}ama_codec_t;

/* \brief Defines the ama audio configuration support */
typedef struct
{
    union
    {
        /* msbc codec just requires bitpool size */
        unsigned msbc_bitpool_size;
        unsigned opus_req_kbps;
        /* Any other codec specific configuration */
    }u;
    ama_codec_t codec;
}ama_audio_data_t;

/*!
    \brief Utility function to set the AMA internal state
*/
void AmaData_SetState(ama_state_t state);

/*!
    \brief Utility function to get the AMA internal state
*/
ama_state_t AmaData_GetState(void);

/*!
    \brief Utility function to check if Ama is ready to start voice session
*/
bool AmaData_IsReadyToSendStartSpeech(void);

/*!
    \brief Utility function to check if Ama is in correct state to send voice data
*/
bool AmaData_IsSendingVoiceData(void);

/*!
    \brief Utility function to get the Ama audio configuration data
*/
ama_audio_data_t* AmaData_GetAudioData(void);

/*!
    \brief Utility function to set the Ama audio configuration
*/
void AmaData_SetAudioData(ama_audio_data_t *config);

void AmaData_SetActiveTransport(ama_transport_t type);

ama_transport_t AmaData_GetActiveTransport(void);

/*! \brief Set the stopping session flag.
    \param stopping Indication of whether stopping session or not
 */
void AmaData_SetStoppingSession(bool stopping);

/*! \brief Get the stopping session flag.
    \return bool Indication of whether stopping session or not
 */
bool AmaData_IsStoppingSession(void);

#endif /* AMA_DATA_H */

