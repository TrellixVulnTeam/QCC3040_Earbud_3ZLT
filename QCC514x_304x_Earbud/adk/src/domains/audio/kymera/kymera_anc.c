/*!
\copyright  Copyright (c) 2017-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version
\file       kymera_anc.c
\brief      Kymera ANC code
*/

#include "kymera_anc.h"
#include "kymera_common.h"
#include "kymera_lock.h"
#include "kymera_state.h"
#include "kymera_internal_msg_ids.h"
#include <audio_clock.h>
#include <audio_power.h>
#include <vmal.h>
#include <file.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>

#include "kymera_config.h"
#include "kymera.h"
#include "microphones.h"
#include "kymera_source_sync.h"

#define ANC_TUNING_SINK_USB_LEFT      0 /*can be any other backend device. PCM used in this tuning graph*/
#define ANC_TUNING_SINK_USB_RIGHT     1
#define ANC_TUNING_SINK_FBMON_LEFT    2 /*reserve slots for FBMON tap. Always connected.*/
#define ANC_TUNING_SINK_FBMON_RIGHT   3
#define ANC_TUNING_SINK_MIC1_LEFT     4 /* must be connected to internal ADC. Analog or digital */
#define ANC_TUNING_SINK_MIC1_RIGHT    5
#define ANC_TUNING_SINK_MIC2_LEFT     6
#define ANC_TUNING_SINK_MIC2_RIGHT    7

#define ANC_TUNING_SOURCE_USB_LEFT    0 /*can be any other backend device. USB used in the tuning graph*/
#define ANC_TUNING_SOURCE_USB_RIGHT   1
#define ANC_TUNING_SOURCE_DAC_LEFT    2 /* must be connected to internal DAC */
#define ANC_TUNING_SOURCE_DAC_RIGHT   3

#define ANC_TUNNING_START_DELAY       (200U)
#define ANC_TUNNING_USB_SAMPLING_RATE  48000   /* Only 48KHz supported for anc tuning */
#define ANC_TUNNING_USB_AUDIO_CHANNELS  2   /* Number of mic and speaker channels in the audio data stream */

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/* Qualcomm-provided downloadable anc tuning capability for QCC517x */
#define CAP_ID_DOWNLOAD_ANC_TUNING_QCC517X (0x40B2)

#ifdef DOWNLOAD_USB_AUDIO
#define EB_CAP_ID_USB_AUDIO_RX CAP_ID_DOWNLOAD_USB_AUDIO_RX
#define EB_CAP_ID_USB_AUDIO_TX CAP_ID_DOWNLOAD_USB_AUDIO_TX
#else
#define EB_CAP_ID_USB_AUDIO_RX CAP_ID_USB_AUDIO_RX
#define EB_CAP_ID_USB_AUDIO_TX CAP_ID_USB_AUDIO_TX
#endif

typedef struct
{
    Source mic_in1;
    Source mic_in2;
    Source fb_mon;
    Sink DAC;
}chan_data_t;

static chan_data_t left, right;

#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
static Sink eanc_second_dac;

#define SPLITTER_TERMINAL_IN_0       0
#define SPLITTER_TERMINAL_OUT_0      0
#define SPLITTER_TERMINAL_OUT_1      1

/*Splitter is needed for eANC tuning mode to activate second DAC path. ANC tuning capability output is required
for both the ANC instances through a Splitter to DAC EP Left and Right for the Echo Cancellation purpose.*/
static void kymeraAnc_CreateSplitter(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    theKymera->output_splitter = (Operator)(VmalOperatorCreate(CAP_ID_SPLITTER));
}

