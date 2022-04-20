/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      HFP library link priority to Voice Source mappings.
*/

#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "hfp_profile_instance.h"

#include <device_properties.h>
#include <logging.h>
#include <panic.h>

hfp_link_priority HfpProfile_GetHfpLinkPrioForVoiceSource(voice_source_t source)
{
    hfp_link_priority link = hfp_invalid_link;

    PanicFalse(source < max_voice_sources);

    device_t device = HfpProfileInstance_FindDeviceFromVoiceSource(source);
    if (device != NULL)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(device);
        link = HfpLinkPriorityFromBdaddr(&addr);
    }

    DEBUG_LOG_VERBOSE(
        "HfpProfile_GetHfpLinkPrioForVoiceSource enum:voice_source_t:%d enum:hfp_link_priority:%d",
        source, link);

    return link;
}

voice_source_t HfpProfile_GetVoiceSourceForHfpLinkPrio(hfp_link_priority priority)
{
    voice_source_t source = voice_source_none;

    PanicFalse(priority <= hfp_secondary_link);

    bdaddr bd_addr = {0};
    if (HfpLinkGetBdaddr(priority, &bd_addr))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr);
        if (instance != NULL)
        {
            source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        }
    }

    DEBUG_LOG_VERBOSE(
        "HfpProfile_GetVoiceSourceForHfpLinkPrio enum:hfp_link_priority:%d enum:voice_source_t:%d",
        priority, source);

    return source;
}



