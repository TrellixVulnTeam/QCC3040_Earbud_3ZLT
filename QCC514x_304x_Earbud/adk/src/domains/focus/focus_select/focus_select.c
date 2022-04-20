/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      This module is an implementation of the focus interface which supports
            selecting the active, focussed device during multipoint use cases.
*/

#include "focus_select.h"
#include "focus_select_audio.h"
#include "focus_select_status.h"
#include "focus_select_tie_break.h"
#include "focus_select_ui.h"

#include <focus_audio_source.h>
#include <focus_device.h>
#include <focus_generic_source.h>
#include <focus_voice_source.h>

#include <logging.h>

static const focus_device_t interface_fns_for_device =
{
    .for_context = FocusSelect_GetDeviceForContext,
    .for_ui_input = FocusSelect_GetDeviceForUiInput,
    .focus = FocusSelect_GetFocusForDevice,
    .add_to_excludelist = FocusSelect_ExcludeDevice,
    .remove_from_excludelist = FocusSelect_IncludeDevice,
    .reset_excludelist = FocusSelect_ResetExcludedDevices
};

static const focus_get_audio_source_t interface_fns =
{
    .for_context = FocusSelect_GetAudioSourceForContext,
    .for_ui_input = FocusSelect_GetAudioSourceForUiInput,
    .focus = FocusSelect_GetFocusForAudioSource
};

static const focus_get_voice_source_t voice_interface_fns =
{
    .for_context = FocusSelect_GetVoiceSourceForContext,
    .for_ui_input = FocusSelect_GetVoiceSourceForUiInput,
    .focus = FocusSelect_GetFocusForVoiceSource,
    .in_contexts = FocusSelect_GetVoiceSourceInContextArray
};

static const focus_get_generic_source_t generic_source_interface_fns =
{
    .for_audio_routing = FocusSelect_GetFocusedSourceForAudioRouting
};

bool FocusSelect_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_FN_ENTRY("FocusSelect_Init");

    Focus_ConfigureDevice(&interface_fns_for_device);
    Focus_ConfigureAudioSource(&interface_fns);
    Focus_ConfigureVoiceSource(&voice_interface_fns);
    Focus_ConfigureGenericSource(&generic_source_interface_fns);
    FocusSelect_TieBreakInit();

    return TRUE;
}
