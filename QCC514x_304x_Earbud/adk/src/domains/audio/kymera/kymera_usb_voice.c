/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_usb_voice.c
\brief      Kymera USB Voice Driver
*/

#include "kymera_common.h"
#include "kymera_config.h"
#include "kymera_aec.h"
#include "kymera_mic_if.h"
#include "kymera_usb_voice.h"
#include "kymera_output_if.h"
#include "kymera_state.h"
#include "kymera_tones_prompts.h"
#include "kymera_volume.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <power_manager.h>
#include <operators.h>
#include <logging.h>

#ifdef INCLUDE_USB_DEVICE

#define USB_VOICE_CHANNEL_MONO                 (1)
#define USB_VOICE_CHANNEL_STEREO               (2)

#define USB_VOICE_FRAME_SIZE            (2) /* 16 Bits */

#define USB_VOICE_NUM_OF_MICS           (2)

#define USB_VOICE_INVALID_NUM_OF_MICS   (3)

/*!@{ \name Useful gains in kymera operators format */
#define GAIN_HALF (-6 * KYMERA_DB_SCALE)
#define GAIN_FULL (0)
#define GAIN_MIN (-90 * KYMERA_DB_SCALE)
/*!@} */

#define MIXER_GAIN_RAMP_SAMPLES 24000

/* TODO: Once generic name (without "SCO") is used, below macros
 * can be removed
 */
#ifdef KYMERA_SCO_USE_2MIC_BINAURAL
#define KYMERA_USB_VOICE_USE_2MIC_BINAURAL
#elif KYMERA_SCO_USE_2MIC
#define KYMERA_USB_VOICE_USE_2MIC
#endif /* KYMERA_SCO_USE_2MIC_BINAURAL */

#if defined KYMERA_SCO_USE_2MIC_BINAURAL || defined KYMERA_USB_VOICE_USE_2MIC
#define MAX_NUM_OF_MICS_SUPPORTED (2)
#else
#define MAX_NUM_OF_MICS_SUPPORTED (1)
#endif

/* Reference data */
#define AEC_USB_TX_BUFFER_SIZE_MS   15
#define AEC_USB_TTP_DELAY_MS    50

static bool kymeraUsbVoice_MicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymeraUsbVoice_MicDisconnectIndication(const mic_change_info_t *info);

static const mic_callbacks_t usb_voice_mic_callbacks =
{
    .MicGetConnectionParameters = kymeraUsbVoice_MicGetConnectionParameters,
    .MicDisconnectIndication = kymeraUsbVoice_MicDisconnectIndication,
    /* MicReconnectedIndication is omited since kymeraUsbVoice_MicDisconnectIndication will give panic */
    .MicReconnectedIndication = NULL,
};

static const microphone_number_t kymeraUsbVoice_mandatory_mic_ids[MAX_NUM_OF_MICS_SUPPORTED] =
{
    microphone_none,
};

static mic_user_state_t usb_voice_mic_state =
{
    mic_user_state_non_interruptible,
};

static const mic_registry_per_user_t usb_voice_mic_registry =
{
    .user = mic_user_usb_voice,
    .callbacks = &usb_voice_mic_callbacks,
    .mandatory_mic_ids = &kymeraUsbVoice_mandatory_mic_ids[0],
    .num_of_mandatory_mics = 0,
    .mic_user_state = &usb_voice_mic_state,
};

static uint32 usb_voice_mic_sample_rate;

static const output_registry_entry_t output_info =
{
    .user = output_user_usb_voice,
    .connection = output_connection_mono,
};

static kymera_chain_handle_t usb_rx_chain = NULL;
static kymera_chain_handle_t the_usb_voice_chain = NULL;

static kymera_chain_handle_t kymeraUsbVoice_GetUsbRxChain(void)
{
    return usb_rx_chain;
}

static void kymeraUsbVoice_SetUsbRxChain(kymera_chain_handle_t chain)
{
    DEBUG_LOG("kymeraUsbVoice_SetUsbRxChain chain %d", chain);
    usb_rx_chain = chain;
}

