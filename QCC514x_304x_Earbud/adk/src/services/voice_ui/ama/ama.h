/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama.h
\brief  Interfaces defination of Amazon AVS interfaces
*/

#ifndef AMA_H
#define AMA_H

#include "ama_protocol.h"
#include "ama_config.h"
#include "logging.h"
#include <csrtypes.h>
#include <stdio.h>
#include <voice_ui_va_client_if.h>

#define ASSISTANT_OVERRIDEN             FALSE
#define ASSISTANT_OVERRIDE_REQUIRED     TRUE

/* Size of locale string, including terminator */
#define AMA_LOCALE_STR_SIZE (sizeof(AMA_DEFAULT_LOCALE))
/* Length of the locale string */
#define AMA_LOCALE_STR_LEN  (AMA_LOCALE_STR_SIZE-1)

#if defined(INCLUDE_AMA) && !defined(INCLUDE_KYMERA_AEC)
    #error AMA needs the INCLUDE_KYMERA_AEC compilation switch for this platform
#endif

/*! Callback function pointer to transmit data to the handset */
typedef bool (*ama_tx_callback_t)(uint8 *data, uint16 size_data);

/*! \brief Initialise the AMA component.

    \param task The init task
    \return TRUE if component initialisation was successful, otherwise FALSE.
*/
bool Ama_Init(Task init_task);

/*! \brief Parse AMA protocol data received from the handset.

    \param data Pointer to uint8 data to parse
    \param size_data Number of octets to parse
    \return TRUE if the data was completely parsed, otherwise FALSE.
*/
bool Ama_ParseData(uint8 *data, uint16 size_data);

/*! \brief Send AMA protocol data to the handset.

    \param data Pointer to uint8 data to send
    \param size_data Number of octets to send
    \return TRUE if the data was successfully sent, otherwise FALSE.
*/
bool Ama_SendData(uint8 *data, uint16 size_data);

/*! \brief Set the callback function to use to send data to the handset
           for a given transport.

    \param callback Pointer to callback function
    \param transport Identifies the transport which will use this callback
*/
void Ama_SetTxCallback(ama_tx_callback_t callback, ama_transport_t transport);

/*! \brief Configures AMA for the selected codec.
    \param ama_codec_t selected codec
 */
void Ama_ConfigureCodec(ama_codec_t codec);

#endif /* AMA_H */
