/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio Function Driver
*/

#include "usb_audio_fd.h"
#include "usb_audio_class_10.h"

#include <panic.h>
#include "kymera.h"
#include "volume_messages.h"
#include "telephony_messages.h"
#include "ui.h"

#include "usb_source.h"

#define USB_AUDIO_MSG_SEND(task, id, msg)       if(task != NULL) \
                                                   task->handler(task, id, msg)

usb_audio_info_t *usbaudio_globaldata = NULL;

static void UsbAudio_HandleMessage(usb_device_index_t device_index,
                                     MessageId id, Message message);
static void UsbAudio_ClassEvent(uac_ctx_t class_ctx,
                                uint8 interface_index,
                                uac_message_t uac_message);
static void usbAudioUpdateHeadphoneVolume(usb_audio_info_t *usb_audio);
static void usbAudioUpdateHeadsetVolume(usb_audio_info_t *usb_audio);
static uint8 usbAudio_VolumeToSteps(const usb_audio_volume_config_t *volume_config,
                                    int8 volume_in_db, uint8 mute_status);

#define USB_AUDIO_GET_DATA()         usbaudio_globaldata

Task usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_COUNT];

/****************************************************************************
    Initialize and add USB Voice Class Driver
*/
static void usbAudioAddHeadset(usb_audio_info_t *usb_audio)
{
    usb_audio_streaming_info_t *streaming_info;

    DEBUG_LOG_WARN("UsbAudio: headset");

    usb_audio->headset = PanicUnlessNew(usb_audio_headset_info_t);
    memset(usb_audio->headset, 0, sizeof(usb_audio_headset_info_t));

    streaming_info = UsbAudio_GetStreamingInfo(usb_audio,
                                               USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER);
    if (streaming_info)
    {
        usb_audio->headset->spkr_enabled = TRUE;
        usb_audio->headset->spkr_src = StreamUsbEndPointSource(streaming_info->ep_address);
    }

    streaming_info = UsbAudio_GetStreamingInfo(usb_audio,
                                               USB_AUDIO_DEVICE_TYPE_VOICE_MIC);
    if (streaming_info)
    {
        usb_audio->headset->mic_enabled = TRUE;
        usb_audio->headset->mic_sink = StreamUsbEndPointSink(streaming_info->ep_address);
    }

    UsbSource_RegisterVoiceControl();

    usb_audio->headset->audio_source = voice_source_usb;
    /* Register with audio source for Audio use case */
    VoiceSources_RegisterAudioInterface(voice_source_usb,
                              UsbAudioFd_GetSourceVoiceInterface());

    /* Register with volume source for Audio use case */
    VoiceSources_RegisterVolume(voice_source_usb,
        UsbAudioFd_GetVoiceSourceVolumeInterface());

    /* Init default volume for USB Voice */
    usb_audio->headset->spkr_volume_steps =
            usbAudio_VolumeToSteps(&usb_audio->config->volume_config,
                                   usb_audio->config->volume_config.target_db, 0 /* mute */);
}

/****************************************************************************
    Initialize and add USB Audio Class Driver
*/
static void usbAudioAddHeadphone(usb_audio_info_t *usb_audio)
{
    usb_audio_streaming_info_t *streaming_info;

    DEBUG_LOG_WARN("UsbAudio: headphone");

    usb_audio->headphone = PanicUnlessNew(usb_audio_headphone_info_t);
    memset(usb_audio->headphone, 0, sizeof(usb_audio_headphone_info_t));

    streaming_info = UsbAudio_GetStreamingInfo(usb_audio,
                                               USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER);
    if (streaming_info)
    {
        usb_audio->headphone->spkr_enabled = TRUE;
        usb_audio->headphone->spkr_src = StreamUsbEndPointSource(streaming_info->ep_address);
    }

    streaming_info = UsbAudio_GetStreamingInfo(usb_audio,
                                               USB_AUDIO_DEVICE_TYPE_AUDIO_MIC);
    if (streaming_info)
    {
        usb_audio->headphone->mic_enabled = TRUE;
        usb_audio->headphone->mic_sink = StreamUsbEndPointSink(streaming_info->ep_address);
    }

    UsbSource_RegisterAudioControl();

    usb_audio->headphone->audio_source = audio_source_usb;
    /* Register with audio source for Audio use case */
    AudioSources_RegisterAudioInterface(audio_source_usb,
                              UsbAudioFd_GetSourceAudioInterface());

    /* Register with volume source for Audio use case */
    AudioSources_RegisterVolume(audio_source_usb,
        UsbAudioFd_GetAudioSourceVolumeInterface());

    usb_audio->headphone->spkr_volume_steps =
            usbAudio_VolumeToSteps(&usb_audio->config->volume_config,
                                   usb_audio->config->volume_config.target_db, 0);
}

