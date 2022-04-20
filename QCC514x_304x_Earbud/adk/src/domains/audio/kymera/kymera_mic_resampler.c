/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage MIC resampler chain used for MIC concurrency
*/

#include "kymera_mic_resampler.h"
#include "kymera_mic_if.h"
#include "kymera_chain_roles.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <chain.h>

static const struct
{
    chain_endpoint_role_t aec_input_role;
    chain_endpoint_role_t aec_output_role;
    chain_endpoint_role_t mic_input_role[MAX_NUM_OF_CONCURRENT_MICS];
    chain_endpoint_role_t mic_output_role[MAX_NUM_OF_CONCURRENT_MICS];
} resampler_endpoints_map =
{
    EPR_AEC_RESAMPLER_IN_REF,
    EPR_AEC_RESAMPLER_OUT_REF,
    {EPR_MIC_RESAMPLER_IN1, EPR_MIC_RESAMPLER_IN2, EPR_MIC_RESAMPLER_IN3},
    {EPR_MIC_RESAMPLER_OUT1, EPR_MIC_RESAMPLER_OUT2, EPR_MIC_RESAMPLER_OUT3},
};

static struct
{
    kymera_chain_handle_t chain;
} resamplers[MAX_NUM_OF_CONCURRENT_MIC_USERS] = {0};

static Operator kymera_GetResamplerMicChainOperator(uint8 stream_index, unsigned operator_role)
{
    Operator result = ChainGetOperatorByRole(resamplers[stream_index].chain, operator_role);
    return result;
}

static void kymera_ConfigureResampler(uint8 stream_index, uint32 input_sample_rate, uint32 output_sample_rate)
{
    Operator op = kymera_GetResamplerMicChainOperator(stream_index, OPR_MIC_RESAMPLER);
    OperatorsConfigureResampler(op, input_sample_rate, output_sample_rate);

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureMicResamplerChain)
    {
        kymera_mic_resmapler_config_params_t params = {0};
        params.input_sample_rate = input_sample_rate;
        params.output_sample_rate = output_sample_rate;
        KymeraGetTaskData()->chain_config_callbacks->ConfigureMicResamplerChain(resamplers[stream_index].chain, &params);
    }
}

void Kymera_MicResamplerCreate(uint8 stream_index, uint32 input_sample_rate, uint32 output_sample_rate)
{
    PanicFalse(Kymera_MicResamplerIsCreated(stream_index) == FALSE);
    DEBUG_LOG("Kymera_MicResamplerCreate: stream_index %u input_sample_rate %u output_sample_rate %u",
              stream_index, input_sample_rate, output_sample_rate);
    resamplers[stream_index].chain = PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_mic_resampler_config));
    kymera_ConfigureResampler(stream_index, input_sample_rate, output_sample_rate);
    ChainConnect(resamplers[stream_index].chain);
}

void Kymera_MicResamplerDestroy(uint8 stream_index)
{
    kymera_chain_handle_t chain = resamplers[stream_index].chain;
    unsigned i;

    PanicFalse(Kymera_MicResamplerIsCreated(stream_index));
    DEBUG_LOG("Kymera_MicResamplerDestroy: stream_index %u",stream_index);
    StreamDisconnect(NULL, Kymera_MicResamplerGetAecInput(stream_index));
    StreamDisconnect(Kymera_MicResamplerGetAecOutput(stream_index), NULL);
    for(i = 0; i < MAX_NUM_OF_CONCURRENT_MICS; i++)
    {
        StreamDisconnect(NULL, Kymera_MicResamplerGetMicInput(stream_index, i));
        StreamDisconnect(Kymera_MicResamplerGetMicOutput(stream_index, i), NULL);
    }
    ChainDestroy(chain);
    resamplers[stream_index].chain = NULL;
}

void Kymera_MicResamplerStart(uint8 stream_index)
{
    DEBUG_LOG("Kymera_MicResamplerStart: stream_index %u",stream_index);
    ChainStart(resamplers[stream_index].chain);
}

void Kymera_MicResamplerStop(uint8 stream_index)
{
    DEBUG_LOG("Kymera_MicResamplerStop: stream_index %u",stream_index);
    ChainStop(resamplers[stream_index].chain);
}

bool Kymera_MicResamplerIsCreated(uint8 stream_index)
{
    bool result = resamplers[stream_index].chain != NULL;
    return result;
}

Sink Kymera_MicResamplerGetAecInput(uint8 stream_index)
{
    Sink result = ChainGetInput(resamplers[stream_index].chain,
                                resampler_endpoints_map.aec_input_role);
    return result;
}

Source Kymera_MicResamplerGetAecOutput(uint8 stream_index)
{
    Source result = ChainGetOutput(resamplers[stream_index].chain,
                                   resampler_endpoints_map.aec_output_role);
    return result;
}

Sink Kymera_MicResamplerGetMicInput(uint8 stream_index, uint8 mic_index)
{
    Sink result = ChainGetInput(resamplers[stream_index].chain,
                                resampler_endpoints_map.mic_input_role[mic_index]);
    return result;
}

Source Kymera_MicResamplerGetMicOutput(uint8 stream_index, uint8 mic_index)
{
    Source result = ChainGetOutput(resamplers[stream_index].chain,
                                   resampler_endpoints_map.mic_output_role[mic_index]);
    return result;
}

void Kymera_MicResamplerSleep(void)
{
    unsigned i;

    for(i = 0; i < ARRAY_DIM(resamplers); i++)
    {
        if (resamplers[i].chain)
        {
            ChainSleep(resamplers[i].chain, NULL);
        }
    }
}

void Kymera_MicResamplerWake(void)
{
    unsigned i;

    for(i = 0; i < ARRAY_DIM(resamplers); i++)
    {
        if (resamplers[i].chain)
        {
            ChainWake(resamplers[i].chain, NULL);
        }
    }
}
