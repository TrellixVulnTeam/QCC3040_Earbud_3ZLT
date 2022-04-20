/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   USB source
\brief      USB audio/voice sources media and volume control interfaces
*/

#ifndef USB_SOURCE_H
#define USB_SOURCE_H

#include "audio_sources.h"
#include "voice_sources.h"
#include "usb_device.h"

/*! Return if USB audio is supported by the current configuration */
bool UsbSource_IsAudioSupported(void);

/*! Set USB audio context
 *
 * Used by USB audio function driver to update audio context reported
 * via Media Control interface. */
void UsbSource_SetAudioContext(audio_source_provider_context_t cxt);

/*! Return USB audio context */
unsigned UsbSource_GetAudioContext(audio_source_t source);

/*! Return if USB Voice is supported by the current configuration */
bool UsbSource_IsVoiceSupported(void);

/* Windows host support USB HID HOOK-SWITCH input report and off-hook & ring LED output report.
 * Android host does not support any of the above, but it shall supports IncomingCallAccept and
 * OngoingCallTerminate through PLAY_PAUSE input report.
 * In USB application usb_app_audio_voice_1af, USB audio & USB Voice will share common speaker.
 * So during voice call, both USB audio and USB voice will be active.
 *
 * When device is enumerated at windows host,
 * usb_source_voice_ctx = context_voice_connected & usb_source_audio_ctx = context_audio_connected
 * So when there is a incoming call and RING is streamed over USB, speaker interface will be activated.
 * ​usb_source_voice_ctx = context_voice_ringing_incoming & usb_source_audio_ctx = context_audio_is_streaming
 * IncomingCallAccept() will sent HOOK_SWITCH_ANSWER input report and windows host shall accept call.
 * Once the call is accepted,
 * ​usb_source_voice_ctx = context_voice_in_call & usb_source_audio_ctx = context_audio_is_streaming
 * OngoingCallTerminate() will send HOOK_SWITCH_TERMINATE input report and android host shall terminate call.
 * Once call is terminated,
 * usb_source_voice_ctx = context_voice_connected & usb_source_audio_ctx = context_audio_connected
 * Windows host also support IncomingCallReject using BUTTON_ONE in USB HID telephony page.
 *
 * When device is enumerated at android host,
 * usb_source_voice_ctx = context_voice_connected & usb_source_audio_ctx = context_audio_connected
 * So when there is a incoming call and RING is streamed over USB, speaker interface will be activated.
 * ​usb_source_voice_ctx = context_voice_connected & usb_source_audio_ctx = context_audio_is_streaming
 * PlayPause() will send PLAY_PAUSE input report and android host shall accept call.
 * Once the call is accepted and mic interface is active,
 * ​usb_source_voice_ctx = context_voice_in_call & usb_source_audio_ctx = context_audio_is_streaming
 * OngoingCallTerminate() will send PLAY_PAUSE input report and android host shall terminate call.
 * Android host shall not support IncomingCallReject
*/

/*! USB voice source state */
typedef enum
{
    USB_SOURCE_VOICE_DISCONNECTED,
    USB_SOURCE_VOICE_CONNECTED,
    USB_SOURCE_VOICE_ACTIVE
} usb_source_voice_state_t;

/*! Set USB Voice context
 * Used by USB audio function driver to update USB Voice state */
void UsbSource_SetVoiceState(usb_source_voice_state_t cxt);

/*! Return USB Voice context */
unsigned UsbSource_GetVoiceContext(voice_source_t source);