/****************************************************************************
    Return function table for supported USB class driver
*/
static const usb_fn_tbl_uac_if * usbAudioGetFnTbl(usb_audio_class_rev_t rev)
{
    const usb_fn_tbl_uac_if *tbl = NULL;

    switch(rev)
    {
        case USB_AUDIO_CLASS_REV_1:
            tbl = UsbAudioClass10_GetFnTbl();
            break;

        default:
            DEBUG_LOG("Unsupported USB Class Revision 0x%x\n", rev);
            break;
    }

    return tbl;
}

/****************************************************************************
    Inform Audio source for USB Audio disconnection.
*/
static void usbAudioDisconnectAudioMsg(usb_audio_info_t *usb_audio)
{
    if(!usb_audio->headphone->source_connected)
    {
        return;
    }

    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA]);

    usb_audio->headphone->source_connected = FALSE;

    /* with respect to audio context, host has just connected but not actively streaming */
    UsbSource_SetAudioContext(context_audio_connected);

    DEBUG_LOG_ALWAYS("USB Audio: Audio Disconnected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA],
                       USB_AUDIO_DISCONNECTED_IND,
                       (Message)(&usb_audio->headphone->audio_source));
}

/****************************************************************************
    Inform Audio source for USb Audio connection.
*/
static void usbAudioConnectAudioMsg(usb_audio_info_t *usb_audio)
{
    if(usb_audio->is_pending_delete || usb_audio->headphone->source_connected)
    {
        return;
    }
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA]);

    usb_audio->headphone->source_connected = TRUE;

    /* with respect to audio context, this means host is actively streaming audio */
    UsbSource_SetAudioContext(context_audio_is_streaming);

    DEBUG_LOG_ALWAYS("USB Audio: Audio Connected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA],
                       USB_AUDIO_CONNECTED_IND,
                       (Message)(&usb_audio->headphone->audio_source));
}

/****************************************************************************
    Inform Voice source for USB Voice disconnection.
*/
static void usbAudioDisconnectVoiceMsg(usb_audio_info_t *usb_audio)
{
    if(!usb_audio->headset->source_connected)
    {
        return;
    }
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY]);

    usb_audio->headset->source_connected = FALSE;

    /* with respect to audio context, host has just connected but not actively streaming */
    UsbSource_SetVoiceState(USB_SOURCE_VOICE_CONNECTED);

    DEBUG_LOG_ALWAYS("USB Audio: Voice Disconnected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY],
                       TELEPHONY_AUDIO_DISCONNECTED,
                       (Message)(&usb_audio->headset->audio_source));

}

/****************************************************************************
    Inform Voice source for USB Voice connection.
*/
static void usbAudioConnectVoiceMsg(usb_audio_info_t *usb_audio)
{
    if(usb_audio->is_pending_delete || usb_audio->headset->source_connected )
    {
        return;
    }
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY]);

    usb_audio->headset->source_connected = TRUE;

    /* with respect to audio context, this means host is actively streaming audio */
    UsbSource_SetVoiceState(USB_SOURCE_VOICE_ACTIVE);

    DEBUG_LOG_ALWAYS("USB Audio: Voice Connected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY],
                       TELEPHONY_AUDIO_CONNECTED,
                       (Message)(&usb_audio->headset->audio_source));
}

