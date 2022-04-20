/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_leakthrough.c
\brief      Kymera implementation to accommodate software leak-through.
*/

#ifdef ENABLE_AEC_LEAKTHROUGH

#include "kymera.h"
#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_va.h"
#include "kymera_aec.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"
#include "kymera_data.h"
#include "kymera_leakthrough.h"

/*The value sidetone_exp[0] and sidetone_mantissa[0] corresponds to value of -46dB and are starting point for leakthrough ramp*/
#define SIDETONE_EXP_MINIMUM sidetone_exp[0]
#define SIDETONE_MANTISSA_MINIMUM sidetone_mantissa[0]

#define AEC_REF_SETTLING_TIME  (100)
#define appConfigLeakthroughMic()                        appConfigMicVoice()
#define MAX_NUM_OF_MICS_SUPPORTED                        (1)
#define appConfigLeakthroughNumMics()                    MAX_NUM_OF_MICS_SUPPORTED

#define LEAKTHROUGH_OUTPUT_RATE  (8000U)
#define ENABLED TRUE
#define DISABLED FALSE

#if defined(INCLUDE_WUW)
    #error The combination of WUW and AEC_LEAKTHROUGH is not yet supported
#endif

static const output_registry_entry_t output_info =
{
    .user = output_user_aec_leakthrough,
    .connection = output_connection_none,
};

/*! The initial value in the Array given below corresponds to -46dB and ramp up is happening till 0dB with increment of 2dB/cycle */
static const unsigned long sidetone_exp[] = {
    0xFFFFFFFAUL, 0xFFFFFFFAUL, 0xFFFFFFFBUL, 0xFFFFFFFBUL,
    0xFFFFFFFBUL, 0xFFFFFFFCUL, 0xFFFFFFFCUL, 0xFFFFFFFCUL,
    0xFFFFFFFDUL, 0xFFFFFFFDUL, 0xFFFFFFFDUL, 0xFFFFFFFEUL,
    0xFFFFFFFEUL, 0xFFFFFFFEUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL,
    0xFFFFFFFFUL, 0x00000000UL, 0x00000000UL, 0x00000000UL,
    0x00000001UL, 0x00000001UL, 0x00000001UL, 0x00000001UL};
static const unsigned long sidetone_mantissa[] = {
    0x290EA879UL, 0x33B02273UL, 0x2089229EUL, 0x28F5C28FUL,
    0x3390CA2BUL, 0x207567A2UL, 0x28DCEBBFUL, 0x337184E6UL,
    0x2061B89DUL, 0x28C423FFUL, 0x33525297UL, 0x204E1588UL,
    0x28AB6B46UL, 0x33333333UL, 0x203A7E5BUL, 0x2892C18BUL,
    0x331426AFUL, 0x2026F310UL, 0x287A26C5UL, 0x32F52CFFUL,
    0x2013739EUL, 0x28619AEAUL, 0x32D64618UL, 0x40000000UL};
static unsigned gain_index = 0;

static bool kymera_LeakthroughMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymera_LeakthroughMicDisconnectIndication(const mic_change_info_t *info);

static const mic_callbacks_t kymera_MicLeakthroughCallbacks =
{
    .MicGetConnectionParameters = kymera_LeakthroughMicGetConnectionParameters,
    .MicDisconnectIndication = kymera_LeakthroughMicDisconnectIndication,
};

static const microphone_number_t kymera_MandatoryLeakthroughMicIds[MAX_NUM_OF_MICS_SUPPORTED] =
{
    appConfigLeakthroughMic(),
};

static const mic_user_state_t kymera_LeakthroughMicState =
{
    mic_user_state_interruptible,
};

static const mic_registry_per_user_t kymera_MicLeakthroughRegistry =
{
    .user = mic_user_leakthrough,
    .callbacks = &kymera_MicLeakthroughCallbacks,
    .mandatory_mic_ids = kymera_MandatoryLeakthroughMicIds,
    .num_of_mandatory_mics = ARRAY_DIM(kymera_MandatoryLeakthroughMicIds),
    .mic_user_state = &kymera_LeakthroughMicState,
};

