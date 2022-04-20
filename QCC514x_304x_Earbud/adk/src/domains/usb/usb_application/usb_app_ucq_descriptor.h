/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for UCQ string descriptor.
*/

#ifndef USB_APP_UCQ_DESCRIPTOR_H_
#define USB_APP_UCQ_DESCRIPTOR_H_


#define USB_APP_UCQ_MS_TEAMS_INDEX 0x21

/*! To get the default UCQ descriptor which support MS Teams & Skype for Business*/
const uint16 * UsbApplication_GetDefaultUcqDescriptors(void);

#endif /* USB_APP_UCQ_DESCRIPTOR_H_ */


