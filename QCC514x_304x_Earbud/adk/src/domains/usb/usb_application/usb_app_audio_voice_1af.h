/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio application - enumerates HID consume transport,
            HID datalink and USB Audio classes which support audio & voice use cases
            in same audio function with common speaker for audio & voice use cases.
            If mic interface is active with or without speaker being active, then 
            voice chain will be created. If speaker interface is active and mic interface
            is not active, then audio chain will be created
*/

#ifndef USB_APP_AUDIO_VOICE_1AF_H_
#define USB_APP_AUDIO_VOICE_1AF_H_

#include "usb_application.h"

/*! USB application which support both audio and voice usecases with shared speaker */
extern const usb_app_interface_t usb_app_audio_voice_1af;

#endif /* USB_APP_AUDIO_VOICE_1AF_H_ */


