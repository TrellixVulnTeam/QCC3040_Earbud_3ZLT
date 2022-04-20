/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_usb_sco.c
\brief      Kymera USB to SCO Driver
*/

#include "kymera_usb_sco.h"
#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "kymera_tones_prompts.h"
#include "kymera_volume.h"
#include "power_manager.h"

#define USB_VOICE_CHANNEL_MONO                 (1)
#define USB_VOICE_CHANNEL_STEREO               (2)

#define MIXER_GAIN_RAMP_SAMPLES 24000

#define USB_SCO_VOICE_FRAME_SIZE            (3) /* 24 Bits */
#define SCO_USB_VOICE_FRAME_SIZE            (2) /* 16 Bits */

#define SCO_NB_SAMPLE_RATE 8000
#define SCO_WB_SAMPLE_RATE 16000
#define SCO_SWB_SAMPLE_RATE 32000

static kymera_chain_handle_t sco_to_usb_voice_chain = NULL;

static kymera_chain_handle_t kymeraUsbScoVoice_GetChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Create input chain */
    return theKymera->chain_input_handle;
}

static kymera_chain_handle_t kymeraUsbScoVoice_CreateUsbToScoChain(uint32_t sample_rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    const chain_config_t *config = NULL;

    switch (sample_rate)
    {
        case SCO_NB_SAMPLE_RATE:
           config = Kymera_GetChainConfigs()->chain_usb_voice_nb_config;
        break;
        case SCO_WB_SAMPLE_RATE:
            config = Kymera_GetChainConfigs()->chain_usb_voice_wb_config;
        break;
        case SCO_SWB_SAMPLE_RATE:
            config = Kymera_GetChainConfigs()->chain_usb_voice_swb_config;
        break;
    default:
        DEBUG_LOG_ERROR("USB Voice: Invalid sample rate %d", sample_rate);
        Panic();
    }

    theKymera->chain_input_handle = PanicNull(ChainCreate(config));

    /* Configure DSP power mode appropriately for USB chain */
    appKymeraConfigureDspPowerMode();

    return theKymera->chain_input_handle;
}

static kymera_chain_handle_t kymeraUsbScoVoice_CreateScoToUsbChain(uint32_t sample_rate)
{
    const chain_config_t *config = NULL;
    kymera_chain_handle_t sco_to_usb_handle;

    switch (sample_rate)
    {
        case SCO_NB_SAMPLE_RATE:
           config = Kymera_GetChainConfigs()->chain_sco_nb_config;
        break;
        case SCO_WB_SAMPLE_RATE:
            config = Kymera_GetChainConfigs()->chain_sco_wb_config;
        break;
        case SCO_SWB_SAMPLE_RATE:
            config = Kymera_GetChainConfigs()->chain_sco_swb_config;
        break;
    default:
        DEBUG_LOG_ERROR("USB Voice: Invalid sample rate %d", sample_rate);
        Panic();
    }

    sco_to_usb_handle = PanicNull(ChainCreate(config));

    return sco_to_usb_handle;
}

