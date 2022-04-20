/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_audio_source_voice.c
\brief Driver for USB Voice source registration and handling.
*/

#include "kymera_adaptation_voice_protected.h"
#include "usb_audio_fd.h"
#include "usb_audio_defines.h"

static bool usbAudio_GetVoiceConnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeVoiceConnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_GetVoiceDisconnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeVoiceDisconnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_IsVoiceAvailable(voice_source_t source);
static void usbAudio_KymeraVoiceStoppedHandler(Source source);

static const voice_source_audio_interface_t usb_voice_interface =
{
    .GetConnectParameters = usbAudio_GetVoiceConnectParameters,
    .ReleaseConnectParameters = usbAudio_FreeVoiceConnectParameters,
    .GetDisconnectParameters = usbAudio_GetVoiceDisconnectParameters,
    .ReleaseDisconnectParameters = usbAudio_FreeVoiceDisconnectParameters,
    .IsAudioRouted = NULL,
    .IsVoiceChannelAvailable = usbAudio_IsVoiceAvailable,
    .SetState = NULL
};

static usb_voice_mode_t usbAudio_GetVoiceBand(uint32 sample_rate)
{
    usb_voice_mode_t ret = usb_voice_mode_wb;
    switch(sample_rate)
    {
        case SAMPLE_RATE_32K:
            ret = usb_voice_mode_uwb;
        break;
        case SAMPLE_RATE_16K:
            ret = usb_voice_mode_wb;
        break;
        case SAMPLE_RATE_8K:
            ret = usb_voice_mode_nb;
        break;
        default:
            DEBUG_LOG("USB Voice: Unsupported Sample rate");
            PanicFalse(FALSE);
        break;
    }
    return ret;
}