static void Kymera_LeakthroughUpdateAecOperatorUcid(void);
static void Kymera_LeakthroughMuteDisconnect(void);
static void Kymera_LeakthroughConnect(void);
static void Kymera_LeakthroughUpdateAecOperatorAndSidetone(void);

typedef struct
{
    bool enabled;
    bool output_chain_created;
    aec_usecase_t prepare_aec_usecase;
} leakthrough_state;

leakthrough_state state;

static bool kymera_LeakthroughIsCurrentStepValueLastOne(void)
{
    return (gain_index >= ARRAY_DIM(sidetone_exp));
}

static uint32 kymera_GetLeakthroughMicSampleRate(void)
{
    /* Setting leakthrough mic path sample rate same as speaker path sampling rate */
    uint32 mic_sample_rate = KymeraOutput_GetMainSampleRate();
    if (mic_sample_rate == 0)
    {
        mic_sample_rate = DEFAULT_MIC_RATE;
    }
    return mic_sample_rate;
}

static void kymera_UpdateLeakthroughState(bool enabled, bool output_chain_created)
{
    DEBUG_LOG("kymera_UpdateLeakthroughState: %d output_chain_created %d", enabled, output_chain_created);
    state.enabled = enabled;
    state.output_chain_created = output_chain_created;
}

static void kymera_LeakthroughResetGainIndex(void)
{
    gain_index = 0;
}

static void kymera_SetMinLeakthroughSidetoneGain(void)
{
    Kymera_AecSetSidetoneGain(SIDETONE_EXP_MINIMUM,SIDETONE_MANTISSA_MINIMUM);
}

void Kymera_LeakthroughSetupSTGain(void)
{
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        kymera_SetMinLeakthroughSidetoneGain();
        kymera_LeakthroughResetGainIndex();
        Kymera_AecEnableSidetonePath(TRUE);
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP, NULL, ST_GAIN_RAMP_STEP_TIME_MS);
    }
}

void Kymera_LeakthroughStepupSTGain(void)
{
    if(!kymera_LeakthroughIsCurrentStepValueLastOne())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        Kymera_AecSetSidetoneGain(sidetone_exp[gain_index], sidetone_mantissa[gain_index]);
        gain_index++;
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP, NULL,ST_GAIN_RAMP_STEP_TIME_MS);
    }
    else
    {
        /* End of ramp is achieved reset the gain index */
        kymeraTaskData *theKymera = KymeraGetTaskData();
        MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP);
        kymera_LeakthroughResetGainIndex();
    }
}

static void kymera_PopulateLeakthroughConnectParams(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 num_mics)
{
    PanicZero(mic_ids);
    PanicZero(mic_sinks);
    PanicFalse(num_mics <= appConfigLeakthroughNumMics());

    if ( num_mics > 0 )
    {
        mic_ids[0] = appConfigLeakthroughMic();
        // Leakthrough doesn't use sinks
        mic_sinks[0] = NULL;
    }
}

static bool kymera_LeakthroughMicDisconnectIndication(const mic_change_info_t *info)
{
    UNUSED(info);
    return TRUE;
}

static bool kymera_LeakthroughMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    UNUSED(aec_ref_sink);

    *sample_rate = kymera_GetLeakthroughMicSampleRate();
    *num_of_mics = appConfigLeakthroughNumMics();
    kymera_PopulateLeakthroughConnectParams(mic_ids, mic_sinks, 1);
    return TRUE;
}

static void kymera_DisconnectLeakthroughMic(void)
{
    Kymera_MicDetachLeakthrough(mic_user_leakthrough);
}


static bool kymera_ConnectLeakthroughMic(void)
{
    /* Connect to Mic interface */
    return (Kymera_MicAttachLeakthrough(mic_user_leakthrough));
}