/****************************************************************************
    Update USB Audio/Voice connection based on status of speaker/mic interface of headphone/headset
*/
static void usbAudioUpdateConnections(void)
{
    usb_audio_info_t *headset_audio = UsbAudioFd_GetHeadsetInfo(voice_source_usb);
    usb_audio_info_t *headphone_audio = UsbAudioFd_GetHeadphoneInfo(audio_source_usb);

    if (headset_audio)
    {
        if (headset_audio->headset->mic_active || headset_audio->headset->spkr_active)
        {
#ifdef USB_SUPPORT_HEADPHONE_SPKR_IN_VOICE_CHAIN
            /* headset MIC is active - connect if not already or reconnect
                    * if speaker changed */
            usb_audio_device_type_t new_voice_speaker, prev_voice_speaker;

            prev_voice_speaker = headset_audio->headset->alt_spkr_connected ?
                        USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER:
                        USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER;
            new_voice_speaker = prev_voice_speaker;

            if(headset_audio->headset->spkr_active ||
                    (headphone_audio && headphone_audio->headphone->spkr_active))
            {
                new_voice_speaker = headset_audio->headset->spkr_active ?
                            USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER:
                            USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER;

                if (headset_audio->headset->source_connected &&
                        new_voice_speaker != prev_voice_speaker)
                {
                    usbAudioDisconnectVoiceMsg(headset_audio);
                }
            }
#endif

            if (!headset_audio->headset->source_connected)
            {
                usbAudioConnectVoiceMsg(headset_audio);
                /* Update mic mute status and update headset speaker volume level if it is active*/
                usbAudioUpdateHeadsetVolume(headset_audio);
#ifdef USB_SUPPORT_HEADPHONE_SPKR_IN_VOICE_CHAIN
                if (new_voice_speaker == USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
                {
                    usbAudioUpdateHeadphoneVolume(headphone_audio);
                }
#endif
            }
        }
        else if (headset_audio->headset->source_connected)
        {
            /* headset Mic & Speaker interfaces are not active - disconnect if connected */
            usbAudioDisconnectVoiceMsg(headset_audio);
        }
    }

    if (headphone_audio)
    {
        if (headphone_audio->headphone->spkr_active &&
                !headphone_audio->headphone->source_connected)
        {
            usbAudioConnectAudioMsg(headphone_audio);
            usbAudioUpdateHeadphoneVolume(headphone_audio);
        }
        else if (!headphone_audio->headphone->spkr_active &&
                 headphone_audio->headphone->source_connected)
        {
            usbAudioDisconnectAudioMsg(headphone_audio);
        }
    }
}

/****************************************************************************
    Update status for MIC/Speaker for Headset and Headphone.
*/
static void usbAudioUpdateDeviceStatus(usb_audio_info_t *usb_audio,
                                  uint16 interface, uint16 altsetting)
{
    const usb_audio_interface_config_list_t *intf_list = usb_audio->config->intf_list;
    usb_audio_streaming_info_t  *streaming_info = usb_audio->streaming_info;

    for (uint8 i=0; i < usb_audio->num_interfaces; i++)
    {
        if (streaming_info[i].interface == interface)
        {
            switch(intf_list->intf[i].type)
            {
                case USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER:
                    DEBUG_LOG_INFO("USB Voice Spkr %x, %x", interface, altsetting);
                    PanicNull(usb_audio->headset);
                    usb_audio->headset->spkr_active = altsetting ? TRUE : FALSE;
                    break;
                case USB_AUDIO_DEVICE_TYPE_VOICE_MIC:
                    DEBUG_LOG_INFO("USB Voice Mic %x, %x",  interface, altsetting);
                    PanicNull(usb_audio->headset);
                    usb_audio->headset->mic_active = altsetting ? TRUE : FALSE;
                    break;
                case USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER:
                    DEBUG_LOG_INFO("USB Audio: Spkr %x, %x", interface, altsetting);
                    PanicNull(usb_audio->headphone);
                    usb_audio->headphone->spkr_active = altsetting ? TRUE : FALSE;
                    break;
                case USB_AUDIO_DEVICE_TYPE_AUDIO_MIC:
                    DEBUG_LOG_INFO("USB Audio: Mic %x, %x",  interface, altsetting);
                    PanicNull(usb_audio->headphone);
                    usb_audio->headphone->mic_active = altsetting ? TRUE : FALSE;
                    break;
                default:
                    DEBUG_LOG_ERROR("usbAudio: Unexpected Device Type %x", intf_list->intf[i].type);
                    Panic();
            }
            usbAudioUpdateConnections();
            return;
        }
    }
}

/****************************************************************************
    Converts volume DB in steps.
*/
static uint8 usbAudio_VolumeToSteps(const usb_audio_volume_config_t *volume_config,
                                    int8 volume_in_db, uint8 mute_status)
{
    int min_db = volume_config->min_db;
    int max_db = volume_config->max_db;

    if (volume_in_db <= min_db || mute_status)
    {
        return USB_AUDIO_VOLUME_MIN_STEPS;
    }
    if (volume_in_db >= max_db)
    {
        return USB_AUDIO_VOLUME_MAX_STEPS;
    }

    /* scale remaining dB values across remaining steps */
    min_db += 1;
    max_db -= 1;
    int steps = USB_AUDIO_VOLUME_NUM_STEPS - 2;
    int range = max_db - min_db + 1;
    int value = volume_in_db - min_db;

    return (uint8)(USB_AUDIO_VOLUME_MIN_STEPS + 1 + steps * value / range);
}

/****************************************************************************
    Update USB audio volume for active Voice chain
*/
static void usbAudioUpdateHeadsetVolume(usb_audio_info_t *usb_audio)
{
    if(usb_audio->headset != NULL && (usb_audio->headset->spkr_active || usb_audio->headset->mic_active))
    {
        uint8 volume_steps = 0;
        int8  out_vol_db = 0;
        int8  in_vol_db = 0;
        uint8 out_mute = 0;
        uint8 in_mute = 0;

        const usb_audio_volume_config_t *volume_config = &usb_audio->config->volume_config;
        const usb_audio_interface_config_list_t *intf_list = usb_audio->config->intf_list;
        usb_audio_streaming_info_t  *streaming_info = usb_audio->streaming_info;;

        for (uint8 i=0; i < usb_audio->num_interfaces; i++)
        {
            if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_MIC)
            {
                in_vol_db =  streaming_info[i].volume_status.volume_db;
                in_mute = streaming_info[i].volume_status.mute_status;
            }
            else if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER)
            {
                out_vol_db = streaming_info[i].volume_status.volume_db;
                out_mute =  streaming_info[i].volume_status.mute_status;
            }
        }

        DEBUG_LOG_DEBUG("USB Audio headset: Scaled Gain %ddB Mute %X\n",
                  out_vol_db, out_mute);

        UNUSED(in_vol_db);

        if(usb_audio->headset->spkr_active)
        {
            volume_steps = usbAudio_VolumeToSteps(volume_config,
                                                  out_vol_db, out_mute);

            if(volume_steps != usb_audio->headset->spkr_volume_steps)
            {
                usb_audio->headset->spkr_volume_steps = volume_steps;

                DEBUG_LOG_DEBUG("USB Audio headset: volume steps = %d\n",
                        usb_audio->headset->spkr_volume_steps);

                /* Update volume structure */
                Volume_SendVoiceSourceVolumeUpdateRequest(
                                      usb_audio->headset->audio_source,
                                      event_origin_external,
                                      usb_audio->headset->spkr_volume_steps);
            }
        }

        /* Re-configure audio chain */
        appKymeraUsbVoiceMicMute(in_mute);
    }
}

