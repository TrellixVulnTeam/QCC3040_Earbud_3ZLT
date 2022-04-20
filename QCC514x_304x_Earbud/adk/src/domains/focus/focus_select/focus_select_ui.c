/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This module is an implementation of the focus interface for UI inputs
            and UI context
*/

#include "focus_select.h"
#include "focus_select_audio.h"
#include "focus_select_status.h"
#include "focus_select_tie_break.h"
#include "focus_select_ui.h"

#include <audio_sources.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <device_list.h>
#include <device_properties.h>
#include <logging.h>
#include <panic.h>
#include <ui.h>
#include <voice_sources.h>

/* Look-up table mapping the voice_context symbol to the relative priority of
   that context in determining focus. This table considers priorities for UI
   interactions. 0 is the lowest priority. */
static int8 voice_context_to_ui_prio_mapping[] = {
    [context_voice_disconnected] = 0,
    [context_voice_connected] = 1,
    [context_voice_ringing_outgoing] = 4,
    [context_voice_ringing_incoming] = 5,
    [context_voice_in_call] = 3,
    [context_voice_in_call_with_incoming] = 5,
    [context_voice_in_call_with_outgoing] = 4,
    [context_voice_in_call_with_held] = 3,
    [context_voice_call_held] = 2,
    [context_voice_in_multiparty_call] = 3,
};
COMPILE_TIME_ASSERT(ARRAY_DIM(voice_context_to_ui_prio_mapping) == max_voice_contexts,
                    FOCUS_SELECT_invalid_size_ui_prio_mapping_table);

static source_cache_data_t * focusSelect_AudioSourceCalculatePriorityForUi(focus_status_t * focus_status, generic_source_t curr_source)
{
    uint8 source_priority = 0;
    unsigned source_context = BAD_CONTEXT;

    if (GenericSource_IsAudio(curr_source))
    {
        source_context = AudioSources_GetSourceContext(curr_source.u.audio);
        source_priority = source_context;
    }
    else
    {
        Panic();
    }

    PanicFalse(source_context != BAD_CONTEXT);

    return FocusSelect_SetCacheDataForSource(focus_status, curr_source, source_context, FALSE, source_priority);
}

static source_cache_data_t * focusSelect_VoiceSourceCalculatePriorityForUi(focus_status_t * focus_status, generic_source_t curr_source)
{
    uint8 source_priority = 0;
    unsigned source_context = BAD_CONTEXT;

    if (GenericSource_IsVoice(curr_source))
    {
        source_context = VoiceSources_GetSourceContext(curr_source.u.voice);
        source_priority = voice_context_to_ui_prio_mapping[source_context];
    }
    else
    {
        Panic();
    }

    PanicFalse(source_context != BAD_CONTEXT);

    return FocusSelect_SetCacheDataForSource(focus_status, curr_source, source_context, FALSE, source_priority);
}

bool FocusSelect_GetAudioSourceForContext(audio_source_t * audio_source)
{
    bool source_found = FALSE;
    focus_status_t focus_status = {0};

    *audio_source = audio_source_none;

    sources_iterator_t iter = SourcesIterator_Create(source_type_audio);
    source_found = FocusSelect_CompileFocusStatus(iter, &focus_status, focusSelect_AudioSourceCalculatePriorityForUi);
    SourcesIterator_Destroy(iter);

    if (source_found)
    {
        FocusSelect_HandleTieBreak(&focus_status);

        /* Assign selected audio source */
        *audio_source = focus_status.highest_priority_source.u.audio;
    }

    DEBUG_LOG_DEBUG("FocusSelect_GetAudioSourceForContext enum:audio_source_t:%d found=%d",
                    *audio_source, source_found);

    return source_found;
}

