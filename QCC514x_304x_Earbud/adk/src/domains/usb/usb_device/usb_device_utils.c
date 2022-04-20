/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Utility functions for USB device framework
*/

#include <sink.h>
#include <stream.h>

#include "usb_device_utils.h"

uint8 *SinkMapClaim(Sink sink, uint16 size)
{
    uint8 *dest = SinkMap(sink);
    uint16 claim_result = SinkClaim(sink, size);
    if (claim_result == 0xffff)
    {
        return NULL;
    }
    return (dest + claim_result);
}
