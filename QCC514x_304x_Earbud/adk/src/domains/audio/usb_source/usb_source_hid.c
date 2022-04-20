/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_source_hid.c
\brief  USB source - HID control commands
*/

#include "usb_source.h"
#include "usb_source_hid.h"
#include "logging.h"
#include <panic.h>
#include <telephony_messages.h>

/* Status of each event is tri-state and will consume 2 bits */
#define USB_SOURCE_HID_STATUS_POS(event)   (event * USB_SOURCE_HID_STATUS_SIZE)

/* Size/datatype of hid_event_status_info(uint8) depends on USB_SOURCE_RX_HID_EVT_COUNT
 * and USB_SOURCE_HID_STATUS_SIZE.*/
static uint8 hid_event_status_info;

static usb_source_hid_interface_t *usb_source_hid_interface = NULL;

static usb_source_hid_event_status_t usbSource_GetHidEventStatus(usb_source_rx_hid_event_t event)
{
    PanicFalse(event < USB_SOURCE_RX_HID_EVT_COUNT);
    return (usb_source_hid_event_status_t)((hid_event_status_info >> USB_SOURCE_HID_STATUS_POS(event)) & USB_SOURCE_HID_STATUS_MASK);
}

static void usbSource_SetHidEventStatus(usb_source_rx_hid_event_t event, usb_source_hid_event_status_t event_status)
{
    PanicFalse(event < USB_SOURCE_RX_HID_EVT_COUNT);
    hid_event_status_info = (hid_event_status_info & ~(USB_SOURCE_HID_STATUS_MASK << USB_SOURCE_HID_STATUS_POS(event))) |
                            ((event_status & USB_SOURCE_HID_STATUS_MASK) << USB_SOURCE_HID_STATUS_POS(event));
    DEBUG_LOG_VERBOSE("usbSource_SetHidEventStatus: hid_event_status_info 0x%X", hid_event_status_info);
}

static void UsbSource_HandleUsbHidEvent(usb_source_rx_hid_event_t event, bool is_active)
{
    usb_source_hid_event_status_t event_status = (is_active ? USB_SOURCE_HID_STATUS_ACTIVE :
                                                              USB_SOURCE_HID_STATUS_INACTIVE);

    if(usbSource_GetHidEventStatus(event) != event_status)
    {
        DEBUG_LOG_DEBUG("UsbSource_HandleUsbHidEvent: Event enum:usb_source_rx_hid_event_t:%d"
                       " status 0x%X", event, is_active);

        usbSource_SetHidEventStatus(event, event_status);

        switch(event)
        {
            case USB_SOURCE_RX_HID_MUTE_EVT:
                if (is_active)
                {
                    Telephony_NotifyMicrophoneMuted(voice_source_usb);
                }
                else
                {
                    Telephony_NotifyMicrophoneUnmuted(voice_source_usb);
                }
                break;
            case USB_SOURCE_RX_HID_OFF_HOOK_EVT:
                if (is_active)
                {
                    Telephony_NotifyCallAnswered(voice_source_usb);
                }
                else
                {
                    Telephony_NotifyCallEnded(voice_source_usb);
                }
                break;
            case USB_SOURCE_RX_HID_RING_EVT:
                if (is_active)
                {
                    Telephony_NotifyCallIncomingOutOfBandRingtone(voice_source_usb);
                }
                else
                {
                    Telephony_NotifyCallIncomingEnded(voice_source_usb);
                }
                break;
            default:
                DEBUG_LOG_ERROR("UsbSource_HandleUsbHidEvent : UNSUPPORTED EVENT");
                Panic();
        }
    }
}

void UsbSource_ResetHidEventStatus(void)
{
    DEBUG_LOG_VERBOSE("UsbSource_ResetHidEventStatus");
    /* Setting all the bits to 1 which will indicate STATUS_UNDEFINED*/
    hid_event_status_info = ~(hid_event_status_info & 0);
}

usb_source_hid_event_status_t UsbSource_GetHidOffHookStatus(void)
{
    return usbSource_GetHidEventStatus(USB_SOURCE_RX_HID_OFF_HOOK_EVT);
}

usb_source_hid_event_status_t UsbSource_GetHidRingStatus(void)
{
    return usbSource_GetHidEventStatus(USB_SOURCE_RX_HID_RING_EVT);
}

usb_source_hid_event_status_t UsbSource_GetHidMuteStatus(void)
{
    return usbSource_GetHidEventStatus(USB_SOURCE_RX_HID_MUTE_EVT);
}

void UsbSource_RegisterHid(usb_source_hid_interface_t *hid_interface)
{
    usb_source_hid_interface = hid_interface;

    /* Ensuring size of hid_event_status_info is sufficient for all supported USB HID events*/
    PanicFalse((sizeof(hid_event_status_info) * 8) >= (USB_SOURCE_RX_HID_EVT_COUNT * USB_SOURCE_HID_STATUS_SIZE));

    UsbSource_ResetHidEventStatus();
    if(usb_source_hid_interface->register_handler)
    {
        usb_source_hid_interface->register_handler(UsbSource_HandleUsbHidEvent);
    }
}

void UsbSource_UnregisterHid(void)
{
    if(usb_source_hid_interface->unregister_handler)
    {
        usb_source_hid_interface->unregister_handler();
    }
    usb_source_hid_interface = NULL;
}

