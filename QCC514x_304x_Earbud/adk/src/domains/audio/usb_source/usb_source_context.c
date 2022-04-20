/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_source_context.c
\brief  USB source context is returned via Media Control interface.
        It is up to the USB audio function driver to update the current context
        when it changes.
*/

#include "usb_source.h"
#include "ui.h"
#include "usb_source_hid.h"

#include "logging.h"
#include <panic.h>

static audio_source_provider_context_t usb_source_audio_ctx = context_audio_disconnected;
static usb_source_voice_state_t usb_source_voice_state = USB_SOURCE_VOICE_DISCONNECTED;

void UsbSource_SetAudioContext(audio_source_provider_context_t ctx)
{
    usb_source_audio_ctx = ctx;
}

unsigned UsbSource_GetAudioContext(audio_source_t source)
{
    return (unsigned)((source == audio_source_usb) ?
                          usb_source_audio_ctx :
                          BAD_CONTEXT);
}

bool UsbSource_IsAudioSupported(void)
{
    return usb_source_audio_ctx != context_audio_disconnected;
}

void UsbSource_SetVoiceState(usb_source_voice_state_t state)
{
    if(state == USB_SOURCE_VOICE_DISCONNECTED)
    {
        UsbSource_ResetHidEventStatus();
    }
    usb_source_voice_state = state;
    DEBUG_LOG_VERBOSE("UsbSource_SetVoiceState: state enum:usb_source_voice_state_t:%d "
                      "ring enum:usb_source_hid_event_status_t:%d "
                      "off-hook enum:usb_source_hid_event_status_t:%d ",
                      state, UsbSource_GetHidRingStatus(), UsbSource_GetHidOffHookStatus());
}

unsigned UsbSource_GetVoiceContext(voice_source_t source)
{
    voice_source_provider_context_t context = BAD_CONTEXT;

    if(source == voice_source_usb)
    {
        if(UsbSource_GetHidRingStatus() == USB_SOURCE_HID_STATUS_ACTIVE)
        {
            context = context_voice_ringing_incoming;
        }
        else if(UsbSource_GetHidOffHookStatus() == USB_SOURCE_HID_STATUS_ACTIVE)
        {
            context = context_voice_in_call;
        }
        else
        {
            /* If USB HID off-hook event status is not USB_SOURCE_HID_STATUS_ACTIVE,
             * USB voice context will be decided by usb_source_voice_state. */
            switch (usb_source_voice_state)
            {
                case USB_SOURCE_VOICE_DISCONNECTED:
                    context = context_voice_disconnected;
                    break;
                case USB_SOURCE_VOICE_CONNECTED:
                    context = context_voice_connected;
                    break;
                case USB_SOURCE_VOICE_ACTIVE:
                    context = context_voice_in_call;
                    break;
            }
        }
    }

    return context;
}

bool UsbSource_IsVoiceSupported(void)
{
    return usb_source_voice_state != USB_SOURCE_VOICE_DISCONNECTED;
}