/*! HID control events */
typedef enum
{
    /*! Send a HID play/pause event over USB */
    USB_SOURCE_PLAY_PAUSE,
    /*! Send a HID stop event over USB */
    USB_SOURCE_STOP,
    /*! Send a HID next track event over USB */
    USB_SOURCE_NEXT_TRACK,
    /*! Send a HID previous track event over USB */
    USB_SOURCE_PREVIOUS_TRACK,
    /*! Send a HID play event over USB */
    USB_SOURCE_PLAY,
    /*! Send a HID pause event over USB */
    USB_SOURCE_PAUSE,
    /*! Send a HID Volume Up event over USB */
    USB_SOURCE_VOL_UP,
    /*! Send a HID Volume Down event over USB */
    USB_SOURCE_VOL_DOWN,
    /*! Send a HID Mute event over USB */
    USB_SOURCE_MUTE,
    /*! Send a HID Fast Forward ON event over USB */
    USB_SOURCE_FFWD_ON,
    /*! Send a HID Fast Forward OFF event over USB */
    USB_SOURCE_FFWD_OFF,
    /*! Send a HID consumer Rewind ON event over USB */
    USB_SOURCE_REW_ON,
    /*! Send a HID consumer Rewind OFF event over USB */
    USB_SOURCE_REW_OFF,

    /*! Send a HID Telephony Mute event over USB */
    USB_SOURCE_PHONE_MUTE,
    /*! Send a HID Telephony Call Answer event over USB */
    USB_SOURCE_HOOK_SWITCH_ANSWER,
    /*! Send a HID Telephony Call Terminate event over USB */
    USB_SOURCE_HOOK_SWITCH_TERMINATE,
    /*! Send a HID Telephony Flash event over USB */
    USB_SOURCE_FLASH,
    /*! Send a HID Telephony Programmable button 1 event over USB */
    USB_SOURCE_BUTTON_ONE,

    /*! Number of supported events */
    USB_SOURCE_EVT_COUNT
} usb_source_control_event_t;

/*! USB source HID events status */
typedef enum
{
    USB_SOURCE_HID_STATUS_INACTIVE   = 0x00,
    USB_SOURCE_HID_STATUS_ACTIVE     = 0x01,

    USB_SOURCE_HID_STATUS_UNDEFINED  = 0x03,
} usb_source_hid_event_status_t;

#define USB_SOURCE_HID_STATUS_SIZE 0x02
#define USB_SOURCE_HID_STATUS_MASK 0x03

/*! Received HID events from host */
typedef enum
{
    /*! Received HID mute event */
    USB_SOURCE_RX_HID_MUTE_EVT,
    /*! Received HID off hook event */
    USB_SOURCE_RX_HID_OFF_HOOK_EVT,
    /*! Received HID ring event */
    USB_SOURCE_RX_HID_RING_EVT,
    // ... etc ...
    /*! Number of supported events */
    USB_SOURCE_RX_HID_EVT_COUNT
} usb_source_rx_hid_event_t;

/*! USB HID event handler for events received from host
 *
 * \param event Received HID events from host 
 * \param status TRUE if event is active, FALSE otherwise  
 */
typedef void (*usb_rx_hid_event_handler_t)(usb_source_rx_hid_event_t event, bool is_active);

/*! HID interface for USB source */
typedef struct
{
    /*! Callback to send HID event */
    usb_result_t (*send_event)(usb_source_control_event_t event);
    /*! Callback to send HID report */
    usb_result_t (*send_report)(const uint8 *report, uint16 size);
    /*! Callback to register handler for receiving HID events */
    void (*register_handler)(usb_rx_hid_event_handler_t handler);
    /*! Callback to unregister handler for receiving HID events */
    void (*unregister_handler)(void);
} usb_source_hid_interface_t;

/*! Register HID send event handler
 *
 * Used by USB HID Consumer Transport class driver to register a callback
 * for sending HID control events to the host. */
void UsbSource_RegisterHid(usb_source_hid_interface_t *hid_interface);

/*! Unregister HID send event handler
 *
 * Called when previously registered HID interfaces is being removed */
void UsbSource_UnregisterHid(void);

/*! Register Media Player and Volume Control interfaces for USB audio source */
void UsbSource_RegisterAudioControl(void);

/*! Deregister USB audio source controls */
void UsbSource_DeregisterAudioControl(void);

/*! Register Telephony control interfaces for USB Voice source */
void UsbSource_RegisterVoiceControl(void);

/*! Deregister USB Voice source control */
void UsbSource_DeregisterVoiceControl(void);

#endif /* USB_SOURCE_H */
