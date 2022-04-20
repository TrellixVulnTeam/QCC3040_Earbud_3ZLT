/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of VA profile.
*/

#include "va_profile.h"

static vapActiveAtBdaddr active_at_bdaddr_callback = NULL;

void VaProfile_RegisterClient(vapActiveAtBdaddr callback)
{
    active_at_bdaddr_callback = callback;
}

bool VaProfile_IsVaActiveAtBdaddr(const bdaddr * bd_addr)
{
    bool active_at_bdaddr = FALSE;

    if (active_at_bdaddr_callback)
    {
        active_at_bdaddr = active_at_bdaddr_callback(bd_addr);
    }

    return active_at_bdaddr;
}