bool UsbSource_SendEvent(usb_source_control_event_t event)
{
    if ((UsbSource_IsAudioSupported() || UsbSource_IsVoiceSupported()) &&
            usb_source_hid_interface && usb_source_hid_interface->send_event)
    {
        return usb_source_hid_interface->send_event(event) == USB_RESULT_OK;
    }
    return FALSE;
}

bool UsbSource_SendReport(const uint8 *report, uint16 size)
{
    if (usb_source_hid_interface &&
            usb_source_hid_interface->send_report)
    {
        return usb_source_hid_interface->send_report(report, size) == USB_RESULT_OK;
    }
    return FALSE;
}

void UsbSource_Play(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PLAY);
    }
}

void UsbSource_Pause(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PAUSE);
    }
}

void UsbSource_PlayPause(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PLAY_PAUSE);
    }
}

void UsbSource_Stop(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_STOP);
    }
}

void UsbSource_Forward(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_NEXT_TRACK);
    }
}

void UsbSource_Back(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PREVIOUS_TRACK);
    }
}

void UsbSource_FastForward(audio_source_t source, bool state)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(state ?
                                USB_SOURCE_FFWD_ON:
                                USB_SOURCE_FFWD_OFF);
    }
}

void UsbSource_FastRewind(audio_source_t source, bool state)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(state ?
                                USB_SOURCE_REW_ON:
                                USB_SOURCE_REW_OFF);
    }
}

void UsbSource_AudioVolumeUp(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_UP);
    }
}

void UsbSource_AudioVolumeDown(audio_source_t source)
{
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_DOWN);
    }
}

void UsbSource_AudioSpeakerMute(audio_source_t source, mute_state_t state)
{
    UNUSED(state);
    if (source == audio_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_MUTE);
    }
}

void UsbSource_AudioVolumeSetAbsolute(audio_source_t source, volume_t volume)
{
    UNUSED(source);
    UNUSED(volume);

    DEBUG_LOG_WARN("UsbSource::SetAbsolute is not supported");
}

void UsbSource_IncomingCallAccept(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        if(UsbSource_GetHidRingStatus() == USB_SOURCE_HID_STATUS_ACTIVE)
        {
            /* This implementation works with hosts which support HOOK SWITCH usage of USB HID. */
            UsbSource_SendEvent(USB_SOURCE_HOOK_SWITCH_ANSWER);
        }
        else if(UsbSource_GetHidRingStatus() == USB_SOURCE_HID_STATUS_UNDEFINED)
        {
            /* This implementation works with Android hosts which does not support HOOK SWITCH, but
             * supports PLAY_PAUSE to accept call. */
            UsbSource_SendEvent(USB_SOURCE_PLAY_PAUSE);
        }
        else
        {
            DEBUG_LOG_WARN("UsbSource: No Incoming Call - HidRingStatus: "
                           "enum:usb_source_hid_event_status_t:%d ", UsbSource_GetHidRingStatus());
        }
    }
}

void UsbSource_IncomingCallReject(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        if(UsbSource_GetHidRingStatus() == USB_SOURCE_HID_STATUS_ACTIVE)
        {
            /* Version 4.0 of the "Microsoft Teams Devices General Specification" specifies
             * "Button 1" for Teams compatibility. The Jabra developer documentation shows that
             * a Button is required for correct operation for a call reject. */
            UsbSource_SendEvent(USB_SOURCE_BUTTON_ONE);
        }
        else if(UsbSource_GetHidRingStatus() == USB_SOURCE_HID_STATUS_UNDEFINED)
        {
            DEBUG_LOG_WARN("UsbSource: Host does not support; HidRingStatus: "
                           "enum:usb_source_hid_event_status_t:%d ", UsbSource_GetHidRingStatus());
        }
        else
        {
            DEBUG_LOG_WARN("UsbSource: No Incoming Call - HidRingStatus: "
                           "enum:usb_source_hid_event_status_t:%d ", UsbSource_GetHidRingStatus());
        }
    }
}

void UsbSource_OngoingCallTerminate(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        if(UsbSource_GetHidOffHookStatus() == USB_SOURCE_HID_STATUS_ACTIVE)
        {
            /* This implementation works with hosts which support HOOK SWITCH usage of USB HID. */
            UsbSource_SendEvent(USB_SOURCE_HOOK_SWITCH_TERMINATE);
        }
        else if(UsbSource_GetHidOffHookStatus() == USB_SOURCE_HID_STATUS_UNDEFINED)
        {
            /* This implementation works with Android hosts which does not support HOOK SWITCH, but
             * supports PLAY_PAUSE to terminate call. */
            UsbSource_SendEvent(USB_SOURCE_PLAY_PAUSE);
        }
        else
        {
            DEBUG_LOG_WARN("UsbSource: No Ongoing Call - HidOffHookStatus:enum:usb_source_hid_event_status_t:%d ", UsbSource_GetHidOffHookStatus());
        }
    }
}

void UsbSource_ToggleMicrophoneMute(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_PHONE_MUTE);
    }
}

void UsbSource_VoiceVolumeUp(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_UP);
    }
}

void UsbSource_VoiceVolumeDown(voice_source_t source)
{
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_VOL_DOWN);
    }
}

void UsbSource_VoiceSpeakerMute(voice_source_t source, mute_state_t state)
{
    UNUSED(state);
    if (source == voice_source_usb)
    {
        UsbSource_SendEvent(USB_SOURCE_MUTE);
    }
}