static void kymeraAnc_ConfigureSplitter(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    OperatorsSplitterSetWorkingMode(theKymera->output_splitter, splitter_mode_clone_input);
    OperatorsSplitterEnableSecondOutput(theKymera->output_splitter, FALSE);
    OperatorsSplitterSetDataFormat(theKymera->output_splitter, operator_data_format_pcm);
}
#endif

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
void KymeraAnc_EnterTuning(const anc_tuning_connect_parameters_t *param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("KymeraAnc_EnterTuning");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ANC_TUNING_START);
    message->usb_rate = param->usb_rate;
    message->spkr_src = param->spkr_src;
    message->mic_sink = param->mic_sink;
    message->spkr_channels = param->spkr_channels;
    message->mic_channels = param->mic_channels;
    message->frame_size = param->frame_size;
    if(theKymera->busy_lock)
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message, &theKymera->busy_lock);
    }
    else if(theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message, ANC_TUNNING_START_DELAY);
    }
    else
    {
       MessageSend(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message);
    }
}
#else
void KymeraAnc_EnterTuning(const anc_tuning_connect_parameters_t *param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("KymeraAnc_EnterTuning");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ANC_TUNING_START);
    message->usb_rate = param->usb_rate;
    if(theKymera->busy_lock)
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message, &theKymera->busy_lock);
    }
    else if(theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message, ANC_TUNNING_START_DELAY);
    }
    else
    {
       MessageSend(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_START, message);
    }
}
#endif

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
void KymeraAnc_ExitTuning(const anc_tuning_disconnect_parameters_t *param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("KymeraAnc_ExitTuning");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ANC_TUNING_STOP);
    message->spkr_src = param->spkr_src;
    message->mic_sink = param->mic_sink;
    message->kymera_stopped_handler = param->kymera_stopped_handler;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_STOP, message);
}
#else
void KymeraAnc_ExitTuning(const anc_tuning_disconnect_parameters_t *param)
{
    PanicNotNull(param);
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("KymeraAnc_ExitTuning");
    MessageSend(&theKymera->task, KYMERA_INTERNAL_ANC_TUNING_STOP, NULL);
}
#endif

static bool kymeraAnc_CheckIfRightChannelMicEnabled(void)
{
    return ((getAncFeedBackRightMic() != microphone_none) || (getAncFeedForwardRightMic() != microphone_none));
}

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
static void kymeraAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints(const KYMERA_INTERNAL_ANC_TUNING_START_T *anc_tuning)
#else
static void kymeraAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints(void)
#endif
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Connect backend (USB) out */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_USB_LEFT),
                             StreamSinkFromOperatorTerminal(theKymera->usb_tx, 0)));
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_USB_RIGHT),
                             StreamSinkFromOperatorTerminal(theKymera->usb_tx, 1)));

    /* Connect backend (USB) in */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_rx, 0),
                             StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_USB_LEFT)));
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_rx, 1),
                             StreamSinkFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SINK_USB_RIGHT)));

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    /* Connect USB ISO in endpoint to USB Rx operator */
    PanicFalse(StreamConnect(anc_tuning->spkr_src,
                             StreamSinkFromOperatorTerminal(theKymera->usb_rx, 0)));

    /* Connect USB Tx operator to USB ISO out endpoint */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_tx, 0),
                             anc_tuning->mic_sink));
#else
    /* Connect USB ISO in endpoint to USB Rx operator */
    PanicFalse(StreamConnect(StreamUsbEndPointSource(end_point_iso_in),
                             StreamSinkFromOperatorTerminal(theKymera->usb_rx, 0)));

    /* Connect USB Tx operator to USB ISO out endpoint */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_tx, 0),
                             StreamUsbEndPointSink(end_point_iso_out)));
#endif
}

static void kymeraAnc_ConnectMicsAndSpeakerToDacChannels(chan_data_t *channel, bool anc_right_channel_enabled)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Connect microphone */
    PanicFalse(StreamConnect(channel->mic_in1,
               StreamSinkFromOperatorTerminal(theKymera->anc_tuning,
                                              ((anc_right_channel_enabled) ? ANC_TUNING_SINK_MIC1_RIGHT : ANC_TUNING_SINK_MIC1_LEFT))));
    if (channel->mic_in2)
        PanicFalse(StreamConnect(channel->mic_in2,
                  StreamSinkFromOperatorTerminal(theKymera->anc_tuning,
                                              ((anc_right_channel_enabled) ? ANC_TUNING_SINK_MIC2_RIGHT : ANC_TUNING_SINK_MIC2_LEFT))));

    /* Connect FBMON microphone */
    PanicFalse(StreamConnect(channel->fb_mon,
               StreamSinkFromOperatorTerminal(theKymera->anc_tuning,
                                              ((anc_right_channel_enabled) ? ANC_TUNING_SINK_FBMON_RIGHT : ANC_TUNING_SINK_FBMON_LEFT))));

