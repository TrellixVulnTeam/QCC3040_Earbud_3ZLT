/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to handle VA mic chain

*/

#ifndef KYMERA_VA_MIC_CHAIN_H_
#define KYMERA_VA_MIC_CHAIN_H_

#include "kymera.h"
#include "kymera_mic_if.h"
#include <sink.h>
#include <source.h>
#include <operator.h>

#define MAX_NUM_OF_MICS_SUPPORTED (2)

/*! \brief Parameters used to configure the VA mic chain operators */
typedef struct
{
    /*! Only used when WuW detection is enabled, max milliseconds of audio to buffer */
    uint16 max_pre_roll_in_ms;
} va_mic_chain_op_params_t;

/*! \brief Parameters used to create the VA mic chain */
typedef struct
{
    kymera_va_mic_chain_params_t chain_params;
    va_mic_chain_op_params_t operators_params;
} va_mic_chain_create_params_t;

/*! \brief Parameters used to connect to the VA mic chain */
typedef struct
{
    Sink capture_output;
    Sink detection_output;
} va_mic_chain_connect_params_t;

/*! \param params Parameters used to create/configure the chain.
*/
void Kymera_CreateVaMicChain(const va_mic_chain_create_params_t *params);
void Kymera_DestroyVaMicChain(void);

/*! \param params Parameters used to connect to the chain.
*/
void Kymera_ConnectToVaMicChain(const va_mic_chain_connect_params_t *params);

void Kymera_StartVaMicChain(void);
void Kymera_StopVaMicChain(void);

void Kymera_VaMicChainSleep(void);
void Kymera_VaMicChainWake(void);

void Kymera_VaMicChainStartGraphManagerDelegation(Operator graph_manager, Operator wuw_engine);
void Kymera_VaMicChainStopGraphManagerDelegation(Operator graph_manager, Operator wuw_engine);

/*! \param start_timestamp Timestamp from which to start the stream.
*/
void Kymera_ActivateVaMicChainEncodeOutputAfterTimestamp(uint32 start_timestamp);
void Kymera_ActivateVaMicChainEncodeOutput(void);
void Kymera_DeactivateVaMicChainEncodeOutput(void);
/*! \brief Start buffering mic data for encode output.
*/
void Kymera_BufferVaMicChainEncodeOutput(void);

void Kymera_ActivateVaMicChainWuwOutput(void);
void Kymera_DeactivateVaMicChainWuwOutput(void);

Source Kymera_GetVaMicChainEncodeOutput(void);
Source Kymera_GetVaMicChainWuwOutput(void);

bool Kymera_IsVaMicChainSupported(const kymera_va_mic_chain_params_t *params);

bool Kymera_GetVaMicChainMicConnectionParams(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);

#endif /* KYMERA_VA_MIC_CHAIN_H_ */
