/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Miscellaneous voice source functions
*/

#include "voice_sources.h"

#include <logging.h>
#include <panic.h>
#include <ui.h>

bool VoiceSources_Init(Task init_task)
{
    UNUSED(init_task);
    VoiceSources_AudioRegistryInit();
    VoiceSources_VolumeRegistryInit();
    VoiceSources_VolumeControlRegistryInit();
    VoiceSources_ObserverRegistryInit();

    return TRUE;
}

voice_source_t VoiceSources_GetRoutedSource(void)
{
    voice_source_t source = voice_source_none;
    while(++source < max_voice_sources)
    {
        if(VoiceSources_IsAudioRouted(source))
        {
            break;
        }
    }
    if(source == max_voice_sources)
    {
        source = voice_source_none;
    }

    DEBUG_LOG_VERBOSE("VoiceSources_GetRoutedSource enum:voice_source_t:%d", source);

    return source;
}

bool VoiceSources_IsAnyVoiceSourceRouted(void)
{
    return (VoiceSources_GetRoutedSource() != voice_source_none) ? TRUE :FALSE;
}
