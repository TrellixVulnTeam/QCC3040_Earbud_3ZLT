/*!
\copyright  Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_sco.c
\brief      Kymera SCO
*/

#include "kymera_sco_private.h"
#include "kymera_state.h"
#include "kymera_config.h"
#include "kymera_common.h"
#include "kymera_tones_prompts.h"
#include "kymera_aec.h"
#include "kymera_adaptive_anc.h"
#include "kymera_leakthrough.h"
#include "kymera_output.h"
#include "av.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <vmal.h>
#include <anc_state_manager.h>
#include <timestamp_event.h>
#include <opmsg_prim.h>

#if defined(KYMERA_SCO_USE_3MIC)
#define MAX_NUM_OF_MICS_SUPPORTED (3)
#elif defined (KYMERA_SCO_USE_2MIC)
#define MAX_NUM_OF_MICS_SUPPORTED (2)
#else
#define MAX_NUM_OF_MICS_SUPPORTED (1)
#endif

#define AWBSDEC_SET_BITPOOL_VALUE    0x0003
#define AWBSENC_SET_BITPOOL_VALUE    0x0001

#ifdef ENABLE_ADAPTIVE_ANC
#define AEC_TX_BUFFER_SIZE_MS 45
#else
#define AEC_TX_BUFFER_SIZE_MS 15
#endif

typedef struct set_bitpool_msg_s
{
    uint16 id;
    uint16 bitpool;
}set_bitpool_msg_t;

#ifdef INCLUDE_CVC_DEMO
typedef struct
{
    uint8 mic_config;
    kymera_cvc_mode_t mode;
    uint8 passthrough_mic;
    uint8 mode_of_operation;

} cvc_send_settings_t;

static cvc_send_settings_t cvc_send_settings = {0};

static void kymera_ScoSetCvc3MicSettings(void);
#endif /* INCLUDE_CVC_DEMO */

static kymera_chain_handle_t the_sco_chain;

static bool kymera_ScoMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymera_ScoMicDisconnectIndication(const mic_change_info_t *info);

static const mic_callbacks_t kymera_MicScoCallbacks =
{
    .MicGetConnectionParameters = kymera_ScoMicGetConnectionParameters,
    .MicDisconnectIndication = kymera_ScoMicDisconnectIndication,
};

static const microphone_number_t kymera_ScoMandatoryMicIds[MAX_NUM_OF_MICS_SUPPORTED] =
{
    microphone_none,
};

static mic_user_state_t kymera_ScoMicState =
{
    mic_user_state_non_interruptible,
};

static const mic_registry_per_user_t kymera_MicScoRegistry =
{
    .user = mic_user_sco,
    .callbacks = &kymera_MicScoCallbacks,
    .mandatory_mic_ids = &kymera_ScoMandatoryMicIds[0],
    .num_of_mandatory_mics = 0,
    .mic_user_state = &kymera_ScoMicState,
};

static const output_registry_entry_t output_info =
{
    .user = output_user_sco,
    .connection = output_connection_mono,
};

static void kymera_DestroyScoChain(void)
{
    ChainDestroy(the_sco_chain);
    the_sco_chain = NULL;

    /* Update state variables */
    appKymeraSetState(KYMERA_STATE_IDLE);

    Kymera_LeakthroughResumeChainIfSuspended();
}

static kymera_chain_handle_t kymera_GetScoChain(void)
{
    return the_sco_chain;
}

static void kymera_ConfigureScoChain(uint16 wesco)
{
    kymera_chain_handle_t sco_chain = kymera_GetScoChain();
    kymeraTaskData *theKymera = KymeraGetTaskData();
    PanicNull((void *)theKymera->sco_info);
    
    const uint32_t rate = theKymera->sco_info->rate;

    /*! \todo Need to decide ahead of time if we need any latency.
        Simple enough to do if we are legacy or not. Less clear if
        legacy but no peer connection */
    /* Enable Time To Play if supported */
    if (appConfigScoChainTTP(wesco) != 0)
    {
        Operator sco_op = PanicZero(ChainGetOperatorByRole(sco_chain, OPR_SCO_RECEIVE));
        OperatorsStandardSetTimeToPlayLatency(sco_op, appConfigScoChainTTP(wesco));
        OperatorsStandardSetBufferSize(sco_op, appConfigScoBufferSize(rate));
    }

    Kymera_SetVoiceUcids(sco_chain);

    if(theKymera->chain_config_callbacks && theKymera->chain_config_callbacks->ConfigureScoInputChain)
    {
        kymera_sco_config_params_t params = {0};
        params.sample_rate = theKymera->sco_info->rate;
        params.mode = theKymera->sco_info->mode;
        params.wesco = wesco;
        theKymera->chain_config_callbacks->ConfigureScoInputChain(sco_chain, &params);
    }
}

