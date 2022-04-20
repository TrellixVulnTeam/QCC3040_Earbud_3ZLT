/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB source - HID control header
*/

#ifndef USB_SOURCE_HID_H
#define USB_SOURCE_HID_H

#include "usb_source.h"

/*! Send HID Play event  */
void UsbSource_Play(audio_source_t source);

/*! Send HID Pause event  */
void UsbSource_Pause(audio_source_t source);

/*! Send HID PlayPause event  */
void UsbSource_PlayPause(audio_source_t source);

/*! Send HID Stop event  */
void UsbSource_Stop(audio_source_t source);

/*! Send HID Forward event  */
void UsbSource_Forward(audio_source_t source);

/*! Send HID Back event  */
void UsbSource_Back(audio_source_t source);

/*! Send HID Fast Forward event  */
void UsbSource_FastForward(audio_source_t source, bool state);

/*! Send HID Fast Rewind event  */
void UsbSource_FastRewind(audio_source_t source, bool state);

/*! Send HID Audio Volume Up event  */
void UsbSource_AudioVolumeUp(audio_source_t source);

/*! Send HID Audio Volume Down event  */
void UsbSource_AudioVolumeDown(audio_source_t source);

/*! Send HID Audio Speaker Mute event  */
void UsbSource_AudioSpeakerMute(audio_source_t source, mute_state_t state);

/*! Stub function for SetAbsolute command which is not supported by USB HID */
void UsbSource_AudioVolumeSetAbsolute(audio_source_t source, volume_t volume);

/*! Send arbitrary HID event */
bool UsbSource_SendEvent(usb_source_control_event_t event);

/*! Send HID Incomming Call Accept event  */
void UsbSource_IncomingCallAccept(voice_source_t source);

/*! Send HID Incomming Call Reject event  */
void UsbSource_IncomingCallReject(voice_source_t source);

/*! Send HID Ongoing Call Terminate event  */
void UsbSource_OngoingCallTerminate(voice_source_t source);

/*! Send HID Toggle Microphone Mute event  */
void UsbSource_ToggleMicrophoneMute(voice_source_t source);

/*! Send HID Voice Volume Up event  */
void UsbSource_VoiceVolumeUp(voice_source_t source);

/*! Send HID Voice Volume Down event  */
void UsbSource_VoiceVolumeDown(voice_source_t source);

/*! Send HID Voice Speaker Mute event  */
void UsbSource_VoiceSpeakerMute(voice_source_t source, mute_state_t state);

/*! Send arbitrary HID report */
bool UsbSource_SendReport(const uint8 *report, uint16 size);

/*! Reset USB HID received event status */
void UsbSource_ResetHidEventStatus(void);

/*! Get USB HID off-hook event status */
usb_source_hid_event_status_t UsbSource_GetHidOffHookStatus(void);

/*! Get USB HID ring event status */
usb_source_hid_event_status_t UsbSource_GetHidRingStatus(void);

/*! Get USB HID mute event status */
usb_source_hid_event_status_t UsbSource_GetHidMuteStatus(void);

#endif /* USB_SOURCE_HID_H */