bool FocusSelect_GetAudioSourceForUiInput(ui_input_t ui_input, audio_source_t * audio_source)
{
    bool source_found = FALSE;
    focus_status_t focus_status = {0};

    /* For audio sources, we don't need to consider the UI Input type. This is because it
       is effectively prescreened by the UI component, which responds to the context returned
       by this module in the API FocusSelect_GetAudioSourceForContext().

       A concrete example being we should only receive ui_input_stop if
       FocusSelect_GetAudioSourceForContext() previously provided context_audio_is_streaming.
       In that case there can only be a single streaming source and it shall consume the
       UI Input. All other contentions are handled by FocusSelect_HandleTieBreak. */

    *audio_source = audio_source_none;

    sources_iterator_t iter = SourcesIterator_Create(source_type_audio);
    source_found = FocusSelect_CompileFocusStatus(iter, &focus_status, focusSelect_AudioSourceCalculatePriorityForUi);
    SourcesIterator_Destroy(iter);

    if (source_found)
    {
        FocusSelect_HandleTieBreak(&focus_status);

        /* Assign selected audio source */
        *audio_source = focus_status.highest_priority_source.u.audio;
    }

    DEBUG_LOG_DEBUG("FocusSelect_GetAudioSourceForUiInput enum:ui_input_t:%d enum:audio_source_t:%d found=%d",
                    ui_input, *audio_source, source_found);

    return source_found;
}

static bool focusSelect_GetVoiceSourceForUiInteractionWithIterator(sources_iterator_t iter, voice_source_t * voice_source)
{
    bool source_found = FALSE;
    focus_status_t focus_status = {0};
    *voice_source = voice_source_none;

    source_found = FocusSelect_CompileFocusStatus(iter, &focus_status, focusSelect_VoiceSourceCalculatePriorityForUi);

    if (source_found)
    {
        FocusSelect_HandleVoiceTieBreak(&focus_status);

        /* Assign selected voice source */
        *voice_source = focus_status.highest_priority_source.u.voice;
    }

    return source_found;
}

static bool focusSelect_GetVoiceSourceForUiInteraction(voice_source_t * voice_source)
{
    bool source_found;
    sources_iterator_t iter = SourcesIterator_Create(source_type_voice);
    source_found = focusSelect_GetVoiceSourceForUiInteractionWithIterator(iter, voice_source);
    SourcesIterator_Destroy(iter);

    return source_found;
}

bool FocusSelect_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t * voice_source)
{
    bool source_found = focusSelect_GetVoiceSourceForUiInteraction(voice_source);

    DEBUG_LOG_DEBUG("FocusSelect_GetVoiceSourceForContext enum:ui_providers_t:%d enum:voice_source_t:%d found=%d",
                    provider, *voice_source, source_found);

    return source_found;
}

bool FocusSelect_GetVoiceSourceInContextArray(ui_providers_t provider, voice_source_t * voice_source, const unsigned* contexts, const unsigned num_contexts)
{
    bool source_found;
    
    /* Create an empty iterator */
    sources_iterator_t iter = SourcesIterator_Create(source_type_invalid);
    
    /* Only add sources in requested contexts */
    SourcesIterator_AddSourcesInContextArray(iter, source_type_voice, contexts, num_contexts);
    
    /* Remove the voice_source passed in (does nothing if voice_source_none) */
    SourcesIterator_RemoveVoiceSource(iter, *voice_source);
    
    source_found = focusSelect_GetVoiceSourceForUiInteractionWithIterator(iter, voice_source);

    SourcesIterator_Destroy(iter);
    
    DEBUG_LOG_DEBUG("FocusSelect_GetVoiceSourceInContextArray enum:ui_providers_t:%d enum:voice_source_t:%d found=%d",
                    provider, *voice_source, source_found);

    return source_found;
}

bool FocusSelect_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t * voice_source)
{
    bool source_found = focusSelect_GetVoiceSourceForUiInteraction(voice_source);

    DEBUG_LOG_DEBUG("FocusSelect_GetVoiceSourceForUiInput enum:ui_input_t:%d enum:voice_source_t:%d found=%d",
                    ui_input, *voice_source, source_found);

    return source_found;
}

static bool focusSelect_GetHandsetDevice(device_t *device)
{
    DEBUG_LOG_FN_ENTRY("focusSelect_GetHandsetDevice");
    bool device_found = FALSE;

    PanicNull(device);

    for (uint8 pdl_index = 0; pdl_index < DeviceList_GetMaxTrustedDevices(); pdl_index++)
    {
        if (BtDevice_GetIndexedDevice(pdl_index, device))
        {
            if (BtDevice_GetDeviceType(*device) == DEVICE_TYPE_HANDSET)
            {

                uint8 is_excluded = FALSE;
                Device_GetPropertyU8(*device, device_property_excludelist, &is_excluded);

                if (!is_excluded)
                {
                    device_found = TRUE;
                    break;
                }
            }
        }
    }

    return device_found;
}