static void kymera_PopulateScoConnectParams(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 num_mics, Sink *aec_ref_sink)
{
    kymera_chain_handle_t sco_chain = kymera_GetScoChain();
    PanicZero(mic_ids);
    PanicZero(mic_sinks);
    PanicZero(aec_ref_sink);
    PanicFalse(num_mics <= 3);

    mic_ids[0] = appConfigMicVoice();
    mic_sinks[0] = ChainGetInput(sco_chain, EPR_CVC_SEND_IN1);
    if(num_mics > 1)
    {
        mic_ids[1] = appConfigMicExternal();
        mic_sinks[1] = ChainGetInput(sco_chain, EPR_CVC_SEND_IN2);
    }
    if(num_mics > 2)
    {
        mic_ids[2] = appConfigMicInternal();
        mic_sinks[2] = ChainGetInput(sco_chain, EPR_CVC_SEND_IN3);
    }
    aec_ref_sink[0] = ChainGetInput(sco_chain, EPR_CVC_SEND_REF_IN);
}

static bool kymera_ScoMicDisconnectIndication(const mic_change_info_t *info)
{
    UNUSED(info);
    DEBUG_LOG_ERROR("kymera_ScoMicDisconnectIndication: SCO shouldn't have to get disconnected");
    Panic();
    return TRUE;
}

static bool kymera_ScoMicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("kymera_ScoMicGetConnectionParameters");

    *sample_rate = theKymera->sco_info->rate;
    *num_of_mics = Kymera_GetNumberOfMics();
    kymera_PopulateScoConnectParams(mic_ids, mic_sinks, theKymera->sco_info->mic_cfg, aec_ref_sink);
    return TRUE;
}

static kymera_chain_handle_t kymera_ScoCreateChain(const appKymeraScoChainInfo *info)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraCreateScoChain, mode %u, mic_cfg %u, rate %u",
               info->mode, info->mic_cfg, info->rate);

    theKymera->sco_info = info;
    return ChainCreate(info->chain);
}