#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT

    Sink splitter_in = StreamSinkFromOperatorTerminal(theKymera->output_splitter, SPLITTER_TERMINAL_IN_0);
    Source splitter_out_0 = StreamSourceFromOperatorTerminal(theKymera->output_splitter, SPLITTER_TERMINAL_OUT_0);
    Source splitter_out_1 = StreamSourceFromOperatorTerminal(theKymera->output_splitter, SPLITTER_TERMINAL_OUT_1);
    Source anc_tuning_dac_left = StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_DAC_LEFT);

    PanicFalse(StreamConnect(anc_tuning_dac_left, splitter_in));
    PanicFalse(StreamConnect(splitter_out_0, channel->DAC));
    PanicFalse(StreamConnect(splitter_out_1, eanc_second_dac));
#else
    /* Connect speaker */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_tuning,
                                              ((anc_right_channel_enabled) ? ANC_TUNING_SOURCE_DAC_RIGHT : ANC_TUNING_SOURCE_DAC_LEFT)),
                                                channel->DAC));
#endif
}

static void kymeraAnc_GetRightMics(microphone_number_t *ffa_mic, microphone_number_t *ffb_mic)
{
    *ffa_mic = microphone_none;
    *ffb_mic = microphone_none;

    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case hybrid_mode:
        case hybrid_mode_right_only:
            *ffa_mic = getAncFeedBackRightMic();
            *ffb_mic = getAncFeedForwardRightMic();
            break;

        case feed_back_mode:
        case feed_back_mode_right_only:
            *ffa_mic = getAncFeedBackRightMic();
            break;

        case feed_forward_mode:
        case feed_forward_mode_right_only:
            *ffa_mic = getAncFeedForwardRightMic();
            break;

        default:
            *ffa_mic = microphone_none;
            *ffb_mic = microphone_none;
            break;
    }
}

static void kymeraAnc_GetLeftMics(microphone_number_t *ffa_mic, microphone_number_t *ffb_mic)
{
    *ffa_mic = microphone_none;
    *ffb_mic = microphone_none;

    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case hybrid_mode:
        case hybrid_mode_left_only:
            *ffa_mic = getAncFeedBackLeftMic();
            *ffb_mic = getAncFeedForwardLeftMic();
            break;

        case feed_back_mode:
        case feed_back_mode_left_only:
            *ffa_mic = getAncFeedBackLeftMic();
            break;

        case feed_forward_mode:
        case feed_forward_mode_left_only:
            *ffa_mic = getAncFeedForwardLeftMic();
            break;

        default:
            *ffa_mic = microphone_none;
            *ffb_mic = microphone_none;
            break;
    }
}

static void kymeraAnc_ConfigureMicsAndDacs(uint16 usb_rate, chan_data_t *channel, bool anc_right_channel_enabled)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    theKymera->usb_rate = usb_rate;

    /* Get the DAC output sinks */
    if(anc_right_channel_enabled)
        channel->DAC = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B));
    else
        channel->DAC = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));

    PanicFalse(SinkConfigure(channel->DAC, STREAM_CODEC_OUTPUT_RATE, usb_rate));

    microphone_number_t mic1, mic2;

    (anc_right_channel_enabled) ? kymeraAnc_GetRightMics(&mic1, &mic2) : kymeraAnc_GetLeftMics(&mic1, &mic2);

    channel->mic_in1 = Kymera_GetMicrophoneSource(mic1, NULL, theKymera->usb_rate, high_priority_user);
    channel->mic_in2 = Kymera_GetMicrophoneSource(mic2, NULL, theKymera->usb_rate, high_priority_user);

    if (channel->mic_in2)
        PanicFalse(SourceSynchronise(channel->mic_in1, channel->mic_in2));

    if(anc_right_channel_enabled)
        channel->fb_mon = Kymera_GetMicrophoneSource(getAncTuningMonitorRightMic(), NULL, theKymera->usb_rate, high_priority_user);

    else
        channel->fb_mon = Kymera_GetMicrophoneSource(getAncTuningMonitorLeftMic(), NULL, theKymera->usb_rate, high_priority_user);

    if (channel->fb_mon)
        PanicFalse(SourceSynchronise(channel->mic_in1, channel->fb_mon));