static void kymeraUsbVoice_CreateUsbRxChain(uint8 usb_rx_channels, Sink usb_voice_mono_receive)
{
    DEBUG_LOG_INFO("kymeraUsbVoice_CreateUsbRxChain: usb_rx_channels %d", usb_rx_channels);

    const chain_config_t *config = NULL;

    switch(usb_rx_channels)
    {
        case USB_VOICE_CHANNEL_MONO:
        {
            config = Kymera_GetChainConfigs()->chain_usb_voice_rx_mono_config;
            break;
        }
        case USB_VOICE_CHANNEL_STEREO:
        {
            config = Kymera_GetChainConfigs()->chain_usb_voice_rx_stereo_config;
            break;
        }
        default:
            DEBUG_LOG_WARN("USB Voice: Invalid configuration usb_rx_channels %x", usb_rx_channels);
            Panic();
            break;
    }

    kymeraUsbVoice_SetUsbRxChain(PanicNull(ChainCreate(config)));

    Source usb_voice_rx_out  = ChainGetOutput(kymeraUsbVoice_GetUsbRxChain(), EPR_USB_RX_RESAMPLER_OUT);

    PanicNull(StreamConnect(usb_voice_rx_out, usb_voice_mono_receive));
}

static void kymeraUsbVoice_ConnectUsbRxChain(void)
{
    DEBUG_LOG_VERBOSE("kymeraUsbVoice_ConnectUsbRxChain");
    ChainConnect(kymeraUsbVoice_GetUsbRxChain());
    ChainStart(kymeraUsbVoice_GetUsbRxChain());
}

static void kymeraUsbVoice_DestroyUsbRxChain(void)
{
    DEBUG_LOG_VERBOSE("kymeraUsbVoice_DestroyUsbRxChain");
    if(kymeraUsbVoice_GetUsbRxChain() != NULL)
    {
        StreamDisconnect(ChainGetOutput(kymeraUsbVoice_GetUsbRxChain(), EPR_USB_RX_RESAMPLER_OUT), NULL);
        PanicNull(kymeraUsbVoice_GetUsbRxChain());
        ChainStop(kymeraUsbVoice_GetUsbRxChain());
        ChainDestroy(kymeraUsbVoice_GetUsbRxChain());
        kymeraUsbVoice_SetUsbRxChain(NULL);
    }
}

static kymera_chain_handle_t kymeraUsbVoice_CreateChain(usb_voice_mode_t mode)
{
    const chain_config_t *config = NULL;

    /* USB Voice does not support 3-mic cVc. So lets panic is user selected this
     * option.
     */
    if (Kymera_GetNumberOfMics() == USB_VOICE_INVALID_NUM_OF_MICS)
    {
        DEBUG_LOG_WARN("kymeraUsbVoice_CreateChain invalid no of mics %x", Kymera_GetNumberOfMics());
        return NULL;
    }

    switch(mode)
    {
        case usb_voice_mode_nb:
        {
            if(USB_VOICE_NUM_OF_MICS == Kymera_GetNumberOfMics())
            {
#ifdef KYMERA_USB_VOICE_USE_2MIC_BINAURAL
                config = Kymera_GetChainConfigs()->chain_usb_voice_nb_2mic_binaural_config;
#else
                config = Kymera_GetChainConfigs()->chain_usb_voice_nb_2mic_config;
#endif /* KYMERA_USB_VOICE_USE_2MIC_BINAURAL */
            }
            else
            {
                config = Kymera_GetChainConfigs()->chain_usb_voice_nb_config;
            }
            break;
        }
        case usb_voice_mode_wb:
        {
            if(USB_VOICE_NUM_OF_MICS == Kymera_GetNumberOfMics())
            {
#ifdef KYMERA_USB_VOICE_USE_2MIC_BINAURAL
                config = Kymera_GetChainConfigs()->chain_usb_voice_wb_2mic_binaural_config;
#else
                config = Kymera_GetChainConfigs()->chain_usb_voice_wb_2mic_config;
#endif /* KYMERA_USB_VOICE_USE_2MIC_BINAURAL */
            }
            else
            {
                config = Kymera_GetChainConfigs()->chain_usb_voice_wb_config;
            }
            break;
        }
        default:
            DEBUG_LOG_WARN("USB Voice: Invalid configuration mode%x", mode);
            break;
    }

    /* Create input chain */
    the_usb_voice_chain = PanicNull(ChainCreate(config));

    /* Configure DSP power mode appropriately for USB chain */
    appKymeraConfigureDspPowerMode();

    return the_usb_voice_chain;
}