bool appKymeraHandleInternalScoStart(Sink sco_snk, const appKymeraScoChainInfo *info,
                                     uint8 wesco, int16 volume_in_db, bool synchronised_start)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    bool started = FALSE;

    DEBUG_LOG("appKymeraHandleInternalScoStart, sink 0x%x, mode %u, wesco %u, state %u", sco_snk, info->mode, wesco, appKymeraGetState());

    /* If there is a tone still playing at this point,
     * it must be an interruptible tone, so cut it off */
    appKymeraTonePromptStop();

    Kymera_LeakthroughStopChainIfRunning();

    /* Can't start voice chain if we're not idle */
    PanicFalse((appKymeraGetState() == KYMERA_STATE_IDLE) || (appKymeraGetState() == KYMERA_STATE_ADAPTIVE_ANC_STARTED));

    /* SCO chain must be destroyed if we get here */
    PanicNotNull(kymera_GetScoChain());

    /* Move to SCO active state now, what ever happens we end up in this state
      (even if it's temporary) */
    appKymeraSetState(KYMERA_STATE_SCO_ACTIVE);

    /* Boost the audio CPU clock to reduce chain setup time */
    appKymeraSetActiveDspClock(AUDIO_DSP_TURBO_PLUS_CLOCK);

    /* Create appropriate SCO chain */
    the_sco_chain = kymera_ScoCreateChain(info);
    kymera_chain_handle_t sco_chain = PanicNull(kymera_GetScoChain());

    /* Connect to Mic interface */
    if (!Kymera_MicConnect(mic_user_sco))
    {
        DEBUG_LOG_ERROR("appKymeraHandleInternalScoStart: Mic connection was not successful. Sco should always be prepared.");
        Panic();
    }    

    /* Configure chain specific operators */
    kymera_ConfigureScoChain(wesco);

    /* Create an appropriate Output chain */
    kymera_output_chain_config output_config;
    KymeraOutput_SetDefaultOutputChainConfig(&output_config, theKymera->sco_info->rate, KICK_PERIOD_VOICE, 0);

    output_config.chain_type = output_chain_mono;
    output_config.chain_include_aec = TRUE;
    PanicFalse(Kymera_OutputPrepare(output_user_sco, &output_config));

    /* Get sources and sinks for chain endpoints */
    Source sco_ep_src = ChainGetOutput(sco_chain, EPR_SCO_TO_AIR);
    Sink sco_ep_snk = ChainGetInput(sco_chain, EPR_SCO_FROM_AIR);

    /* Connect SCO to chain SCO endpoints */
    /* Get SCO source from SCO sink */
    Source sco_src = StreamSourceFromSink(sco_snk);

    StreamConnect(sco_ep_src, sco_snk);
    StreamConnect(sco_src, sco_ep_snk);
    
    /* Connect chain */
    ChainConnect(sco_chain);
 
    /* Connect to the Ouput chain */
    output_source_t sources = {.mono = ChainGetOutput(sco_chain, EPR_SCO_SPEAKER)};
    PanicFalse(Kymera_OutputConnect(output_user_sco, &sources));
    if(synchronised_start)
    {
        KymeraOutput_MuteMainChannel(TRUE);
    }
    KymeraOutput_ChainStart();

    /* The chain can fail to start if the SCO source disconnects whilst kymera
    is queuing the SCO start request or starting the chain. If the attempt fails,
    ChainStartAttempt will stop (but not destroy) any operators it started in the chain. */
    if (ChainStartAttempt(sco_chain))
    {
        TimestampEvent(TIMESTAMP_EVENT_SCO_MIC_STREAM_STARTED);

        appKymeraHandleInternalScoSetVolume(volume_in_db);
        
        if(theKymera->enable_cvc_passthrough)
        {
            Kymera_ScoSetCvcPassthroughMode(KYMERA_CVC_RECEIVE_PASSTHROUGH | KYMERA_CVC_SEND_PASSTHROUGH, 0);
        }
#ifdef INCLUDE_CVC_DEMO
        kymera_ScoSetCvc3MicSettings();
#endif

        Kymera_LeakthroughSetAecUseCase(aec_usecase_enable_leakthrough);

        started = TRUE;
    }
    else
    {
        DEBUG_LOG("appKymeraHandleInternalScoStart, could not start chain");
        /* Stop/destroy the chain, returning state to KYMERA_STATE_IDLE.
        This needs to be done here, since between the failed attempt to start
        and the subsequent stop (when appKymeraScoStop() is called), a tone
        may need to be played - it would not be possible to play a tone in a
        stopped SCO chain. The state needs to be KYMERA_STATE_SCO_ACTIVE for
        appKymeraHandleInternalScoStop() to stop/destroy the chain. */
        appKymeraHandleInternalScoStop();
    }

    /* Configure DSP power mode appropriately for SCO chain */
    appKymeraConfigureDspPowerMode();

    return started;
}

void appKymeraHandleInternalScoStop(void)
{
    DEBUG_LOG("appKymeraHandleInternalScoStop, state %u", appKymeraGetState());

    /* Get current SCO chain */
    kymera_chain_handle_t sco_chain = kymera_GetScoChain();
    if (appKymeraGetState() != KYMERA_STATE_SCO_ACTIVE)
    {
        if (!sco_chain)        
        {
            /* Attempting to stop a SCO chain when not ACTIVE. This happens when the user
            calls appKymeraScoStop() following a failed attempt to start the SCO
            chain - see ChainStartAttempt() in appKymeraHandleInternalScoStart().
            In this case, there is nothing to do, since the failed start attempt
            cleans up by calling this function in state KYMERA_STATE_SCO_ACTIVE */
            DEBUG_LOG("appKymeraHandleInternalScoStop, not stopping - already idle");
            return;
        }
        else
        {
            Panic();
        }
    }
    
    Source sco_ep_src = ChainGetOutput(sco_chain, EPR_SCO_TO_AIR);
    Sink sco_ep_snk = ChainGetInput(sco_chain, EPR_SCO_FROM_AIR);

    /* Disable AEC_REF sidetone path */
    Kymera_AecEnableSidetonePath(FALSE);

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains */
    ChainStop(sco_chain);

    /* Disconnect SCO from chain SCO endpoints */
    StreamDisconnect(sco_ep_src, NULL);
    StreamDisconnect(NULL, sco_ep_snk);
   
    Kymera_MicDisconnect(mic_user_sco);

    Kymera_OutputDisconnect(output_user_sco);

    /* Destroy chains */
    kymera_DestroyScoChain();
    sco_chain = NULL;
}

