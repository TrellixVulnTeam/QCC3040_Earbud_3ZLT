/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio application interface - enumerates HID consume transport,
            HID datalink and USB Audio classes which support both headphone
            (for music playback) and headset (for voice calls) devices with
            separate audio function.
*/

#ifndef USB_APP_AUDIO_VOICE_2AF_H_
#define USB_APP_AUDIO_VOICE_2AF_H_

#include "usb_application.h"

/*! USB application which support both audio and voice */
extern const usb_app_interface_t usb_app_audio_voice_2af;

#endif /* USB_APP_AUDIO_VOICE_2AF_H_ */