#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
    /*Create and configure the Splitter.*/
    if(!anc_right_channel_enabled)
    {
        eanc_second_dac = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B));
        PanicFalse(SinkConfigure(eanc_second_dac, STREAM_CODEC_OUTPUT_RATE, usb_rate));
        kymeraAnc_CreateSplitter();
        kymeraAnc_ConfigureSplitter();
    }
#endif

}

void KymeraAnc_TuningCreateChain(const KYMERA_INTERNAL_ANC_TUNING_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint32 usb_rate = msg->usb_rate;
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    PanicFalse(msg->spkr_channels == ANC_TUNNING_USB_AUDIO_CHANNELS);
    PanicFalse(msg->mic_channels == ANC_TUNNING_USB_AUDIO_CHANNELS);
#endif
    theKymera->usb_rate = usb_rate;

    bool anc_right_channel_enabled = kymeraAnc_CheckIfRightChannelMicEnabled();

    const char anc_tuning_edkcs[] = "download_anc_tuning.edkcs";
    DEBUG_LOG("KymeraAnc_TuningCreateChain, rate %u", usb_rate);

    PanicFalse(usb_rate == ANC_TUNNING_USB_SAMPLING_RATE);

    /* Turn on audio subsystem */
    OperatorFrameworkEnable(1);

    /* Move to ANC tuning state, this prevents A2DP and HFP from using kymera */
    appKymeraSetState(KYMERA_STATE_ANC_TUNING);

    /* Set DSP clock to run at 120MHz for ANC runing use case*/
    appKymeraConfigureDspPowerMode();

    /* Create tuning chain for rihgt channel */
    if(anc_right_channel_enabled)
        kymeraAnc_ConfigureMicsAndDacs(usb_rate, &right, TRUE);
    /* Create tuning chain for left channel */
    kymeraAnc_ConfigureMicsAndDacs(usb_rate, &left, FALSE);

    /* Create ANC tuning operator */
    FILE_INDEX index = FileFind(FILE_ROOT, anc_tuning_edkcs, strlen(anc_tuning_edkcs));
    PanicFalse(index != FILE_NONE);
    theKymera->anc_tuning_bundle_id = PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */
#if defined(__QCC517X__)
    theKymera->anc_tuning = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_DOWNLOAD_ANC_TUNING_QCC517X));
#else
    theKymera->anc_tuning = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_DOWNLOAD_ANC_TUNING));
#endif
    OperatorsStandardSetUCID(theKymera->anc_tuning,UCID_ANC_TUNING);

    /* Create the operators for USB Rx & Tx audio */
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    uint16 usb_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,                              // data_format
        usb_rate / 25,                  // sample_rate
        ANC_TUNNING_USB_AUDIO_CHANNELS, // number_of_channels
        msg->frame_size * 8,            // subframe_size
        msg->frame_size * 8,            // subframe_resolution
    };
#else
    uint16 usb_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,              // data_format
        usb_rate / 25,  // sample_rate
        2,              // number_of_channels
        16,             // subframe_size
        16,             // subframe_resolution
    };
#endif
#ifdef DOWNLOAD_USB_AUDIO
    const char usb_audio_edkcs[] = "download_usb_audio.edkcs";
    index = FileFind(FILE_ROOT, usb_audio_edkcs, strlen(usb_audio_edkcs));
    PanicFalse(index != FILE_NONE);
    theKymera->usb_audio_bundle_id = PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */
#endif
    theKymera->usb_rx = (Operator)PanicFalse(VmalOperatorCreate(EB_CAP_ID_USB_AUDIO_RX));

    PanicFalse(VmalOperatorMessage(theKymera->usb_rx,
                                   usb_config, SIZEOF_OPERATOR_MESSAGE(usb_config),
                                   NULL, 0));
    theKymera->usb_tx = (Operator)PanicFalse(VmalOperatorCreate(EB_CAP_ID_USB_AUDIO_TX));
    PanicFalse(VmalOperatorMessage(theKymera->usb_tx,
                                   usb_config, SIZEOF_OPERATOR_MESSAGE(usb_config),
                                   NULL, 0));

    uint16 anc_tuning_frontend_config[4] =
    {
        OPMSG_ANC_TUNING_ID_FRONTEND_CONFIG,         // ID
        anc_right_channel_enabled,                   // 0 = mono, 1 = stereo
        (left.mic_in2 || right.mic_in2) ? 1 : 0,     // 0 = 1-mic, 1 = 2-mic
        appKymeraIsParallelAncFilterEnabled() ? 1: 0 // 0 = Normal anc mode, 1 = parallel anc or eANC mode
    };

    PanicFalse(VmalOperatorMessage(theKymera->anc_tuning,
                               &anc_tuning_frontend_config, SIZEOF_OPERATOR_MESSAGE(anc_tuning_frontend_config),
                               NULL, 0));

    /* Connect Microphones and Speaker to DAC */
    if(anc_right_channel_enabled)
        kymeraAnc_ConnectMicsAndSpeakerToDacChannels(&right, TRUE);

    kymeraAnc_ConnectMicsAndSpeakerToDacChannels(&left, FALSE);

    /* Connect USB Rx and Tx operators to USB in and out endpoints */
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    kymeraAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints(msg);
#else
    kymeraAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints();
#endif

#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
    OperatorsSplitterEnableSecondOutput(theKymera->output_splitter, TRUE);

    /* Finally start the operators */
    Operator op_list[] = {theKymera->usb_rx, theKymera->anc_tuning, theKymera->output_splitter, theKymera->usb_tx};
    PanicFalse(OperatorStartMultiple(4, op_list, NULL));
#else
    Operator op_list[] = {theKymera->usb_rx, theKymera->anc_tuning, theKymera->usb_tx};
    PanicFalse(OperatorStartMultiple(3, op_list, NULL));
#endif

    /* Ensure audio amp is on */
    appKymeraExternalAmpControl(TRUE);

    /* Set kymera lock to prevent anything else using kymera */
    appKymeraSetAncStartingLock(theKymera);
}

void KymeraAnc_TuningDestroyChain(const KYMERA_INTERNAL_ANC_TUNING_STOP_T *msg)
{
#ifndef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    PanicNotNull(msg);
#endif
    if(appKymeraGetState() == KYMERA_STATE_ANC_TUNING)
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        bool anc_right_channel_enabled = kymeraAnc_CheckIfRightChannelMicEnabled();

        /* Turn audio amp is off */
        appKymeraExternalAmpControl(FALSE);

        /* Stop the operators */
#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
        Operator op_list[] = {theKymera->usb_rx, theKymera->anc_tuning, theKymera->output_splitter, theKymera->usb_tx};
        PanicFalse(OperatorStopMultiple(4, op_list, NULL));
#else
        Operator op_list[] = {theKymera->usb_rx, theKymera->anc_tuning, theKymera->usb_tx};
        PanicFalse(OperatorStopMultiple(3, op_list, NULL));
#endif

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
        /* Disconnect USB ISO in endpoint */
        StreamDisconnect(msg->spkr_src, 0);

        /* Disconnect USB ISO out endpoint */
        StreamDisconnect(0, msg->mic_sink);
#else
        /* Disconnect USB ISO in endpoint */
        StreamDisconnect(StreamUsbEndPointSource(end_point_iso_in), 0);

        /* Disconnect USB ISO out endpoint */
        StreamDisconnect(0, StreamUsbEndPointSink(end_point_iso_out));
#endif
        /* Get the DAC output sinks */
        Sink DAC_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));

        microphone_number_t mic0, mic1;
        kymeraAnc_GetLeftMics(&mic0, &mic1);

        Source mic_in0 = Microphones_GetMicrophoneSource(mic0);
        Source fb_mon0 = Microphones_GetMicrophoneSource(getAncTuningMonitorLeftMic());

        StreamDisconnect(mic_in0, 0);
        Kymera_CloseMicrophone(mic0, high_priority_user);

        if(mic1)
        {
            Source mic_in1 = Microphones_GetMicrophoneSource(mic1);
            StreamDisconnect(mic_in1, 0);
            Kymera_CloseMicrophone(mic1, high_priority_user);
        }

        StreamDisconnect(fb_mon0, 0);
        Kymera_CloseMicrophone(getAncTuningMonitorLeftMic(), high_priority_user);

        /* Disconnect speaker */
        StreamDisconnect(0, DAC_L);