/****************************************************************************
    Update USB audio volume for active Audio chain
*/
static void usbAudioUpdateHeadphoneVolume(usb_audio_info_t *usb_audio)
{
    if(usb_audio->headphone != NULL && (usb_audio->headphone->spkr_active || usb_audio->headphone->mic_active))
    {
        uint8 volume_steps = 0;
        int8  out_vol_db = 0;
        int8  in_vol_db = 0;
        uint8 out_mute = 0;
        uint8 in_mute = 0;

        const usb_audio_volume_config_t *volume_config = &usb_audio->config->volume_config;
        const usb_audio_interface_config_list_t *intf_list = usb_audio->config->intf_list;
        usb_audio_streaming_info_t  *streaming_info = usb_audio->streaming_info;;

        for (uint8 i=0; i < usb_audio->num_interfaces; i++)
        {
            if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_AUDIO_MIC)
            {
                in_vol_db =  streaming_info[i].volume_status.volume_db;
                in_mute = streaming_info[i].volume_status.mute_status;
            }
            else if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
            {
                out_vol_db = streaming_info[i].volume_status.volume_db;
                out_mute =  streaming_info[i].volume_status.mute_status;
            }
        }

        DEBUG_LOG_DEBUG("USB Audio headphone: Scaled Gain %ddB Mute %X\n",
                  out_vol_db, out_mute);

        UNUSED(in_vol_db);
        UNUSED(in_mute);

        if(usb_audio->headphone->spkr_active)
        {
            volume_steps = usbAudio_VolumeToSteps(volume_config,
                                                  out_vol_db, out_mute);

            if(volume_steps != usb_audio->headphone->spkr_volume_steps)
            {
                usb_audio->headphone->spkr_volume_steps = volume_steps;

                DEBUG_LOG_DEBUG("USB Audio headphone: volume steps = %d\n",
                          usb_audio->headphone->spkr_volume_steps);

                /* Update volume structure */
                Volume_SendAudioSourceVolumeUpdateRequest(
                            usb_audio->headphone->audio_source,
                            event_origin_external,
                            usb_audio->headphone->spkr_volume_steps);

#ifdef USB_SUPPORT_HEADPHONE_SPKR_IN_VOICE_CHAIN
                usb_audio_info_t *headset_audio = UsbAudioFd_GetHeadsetInfo(voice_source_usb);
                if(headset_audio && headset_audio->headset->alt_spkr_connected)
                {
                    /* Update volume structure */
                    Volume_SendVoiceSourceVolumeUpdateRequest(
                                voice_source_usb,
                                event_origin_external,
                                usb_audio->headphone->spkr_volume_steps);
                }
#endif
            }
        }
    }
}