void appKymeraHandleInternalScoSetVolume(int16 volume_in_db)
{
    DEBUG_LOG("appKymeraHandleInternalScoSetVolume, vol %d", volume_in_db);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        {
            KymeraOutput_SetMainVolume(volume_in_db);
        }
        break;
        default:
            break;
    }
}

void appKymeraHandleInternalScoMicMute(bool mute)
{
    DEBUG_LOG("appKymeraHandleInternalScoMicMute, mute %u", mute);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        {
            Operator cvc_send_op;
            if (GET_OP_FROM_CHAIN(cvc_send_op, kymera_GetScoChain(), OPR_CVC_SEND))
            {
                OperatorsStandardSetControl(cvc_send_op, cvc_send_mute_control, mute);
            }
            else
            {
                /* This is just in case fall-back when CVC send is not present,
                   otherwise input mute should be applied in CVC send operator. */
                Operator aec_op = Kymera_GetAecOperator();
                if (aec_op != INVALID_OPERATOR)
                {
                    OperatorsAecMuteMicOutput(aec_op, mute);
                }
            }
        }
        break;

        default:
            break;
    }
}

uint8 appKymeraScoVoiceQuality(void)
{
    uint8 quality = appConfigVoiceQualityWorst();

    if (appConfigVoiceQualityMeasurementEnabled())
    {
        Operator cvc_send_op;
        if (GET_OP_FROM_CHAIN(cvc_send_op, kymera_GetScoChain(), OPR_CVC_SEND))
        {
            uint16 rx_msg[2], tx_msg = OPMSG_COMMON_GET_VOICE_QUALITY;
            PanicFalse(OperatorMessage(cvc_send_op, &tx_msg, 1, rx_msg, 2));
            quality = MIN(appConfigVoiceQualityBest() , rx_msg[1]);
            quality = MAX(appConfigVoiceQualityWorst(), quality);
        }
    }
    else
    {
        quality = appConfigVoiceQualityWhenDisabled();
    }

    DEBUG_LOG("appKymeraScoVoiceQuality %u", quality);

    return quality;
}

void Kymera_ScoInit(void)
{
    Kymera_OutputRegister(&output_info);
    Kymera_MicRegisterUser(&kymera_MicScoRegistry);
#ifdef INCLUDE_CVC_DEMO
    cvc_send_settings.mic_config = 3;
#endif
}

#define CONTROL_MODE_FULL_PROCESSING          2
#define CONTROL_MODE_CVC_RCV_PASSTHROUGH_MIC1 3
#define CONTROL_MODE_CVC_SND_PASSTHROUGH_MIC1 4

void Kymera_ScoSetCvcPassthroughInChain(kymera_chain_handle_t chain_containing_cvc,
                                        kymera_cvc_mode_t mode,
                                        uint8 passthrough_mic)
{
    if(chain_containing_cvc != NULL)
    {
        Operator cvc_snd_op;
        Operator cvc_rcv_op;

        if(mode & KYMERA_CVC_SEND_PASSTHROUGH)
        {
            PanicFalse(GET_OP_FROM_CHAIN(cvc_snd_op, chain_containing_cvc, OPR_CVC_SEND));
            OperatorsStandardSetControl(cvc_snd_op, OPMSG_CONTROL_MODE_ID,
                                        CONTROL_MODE_CVC_SND_PASSTHROUGH_MIC1 + passthrough_mic);
        }
        else if(mode & KYMERA_CVC_SEND_FULL_PROCESSING)
        {
            PanicFalse(GET_OP_FROM_CHAIN(cvc_snd_op, chain_containing_cvc, OPR_CVC_SEND));
            OperatorsStandardSetControl(cvc_snd_op, OPMSG_CONTROL_MODE_ID, CONTROL_MODE_FULL_PROCESSING);
        }

        if(mode & KYMERA_CVC_RECEIVE_PASSTHROUGH)
        {
            PanicFalse(GET_OP_FROM_CHAIN(cvc_rcv_op, chain_containing_cvc, OPR_CVC_RECEIVE));
            OperatorsStandardSetControl(cvc_rcv_op, OPMSG_CONTROL_MODE_ID,
                                        CONTROL_MODE_CVC_RCV_PASSTHROUGH_MIC1 + passthrough_mic);
        }
        else if(mode & KYMERA_CVC_RECEIVE_FULL_PROCESSING)
        {
            PanicFalse(GET_OP_FROM_CHAIN(cvc_rcv_op, chain_containing_cvc, OPR_CVC_RECEIVE));
            OperatorsStandardSetControl(cvc_rcv_op, OPMSG_CONTROL_MODE_ID, CONTROL_MODE_FULL_PROCESSING);
        }
    }
}