static void kymeraUsbVoice_ConfigureChain(KYMERA_INTERNAL_USB_VOICE_START_T *usb_voice)
{
    usb_config_t config;
    Operator usb_audio_rx_op = ChainGetOperatorByRole(kymeraUsbVoice_GetUsbRxChain(), OPR_USB_AUDIO_RX);
    Operator usb_audio_tx_op = ChainGetOperatorByRole(the_usb_voice_chain, OPR_USB_AUDIO_TX);
    Operator resampler_op = ChainGetOperatorByRole(kymeraUsbVoice_GetUsbRxChain(), OPR_SPEAKER_RESAMPLER);

    OperatorsResamplerSetConversionRate(resampler_op, usb_voice->spkr_sample_rate, usb_voice->mic_sample_rate);

    if(usb_voice->spkr_channels == USB_VOICE_CHANNEL_STEREO)
    {
        Operator mixer_op = ChainGetOperatorByRole(kymeraUsbVoice_GetUsbRxChain(), OPR_LEFT_RIGHT_MIXER);
        DEBUG_LOG_VERBOSE("kymeraUsbVoice_ConfigureChain: resampler_op %x, mixer_op %x", resampler_op, mixer_op);
        OperatorsConfigureMixer(mixer_op, usb_voice->spkr_sample_rate, 1, GAIN_HALF, GAIN_HALF, GAIN_MIN, 1, 1, 0);
        OperatorsMixerSetNumberOfSamplesToRamp(mixer_op, MIXER_GAIN_RAMP_SAMPLES);

    }

    config.sample_rate = usb_voice->spkr_sample_rate;
    config.sample_size = USB_VOICE_FRAME_SIZE;
    config.number_of_channels = usb_voice->spkr_channels;

    DEBUG_LOG_VERBOSE("kymeraUsbVoice_ConfigureChain: Operators rx %x, tx %x", usb_audio_rx_op, usb_audio_tx_op);

    OperatorsConfigureUsbAudio(usb_audio_rx_op, config);

    OperatorsStandardSetLatencyLimits(usb_audio_rx_op,
                                      MS_TO_US(usb_voice->min_latency_ms),
                                      MS_TO_US(usb_voice->max_latency_ms));

    OperatorsStandardSetTimeToPlayLatency(usb_audio_rx_op, MS_TO_US(usb_voice->target_latency_ms));
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_rx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    config.sample_rate = usb_voice->mic_sample_rate;
    config.sample_size = USB_VOICE_FRAME_SIZE;
    config.number_of_channels = USB_VOICE_CHANNEL_MONO;

    OperatorsConfigureUsbAudio(usb_audio_tx_op, config);
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_tx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureUsbVoiceRxChain)
    {
        kymera_usb_voice_rx_config_params_t params = {0};
        params.sample_rate = usb_voice->spkr_sample_rate;
        params.sample_size = USB_VOICE_FRAME_SIZE;
        params.number_of_channels = usb_voice->spkr_channels;
        KymeraGetTaskData()->chain_config_callbacks->ConfigureUsbVoiceRxChain(kymeraUsbVoice_GetUsbRxChain(), &params);
    }

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureUsbVoiceTxChain)
    {
        kymera_usb_voice_tx_config_params_t params = {0};
        params.sample_rate = usb_voice->mic_sample_rate;
        params.sample_size = USB_VOICE_FRAME_SIZE;
        params.number_of_channels = USB_VOICE_CHANNEL_MONO;
        KymeraGetTaskData()->chain_config_callbacks->ConfigureUsbVoiceTxChain(the_usb_voice_chain, &params);
    }

}



static void kymeraUsbVoice_PopulateConnectParams(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 num_mics, Sink *aec_ref_sink)
{
    kymera_chain_handle_t usb_voice_chain = the_usb_voice_chain;
    PanicZero(mic_ids);
    PanicZero(mic_sinks);
    PanicZero(aec_ref_sink);
    PanicFalse(num_mics <= 2);

    mic_ids[0] = appConfigMicVoice();
    mic_sinks[0] = ChainGetInput(usb_voice_chain, EPR_CVC_SEND_IN1);
    if(num_mics > 1)
    {
        mic_ids[1] = appConfigMicExternal();
        mic_sinks[1] = ChainGetInput(usb_voice_chain, EPR_CVC_SEND_IN2);
    }

    aec_ref_sink[0] = ChainGetInput(usb_voice_chain, EPR_CVC_SEND_REF_IN);
}

/*! If the microphones are disconnected, all users get informed with a DisconnectIndication.
 *  return FALSE: accept disconnection
 *  return TRUE: Try to reconnect the microphones. This will trigger a kymeraUsbVoice_MicGetConnectionParameters
 */
static bool kymeraUsbVoice_MicDisconnectIndication(const mic_change_info_t *info)
{
    UNUSED(info);
    DEBUG_LOG_ERROR("kymeraUsbVoice_MicDisconnectIndication: USB Voice shouldn't have to get disconnected");
    Panic();
    return TRUE;
}

/*! For a reconnection the mic parameters are sent to the mic interface.
 *  return TRUE to reconnect with the given parameters
 */
