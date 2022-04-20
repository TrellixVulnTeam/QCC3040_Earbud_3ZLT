/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB application for ANC tuning - enumerates USB Audio class.
*/

#ifdef ENABLE_ANC
#ifndef USB_APP_ANC_TUNING_H_
#define USB_APP_ANC_TUNING_H_

#include "usb_application.h"

/*! USB application interface for ANC tuning */
extern const usb_app_interface_t usb_app_anc_tuning;

#endif /* USB_APP_ANC_TUNING_H_ */

#endif /*ENABLE_ANC */