bool Kymera_ScoSetCvcPassthroughMode(kymera_cvc_mode_t mode, uint8 passthrough_mic)
{
    bool setting_changed = FALSE;
#ifdef INCLUDE_CVC_DEMO
    if((cvc_send_settings.mode != mode) || (cvc_send_settings.passthrough_mic != passthrough_mic))
    {
        setting_changed = TRUE;
    }
    cvc_send_settings.mode = mode;
    cvc_send_settings.passthrough_mic = passthrough_mic;
#endif
    if(mode == (KYMERA_CVC_RECEIVE_PASSTHROUGH | KYMERA_CVC_SEND_PASSTHROUGH))
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        theKymera->enable_cvc_passthrough = 1;
    }

    if( appKymeraGetState() == KYMERA_STATE_SCO_ACTIVE )
    {
        kymera_chain_handle_t sco_chain = kymera_GetScoChain();
        Kymera_ScoSetCvcPassthroughInChain(sco_chain, mode, passthrough_mic);
        DEBUG_LOG("Kymera_ScoSetCvcPassthroughMode: mode enum:kymera_cvc_mode_t:%d passthrough mic %d", mode, passthrough_mic);
    }
    else
    {
        DEBUG_LOG("Kymera_ScoSetCvcPassthroughMode: Storing mode enum:kymera_cvc_mode_t:%d passthrough_mic %d for next SCO call",
                  mode, passthrough_mic);
    }
    return setting_changed;
}

get_status_data_t* Kymera_GetOperatorStatusDataInScoChain(unsigned operator_role, uint8 number_of_params)
{
    get_status_data_t* get_status = NULL;

    if( appKymeraGetState() == KYMERA_STATE_SCO_ACTIVE )
    {

        Operator op = ChainGetOperatorByRole(the_sco_chain, operator_role);
        get_status = OperatorsCreateGetStatusData(number_of_params);
        OperatorsGetStatus(op, get_status);
    }
    else
    {
        DEBUG_LOG("Kymera_GetOperatorStatusDataInScoChain:Sco Not active yet");
    }
    return get_status;
}

#ifdef INCLUDE_CVC_DEMO
#define CAP_ID_DOWNLOAD_CVCEB3MIC_NB           0x40A9
#define CAP_ID_DOWNLOAD_CVCEB3MIC_WB           0x40A5
#define CAP_ID_DOWNLOAD_CVCEB3MIC_SWB          0x40B3
#define CAP_ID_DOWNLOAD_CVCHS3MIC_MONO_SEND_FB 0x4038
#define CAP_ID_CVCEB3MIC_NB                    0x0086
#define CAP_ID_CVCEB3MIC_WB                    0x0083
#define CAP_ID_CVCHS3MIC_MONO_SEND_FB          0x0067

#define CVC_3_MIC_SET_OCCLUDED_MODE       2
#define CVC_3_MIC_SET_EXTERNAL_MIC_MODE   0
#define CVC_3_MIC_BYP_NPC_MASK          0x8

#define CVC_3_MIC_NUM_STATUS_VAR         34
#define CVC_3_MIC_THREE_MIC_FLAG_OFFSET  31