bool FocusSelect_ExcludeDevice(device_t device)
{
    DEBUG_LOG_FN_ENTRY("FocusSelect_ExcludeDevice device 0x%p", device);
    if (device)
    {
        return Device_SetPropertyU8(device, device_property_excludelist, TRUE);
    }
    return FALSE;
}

bool FocusSelect_IncludeDevice(device_t device)
{
    DEBUG_LOG_FN_ENTRY("FocusSelect_IncludeDevice device 0x%p", device);

    if (device)
    {
        return Device_SetPropertyU8(device, device_property_excludelist, FALSE);
    }
    return FALSE;
}

void FocusSelect_ResetExcludedDevices(void)
{
    DEBUG_LOG_FN_ENTRY("FocusSelect_ResetExcludedDevices");

    device_t* devices = NULL;
    unsigned num_devices = 0;
    uint8 excluded = TRUE;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_excludelist, 
                                        (void*)&excluded, sizeof(excluded), 
                                        &devices, &num_devices);

    if (devices && num_devices)
    {
        for (unsigned i = 0; i < num_devices; i++)
        {
            bdaddr handset_addr = DeviceProperties_GetBdAddr(devices[i]);
            bool is_acl_connected = ConManagerIsConnected(&handset_addr);

            /* Only remove the device from exclude list if ACL is not connected. */
            if(!is_acl_connected)
            {
                FocusSelect_IncludeDevice(devices[i]);
            }
        }
    }
    free(devices);
    devices = NULL;
}

/* Used to collect the device information to identify the lowest priority device.*/
typedef struct
{
    device_t highest_priority_device;
    device_t lowest_priority_device;
} device_focus_status_t;

static void focusSelect_GetMruHandsetDevice(device_focus_status_t *focus_status)
{
    uint8 is_mru_handset = TRUE;
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if(device != NULL)
    {
        focus_status->highest_priority_device = device;
    }
    else
    {
        deviceType type = DEVICE_TYPE_HANDSET;
        device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
        focus_status->highest_priority_device = device;
    }
}

static bool focusSelect_CompileConnectedDevicesFocusStatus(device_focus_status_t *focus_status)
{
    device_t* devices = NULL;
    bool device_found = FALSE;

    unsigned num_connected_handsets = BtDevice_GetConnectedBredrHandsets(&devices);

    if(num_connected_handsets == 1)
    {
        focus_status->highest_priority_device = devices[0];
        focus_status->lowest_priority_device = devices[0];
        device_found = TRUE;
    }
    else
    {
        if(FocusSelect_DeviceHasVoiceAudioFocus(devices[0]))
        {
            focus_status->highest_priority_device = devices[0];
        }
        else if(FocusSelect_DeviceHasVoiceAudioFocus(devices[1]))
        {
            focus_status->highest_priority_device = devices[1];
        }
        else
        {
            focusSelect_GetMruHandsetDevice(focus_status);
        }
        
        if(focus_status->highest_priority_device == devices[0])
        {
            focus_status->lowest_priority_device = devices[1];
        }
        else
        {
            focus_status->lowest_priority_device = devices[0];
        }

        if(focus_status->lowest_priority_device != NULL)
        {
            device_found = TRUE;
        }
    }
    free(devices);
    return device_found;
}

bool FocusSelect_GetDeviceForUiInput(ui_input_t ui_input, device_t * device)
{
    bool device_found = FALSE;

    switch (ui_input)
    {
        case ui_input_connect_handset:
            device_found = focusSelect_GetHandsetDevice(device);
            break;
        case ui_input_disconnect_lru_handset:
            DEBUG_LOG_DEBUG("FocusSelect_GetDeviceForUiInput enum:ui_input_t:%d", ui_input);
            device_focus_status_t focus_status = {0};
            device_found = focusSelect_CompileConnectedDevicesFocusStatus(&focus_status);
            if(device_found)
            {
                *device = focus_status.lowest_priority_device;
            }
            break;
        default:
            DEBUG_LOG_WARN("FocusSelect_GetDeviceForUiInput enum:ui_input_t:%d not supported", ui_input);
            break;
    }

    return device_found;
}

bool FocusSelect_GetDeviceForContext(ui_providers_t provider, device_t* device)
{
    UNUSED(provider);
    UNUSED(device);

    return FALSE;
}