static void kymera_PrepareOutputChain(void)
{
    kymera_output_chain_config config = {0};
    KymeraOutput_SetDefaultOutputChainConfig(&config, LEAKTHROUGH_OUTPUT_RATE, KICK_PERIOD_LEAKTHROUGH, 0);
    PanicFalse(Kymera_OutputPrepare(output_user_aec_leakthrough, &config));
}

void Kymera_CreateLeakthroughChain(void)
{
    DEBUG_LOG("KymeraLeakthrough_CreateChain: Preparing enum:aec_usecase_t:%d", state.prepare_aec_usecase);
    if(!kymera_ConnectLeakthroughMic())
    {
        MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_LEAKTHROUGH,
                         NULL, MIC_CONNECT_RETRY_MS);
        /* Clear state, when interrupted by another non-interruptible client.
         * Prevent setting an outdated aec_usecase at a later point in time */
        state.prepare_aec_usecase = aec_usecase_default;
    }
    else
    {
        if(state.prepare_aec_usecase != aec_usecase_default)
        {
            Kymera_SetAecUseCase(state.prepare_aec_usecase);
        }
        if(!Kymera_OutputIsChainInUse())
        {
            appKymeraSetState(KYMERA_STATE_STANDALONE_LEAKTHROUGH);
            kymera_PrepareOutputChain();
            KymeraOutput_ChainStart();
            kymera_UpdateLeakthroughState(ENABLED, ENABLED);
        }
        else
        {
            kymera_UpdateLeakthroughState(ENABLED, DISABLED);
        }
        appKymeraConfigureDspPowerMode();
        Kymera_LeakthroughUpdateAecOperatorAndSidetone();
    }
}

static bool kymera_IsLeakthroughOutputChainCreated(void)
{
    DEBUG_LOG("kymera_IsLeakthroughOutputChainCreated: %d", state.output_chain_created);
    return state.output_chain_created;
}

void Kymera_DestroyLeakthroughChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("Kymera_DestroyLeakthroughChain");

    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP);
    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE);

    Kymera_LeakthroughMuteDisconnect();

    if(kymera_IsLeakthroughOutputChainCreated())
    {
        Kymera_OutputDisconnect(output_user_aec_leakthrough);
        appKymeraSetState(KYMERA_STATE_IDLE);
    }
    kymera_UpdateLeakthroughState(DISABLED, DISABLED);
}

static void Kymera_LeakthroughEnableAecSideToneAfterTimeout(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE);
    MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_ENABLE, NULL, AEC_REF_SETTLING_TIME);
}

bool Kymera_IsLeakthroughActive(void)
{
    DEBUG_LOG("Kymera_IsLeakthroughActive: %d", state.enabled);
    return state.enabled;
}

void Kymera_LeakthroughStopChainIfRunning(void)
{
    if(Kymera_IsLeakthroughActive())
    {
        Kymera_DestroyLeakthroughChain();
    }
}

void Kymera_LeakthroughResumeChainIfSuspended(void)
{
    DEBUG_LOG("Kymera_LeakthroughResumeChainIfSuspended: enum:appKymeraState:%d", appKymeraGetState());
    if(AecLeakthrough_IsLeakthroughEnabled() && (appKymeraGetState() == KYMERA_STATE_IDLE))
    {
        state.prepare_aec_usecase = aec_usecase_enable_leakthrough;
        Kymera_CreateLeakthroughChain();
        appKymeraSetState(KYMERA_STATE_STANDALONE_LEAKTHROUGH);
    }
}

static void Kymera_LeakthroughUpdateAecOperatorUcid(void)
{
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        uint8 ucid;
        Operator aec_ref = Kymera_GetAecOperator();
        if (aec_ref != INVALID_OPERATOR)
        {
            ucid = Kymera_GetAecUcid();
            DEBUG_LOG("Kymera_LeakthroughUpdateAecOperatorUcid: enum:kymera_operator_ucid_t:%d", ucid);
            OperatorsStandardSetUCID(aec_ref,ucid);
        }
    }
}

