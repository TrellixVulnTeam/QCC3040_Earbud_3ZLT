/*!
\copyright  Copyright (c) 2020 -2021  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      loop back from Mic input to DAC chain
*/

#include "kymera_loopback_audio.h"
#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera.h"
#include "kymera_output_if.h"


static const output_registry_entry_t output_info =
{
    .user = output_user_loopback,
    .connection = output_connection_aux,
};

static void kymera_PrepareOutputChain(uint16 sample_rate)
{
    kymera_output_chain_config config = {0};
    KymeraOutput_SetDefaultOutputChainConfig(&config, sample_rate, KICK_PERIOD_SLOW, 0);
    PanicFalse(Kymera_OutputPrepare(output_user_loopback, &config));
    KymeraOutput_SetAuxVolume(0);
    KymeraOutput_ChainStart();
}

static void kymera_ConnectToOutputChain(Source source)
{
    output_source_t output = {0};
    output.aux = source;
    PanicFalse(Kymera_OutputConnect(output_user_loopback, &output));
}

void appKymeraCreateLoopBackAudioChain(microphone_number_t mic_number, uint32 sample_rate)
{
    DEBUG_LOG_FN_ENTRY("appKymeraCreateLoopBackAudioChain, mic %u, sample rate %u ",
                        mic_number, sample_rate);

    if (Kymera_IsIdle())
    {
        OperatorsFrameworkEnable();
        Source mic = Kymera_GetMicrophoneSource(mic_number, NULL, sample_rate, high_priority_user);
        kymera_PrepareOutputChain(sample_rate);
        appKymeraSetState(KYMERA_STATE_MIC_LOOPBACK);
        kymera_ConnectToOutputChain(mic);
    }
}

void appKymeraDestroyLoopbackAudioChain(microphone_number_t mic_number)
{
    DEBUG_LOG_FN_ENTRY("appKymeraDestroyLoopbackAudioChain, mic %u", mic_number);

    if (appKymeraGetState() == KYMERA_STATE_MIC_LOOPBACK)
    {
        KymeraOutput_SetAuxVolume(0);
        Kymera_CloseMicrophone(mic_number, high_priority_user);
        Kymera_OutputDisconnect(output_user_loopback);
        appKymeraSetState(KYMERA_STATE_IDLE);
        OperatorsFrameworkDisable();
    }
}

void appKymeraLoopbackInit(void)
{
    Kymera_OutputRegister(&output_info);
}
