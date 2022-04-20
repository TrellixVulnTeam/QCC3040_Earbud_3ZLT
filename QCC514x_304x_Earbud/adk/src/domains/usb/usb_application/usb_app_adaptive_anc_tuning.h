/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB application for Adaptive ANC tuning - enumerates USB Audio class.
*/

#if defined ENABLE_ANC && defined ENABLE_ADAPTIVE_ANC
#ifndef USB_APP_ADAPTIVE_ANC_TUNING_H_
#define USB_APP_ADAPTIVE_ANC_TUNING_H_

#include "usb_application.h"

/*! USB application interface for Adaptive ANC tuning */
extern const usb_app_interface_t usb_app_adaptive_anc_tuning;

#endif /* USB_APP_ADAPTIVE_ANC_TUNING_H_ */

#endif /*ENABLE_ANC && ENABLE_ADAPTIVE_ANC */