void Kymera_EnableLeakthrough(void)
{
    DEBUG_LOG("Kymera_EnableLeakthrough");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(Kymera_IsVaActive())
    {
        Kymera_LeakthroughConnect();
    }
    else
    {
        switch(appKymeraGetState())
        {
            case KYMERA_STATE_IDLE:
            case KYMERA_STATE_TONE_PLAYING:
                MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_CREATE_STANDALONE_CHAIN, NULL, &theKymera->lock);
                break;

            case KYMERA_STATE_A2DP_STREAMING:
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
                Kymera_LeakthroughConnect();
                break;

            case KYMERA_STATE_SCO_ACTIVE:
            case KYMERA_STATE_SCO_SLAVE_ACTIVE:
            case KYMERA_STATE_LE_AUDIO_ACTIVE:
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;

            default:
                break;
        }
    }
}

void Kymera_LeakthroughSetAecUseCase(aec_usecase_t usecase)
{
    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        switch ( usecase ) {
            case aec_usecase_default:
                Kymera_LeakthroughMuteDisconnect();
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;
            case aec_usecase_create_leakthrough_chain:
                Kymera_LeakthroughConnect();
                break;
            default:
                Kymera_LeakthroughUpdateAecOperatorAndSidetone();
                break;
        }
    }
}

void Kymera_DisableLeakthrough(void)
{
    DEBUG_LOG("Kymera_DisableLeakthrough");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* cancel all the pending messsage used for leakthrough ramp */
    MessageCancelAll(&theKymera->task,KYMERA_INTERNAL_AEC_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP);

    /* Reset the gain index used for leakthrough ramp*/
    kymera_LeakthroughResetGainIndex();

    if(Kymera_IsVaActive())
    {
        Kymera_LeakthroughMuteDisconnect();
        Kymera_LeakthroughUpdateAecOperatorUcid();
    }
    else
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
                MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_AEC_LEAKTHROUGH_DESTROY_STANDALONE_CHAIN, NULL, &theKymera->lock);
                break;

            case KYMERA_STATE_A2DP_STREAMING:
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
                Kymera_LeakthroughMuteDisconnect();
                Kymera_LeakthroughUpdateAecOperatorUcid();
                break;

            case KYMERA_STATE_SCO_ACTIVE:
            case KYMERA_STATE_SCO_SLAVE_ACTIVE:
            case KYMERA_STATE_LE_AUDIO_ACTIVE:
                Kymera_LeakthroughMuteDisconnect();
                Kymera_LeakthroughUpdateAecOperatorUcid();
                break;

            default:
                break;
        }
    }
}

void Kymera_LeakthroughUpdateMode(leakthrough_mode_t mode)
{
    UNUSED(mode);
    Kymera_LeakthroughUpdateAecOperatorAndSidetone();
}

static void Kymera_LeakthroughMuteDisconnect(void)
{
    kymera_SetMinLeakthroughSidetoneGain();
    Kymera_AecEnableSidetonePath(FALSE);
    kymera_DisconnectLeakthroughMic();
}

static void Kymera_LeakthroughConnect(void)
{
    state.prepare_aec_usecase = aec_usecase_enable_leakthrough;
    Kymera_CreateLeakthroughChain();
}

static void Kymera_LeakthroughUpdateAecOperatorAndSidetone(void)
{
    Kymera_LeakthroughUpdateAecOperatorUcid();
    Kymera_LeakthroughEnableAecSideToneAfterTimeout();
}

void Kymera_LeakthroughInit(void)
{
    state.enabled = DISABLED;
    state.output_chain_created = DISABLED;
    Kymera_OutputRegister(&output_info);
    Kymera_MicRegisterUser(&kymera_MicLeakthroughRegistry);
}

#endif  /* ENABLE_AEC_LEAKTHROUGH */