static void kymeraUsbScoVoice_ConfigureUsbToScoChain(KYMERA_INTERNAL_USB_SCO_VOICE_START_T *usb_voice)
{
    usb_config_t config;
    Operator usb_audio_rx_op = ChainGetOperatorByRole(kymeraUsbScoVoice_GetChain(), OPR_USB_AUDIO_RX);
    Operator resampler_op = ChainGetOperatorByRole(kymeraUsbScoVoice_GetChain(), OPR_SPEAKER_RESAMPLER);
    Operator rate_adjust_op = ChainGetOperatorByRole(kymeraUsbScoVoice_GetChain(), OPR_RATE_ADJUST);

    OperatorsResamplerSetConversionRate(resampler_op, usb_voice->spkr_sample_rate, usb_voice->sco_sample_rate);

    OperatorsStandardSetSampleRate(rate_adjust_op, usb_voice->sco_sample_rate);


    if(usb_voice->spkr_channels == USB_VOICE_CHANNEL_STEREO)
    {
        DEBUG_LOG_INFO("Kymera USB SCO USB_VOICE_CHANNEL_STEREO");
        Operator mixer_op = ChainGetOperatorByRole(kymeraUsbScoVoice_GetChain(), OPR_LEFT_RIGHT_MIXER);
        DEBUG_LOG_INFO("kymeraUsbScoVoice_ConfigureUsbToScoChain: resampler_op %x, mixer_op %x", resampler_op, mixer_op);
        OperatorsConfigureMixer(mixer_op, usb_voice->spkr_sample_rate, 1, GAIN_HALF, GAIN_HALF, GAIN_MIN, 1, 1, 0);
        OperatorsMixerSetNumberOfSamplesToRamp(mixer_op, MIXER_GAIN_RAMP_SAMPLES);
    }

    config.sample_rate = usb_voice->spkr_sample_rate;
    config.sample_size = USB_SCO_VOICE_FRAME_SIZE;
    config.number_of_channels = usb_voice->spkr_channels;

    OperatorsConfigureUsbAudio(usb_audio_rx_op, config);

    OperatorsStandardSetLatencyLimits(usb_audio_rx_op,
                                      MS_TO_US(usb_voice->min_latency_ms),
                                      MS_TO_US(usb_voice->max_latency_ms));

    OperatorsStandardSetTimeToPlayLatency(usb_audio_rx_op, MS_TO_US(usb_voice->target_latency_ms));
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_rx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    SinkConfigure(usb_voice->sco_sink, STREAM_RM_USE_RATE_ADJUST_OPERATOR, rate_adjust_op);
}

static void kymeraUsbScoVoice_ConfigureScoToUsbChain(KYMERA_INTERNAL_USB_SCO_VOICE_START_T *usb_voice)
{
    Operator sco_audio_rx_op = ChainGetOperatorByRole(sco_to_usb_voice_chain, OPR_SCO_RECEIVE);
    Operator resampler_op = ChainGetOperatorByRole(sco_to_usb_voice_chain, OPR_SPEAKER_RESAMPLER);
    Operator usb_audio_tx_op = ChainGetOperatorByRole(sco_to_usb_voice_chain, OPR_USB_AUDIO_TX);
    usb_config_t config;

    config.sample_rate = usb_voice->mic_sample_rate;
    config.sample_size = SCO_USB_VOICE_FRAME_SIZE;
    config.number_of_channels = USB_VOICE_CHANNEL_MONO;

    OperatorsConfigureUsbAudio(usb_audio_tx_op, config);
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_tx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    OperatorsResamplerSetConversionRate(resampler_op, usb_voice->sco_sample_rate, usb_voice->mic_sample_rate);
    OperatorsStandardSetTimeToPlayLatency(sco_audio_rx_op, MS_TO_US(usb_voice->target_latency_ms));
    OperatorsStandardSetBufferSize(sco_audio_rx_op, TTP_BUFFER_SIZE);
}

void KymeraUsbScoVoice_Start(KYMERA_INTERNAL_USB_SCO_VOICE_START_T *usb_sco_voice)
{
    DEBUG_LOG("KymeraUsbScoVoice_Start");

    /* If there is a tone still playing at this point,
     * it must be an interruptible tone, so cut it off */
    appKymeraTonePromptStop();

    /* Can't start voice chain if we're not idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);

    appKymeraSetState(KYMERA_STATE_USB_SCO_VOICE_ACTIVE);

    /* USB audio requires higher clock speeds, so request a switch to the "performance" power profile */
    appPowerPerformanceProfileRequest();

    /* Create appropriate USB chain */
    kymera_chain_handle_t usb_to_sco_voice_chain = PanicNull(kymeraUsbScoVoice_CreateUsbToScoChain(usb_sco_voice->sco_sample_rate));

    sco_to_usb_voice_chain = PanicNull(kymeraUsbScoVoice_CreateScoToUsbChain(usb_sco_voice->sco_sample_rate));

    Sink usb_ep_snk = ChainGetInput(usb_to_sco_voice_chain, EPR_USB_FROM_HOST);
    Source sco_ep_src = ChainGetOutput(usb_to_sco_voice_chain, EPR_SCO_TO_AIR);

    Sink sco_ep_snk = ChainGetInput(sco_to_usb_voice_chain, EPR_SCO_FROM_AIR);
    Source usb_ep_src = ChainGetOutput(sco_to_usb_voice_chain, EPR_USB_TO_HOST);

    Source sco_source = PanicNull(StreamSourceFromSink(usb_sco_voice->sco_sink));

    /* Configure chain specific operators */
    kymeraUsbScoVoice_ConfigureUsbToScoChain(usb_sco_voice);
    kymeraUsbScoVoice_ConfigureScoToUsbChain(usb_sco_voice);

    StreamDisconnect(sco_ep_src, NULL);
    StreamDisconnect(NULL, usb_ep_snk);

    StreamDisconnect(usb_sco_voice->spkr_src, NULL);
    StreamDisconnect(NULL, usb_sco_voice->sco_sink);

    StreamConnect(usb_sco_voice->spkr_src, usb_ep_snk);
    StreamConnect(sco_ep_src, usb_sco_voice->sco_sink);

    StreamConnect(sco_source, sco_ep_snk);
    StreamConnect(usb_ep_src, usb_sco_voice->mic_sink);

    ChainConnect(sco_to_usb_voice_chain);
    ChainConnect(usb_to_sco_voice_chain);

    ChainStart(sco_to_usb_voice_chain);

    ChainStart(usb_to_sco_voice_chain);
}



