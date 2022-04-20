/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_source_telephony_control.c
\brief  USB source - telephony control interface
*/

#include "usb_source.h"
#include "usb_source_hid.h"
#include "voice_sources_telephony_control_interface.h"


static const voice_source_telephony_control_interface_t usb_source_telephony_control =
{
    .IncomingCallAccept = UsbSource_IncomingCallAccept,
    .IncomingCallReject = UsbSource_IncomingCallReject,
    .OngoingCallTerminate = UsbSource_OngoingCallTerminate,
    .OngoingCallTransferAudio = NULL,
    .InitiateCallUsingNumber = NULL,
    .InitiateVoiceDial = NULL,
    .InitiateCallLastDialled = NULL,
    .ToggleMicrophoneMute = UsbSource_ToggleMicrophoneMute,
    .GetUiProviderContext = UsbSource_GetVoiceContext
};

static const voice_source_volume_control_interface_t usb_voice_source_volume_control_interface =
{
    .VolumeUp = UsbSource_VoiceVolumeUp,
    .VolumeDown = UsbSource_VoiceVolumeDown,
    .VolumeSetAbsolute = NULL,
    .Mute = UsbSource_VoiceSpeakerMute
};

void UsbSource_RegisterVoiceControl(void)
{
    VoiceSources_RegisterTelephonyControlInterface(voice_source_usb,
                                               &usb_source_telephony_control);
    VoiceSources_RegisterVolumeControl(voice_source_usb,
                                       &usb_voice_source_volume_control_interface);
}

void UsbSource_DeregisterVoiceControl(void)
{
    VoiceSources_DeregisterTelephonyControlInterface(voice_source_usb);
}