#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
        Source anc_tuning_dac_left = StreamSourceFromOperatorTerminal(theKymera->anc_tuning, ANC_TUNING_SOURCE_DAC_LEFT);
        StreamDisconnect(anc_tuning_dac_left, 0);
        StreamDisconnect(0, eanc_second_dac);
#endif

        if(anc_right_channel_enabled)
        {
            /* Get the DAC output sinks */
            Sink DAC_R = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B));

            microphone_number_t mic2, mic3;
            kymeraAnc_GetRightMics(&mic2, &mic3);

            Source mic_in2 = Microphones_GetMicrophoneSource(mic2);
            Source fb_mon1 = Microphones_GetMicrophoneSource(getAncTuningMonitorRightMic());

            StreamDisconnect(mic_in2, 0);
            Kymera_CloseMicrophone(mic2, high_priority_user);
            if(mic3)
            {
                Source mic_in3 = Microphones_GetMicrophoneSource(mic3);
                StreamDisconnect(mic_in3, 0);
                Kymera_CloseMicrophone(mic3, high_priority_user);
            }
            StreamDisconnect(fb_mon1, 0);
            Kymera_CloseMicrophone(getAncTuningMonitorRightMic(), high_priority_user);

            /* Disconnect speaker */
            StreamDisconnect(0, DAC_R);
        }
        /* Distroy operators */
#ifdef ENHANCED_ANC_USE_2ND_DAC_ENDPOINT
        OperatorsDestroy(op_list, 4);
#else
        OperatorsDestroy(op_list, 3);
#endif

        /* Unload bundle */
        PanicFalse(OperatorBundleUnload(theKymera->anc_tuning_bundle_id));
#ifdef DOWNLOAD_USB_AUDIO
        PanicFalse(OperatorBundleUnload(theKymera->usb_audio_bundle_id));
#endif
        /* Clear kymera lock and go back to idle state to allow other uses of kymera */
        appKymeraClearAncStartingLock(theKymera);
        appKymeraSetState(KYMERA_STATE_IDLE);

        /* Reset DSP clock to default*/
        appKymeraConfigureDspPowerMode();

        /* Turn off audio subsystem */
        OperatorFrameworkEnable(0);
    }

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    PanicZero(msg->kymera_stopped_handler);
    msg->kymera_stopped_handler(msg->spkr_src);
#endif
}

#if defined INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN && defined ENABLE_ANC
static bool anc_passthroughDacConnectionStatus = 0;

static bool kymeraAnc_IsPassthroughSupportChainConnectedToDac(void)
{
    DEBUG_LOG("kymeraAnc_IsPassthroughSupportChainConnectedToDac");
    return anc_passthroughDacConnectionStatus;
}

static void kymeraAnc_UpdatePassthroughSupportChainConnectionStatus(bool status)
{
    DEBUG_LOG("kymeraAnc_UpdatePassthroughSupportChainConnectionStatus");
    anc_passthroughDacConnectionStatus = status;
}

void KymeraAnc_ConnectPassthroughSupportChainToDac(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if((theKymera->state == KYMERA_STATE_IDLE) &&
       (theKymera->anc_passthough_operator) && !kymeraAnc_IsPassthroughSupportChainConnectedToDac())
    {
        DEBUG_LOG("KymeraAnc_ConnectPassthroughSupportChainToDac");
        #define DAC_RATE (48000U)
        #define PT_OUTPUT_TERMINAL (0U)

        Sink DAC_SNK_L = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
        PanicFalse(SinkConfigure(DAC_SNK_L, STREAM_CODEC_OUTPUT_RATE, DAC_RATE));
        PanicFalse(SinkConfigure(DAC_SNK_L, STREAM_RM_ENABLE_DEFERRED_KICK, 0));

        /* Connect speaker */
        PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->anc_passthough_operator, PT_OUTPUT_TERMINAL),
                                 DAC_SNK_L));

        /* Configure DSP for low power */
        appKymeraConfigureDspPowerMode();

        /* Start the operator */
        Operator op_list[] = {theKymera->anc_passthough_operator};
        PanicFalse(OperatorStartMultiple(1, op_list, NULL));

        kymeraAnc_UpdatePassthroughSupportChainConnectionStatus(TRUE);
    }
    else
    {
        DEBUG_LOG("KymeraAnc_ConnectPassthroughSupportChainToDac, igored as either passthrough chain is not created or already connected to Dac");
    }
}