/****************************************************************************
    Update sample rate for audio devices
*/
static void usbAudioSetDeviceSamplingRate(usb_audio_info_t *usb_audio, uint8 interface_index)
{
    const usb_audio_interface_config_list_t *intf_list = usb_audio->config->intf_list;
    usb_audio_streaming_info_t  *streaming_info = usb_audio->streaming_info;
    bool is_headset_rate_modified = FALSE;
    bool is_headphone_rate_modified = FALSE;

    switch(intf_list->intf[interface_index].type)
    {
        case USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER:
        {
            PanicNull(usb_audio->headset);
            if(usb_audio->headset->spkr_active && usb_audio->headset->spkr_sample_rate != streaming_info[interface_index].current_sampling_rate)
            {
                DEBUG_LOG_INFO("USB Audio: Headset Spkr sample rate %d -> %d",
                               usb_audio->headset->spkr_sample_rate,  streaming_info[interface_index].current_sampling_rate);
                is_headset_rate_modified = TRUE;
            }
            break;
        }
        case USB_AUDIO_DEVICE_TYPE_VOICE_MIC:
        {
            PanicNull(usb_audio->headset);
            if(usb_audio->headset->mic_active && usb_audio->headset->mic_sample_rate != streaming_info[interface_index].current_sampling_rate)
            {
                DEBUG_LOG_INFO("USB Audio: Headset Mic sample rate %d -> %d",
                               usb_audio->headset->mic_sample_rate,  streaming_info[interface_index].current_sampling_rate);
                is_headset_rate_modified = TRUE;
            }
            break;
        }
        case USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER:
        {
            PanicNull(usb_audio->headphone);
            if(usb_audio->headphone->spkr_sample_rate != streaming_info[interface_index].current_sampling_rate)
            {
                DEBUG_LOG_INFO("USB Audio: Headphone Spkr sample rate %d -> %d",
                               usb_audio->headphone->spkr_sample_rate,  streaming_info[interface_index].current_sampling_rate);
                is_headphone_rate_modified = TRUE;
            }
            break;
        }
        default:
            DEBUG_LOG_ERROR("usbAudio: Set Sampling Rate Unexpected Device Type %x", intf_list->intf[interface_index].type);
            Panic();
    }

    /*If sample rate is read by audio source then first disconnect existing chain
     *  and connect with new sample rate.*/
    if(is_headset_rate_modified && usb_audio->headset->source_connected &&
            usb_audio->headset->chain_active)
    {
        usbAudioDisconnectVoiceMsg(usb_audio);
        usbAudioConnectVoiceMsg(usb_audio);
    }

    if(is_headphone_rate_modified)
    {
        if(usb_audio->headphone->source_connected && usb_audio->headphone->chain_active)
        {
            usbAudioDisconnectAudioMsg(usb_audio);
            usbAudioConnectAudioMsg(usb_audio);
        }

#ifdef USB_SUPPORT_HEADPHONE_SPKR_IN_VOICE_CHAIN
        usb_audio_info_t *headset_audio = UsbAudioFd_GetHeadsetInfo(voice_source_usb);
        if(headset_audio && headset_audio->headset->alt_spkr_connected)
        {
            usbAudioDisconnectVoiceMsg(headset_audio);
            usbAudioConnectVoiceMsg(headset_audio);
        }
#endif
    }
}

usb_audio_streaming_info_t *UsbAudio_GetStreamingInfo(usb_audio_info_t *usb_audio,
                                                      usb_audio_device_type_t type)
{
     for (uint8 i=0; i < usb_audio->num_interfaces; i++)
     {
         if (usb_audio->config->intf_list->intf[i].type == type)
         {
             return &(usb_audio->streaming_info[i]);
         }
     }

     return NULL;
}


usb_audio_info_t *UsbAudioFd_GetHeadphoneInfo(audio_source_t source)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    while (usb_audio)
    {
        if(usb_audio->headphone != NULL &&
            usb_audio->headphone->audio_source == source)
        {
            break;
        }
        usb_audio = usb_audio->next;
    }

    return usb_audio;
}

usb_audio_info_t *UsbAudio_FindInfoBySource(Source source)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    while (usb_audio)
    {
        if(usb_audio->headphone != NULL &&
            usb_audio->headphone->spkr_src == source)
        {
            break;
        }
        if(usb_audio->headset != NULL &&
            usb_audio->headset->spkr_src == source)
        {
            break;
        }
        usb_audio = usb_audio->next;
    }

    return usb_audio;
}

