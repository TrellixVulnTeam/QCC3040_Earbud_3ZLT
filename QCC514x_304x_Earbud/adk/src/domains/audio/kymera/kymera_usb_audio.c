/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_usb_audio.c
\brief      Kymera USB Audio Driver
*/
#if !defined(INCLUDE_A2DP_USB_SOURCE)

#include "kymera_usb_audio.h"
#include "kymera_chain_roles.h"
#include "kymera_common.h"
#include "kymera_config.h"
#include "kymera_music_processing.h"
#include "kymera_output_if.h"
#include "kymera_state.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <operators.h>
#include <logging.h>
#include <rtime.h>

#ifdef INCLUDE_USB_DEVICE

#define USB_AUDIO_CHANNEL_STEREO               (2)

static const output_registry_entry_t output_info =
{
    .user = output_user_usb_audio,
    .connection = output_connection_stereo,
};

static void kymeraUSbAudio_ConfigureInputChain(KYMERA_INTERNAL_USB_AUDIO_START_T *usb_audio)
{
    usb_config_t config;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    Operator usb_audio_rx_op = ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_USB_AUDIO_RX);

    config.sample_rate = usb_audio->sample_freq;
    config.sample_size = usb_audio->frame_size;
    config.number_of_channels = usb_audio->channels;

    OperatorsConfigureUsbAudio(usb_audio_rx_op, config);

    OperatorsStandardSetLatencyLimits(usb_audio_rx_op,
                                      MS_TO_US(usb_audio->min_latency_ms),
                                      MS_TO_US(usb_audio->max_latency_ms));

    OperatorsStandardSetTimeToPlayLatency(usb_audio_rx_op,MS_TO_US(usb_audio->target_latency_ms));
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_rx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    if(theKymera->chain_config_callbacks && theKymera->chain_config_callbacks->ConfigureUsbAudioInputChain)
    {
        kymera_usb_audio_config_params_t params = {0};
        params.sample_rate = usb_audio->sample_freq;
        params.sample_size = usb_audio->frame_size;
        params.number_of_channels = usb_audio->channels;
        theKymera->chain_config_callbacks->ConfigureUsbAudioInputChain(theKymera->chain_input_handle, &params);
    }
}

static void kymeraUSbAudio_CreateInputChain(kymeraTaskData *theKymera)
{

    const chain_config_t *config =
        Kymera_GetChainConfigs()->chain_input_usb_stereo_config;

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static void kymeraUSbAudio_CreateAndConfigureOutputChain(uint32 rate,
                                                   int16 volume_in_db)
{
    kymera_output_chain_config config = {0};
    KymeraOutput_SetDefaultOutputChainConfig(&config, rate, KICK_PERIOD_FAST, outputLatencyBuffer());
    PanicFalse(Kymera_OutputPrepare(output_user_usb_audio, &config));
    KymeraOutput_SetMainVolume(volume_in_db);
}

static void kymeraUSbAudio_StartChains(kymeraTaskData *theKymera, Source media_source)
{
    bool connected;

    DEBUG_LOG("kymeraUSbAudio_StartChains");
    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();
    /* The media source may fail to connect to the input chain if the source
    disconnects between the time A2DP asks Kymera to start and this
    function being called. A2DP will subsequently ask Kymera to stop. */
    connected = ChainConnectInput(theKymera->chain_input_handle, media_source, EPR_USB_FROM_HOST);
    if (connected)
    {
        ChainStart(theKymera->chain_input_handle);
    }
}

static void kymeraUSbAudio_JoinChains(kymeraTaskData *theKymera)
{
    output_source_t output = {0};
    output.stereo.left = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    output.stereo.right = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT);

    if(Kymera_IsMusicProcessingPresent())
    {
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.left, EPR_MUSIC_PROCESSING_IN_L));
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.right, EPR_MUSIC_PROCESSING_IN_R));
        output.stereo.left = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_L);
        output.stereo.right = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_R);
    }

    PanicFalse(Kymera_OutputConnect(output_user_usb_audio, &output));
}

