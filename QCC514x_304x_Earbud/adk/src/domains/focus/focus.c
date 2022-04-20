/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domains Focus
\ingroup    domains
\brief      Module implementing the interface by which the application framework can
            call a concrete focus select implementation, either focus select or a
            customer module.
*/

#include "focus_audio_source.h"
#include "focus_device.h"
#include "focus_generic_source.h"
#include "focus_voice_source.h"

static const focus_device_t * select_focused_device_fns = NULL;
static const focus_get_audio_source_t * select_focused_audio_source_fns = NULL;
static const focus_get_voice_source_t * select_focused_voice_source_fns = NULL;
static const focus_get_generic_source_t * select_focused_generic_source_fns = NULL;

void Focus_ConfigureDevice(focus_device_t const * focus_device)
{
    select_focused_device_fns = focus_device;
}

bool Focus_GetDeviceForContext(ui_providers_t provider, device_t* device)
{
    if (select_focused_device_fns && select_focused_device_fns->for_context)
    {
        return select_focused_device_fns->for_context(provider, device);
    }
    return FALSE;
}

bool Focus_GetDeviceForUiInput(ui_input_t ui_input, device_t* device)
{
    if (select_focused_device_fns && select_focused_device_fns->for_ui_input)
    {
        return select_focused_device_fns->for_ui_input(ui_input, device);
    }
    return FALSE;
}

focus_t Focus_GetFocusForDevice(const device_t device)
{
    focus_t device_focus = focus_none;
    if (select_focused_device_fns && select_focused_device_fns->focus)
    {
        device_focus = select_focused_device_fns->focus(device);
    }
    return device_focus;
}

bool Focus_ExcludeDevice(device_t device)
{
    if (select_focused_device_fns && select_focused_device_fns->add_to_excludelist)
    {
        return select_focused_device_fns->add_to_excludelist(device);
    }

    return FALSE;
}

bool Focus_IncludeDevice(device_t device)
{
    if (select_focused_device_fns && select_focused_device_fns->remove_from_excludelist)
    {
        return select_focused_device_fns->remove_from_excludelist(device);
    }

    return FALSE;
}

void Focus_ResetExcludedDevices(void)
{
    if (select_focused_device_fns && select_focused_device_fns->reset_excludelist)
    {
        select_focused_device_fns->reset_excludelist();
    }
}

void Focus_ConfigureAudioSource(focus_get_audio_source_t const * focus_audio_source)
{
    select_focused_audio_source_fns = focus_audio_source;
}

bool Focus_GetAudioSourceForContext(audio_source_t* audio_source)
{
    if (select_focused_audio_source_fns && select_focused_audio_source_fns->for_context)
    {
        return select_focused_audio_source_fns->for_context(audio_source);
    }
    return FALSE;
}

bool Focus_GetAudioSourceForUiInput(ui_input_t ui_input, audio_source_t* audio_source)
{
    if (select_focused_audio_source_fns && select_focused_audio_source_fns->for_ui_input)
    {
        return select_focused_audio_source_fns->for_ui_input(ui_input, audio_source);
    }
    return FALSE;
}

focus_t Focus_GetFocusForAudioSource(const audio_source_t audio_source)
{
    focus_t audio_source_focus = focus_none;
    if (select_focused_audio_source_fns && select_focused_audio_source_fns->focus)
    {
        audio_source_focus = select_focused_audio_source_fns->focus(audio_source);
    }
    return audio_source_focus;
}

void Focus_ConfigureVoiceSource(focus_get_voice_source_t const * focus_voice_source)
{
    select_focused_voice_source_fns = focus_voice_source;
}

bool Focus_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t* voice_source)
{
    if (select_focused_voice_source_fns && select_focused_voice_source_fns->for_context)
    {
        return select_focused_voice_source_fns->for_context(provider, voice_source);
    }
    return FALSE;
}

bool Focus_GetVoiceSourceInContextArray(ui_providers_t provider, voice_source_t* voice_source, const unsigned* contexts, const unsigned num_contexts)
{
    if (select_focused_voice_source_fns && select_focused_voice_source_fns->in_contexts)
    {
        return select_focused_voice_source_fns->in_contexts(provider, voice_source, contexts, num_contexts);
    }
    return FALSE;
}

bool Focus_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t* voice_source)
{
    if (select_focused_voice_source_fns && select_focused_voice_source_fns->for_ui_input)
    {
        return select_focused_voice_source_fns->for_ui_input(ui_input, voice_source);
    }
    return FALSE;
}

focus_t Focus_GetFocusForVoiceSource(const voice_source_t voice_source)
{
    focus_t voice_source_focus = focus_none;
    if (select_focused_voice_source_fns && select_focused_voice_source_fns->focus)
    {
        voice_source_focus = select_focused_voice_source_fns->focus(voice_source);
    }
    return voice_source_focus;
}

void Focus_ConfigureGenericSource(focus_get_generic_source_t const * focus_generic_source)
{
    select_focused_generic_source_fns = focus_generic_source;
}

generic_source_t Focus_GetFocusedGenericSourceForAudioRouting(void)
{
    generic_source_t source = {.type=source_type_invalid, .u.voice=voice_source_none};
    if (select_focused_generic_source_fns && select_focused_generic_source_fns->for_audio_routing)
    {
        source = select_focused_generic_source_fns->for_audio_routing();
    }
    return source;
}