usb_audio_info_t *UsbAudioFd_GetHeadsetInfo(voice_source_t source)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    while (usb_audio)
    {
        if(usb_audio->headset != NULL &&
            usb_audio->headset->audio_source == source)
        {
            break;
        }
        usb_audio = usb_audio->next;
    }

    return usb_audio;
}

Task UsbAudio_ClientRegister(Task client_task,
                             usb_audio_registered_client_t name)
{
    Task old_task = usb_audio_client_cb[name];

    usb_audio_client_cb[name]= client_task;

    return old_task;
}

void UsbAudio_ClientUnRegister(Task client_task,
                               usb_audio_registered_client_t name)
{
    if (usb_audio_client_cb[name] == client_task)
    {
        usb_audio_client_cb[name]= NULL;
    }
}

/****************************************************************************
    Handle USB device and audio class messages.
*/
static void UsbAudio_HandleMessage(usb_device_index_t device_index,
                                     MessageId id, Message message)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();

    DEBUG_LOG_DEBUG("USB Audio device %d event MESSAGE:0x%x", device_index, id);

    while (usb_audio)
    {
        if(device_index == usb_audio->device_index)
        {
            switch(id)
            {
                case MESSAGE_USB_DETACHED:
                case MESSAGE_USB_DECONFIGURED:
                    usb_audio->usb_fn_uac->Reset(usb_audio->class_ctx);
                    if(usb_audio->headphone != NULL)
                    {
                        if (usb_audio->headphone->source_connected)
                        {
                            usbAudioDisconnectAudioMsg(usb_audio);
                        }
                        /* with respect to usb audio, this means the host as disconnected audio */
                        UsbSource_SetAudioContext(context_audio_disconnected);
                    }
                    if(usb_audio->headset != NULL)
                    {
                        if (usb_audio->headset->source_connected)
                        {
                            usbAudioDisconnectVoiceMsg(usb_audio);
                        }
                        /* with respect to usb audio, this means the host as disconnected audio */
                        UsbSource_SetVoiceState(USB_SOURCE_VOICE_DISCONNECTED);
                    }
                    break;
                case MESSAGE_USB_ALT_INTERFACE:
                {
                    const MessageUsbAltInterface* ind = (const MessageUsbAltInterface*)message;

                    usbAudioUpdateDeviceStatus(usb_audio, ind->interface, ind->altsetting);
                    break;
                }
                case MESSAGE_USB_ENUMERATED:
                    {
                        if(usb_audio->headphone != NULL)
                        {
                            /* with respect to usb audio, this means that host has just connected and not streaming */
                            UsbSource_SetAudioContext(context_audio_connected);
                        }

                        if(usb_audio->headset != NULL)
                        {
                            /* with respect to usb voice, this means that host has just connected and not streaming */
                            UsbSource_SetVoiceState(USB_SOURCE_VOICE_CONNECTED);
                        }
                    }
                    break;
                default:
                    DEBUG_LOG_VERBOSE("Unhandled USB message MESSAGE:0x%x\n", id);
                    break;
            }
        }
        usb_audio = usb_audio->next;
    }
}

/* Handle event from USB audio class driver */
static void UsbAudio_ClassEvent(uac_ctx_t class_ctx,
                                uint8 interface_index,
                                uac_message_t uac_message)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();

    DEBUG_LOG_DEBUG("UsbAudio_ClassEvent intf_index %d  message:%d", interface_index, uac_message);

    while (usb_audio)
    {
        if(usb_audio->class_ctx == class_ctx)
        {
            switch(uac_message)
            {
                case USB_AUDIO_CLASS_MSG_LEVELS:
                    if (usb_audio->headset)
                    {
                        usbAudioUpdateHeadsetVolume(usb_audio);
                    }
                    if (usb_audio->headphone)
                    {
                        usbAudioUpdateHeadphoneVolume(usb_audio);
                    }
                    break;

                case USB_AUDIO_CLASS_MSG_SAMPLE_RATE:
                    usbAudioSetDeviceSamplingRate(usb_audio, interface_index);
                    break;

                default:
                    DEBUG_LOG_VERBOSE("Unhandled USB message 0x%x\n", uac_message);
                    break;
            }

            break;
        }
        usb_audio = usb_audio->next;
    }
}