void KymeraUsbScoVoice_Stop(KYMERA_INTERNAL_USB_SCO_VOICE_STOP_T *usb_sco_stop)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    kymera_chain_handle_t usb_to_sco_voice_chain = kymeraUsbScoVoice_GetChain();
    if (appKymeraGetState() != KYMERA_STATE_USB_SCO_VOICE_ACTIVE)
    {
        if (usb_to_sco_voice_chain == NULL)
        {
            /* Attempting to stop a USB Voice chain when not ACTIVE.*/
            DEBUG_LOG_INFO("USB Voice: KymeraUsbScoVoice_Stop, not stopping - already idle");
        }
        else
        {
            DEBUG_LOG_WARN("USB Voice: KymeraUsbScoVoice_Stop, state %d, usb_voice_chain %lu ",
                            appKymeraGetState(), usb_to_sco_voice_chain);
            Panic();
        }

        PanicZero(usb_sco_stop->kymera_stopped_handler);
        usb_sco_stop->kymera_stopped_handler(usb_sco_stop->spkr_src);
        return;
    }

    appKymeraTonePromptStop();
    ChainStop(usb_to_sco_voice_chain);
    ChainStop(sco_to_usb_voice_chain);

    Sink usb_ep_snk = ChainGetInput(usb_to_sco_voice_chain, EPR_USB_FROM_HOST);
    Source sco_ep_src = ChainGetOutput(usb_to_sco_voice_chain, EPR_SCO_TO_AIR);

    Sink sco_ep_snk = ChainGetInput(sco_to_usb_voice_chain, EPR_SCO_FROM_AIR);
    Source usb_ep_src = ChainGetOutput(sco_to_usb_voice_chain, EPR_USB_TO_HOST);

    Source sco_source = StreamSourceFromSink(usb_sco_stop->sco_sink);

    StreamDisconnect(NULL, usb_sco_stop->mic_sink);
    StreamDisconnect(usb_sco_stop->spkr_src, NULL);

    StreamDisconnect(NULL, usb_sco_stop->sco_sink);
    StreamDisconnect(sco_source, NULL);

    StreamDisconnect(NULL, usb_ep_snk);
    StreamDisconnect(usb_ep_src, NULL);

    StreamDisconnect(NULL, sco_ep_snk);
    StreamDisconnect(sco_ep_src, NULL);

    ChainDestroy(usb_to_sco_voice_chain);
    ChainDestroy(sco_to_usb_voice_chain);

    theKymera->chain_input_handle = NULL;
    sco_to_usb_voice_chain = NULL;

    /* No longer need to be in high performance power profile */
    appPowerPerformanceProfileRelinquish();

    /* Update state variables */
    appKymeraSetState(KYMERA_STATE_IDLE);

    PanicZero(usb_sco_stop->kymera_stopped_handler);
    usb_sco_stop->kymera_stopped_handler(usb_sco_stop->spkr_src);
}