void KymeraAnc_DisconnectPassthroughSupportChainFromDac(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if((theKymera->anc_passthough_operator) && kymeraAnc_IsPassthroughSupportChainConnectedToDac())
    {
        DEBUG_LOG("KymeraAnc_DisconnectPassthroughSupportChainFromDac");
        /* Stop the operators */
        Operator op_list[] = {theKymera->anc_passthough_operator};
        PanicFalse(OperatorStopMultiple(1, op_list, NULL));

        /* Get the DAC */
        Sink sink_line_outL = PanicNull(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
        /* Disconnect Dac */
        StreamDisconnect(0, sink_line_outL);
        kymeraAnc_UpdatePassthroughSupportChainConnectionStatus(FALSE);
    }
    else
    {
        DEBUG_LOG("KymeraAnc_DisconnectPassthroughSupportChainFromDac, ignored as passthrough support chain is not active");
    }
}

void KymeraAnc_PreStateTransition(appKymeraState state)
{
    if(AncIsEnabled())
    {
        if (state == KYMERA_STATE_IDLE)
        {
            /*! Kymera new state is idle; so run ANC passthrough support chain to suppress spurious tones */
            KymeraAnc_ConnectPassthroughSupportChainToDac();
        }
        else
        {
            /*! Kymera new state is not idle; so stop ANC passthrough support chain */
            KymeraAnc_DisconnectPassthroughSupportChainFromDac();
        }
    }
}

void KymeraAnc_CreatePassthroughSupportChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(theKymera->anc_passthough_operator == INVALID_OPERATOR)
    {
        DEBUG_LOG("KymeraAnc_CreatePassthroughSupportChain");
        /*Enable Audio subsystem before creating the support chain*/
        OperatorFrameworkEnable(1);
        #define DAC_RATE (48000U)
        /* Operator applies unity gain (0dB) */
        #define INITIAL_OPERATOR_GAIN    (0U)
        Operator op_pt;

        op_pt = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_BASIC_PASS));

        /* Configure passthrough operator */
        if(op_pt)
        {
            OperatorsSetPassthroughDataFormat(op_pt, operator_data_format_pcm);
            OperatorsSetPassthroughGain(op_pt, INITIAL_OPERATOR_GAIN);
            theKymera->anc_passthough_operator = op_pt;
        }
    }
    else
    {
        DEBUG_LOG("KymeraAnc_CreatePassthroughSupportChain, ignored as it has already created");

    }
}


void KymeraAnc_DestroyPassthroughSupportChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if((theKymera->anc_passthough_operator) && !kymeraAnc_IsPassthroughSupportChainConnectedToDac())
    {
        /* Destroy the operator */
        Operator op_list[] = {theKymera->anc_passthough_operator};
        OperatorsDestroy(op_list, 1);
        theKymera->anc_passthough_operator = INVALID_OPERATOR;
        /*Disable Audio subsystem */
        OperatorFrameworkEnable(0);
    }
    else
    {
        DEBUG_LOG("KymeraAnc_DestroyPassthroughSupportChain, ignored as passthrough support chain is not active");
    }
}

#else

void KymeraAnc_CreatePassthroughSupportChain(void)
{

}

void KymeraAnc_DestroyPassthroughSupportChain(void)
{

}

void KymeraAnc_ConnectPassthroughSupportChainToDac(void)
{

}

void KymeraAnc_DisconnectPassthroughSupportChainFromDac(void)
{

}

#endif

#ifdef ENABLE_ANC

void KymeraAnc_UpdateDspClock(void)
{
    appKymeraConfigureDspPowerMode();
}

#else

void KymeraAnc_UpdateDspClock(void)
{

}
#endif
