/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_audio_source_audio.c
\brief  Driver for USB Audio source registration and handling.
*/

#include "kymera_adaptation_audio_protected.h"
#include "usb_audio_fd.h"
#include "usb_audio_defines.h"

static bool usbAudio_GetConnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeConnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_GetDisconnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeDisconnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_IsAudioRouted(audio_source_t source);
static source_status_t usbAudio_SetState(audio_source_t source, source_state_t state);

static const audio_source_audio_interface_t usb_audio_interface =
{
    .GetConnectParameters = usbAudio_GetConnectParameters,
    .ReleaseConnectParameters = usbAudio_FreeConnectParameters,
    .GetDisconnectParameters = usbAudio_GetDisconnectParameters,
    .ReleaseDisconnectParameters = usbAudio_FreeDisconnectParameters,
    .IsAudioRouted = NULL,
    .SetState = usbAudio_SetState
};

static bool usbAudio_GetConnectParameters(audio_source_t source,
                                       source_defined_params_t * source_params)
{
    DEBUG_LOG_VERBOSE("usbAudio_GetConnectParameters");
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(usbAudio_IsAudioRouted(source))
    {
        /* Note that USB Audio maybe available, but could have lost to priority so disconnected.
         * It can then get foreground focus again. */
        PanicNull(source_params);
        usb_audio_connect_parameters_t *connect_params =
                    (usb_audio_connect_parameters_t *)PanicNull(malloc(sizeof(usb_audio_connect_parameters_t)));
        memset(connect_params, 0, sizeof(usb_audio_connect_parameters_t));

        usb_audio_streaming_info_t *streaming_info =
                UsbAudio_GetStreamingInfo(usb_audio,
                                          USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER);
        PanicNull(streaming_info);
        usb_audio->headphone->spkr_sample_rate = streaming_info->current_sampling_rate;

        connect_params->spkr_src = usb_audio->headphone->spkr_src;
        connect_params->mic_sink = usb_audio->headphone->mic_sink;
        connect_params->sample_freq = usb_audio->headphone->spkr_sample_rate;

        DEBUG_LOG_DEBUG("USB Audio: spkr_sample_rate %d", connect_params->sample_freq);

        connect_params->channels = streaming_info->channels;
        connect_params->frame_size = streaming_info->frame_size;

        connect_params->volume = AudioSources_CalculateOutputVolume(usb_audio->headphone->audio_source);

        /* Update TTP values for Audio chain */
        connect_params->max_latency_ms = usb_audio->config->max_latency_ms;
        connect_params->min_latency_ms = usb_audio->config->min_latency_ms;
        connect_params->target_latency_ms = usb_audio->config->target_latency_ms;

        /* usbAudio_Audio_KymeraStoppedHandler() need to know whether there is 
         * a pending connect request, so that it can reset chain_active to FALSE.
         */ 
        usb_audio->headphone->chain_required = TRUE;

        /* Audio source has read data for Audio chain creation, it will not
         * do again until it is informed. Lets keep this status and if
         * Host change any of above parameter then need to inform
         * Audio source.
         */
        usb_audio->headphone->chain_active = TRUE;

        DEBUG_LOG_VERBOSE("USB Audio channels = %x, frame=%x, Freq=%d",
            connect_params->channels, connect_params->frame_size, connect_params->sample_freq);

        source_params->data = (void *)connect_params;
        source_params->data_length = sizeof(usb_audio_connect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeConnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(usb_audio_connect_parameters_t));
    if(source == audio_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static void usbAudio_KymeraAudioStoppedHandler(Source source)
{
    DEBUG_LOG_VERBOSE("usbAudio_KymeraAudioStoppedHandler");
    usb_audio_info_t *usb_audio = UsbAudio_FindInfoBySource(source);

    if (usb_audio && usb_audio->headphone && !(usb_audio->headphone->chain_required))
    {
        usb_audio->headphone->chain_active = FALSE;

        DEBUG_LOG_WARN("UsbAudio: Audio chain released");

        if (usb_audio->is_pending_delete)
        {
            UsbAudio_TryFreeData(usb_audio);
        }
    }
}

static bool usbAudio_GetDisconnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params)
{
    DEBUG_LOG_VERBOSE("usbAudio_GetDisconnectParameters");
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(source == audio_source_usb && usb_audio != NULL)
    {
        usb_audio_disconnect_parameters_t *disconnect_params;
        PanicNull(source_params);
        disconnect_params = (usb_audio_disconnect_parameters_t *)PanicNull(malloc(sizeof(usb_audio_disconnect_parameters_t)));

        disconnect_params->source = usb_audio->headphone->spkr_src;
        disconnect_params->sink = usb_audio->headphone->mic_sink;
        disconnect_params->kymera_stopped_handler = usbAudio_KymeraAudioStoppedHandler;

        usb_audio->headphone->chain_required = FALSE;

        source_params->data = (void *)disconnect_params;
        source_params->data_length = sizeof(usb_audio_disconnect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeDisconnectParameters(audio_source_t source,
                                       source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(usb_audio_disconnect_parameters_t));
    if(source == audio_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static bool usbAudio_IsAudioRouted(audio_source_t source)
{
    bool is_available = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(usb_audio != NULL && usb_audio->headphone->source_connected)
    {
        is_available = TRUE;
    }

    return is_available;
}

static source_status_t usbAudio_SetState(audio_source_t source, source_state_t state)
{
    DEBUG_LOG_INFO("usbAudio_SetState source=%d state=%d", source, state);
    /* Function logic to be implemented */
    return source_status_ready;
}

const audio_source_audio_interface_t * UsbAudioFd_GetSourceAudioInterface(void)
{
    return &usb_audio_interface;
}
