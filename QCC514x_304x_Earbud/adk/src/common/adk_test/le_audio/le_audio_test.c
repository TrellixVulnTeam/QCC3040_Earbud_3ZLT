/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common LE Audio specifc testing functions.
*/

#include <logging.h>

#include "focus_audio_source.h"
#include "focus_voice_source.h"
#include "le_advertising_manager_select_extended.h"
#ifdef INCLUDE_TWS
#include "tws_topology.h"
#endif
#include "volume_messages.h"

#include "connection_manager_list.h"
#include "bt_device.h"

#include "le_audio_test.h"


#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif


bool leAudioTest_IsExtendedAdvertisingActive(void)
{
    return LeAdvertisingManager_IsExtendedAdvertisingActive();
}

bool leAudioTest_IsBroadcastReceiveActive(void)
{
    return FALSE;
}

bool leAudioTest_IsAnyBroadcastSourceSyncedToPa(void)
{
    return FALSE;
}

bool leAudioTest_IsAnyBroadcastSourceSyncedToBis(void)
{
    return FALSE;
}

static bool leAudioTest_SetVolumeForLeaAudioSource(audio_source_t source, uint8 volume)
{
#ifdef INCLUDE_TWS
    if (TwsTopology_IsSecondary())
    {
        DEBUG_LOG_ALWAYS("This Test API should never be called on the secondary Earbud");
        return FALSE;
    }
#endif
    audio_source_t focused_source = audio_source_none;
    if (!Focus_GetAudioSourceForContext(&focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused audio source");
        return FALSE;
    }
    if (focused_source != source)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not enum:audio_source_t:%d", source);
        return FALSE;
    }
    Volume_SendAudioSourceVolumeUpdateRequest(focused_source, event_origin_local, volume);
    return TRUE;
}

bool leAudioTest_SetVolumeForBroadcast(uint8 volume)
{
    DEBUG_LOG_ALWAYS("leAudioTest_SetVolumeForBroadcast %d", volume);
    return leAudioTest_SetVolumeForLeaAudioSource(audio_source_le_audio_broadcast, volume);
}

bool leAudioTest_SetVolumeForUnicastMusic(uint8 volume)
{
    DEBUG_LOG_ALWAYS("leAudioTest_SetVolumeForUnicast %d", volume);
    return leAudioTest_SetVolumeForLeaAudioSource(audio_source_le_audio_unicast, volume);
}


static bool leAudioTest_SetMuteForLeaAudioSource(audio_source_t source, bool mute_state)
{
#ifdef INCLUDE_TWS
    if (TwsTopology_IsSecondary())
    {
        DEBUG_LOG_ALWAYS("This Test API should never be called on the secondary Earbud");
        return FALSE;
    }
#endif
    audio_source_t focused_source = audio_source_none;
    if (!Focus_GetAudioSourceForContext(&focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused audio source");
        return FALSE;
    }
    if (focused_source != source)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not enum:audio_source_t:%d", source);
        return FALSE;
    }
    Volume_SendAudioSourceMuteRequest(focused_source, event_origin_local, mute_state);
    return TRUE;
}

bool leAudioTest_SetMuteForBroadcast(bool mute_state)
{
    DEBUG_LOG_ALWAYS("leAudioTest_SetMuteForBroadcast %d", mute_state);
    return leAudioTest_SetMuteForLeaAudioSource(audio_source_le_audio_broadcast, mute_state);
}

bool leAudioTest_SetMuteForUnicastMusic(bool mute_state)
{
    DEBUG_LOG_ALWAYS("leAudioTest_SetMuteForUnicastMusic %d", mute_state);
    return leAudioTest_SetMuteForLeaAudioSource(audio_source_le_audio_unicast, mute_state);
}

bool leAudioTest_PauseBroadcast(void)
{
    return FALSE;
}

bool leAudioTest_ResumeBroadcast(void)
{
    return FALSE;
}

bool leAudioTest_IsBroadcastPaused(void)
{
    return TRUE;
}

bool leAudioTest_SetVolumeForUnicastVoice(uint8 volume)
{
    DEBUG_LOG_ALWAYS("leAudioTest_SetVolumeForUnicastVoice %d", volume);
#ifdef INCLUDE_TWS
    if (TwsTopology_IsSecondary())
    {
        DEBUG_LOG_ALWAYS("This Test API should never be called on the secondary Earbud");
        return FALSE;
    }
#endif
    voice_source_t focused_source = voice_source_none;
    if (!Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused voice source");
        return FALSE;
    }
    if (focused_source != voice_source_le_audio_unicast)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not enum:voice_source_t:%d", voice_source_le_audio_unicast);
        return FALSE;
    }
    Volume_SendVoiceSourceVolumeUpdateRequest(focused_source, event_origin_local, volume);
    return TRUE;
}

bool leAudioTest_SetMuteForUnicastVoice(bool mute_state)
{
    DEBUG_LOG_ALWAYS("leAudioTest_SetMuteForUnicastVoice %d", mute_state);
#ifdef INCLUDE_TWS
    if (TwsTopology_IsSecondary())
    {
        DEBUG_LOG_ALWAYS("This Test API should never be called on the secondary Earbud");
        return FALSE;
    }
#endif
    voice_source_t focused_source = voice_source_none;
    if (!Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused voice source");
        return FALSE;
    }
    if (focused_source != voice_source_le_audio_unicast)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not enum:voice_source_t:%d", voice_source_le_audio_unicast);
        return FALSE;
    }
    Volume_SendVoiceSourceMuteRequest(focused_source, event_origin_local, mute_state);
    return TRUE;
}

int leAudioTest_GetCurrentVcpAudioVolume(void)
{
    return AudioSources_GetVolume(audio_source_le_audio_broadcast).value;
}

bool leAudioTest_AnyHandsetConnectedBothBredrAndLe(void)
{
    cm_connection_t *le_connection;

    le_connection = ConManagerFindFirstActiveLink(cm_transport_ble);
    while (le_connection)
    {
        const tp_bdaddr *address = ConManagerGetConnectionTpAddr(le_connection);
        if (address)
        {
            tp_bdaddr le_address = {0};

            if (   le_address.taddr.type != TYPED_BDADDR_RANDOM
                || !VmGetPublicAddress(address, &le_address))
            {
                le_address = *address;
            }

            if (le_address.taddr.type == TYPED_BDADDR_PUBLIC)
            {
                tp_bdaddr bredr_address = le_address;
                bredr_address.transport = TRANSPORT_BREDR_ACL;
                if (ConManagerFindConnectionFromBdAddr(&bredr_address))
                {
                    DEBUG_LOG_ALWAYS("leAudioTest_AnyHandsetConnectedBothBredrAndLe. Found device with LE and BREDR addr:(0x%6lx)",
                                le_address.taddr.addr.lap);
                    return TRUE;
                }
            }
        }
        le_connection = ConManagerFindNextActiveLink(le_connection, cm_transport_ble);
    }

    DEBUG_LOG_ALWAYS("leAudioTest_AnyHandsetConnectedBothBredrAndLe. No devices.");

    return FALSE;
}