static bool kymera_Is3MicCvcInScoChain(kymera_chain_handle_t chain_containing_cvc)
{
    bool cvc_3mic_found = FALSE;
    if(chain_containing_cvc != NULL)
    {
        cvc_3mic_found = ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_DOWNLOAD_CVCEB3MIC_NB) |
                         ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_DOWNLOAD_CVCEB3MIC_WB) |
                         ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_DOWNLOAD_CVCEB3MIC_SWB) |
                         ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_CVCEB3MIC_NB) |
                         ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_CVCEB3MIC_WB) |
                         ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_DOWNLOAD_CVCHS3MIC_MONO_SEND_FB) |
                         ChainCheckCapabilityId(chain_containing_cvc, CAP_ID_CVCHS3MIC_MONO_SEND_FB);
    }
    return cvc_3mic_found;
}

static void kymera_SetCvcSendIntMicModeInChain(kymera_chain_handle_t chain_containing_cvc, uint16 mic_mode)
{
    Operator cvc_op;

    PanicFalse(GET_OP_FROM_CHAIN(cvc_op, chain_containing_cvc, OPR_CVC_SEND));
    OperatorsCvcSendSetIntMicMode(cvc_op, mic_mode);
}

static void kymera_SetCvcSendOmniModeInChain(kymera_chain_handle_t chain_containing_cvc, bool enable)
{
    Operator cvc_op;

    PanicFalse(GET_OP_FROM_CHAIN(cvc_op, chain_containing_cvc, OPR_CVC_SEND));
    if(enable)
    {
        OperatorsCvcSendEnableOmniMode(cvc_op);
    }
    else
    {
        OperatorsCvcSendDisableOmniMode(cvc_op);
    }
}

static void kymera_SetCvcSendBypNpcInChain(kymera_chain_handle_t chain_containing_cvc, bool enable)
{
    Operator cvc_op;
    uint32 dmss_config;
    PanicFalse(GET_OP_FROM_CHAIN(cvc_op, chain_containing_cvc, OPR_CVC_SEND));

    dmss_config = OperatorsCvcSendGetDmssConfig(cvc_op);
    if(enable)
    {
        dmss_config |= CVC_3_MIC_BYP_NPC_MASK;
    }
    else
    {
        dmss_config &= (~CVC_3_MIC_BYP_NPC_MASK);
    }
    OperatorsCvcSendSetDmssConfig(cvc_op, dmss_config);
}

bool Kymera_ScoSetCvcSend3MicMicConfig(uint8 mic_config)
{
    bool setting_changed = FALSE;

    if (cvc_send_settings.mic_config != mic_config)
    {
        setting_changed = TRUE;
    }
    cvc_send_settings.mic_config = mic_config;

    if (appKymeraGetState() == KYMERA_STATE_SCO_ACTIVE)
    {
        kymera_chain_handle_t sco_chain = kymera_GetScoChain();
        if(kymera_Is3MicCvcInScoChain(sco_chain))
        {
            DEBUG_LOG("Kymera_ScoSetCvcSend3MicMicConfig: %dmic cVc", mic_config);
            switch(mic_config)
            {
                case 0:
                    DEBUG_LOG("Kymera_ScoSetCvcSend3MicMicConfig: Passthrough active");
                    break;
                case 1:
                    kymera_SetCvcSendOmniModeInChain(sco_chain, TRUE);
                    kymera_SetCvcSendBypNpcInChain(sco_chain, FALSE);
                    break;
                case 2:
                    kymera_SetCvcSendOmniModeInChain(sco_chain, FALSE);
                    kymera_SetCvcSendBypNpcInChain(sco_chain, TRUE);
                    /* 2Mic mode in combination with HW Leakthrough required */
                    kymera_SetCvcSendIntMicModeInChain(sco_chain, CVC_3_MIC_SET_EXTERNAL_MIC_MODE);
                    break;
                case 3:
                    kymera_SetCvcSendOmniModeInChain(sco_chain, FALSE);
                    kymera_SetCvcSendBypNpcInChain(sco_chain, FALSE);
                    kymera_SetCvcSendIntMicModeInChain(sco_chain, CVC_3_MIC_SET_OCCLUDED_MODE);
                    break;
                default:
                    DEBUG_LOG("Kymera_ScoSetCvcSend3MicMicConfig: Unknown mic config");
            }
        }
        else
        {
            DEBUG_LOG("Kymera_ScoSetCvcSend3MicMicConfig: No 3Mic cVc found in chain");
        }
    }
    else
    {
        DEBUG_LOG("Kymera_ScoSetCvcSend3MicMicConfig: Storing mic_config %d for next SCO call", mic_config);
    }
    return setting_changed;
}

