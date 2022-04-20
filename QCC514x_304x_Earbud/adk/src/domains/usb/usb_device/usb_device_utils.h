/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Utility functions for USB device framework
*/

#ifndef USB_DEVICE_UTILS_H_
#define USB_DEVICE_UTILS_H_

/*! Helper function for allocating space in Sink */
uint8 *SinkMapClaim(Sink sink, uint16 size);

#define assert(x) PanicFalse(x)

#endif /* USB_DEVICE_UTILS_H_ */
