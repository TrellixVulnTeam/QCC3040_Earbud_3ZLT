/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private Header file for USB Audio class 1.0 driver
*/

#ifndef USB_AUDIO_CLASS_10_H_
#define USB_AUDIO_CLASS_10_H_


#include "usb_audio_class.h"


/*! \brief Interface to get function pointers for class driver.
    
    \return Return function table for usb_audio_class_10 driver interface .
*/
usb_fn_tbl_uac_if *UsbAudioClass10_GetFnTbl(void);


#endif /* USB_AUDIO_CLASS_10_H_ */