static bool usbAudio_GetVoiceConnectParameters(voice_source_t source,
                                       source_defined_params_t * source_params)
{
    DEBUG_LOG_VERBOSE("usbAudio_GetVoiceConnectParameters");
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);

    if(usbAudio_IsVoiceAvailable(source))
    {
        DEBUG_LOG_VERBOSE("usbAudio_GetVoiceConnectParameters: spkr %d  mic %d",
                       usb_audio->headset->spkr_active, usb_audio->headset->mic_active);
        PanicNull(source_params);

        usb_audio_streaming_info_t *streaming_info = NULL;
        usb_voice_connect_parameters_t *connect_params =
                    (usb_voice_connect_parameters_t *)PanicNull(malloc(sizeof(usb_voice_connect_parameters_t)));
        memset(connect_params, 0, sizeof(usb_voice_connect_parameters_t));

        streaming_info = UsbAudio_GetStreamingInfo(usb_audio,USB_AUDIO_DEVICE_TYPE_VOICE_MIC);
        PanicNull(streaming_info);
        usb_audio->headset->mic_sample_rate = streaming_info->current_sampling_rate;

        connect_params->mic_sink = usb_audio->headset->mic_sink;
        connect_params->mic_sample_rate = usb_audio->headset->mic_sample_rate;

        PanicZero(usb_audio->headset->mic_enabled);

#ifdef USB_SUPPORT_HEADPHONE_SPKR_IN_VOICE_CHAIN
        usb_audio_info_t *headphone_audio = UsbAudioFd_GetHeadphoneInfo(audio_source_usb);
        if((!usb_audio->headset->spkr_enabled && headphone_audio && headphone_audio->headphone->spkr_enabled) ||
                (!usb_audio->headset->spkr_active && headphone_audio && headphone_audio->headphone->spkr_active))
        {
            usb_audio->headset->alt_spkr_connected = TRUE;
            DEBUG_LOG_VERBOSE("USB Voice: alt_spkr_connected");
            streaming_info = UsbAudio_GetStreamingInfo(headphone_audio,USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER);
            PanicNull(streaming_info);
            headphone_audio->headphone->spkr_sample_rate = streaming_info->current_sampling_rate;

            connect_params->spkr_src = headphone_audio->headphone->spkr_src;
            connect_params->spkr_sample_rate = headphone_audio->headphone->spkr_sample_rate;
            connect_params->spkr_channels = streaming_info->channels;
            connect_params->volume = AudioSources_CalculateOutputVolume(headphone_audio->headphone->audio_source);
        }
        else
#endif
        {
            PanicZero(usb_audio->headset->spkr_enabled);
            streaming_info = UsbAudio_GetStreamingInfo(usb_audio,USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER);
            PanicNull(streaming_info);
            usb_audio->headset->spkr_sample_rate = streaming_info->current_sampling_rate;

            connect_params->spkr_src = usb_audio->headset->spkr_src;
            connect_params->spkr_sample_rate = usb_audio->headset->spkr_sample_rate;
            connect_params->spkr_channels = streaming_info->channels;
            connect_params->volume = VoiceSources_CalculateOutputVolume(usb_audio->headset->audio_source);
        }

        DEBUG_LOG_DEBUG("USB Voice: mic_sample_rate %d  spkr_sample_rate %d spkr_channels = %x",
                        connect_params->mic_sample_rate, connect_params->spkr_sample_rate,
                        connect_params->spkr_channels);

        connect_params->mode = usbAudio_GetVoiceBand(connect_params->mic_sample_rate);

        /* Update TTP values for Voice chain */
        connect_params->max_latency_ms = usb_audio->config->max_latency_ms;
        connect_params->min_latency_ms = usb_audio->config->min_latency_ms;
        connect_params->target_latency_ms = usb_audio->config->target_latency_ms;

        connect_params->kymera_stopped_handler = usbAudio_KymeraVoiceStoppedHandler;

        /* usbAudio_Voice_KymeraStoppedHandler() need to know whether there is 
         * a pending connect request, so that it can reset chain_active to FALSE.
         */ 
        usb_audio->headset->chain_required = TRUE;

        /* Audio source has read data for Audio chain creation, it will not
         * do again until it is informed. Lets keep this status and if
         * Host change any of above parameter then need to inform
         * Audio source.
         */
        usb_audio->headset->chain_active = TRUE;

        source_params->data = (void *)connect_params;
        source_params->data_length = sizeof(usb_voice_connect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeVoiceConnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(usb_voice_connect_parameters_t));
    if(source == voice_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static void usbAudio_KymeraVoiceStoppedHandler(Source source)
{
    DEBUG_LOG_VERBOSE("usbAudio_KymeraVoiceStoppedHandler");
    usb_audio_info_t *usb_audio = UsbAudio_FindInfoBySource(source);

    if (usb_audio && usb_audio->headset && !(usb_audio->headset->chain_required))
    {
        usb_audio->headset->chain_active = FALSE;

        DEBUG_LOG_WARN("UsbAudio: Voice chain released");

        if (usb_audio->is_pending_delete)
        {
            UsbAudio_TryFreeData(usb_audio);
        }
    }
}

static bool usbAudio_GetVoiceDisconnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params)
{
    DEBUG_LOG_VERBOSE("usbAudio_GetVoiceDisconnectParameters");
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);

    if(source == voice_source_usb && usb_audio != NULL)
    {
        usb_audio->headset->alt_spkr_connected = FALSE;
        usb_voice_disconnect_parameters_t *disconnect_params;
        PanicNull(source_params);
        disconnect_params = (usb_voice_disconnect_parameters_t *)PanicNull(malloc(sizeof(usb_voice_disconnect_parameters_t)));

        disconnect_params->spkr_src = usb_audio->headset->spkr_src;
        disconnect_params->mic_sink = usb_audio->headset->mic_sink;
        disconnect_params->kymera_stopped_handler = usbAudio_KymeraVoiceStoppedHandler;

        usb_audio->headset->chain_required = FALSE;

        source_params->data = (void *)disconnect_params;
        source_params->data_length = sizeof(usb_voice_disconnect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeVoiceDisconnectParameters(voice_source_t source,
                                       source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(usb_voice_disconnect_parameters_t));

    if(source == voice_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static bool usbAudio_IsVoiceAvailable(voice_source_t source)
{
    bool is_available = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);

    if(usb_audio != NULL && usb_audio->headset->source_connected)
    {
        is_available = TRUE;
    }

    return is_available;
}

const voice_source_audio_interface_t * UsbAudioFd_GetSourceVoiceInterface(void)
{
    return &usb_voice_interface;
}