/* Register for USB device events */
static void usbAudio_RegisterForUsbDeviceEvents(usb_device_index_t device_index)
{
    usb_audio_info_t *usb_audio_data = USB_AUDIO_GET_DATA();
    while (usb_audio_data)
    {
        if(usb_audio_data->device_index == device_index)
        {
            break;
        }
        usb_audio_data = usb_audio_data->next;
    }

    if(usb_audio_data == NULL)
    {
        UsbDevice_RegisterEventHandler(device_index, UsbAudio_HandleMessage);
    }
}

static usb_audio_device_type_t usbAudio_GetDeviceTypesCreated(void)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    usb_audio_device_type_t audio_device_types = 0;

    while (usb_audio)
    {
        const usb_audio_interface_config_list_t *intf_list = usb_audio->config->intf_list;

        for(uint8 index = 0; index<intf_list->num_interfaces;index++)
        {
            audio_device_types |= intf_list->intf[index].type;
        }

        usb_audio = usb_audio->next;
    }

    return audio_device_types;
}
/****************************************************************************
    Create context for USB Audio function driver.
*/
static usb_class_context_t UsbAudio_Create(usb_device_index_t device_index,
                                usb_class_interface_config_data_t config_data)
{
    const usb_audio_config_params_t *config = (const usb_audio_config_params_t *)config_data;
    usb_audio_info_t *usb_audio;
    usb_audio_device_type_t audio_device_types, audio_device_type;
    usb_audio_device_type_t device_types_created = usbAudio_GetDeviceTypesCreated();

    /* Configuration data is required */
    PanicZero(config);
    PanicZero(config->intf_list);
    PanicZero(config->intf_list->num_interfaces);

    usb_audio = (usb_audio_info_t *)PanicUnlessMalloc(sizeof(usb_audio_info_t));
    memset(usb_audio, 0, sizeof(usb_audio_info_t));

    usb_audio->usb_fn_uac = usbAudioGetFnTbl(config->rev);
    /* Requested device class revision must be supported */
    PanicZero(usb_audio->usb_fn_uac);

    usb_audio->num_interfaces = config->intf_list->num_interfaces;

    /* Check only supported types requested */
    audio_device_types = 0;
    for (uint8 i=0; i < usb_audio->num_interfaces; i++)
    {
        audio_device_type = config->intf_list->intf[i].type;
        /* Same type of device can only be created once*/
        PanicNotZero(device_types_created & audio_device_type);
        PanicNotZero(audio_device_types & audio_device_type);
        audio_device_types |= audio_device_type;
    }

    DEBUG_LOG_DEBUG("UsbAudio_Create audio_device_types %X  num_interfaces:%x", audio_device_types, usb_audio->num_interfaces);

    /* At least one supported device typs must be requested */
    PanicZero(audio_device_types & USB_AUDIO_SUPPORTED_DEVICE_TYPES);
    /* Requested device types must be supported*/
    PanicNotZero(audio_device_types & ~USB_AUDIO_SUPPORTED_DEVICE_TYPES);

    usb_audio->device_index = device_index;
    usb_audio->config = config;
    /* create USB audio class instance */
    usb_audio->class_ctx = usb_audio->usb_fn_uac->Create(usb_audio->device_index,
                                              config,
                                              &(usb_audio->streaming_info),
                                              UsbAudio_ClassEvent);

    if (audio_device_types & (USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER |
                                  USB_AUDIO_DEVICE_TYPE_VOICE_MIC))
    {
        usbAudioAddHeadset(usb_audio);
    }

    if (audio_device_types & (USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER |
                              USB_AUDIO_DEVICE_TYPE_AUDIO_MIC))
    {
        usbAudioAddHeadphone(usb_audio);
    }


    usbAudio_RegisterForUsbDeviceEvents(device_index);

    usb_audio->next = usbaudio_globaldata;
    usbaudio_globaldata = usb_audio;

    return usb_audio;
}

static bool usbAudio_FreeData(usb_audio_info_t *usb_audio)
{
    bool postpone_delete = FALSE;

    if (usb_audio->headset)
    {
        if (usb_audio->headset->source_connected)
        {
            usbAudioDisconnectVoiceMsg(usb_audio);
        }
        if (usb_audio->headset->chain_active)
        {
            postpone_delete = TRUE;
        }
    }

    if (usb_audio->headphone)
    {
        if (usb_audio->headphone->source_connected)
        {
            usbAudioDisconnectAudioMsg(usb_audio);
        }
        if (usb_audio->headphone->chain_active)
        {
            postpone_delete = TRUE;
        }
    }

    if (postpone_delete)
    {
        usb_audio->is_pending_delete = 1;
        return FALSE;
    }

    if(usb_audio->headset != NULL)
    {
        UsbSource_SetVoiceState(USB_SOURCE_VOICE_DISCONNECTED);
        UsbSource_DeregisterVoiceControl();

        free(usb_audio->headset);
        usb_audio->headset = NULL;
    }

    if(usb_audio->headphone != NULL)
    {
        UsbSource_SetAudioContext(context_audio_disconnected);
        UsbSource_DeregisterAudioControl();

        free(usb_audio->headphone);
        usb_audio->headphone = NULL;
    }

    usb_audio->usb_fn_uac->Delete(usb_audio->class_ctx);

    return TRUE;
}