static bool kymeraUsbVoice_MicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    DEBUG_LOG("kymeraUsbVoice_MicGetConnectionParameters");

    *sample_rate = usb_voice_mic_sample_rate;
    *num_of_mics = Kymera_GetNumberOfMics();
    kymeraUsbVoice_PopulateConnectParams(mic_ids, mic_sinks, *num_of_mics, aec_ref_sink);
    return TRUE;
}


void KymeraUsbVoice_Start(KYMERA_INTERNAL_USB_VOICE_START_T *usb_voice)
{

    DEBUG_LOG_INFO("USB Voice: KymeraUsbVoice_Start Sink %x", usb_voice->mic_sink);

    /* If there is a tone still playing at this point,
     * it must be an interruptible tone, so cut it off */
    appKymeraTonePromptStop();

    /* Can't start voice chain if we're not idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);

    /* USB chain must be destroyed if we get here */
    PanicNotNull(the_usb_voice_chain);

    /* Move to USB active state now, what ever happens we end up in this state
      (even if it's temporary) */
    appKymeraSetState(KYMERA_STATE_USB_VOICE_ACTIVE);

    /* USB audio requires higher clock speeds, so request a switch to the "performance" power profile */
    appPowerPerformanceProfileRequest();

    /* Create appropriate USB chain */
    kymera_chain_handle_t usb_voice_chain = PanicNull(kymeraUsbVoice_CreateChain(usb_voice->mode));

    kymeraUsbVoice_CreateUsbRxChain(usb_voice->spkr_channels, ChainGetInput(usb_voice_chain, EPR_USB_CVC_RECEIVE_IN));

    usb_voice_mic_sample_rate = usb_voice->mic_sample_rate;

    /* Connect to Mic interface */
    if (!Kymera_MicConnect(mic_user_usb_voice))
    {
        DEBUG_LOG_ERROR("KymeraUsbVoice_Start: Mic connection was not successful. USB Voice should always be prepared.");
        Panic();
    }

    /* Get sources and sinks for chain endpoints */
    Source usb_ep_src  = ChainGetOutput(usb_voice_chain, EPR_USB_TO_HOST);
    Sink usb_ep_snk    = ChainGetInput(kymeraUsbVoice_GetUsbRxChain(), EPR_USB_FROM_HOST);

    DEBUG_LOG_VERBOSE("USB Voice: KymeraUsbVoice_Start usb_ep_src %x, usb_ep_snk %x", usb_ep_src, usb_ep_snk);

    /* Configure chain specific operators */
    kymeraUsbVoice_ConfigureChain(usb_voice);
    Kymera_SetVoiceUcids(usb_voice_chain);

    /* Create an appropriate Output chain */
    kymera_output_chain_config output_config;
    KymeraOutput_SetDefaultOutputChainConfig(&output_config, usb_voice->mic_sample_rate, KICK_PERIOD_VOICE, 0);

    output_config.chain_type = output_chain_mono;
    output_config.chain_include_aec = TRUE;
    PanicFalse(Kymera_OutputPrepare(output_user_usb_voice, &output_config));

    StreamDisconnect(usb_voice->spkr_src, NULL);
    StreamDisconnect(usb_ep_src, NULL);

    /* Disconnect USB ISO in endpoint */
    StreamDisconnect(usb_voice->spkr_src, NULL);

    /* Disconnect USB ISO out endpoint */
    StreamDisconnect(NULL, usb_voice->mic_sink);

    /* Connect USB chain to USB endpoints */
    StreamConnect(usb_voice->spkr_src, usb_ep_snk);
    StreamConnect(usb_ep_src, usb_voice->mic_sink);

    /* Connect chain */
    ChainConnect(usb_voice_chain);

    kymeraUsbVoice_ConnectUsbRxChain();

    /* Connect to the Ouput chain */
    output_source_t sources = {.mono = ChainGetOutput(usb_voice_chain, EPR_SCO_SPEAKER)};
    PanicFalse(Kymera_OutputConnect(output_user_usb_voice, &sources));
    KymeraOutput_ChainStart();

    /* The chain can fail to start if the USB source disconnects whilst kymera
    is queuing the USB start request or starting the chain. If the attempt fails,
    ChainStartAttempt will stop (but not destroy) any operators it started in the chain. */
    if (ChainStartAttempt(usb_voice_chain))
    {
        KymeraUsbVoice_SetVolume(usb_voice->volume);
    }
    else
    {
        KYMERA_INTERNAL_USB_VOICE_STOP_T disconnect_params;
        DEBUG_LOG_WARN("USB Voice: KymeraUsbVoiceStart, could not start chain");
        disconnect_params.mic_sink = usb_voice->mic_sink;
        disconnect_params.spkr_src = usb_voice->spkr_src;
        disconnect_params.kymera_stopped_handler = usb_voice->kymera_stopped_handler;
        KymeraUsbVoice_Stop(&disconnect_params);
    }
}