void KymeraUsbAudio_Start(KYMERA_INTERNAL_USB_AUDIO_START_T *usb_audio)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG_INFO("KymeraUsbAudio_Start, state %d", appKymeraGetState());

    switch (appKymeraGetState())
    {
        /* Headset audio chains are started in one step */
        case KYMERA_STATE_IDLE:
        {
            /* Ensure there are no audio chains already */
            PanicNotNull(theKymera->chain_input_handle);
            PanicFalse(usb_audio->channels == USB_AUDIO_CHANNEL_STEREO);

            kymeraUSbAudio_CreateAndConfigureOutputChain(usb_audio->sample_freq,
                                                    usb_audio->volume_in_db);

            kymeraUSbAudio_CreateInputChain(theKymera);
            kymeraUSbAudio_ConfigureInputChain(usb_audio);
            Kymera_CreateMusicProcessingChain();
            Kymera_ConfigureMusicProcessing(usb_audio->sample_freq);
    
            StreamDisconnect(usb_audio->spkr_src, NULL);

            ChainConnect(theKymera->chain_input_handle);

            kymeraUSbAudio_JoinChains(theKymera);

            appKymeraSetState(KYMERA_STATE_USB_AUDIO_ACTIVE);

            appKymeraConfigureDspPowerMode();

            kymeraUSbAudio_StartChains(theKymera, usb_audio->spkr_src);

            break;
        }

        default:
            // Report, but ignore attempts to stop in invalid states
            DEBUG_LOG("KymeraUsbAudio_Start, invalid state %u", appKymeraGetState());
            break;
    }
}

void KymeraUsbAudio_Stop(KYMERA_INTERNAL_USB_AUDIO_STOP_T *usb_audio)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG_INFO("KymeraUsbAudio_Stop, state %d", appKymeraGetState());

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        {

            PanicNull(theKymera->chain_input_handle);

            /* Stop chains before disconnecting */
            ChainStop(theKymera->chain_input_handle);

            /* Disconnect USB source from the USB_AUDIO_RX operator then dispose */
            StreamDisconnect(usb_audio->source, 0);
            StreamConnectDispose(usb_audio->source);

            StreamDisconnect(ChainGetOutput(theKymera->chain_input_handle,
                                 EPR_SOURCE_DECODED_PCM), NULL);
            StreamDisconnect(ChainGetOutput(theKymera->chain_input_handle,
                                 EPR_SOURCE_DECODED_PCM_RIGHT),NULL);

            Sink usb_ep_snk = ChainGetInput(theKymera->chain_input_handle, EPR_USB_FROM_HOST);
            StreamDisconnect(NULL, usb_ep_snk);

            Kymera_StopMusicProcessingChain();
            Kymera_OutputDisconnect(output_user_usb_audio);
            Kymera_DestroyMusicProcessingChain();
            
            /* Keep framework enabled until after DSP clock update */
            OperatorsFrameworkEnable();

            /* Destroy chains now that input has been disconnected */
            ChainDestroy(theKymera->chain_input_handle);
            theKymera->chain_input_handle = NULL;
            theKymera->usb_rx = 0;
            appKymeraSetState(KYMERA_STATE_IDLE);
            
            /* Return to low power mode (if applicable) */
            appKymeraConfigureDspPowerMode();
            OperatorsFrameworkDisable();

            break;
        }

        case KYMERA_STATE_IDLE:
            break;

        default:
            /* Report, but ignore attempts to stop in invalid states */
            DEBUG_LOG("KymeraUsbAudio_Stop, invalid state %u", appKymeraGetState());
            break;
    }

    PanicZero(usb_audio->kymera_stopped_handler);
    usb_audio->kymera_stopped_handler(usb_audio->source);
}

void KymeraUsbAudio_SetVolume(int16 volume_in_db)
{
    DEBUG_LOG("KymeraUsbAudio_SetVolume, vol %d", volume_in_db);

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
            KymeraOutput_SetMainVolume(volume_in_db);
            break;

        default:
            break;
    }
}

void KymeraUsbAudio_Init(void)
{
    Kymera_OutputRegister(&output_info);
}

#endif  /* INCLUDE_USB_DEVICE*/
#endif