usb_result_t UsbAudio_TryFreeData(usb_audio_info_t *usb_audio)
{
    usb_audio_info_t **dp = &usbaudio_globaldata;

    while (*dp)
    {
        if (*dp == usb_audio)
        {
            if (!usbAudio_FreeData(usb_audio))
            {
                DEBUG_LOG_DEBUG("usbAudio_FreeData: BUSY");
                return USB_RESULT_BUSY;
            }
            else
            {
                DEBUG_LOG_DEBUG("usbAudio_FreeData: OK");
                /* Before calling UsbDevice_ReleaseClass, audio device instance
                 * should be removed from the linked list. */
                *dp = usb_audio->next;
                if (usb_audio->is_pending_delete)
                {
                    UsbDevice_ReleaseClass(usb_audio->device_index, usb_audio);
                }

                free(usb_audio);
                return USB_RESULT_OK;
            }
        }
        dp = &((*dp)->next);
    }
    return USB_RESULT_NOT_FOUND;
}

static usb_result_t UsbAudio_Destroy(usb_class_context_t context)
{
    DEBUG_LOG_WARN("UsbAudio: closed");
    return UsbAudio_TryFreeData((usb_audio_info_t *)context);
}

const usb_class_interface_cb_t UsbAudio_Callbacks =
{
    .Create = UsbAudio_Create,
    .Destroy = UsbAudio_Destroy,
    .SetInterface = NULL
};

bool UsbAudio_GetInterfaceInfoFromDeviceType(usb_audio_device_type_t intf_type, usb_audio_interface_info_t * interface_info)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();

    PanicNull(interface_info);

    while (usb_audio)
    {
        usb_audio_streaming_info_t  *streaming_info = UsbAudio_GetStreamingInfo(usb_audio, intf_type);

        if(streaming_info)
        {
            interface_info->is_to_host = (streaming_info->ep_address & end_point_to_host) ? 1 : 0;
            if(interface_info->is_to_host)
            {
                interface_info->streamu.mic_sink = StreamUsbEndPointSink(streaming_info->ep_address);
            }
            else
            {
                interface_info->streamu.spkr_src = StreamUsbEndPointSource(streaming_info->ep_address);
            }

            interface_info->sampling_rate = streaming_info->current_sampling_rate;
            interface_info->volume_db = streaming_info->volume_status.volume_db;
            interface_info->mute_status = streaming_info->volume_status.mute_status;
            interface_info->channels = streaming_info->channels;
            interface_info->frame_size = streaming_info->frame_size;

            return TRUE;
        }

        usb_audio = usb_audio->next;
    }

    return FALSE;
}


bool UsbAudio_SetAudioChainBusy(Source source)
{
    DEBUG_LOG_VERBOSE("UsbAudio_SetAudioChainBusy");
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();

    while (usb_audio)
    {
        if(!usb_audio->is_pending_delete)
        {
            if(usb_audio->headphone != NULL && usb_audio->headphone->spkr_src == source)
            {
                usb_audio->headphone->chain_active = TRUE;
                return TRUE;
            }
            if(usb_audio->headset != NULL && usb_audio->headset->spkr_src == source)
            {
                usb_audio->headset->chain_active = TRUE;
                return TRUE;
            }
        }
        usb_audio = usb_audio->next;
    }
    return FALSE;
}

void UsbAudio_ClearAudioChainBusy(Source source)
{
    DEBUG_LOG_VERBOSE("UsbAudio_ClearAudioChainBusy");
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    while (usb_audio)
    {
        if(usb_audio->headphone != NULL && usb_audio->headphone->spkr_src == source)
        {
            usb_audio->headphone->chain_active = FALSE;
            break;
        }
        if(usb_audio->headset != NULL && usb_audio->headset->spkr_src == source)
        {
            usb_audio->headset->chain_active = FALSE;
            break;
        }
        usb_audio = usb_audio->next;
    }

    if (usb_audio && usb_audio->is_pending_delete)
    {
        UsbAudio_TryFreeData(usb_audio);
    }
}