void KymeraUsbVoice_Stop(KYMERA_INTERNAL_USB_VOICE_STOP_T *usb_voice)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG_INFO("USB Voice: KymeraUsbVoice_Stop, mic sink %u", usb_voice->mic_sink);

    /* Get current USB chain */
    kymera_chain_handle_t usb_voice_chain = the_usb_voice_chain;
    if (appKymeraGetState() != KYMERA_STATE_USB_VOICE_ACTIVE)
    {
        /* Following code need to br re-visited once audio router supports voice.
         * Till then KymeraUsbVoice_Stop can be called when kymera state is not in 
         * USB_VOICE_ACTIVE with USB_VOICE_START message waiting to execute.
         * If that happens, we are deleting the pending message.
         */
        if(MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_START))
        {
            DEBUG_LOG_INFO("USB Voice: KymeraUsbVoice_Stop, state %d, USB_VOICE_START message cancelled",
                             appKymeraGetState());
        }
        else
        {
            if (usb_voice_chain == NULL)
            {
                /* Attempting to stop a USB Voice chain when not ACTIVE.*/
                DEBUG_LOG_INFO("USB Voice: KymeraUsbVoice_Stop, not stopping - already idle");
            }
            else
            {
                DEBUG_LOG_WARN("USB Voice: KymeraUsbVoice_Stop, state %d, usb_voice_chain %lu ",
                                appKymeraGetState(), usb_voice_chain);
                Panic();
            }
        }

        PanicZero(usb_voice->kymera_stopped_handler);
        usb_voice->kymera_stopped_handler(usb_voice->spkr_src);
        return;
    }

    /* Get sources and sinks for chain endpoints */
    Source usb_ep_src  = ChainGetOutput(usb_voice_chain, EPR_USB_TO_HOST);
    Sink usb_ep_snk    = ChainGetInput(kymeraUsbVoice_GetUsbRxChain(), EPR_USB_FROM_HOST);

    DEBUG_LOG_VERBOSE("USB Voice: KymeraUsbVoice_Stop usb_ep_src %x, usb_ep_snk %x", usb_ep_src, usb_ep_snk);

    appKymeraTonePromptStop();

    /* Stop chains */
    ChainStop(usb_voice_chain);

    kymeraUsbVoice_DestroyUsbRxChain();

    /* Disconnect USB ISO in endpoint */
    StreamDisconnect(usb_voice->spkr_src, NULL);

    /* Disconnect USB ISO out endpoint */
    StreamDisconnect(NULL, usb_voice->mic_sink);

    Kymera_MicDisconnect(mic_user_usb_voice);
    StreamDisconnect(ChainGetOutput(usb_voice_chain, EPR_SCO_VOL_OUT), NULL);

    /* Disconnect USB from chain USB endpoints */
    StreamDisconnect(usb_ep_src, NULL);
    StreamDisconnect(NULL, usb_ep_snk);

    Kymera_OutputDisconnect(output_user_usb_voice);

    /* Destroy chains */
    ChainDestroy(usb_voice_chain);
    the_usb_voice_chain = usb_voice_chain = NULL;

    /* No longer need to be in high performance power profile */
    appPowerPerformanceProfileRelinquish();

    /* Update state variables */
    appKymeraSetState(KYMERA_STATE_IDLE);
    theKymera->output_rate = 0;

    PanicZero(usb_voice->kymera_stopped_handler);
    usb_voice->kymera_stopped_handler(usb_voice->spkr_src);

}

void KymeraUsbVoice_SetVolume(int16 volume_in_db)
{
    DEBUG_LOG_VERBOSE("KymeraUsbVoice_SetVolume, vol %d", volume_in_db);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        {
            KymeraOutput_SetMainVolume(volume_in_db);
        }
        break;
        default:
            break;
    }
}

void KymeraUsbVoice_MicMute(bool mute)
{
    DEBUG_LOG_VERBOSE("KymeraUsbVoice_MicMute, mute %u", mute);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        {
            Operator aec_op = Kymera_GetAecOperator();
            if (aec_op != INVALID_OPERATOR)
            {
                OperatorsAecMuteMicOutput(aec_op, mute);
            }
        }
        break;

        default:
            break;
    }
}

void KymeraUsbVoice_Init(void)
{
    Kymera_OutputRegister(&output_info);
	Kymera_MicRegisterUser(&usb_voice_mic_registry);
}

#endif  /* INCLUDE_USB_DEVICE*/