void Kymera_ScoGetCvcPassthroughMode(kymera_cvc_mode_t *mode, uint8 *passthrough_mic)
{
    *mode = cvc_send_settings.mode;
    *passthrough_mic = cvc_send_settings.passthrough_mic;
    DEBUG_LOG("Kymera_ScoGetCvcPassthroughMode: mode enum:kymera_cvc_mode_t:%d passthrough_mic %d", *mode, *passthrough_mic);
}

void Kymera_ScoGetCvcSend3MicMicConfig(uint8 *mic_config)
{
    *mic_config = cvc_send_settings.mic_config;
    DEBUG_LOG("Kymera_ScoGetCvcSend3MicMicConfig: mic_config %d", *mic_config);
}

void Kymera_ScoGetCvcSend3MicModeOfOperation(uint8 *mode_of_operation)
{
    *mode_of_operation = cvc_send_settings.mode_of_operation;
    DEBUG_LOG("Kymera_ScoGetCvcSend3MicModeOfOperation: mode_of_operation %d", *mode_of_operation);
}

static get_status_data_t* kymera_ScoGetCvcSend3MicStatusData(kymera_chain_handle_t chain_containing_cvc)
{
    Operator cvc_op;
    PanicFalse(GET_OP_FROM_CHAIN(cvc_op, chain_containing_cvc, OPR_CVC_SEND));
    get_status_data_t* get_status = OperatorsCreateGetStatusData(CVC_3_MIC_NUM_STATUS_VAR);
    OperatorsGetStatus(cvc_op, get_status);
    return get_status;
}

static void kymera_ScoGetCvcSend3MicStatusFlagsInChain(kymera_chain_handle_t chain_containing_cvc, uint32 *mode_of_operation)
{
    if(chain_containing_cvc != NULL)
    {
        get_status_data_t* get_status = kymera_ScoGetCvcSend3MicStatusData(chain_containing_cvc);
        *mode_of_operation = (uint32)(get_status->value[CVC_3_MIC_THREE_MIC_FLAG_OFFSET]);
        free(get_status);
    }
}

void Kymera_ScoPollCvcSend3MicModeOfOperation(void)
{
    kymera_chain_handle_t sco_chain = kymera_GetScoChain();
    if(kymera_Is3MicCvcInScoChain(sco_chain))
    {
        uint32 mode_of_operation = 0;
        kymera_ScoGetCvcSend3MicStatusFlagsInChain(sco_chain, &mode_of_operation);

        DEBUG_LOG("Kymera_ScoPollCvcSend3MicModeOfOperation");
        if(mode_of_operation != cvc_send_settings.mode_of_operation)
        {
            /* Mode change detected -> send GAIA notification */
            DEBUG_LOG("Kymera_ScoPollCvcSend3MicModeOfOperation: Change detected to %d", mode_of_operation);
            cvc_send_settings.mode_of_operation = mode_of_operation;
            kymeraTaskData *theKymera = KymeraGetTaskData();
            TaskList_MessageSendId(theKymera->listeners, KYMERA_NOTIFICATION_CVC_SEND_MODE_CHANGED);
        }
        MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_CVC_3MIC_POLL_MODE_OF_OPERATION,
                         NULL, POLL_SETTINGS_MS);
    }
}

static void kymera_ScoSetCvc3MicSettings(void)
{
    kymera_chain_handle_t sco_chain = kymera_GetScoChain();
    if(kymera_Is3MicCvcInScoChain(sco_chain))
    {
        if(cvc_send_settings.mode != KYMERA_CVC_NOTHING_SET)
        {
            Kymera_ScoSetCvcSend3MicMicConfig(cvc_send_settings.mic_config);
            Kymera_ScoSetCvcPassthroughMode(cvc_send_settings.mode, cvc_send_settings.passthrough_mic);
        }
        else
        {
            DEBUG_LOG("kymera_ScoSetCvc3MicSettings: No valid settings found for 3Mic cVc");
        }
        /* Start polling for 3Mic cVc mode of operation */
        /* This function is not yet supported */
        //Kymera_ScoPollCvcSend3MicModeOfOperation();
    }
    else
    {
        DEBUG_LOG("kymera_ScoSetCvc3MicSettings: No 3Mic cVc capability found");
    }
}

#endif /* INCLUDE_CVC_DEMO */
