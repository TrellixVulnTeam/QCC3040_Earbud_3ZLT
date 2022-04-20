/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief      Implementation of audio router functions for single signal path.
*/

#include "single_entity.h"
#include "audio_router.h"
#include "focus_audio_source.h"
#include "focus_generic_source.h"
#include "focus_voice_source.h"
#include "logging.h"
#include "single_entity_data.h"
#include "device.h"
#include "device_properties.h"
#include "av.h"
#include "panic.h"
#include "stdlib.h"

static void singleEntity_AddSource(generic_source_t source);
static bool singleEntity_RemoveSource(generic_source_t source);
static bool singleEntity_IsDeviceInUse(device_t device);
static void singleEntity_Update(void);

static const audio_router_t single_entity_router_functions = 
{
    .add_source = singleEntity_AddSource,
    .remove_source = singleEntity_RemoveSource,
    .is_device_in_use = singleEntity_IsDeviceInUse,
    .update = singleEntity_Update
};

bool SingleEntity_Init(Task init_task)
{
    UNUSED(init_task);
    AudioRouter_Init();
    AudioRouter_ConfigureHandlers(SingleEntity_GetHandlers());
    AudioRouter_InitData();
    return TRUE;
}

const audio_router_t* SingleEntity_GetHandlers(void)
{
    DEBUG_LOG_FN_ENTRY("SingleEntity_GetHandlers");

    return &single_entity_router_functions;
}

static source_status_t singleEntity_StateConnectingAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_connecting);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateConnectingAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        if(!SingleEntityData_IsSourcePresent(source))
        {   /* don't continue connecting if source has gone */
            /* start disconnecting */
            SingleEntityData_SetSourceState(source, audio_router_state_disconnecting_no_connect);
        }
        else
        {
            if(!AudioRouter_CommonConnectSource(source))
            {
                DEBUG_LOG_INFO("singleEntity_StateConnectingAction unable to connect audio");
                /* If audio routing failed, moved to disconnecting_no_connect state */
                SingleEntityData_SetSourceState(source, audio_router_state_disconnecting_no_connect);
            }
            else
            {
                SingleEntityData_SetSourceState(source, audio_router_state_connected_pending);
            }
        }
    }
    return status;
}

static source_status_t singleEntity_StateConnectedPendingAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_connected);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateConnectedPendingAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        SingleEntityData_SetSourceState(source, audio_router_state_connected);
    }

    return status;
}

static source_status_t singleEntity_StateDisconnectingAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_disconnecting);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateDisconnectingAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        if(AudioRouter_CommonDisconnectSource(source))
        {
            SingleEntityData_SetSourceState(source, audio_router_state_disconnected_pending);
        }
        else
        {
            DEBUG_LOG_INFO("singleEntity_StateDisconnectingAction unable to disconnect audio");
            Panic();
        }
    }
    return status;
}

static source_status_t singleEntity_StateInterruptingAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_disconnecting);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateInterruptingAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        if(AudioRouter_CommonDisconnectSource(source))
        {
            SingleEntityData_SetSourceState(source, audio_router_state_interrupted_pending);
        }
        else
        {
            DEBUG_LOG_INFO("singleEntity_StateInterruptingAction unable to disconnect audio");
            Panic();
        }
    }
    return status;
}

static source_status_t singleEntity_StateInterruptedPendingAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_disconnected);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateDisconnectedPendingAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        SingleEntityData_SetSourceState(source, audio_router_state_interrupted);
    }

    return status;
}

static source_status_t singleEntity_StateDisconnectingNoConnectAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_disconnecting);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateDisconnectingNoConnectAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        SingleEntityData_SetSourceState(source, audio_router_state_disconnected_pending);
    }

    return status;
}

static source_status_t singleEntity_StateDisconnectedPendingAction(generic_source_t source)
{
    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_disconnected);

    DEBUG_LOG_FN_ENTRY("singleEntity_StateDisconnectedPendingAction response enum:source_status_t:%d", status);

    if(status == source_status_ready)
    {
        SingleEntityData_SetSourceState(source, audio_router_state_disconnected);
    }

    return status;
}

static bool singleEntity_AttemptStableState(generic_source_t source)
{
#define MAX_LOOP 8

    bool stable = FALSE;
    unsigned iterations = MAX_LOOP;
    source_status_t response = source_status_error;

    DEBUG_LOG_FN_ENTRY("singleEntity_AttemptStableState source enum:source_type_t:%d, source=%d",
                        source.type, source.u.audio);

    while(iterations-- && !stable)
    {
        switch(SingleEntityData_GetSourceState(source))
        {
            case audio_router_state_connected:
            case audio_router_state_disconnected:
            case audio_router_state_interrupted:
            case audio_router_state_invalid:
            case audio_router_state_to_be_interrupted:
            case audio_router_state_to_be_resumed:
                stable = TRUE;
                break;

            case audio_router_state_connected_pending:
                response = singleEntity_StateConnectedPendingAction(source);
                break;

            case audio_router_state_connecting:
                response = singleEntity_StateConnectingAction(source);
                break;

            case audio_router_state_disconnecting:
                response = singleEntity_StateDisconnectingAction(source);
                break;

            case audio_router_state_disconnecting_no_connect:
                response = singleEntity_StateDisconnectingNoConnectAction(source);
                break;

            case audio_router_state_disconnected_pending:
                response = singleEntity_StateDisconnectedPendingAction(source);
                break;

            case audio_router_state_interrupting:
                response = singleEntity_StateInterruptingAction(source);
                break;

            case audio_router_state_interrupted_pending:
                response = singleEntity_StateInterruptedPendingAction(source);
                break;

            default:
                Panic();
                break;
        }

        if(!stable)
        {
            if(response == source_status_preparing)
            {
                break;
            }
            /* If response is anything other than ready at this point we have
               encountered an error */
            PanicFalse(response == source_status_ready);
        }
    }
    /* If we exited the while loop because the counter hit zero something has gone wrong */
    if(!iterations)
    {
        DEBUG_LOG_INFO("singleEntity_AttemptStableState failed to reach stable state");
        Panic();
    }

    return stable;
}

static bool singleEntity_RetryIfIntermediate(void)
{
    generic_source_t source = {0};

    DEBUG_LOG_FN_ENTRY("singleEntity_RetryIfIntermediate");

    if(SingleEntityData_FindTransientSource(&source))
    {
        return singleEntity_AttemptStableState(source);
    }

    return TRUE;
}

static bool singleEntity_IsSourceIncoming(source_routing_t * sources)
{
    bool is_incoming_audio_source = sources->highest_priority_source.type == source_type_audio &&
                                    sources->highest_priority_source.u.audio != audio_source_none &&
                                    sources->highest_priority_source_context.audio > context_audio_is_playing;
    bool is_incoming_voice_source = sources->highest_priority_source.type == source_type_voice &&
                                    sources->highest_priority_source.u.voice != voice_source_none &&
                                    sources->highest_priority_source_context.voice > context_voice_connected;
    bool is_source_incoming =  (is_incoming_audio_source || is_incoming_voice_source) &&
                               !SingleEntityData_AreSourcesSame(sources->highest_priority_source, sources->source_to_route) &&
                               !SingleEntityData_AreSourcesSame(sources->highest_priority_source, sources->routed_source);

    DEBUG_LOG_VERBOSE("singleEntity_IsSourceIncoming %u", is_source_incoming);

    return is_source_incoming;
}

static void singleEntity_PrintSources(source_routing_t * sources)
{
    DEBUG_LOG_INFO("singleEntity_PrintSources highest_priority_source type=enum:source_type_t:%d, source=%d state=enum:audio_router_state_t:%d",
                   sources->highest_priority_source.type, (sources->highest_priority_source.type == source_type_audio) ? sources->highest_priority_source.u.audio : sources->highest_priority_source.u.voice, SingleEntityData_GetSourceState(sources->highest_priority_source));

    if(sources->have_interrupted_source)
    {
        DEBUG_LOG_INFO("singleEntity_PrintSources interrupted_source type=enum:source_type_t:%d, source=%d state=enum:audio_router_state_t:%d",
                       sources->interrupted_source.type, (sources->interrupted_source.type == source_type_audio) ? sources->interrupted_source.u.audio : sources->interrupted_source.u.voice, SingleEntityData_GetSourceState(sources->interrupted_source));
    }

    if(sources->have_source_to_route)
    {
        DEBUG_LOG_INFO("singleEntity_PrintSources source_to_route type=enum:source_type_t:%d, source=%d state=enum:audio_router_state_t:%d",
                       sources->source_to_route.type, (sources->source_to_route.type == source_type_audio) ? sources->source_to_route.u.audio : sources->source_to_route.u.voice, SingleEntityData_GetSourceState(sources->source_to_route));
    }

    if(sources->have_routed_source)
    {
        DEBUG_LOG_INFO("singleEntity_PrintSources routed_source type=enum:source_type_t:%d, source=%d state=enum:audio_router_state_t:%d",
                       sources->routed_source.type, (sources->routed_source.type == source_type_audio) ? sources->routed_source.u.audio : sources->routed_source.u.voice, SingleEntityData_GetSourceState(sources->routed_source));
    }
}

static bool singleEntity_IsDisconnectingOnly(source_routing_t * sources)
{
    return sources->have_routed_source && (!sources->have_source_to_route);
}

static bool singleEntity_IsConnectingOnly(source_routing_t * sources)
{
    return (!sources->have_routed_source) && sources->have_source_to_route;
}

static bool singleEntity_IsChangingSource(source_routing_t * sources)
{
    return sources->have_routed_source && sources->have_source_to_route && !SingleEntityData_AreSourcesSame(sources->source_to_route, sources->routed_source);
}

static bool singleEntity_HasRoutedSourceBeenRemoved(source_routing_t * sources)
{
    return sources->have_routed_source && sources->have_source_to_route && SingleEntityData_AreSourcesSame(sources->source_to_route, sources->routed_source) && !SingleEntityData_IsSourcePresent(sources->source_to_route);
}

static bool singleEntity_ConnectSourceToRoute(source_routing_t * sources)
{
    audio_router_state_t source_to_route_state = SingleEntityData_GetSourceState(sources->source_to_route);

    DEBUG_LOG_VERBOSE("singleEntity_ConnectSourceToRoute Connecting enum:source_type_t:%d, source=%d",
                      sources->source_to_route.type, sources->source_to_route.u.audio);

    if((source_to_route_state != audio_router_state_disconnected) && (source_to_route_state != audio_router_state_new_source) && (source_to_route_state != audio_router_state_to_be_resumed))
    {
        DEBUG_LOG_INFO("singleEntity_ConnectSourceToRoute cannot connect a source in enum:audio_router_state_t:%d", source_to_route_state);
        Panic();
    }

    if(sources->have_routed_source && sources->have_interrupted_source)
    {
        DEBUG_LOG_INFO("singleEntity_ConnectSourceToRoute something was routed, and we are replacing it with something other than the interrupted source");
        SingleEntityData_SetSourceState(sources->interrupted_source, audio_router_state_disconnected);
    }

    SingleEntityData_SetSourceState(sources->source_to_route, audio_router_state_connecting);

    return singleEntity_AttemptStableState(sources->source_to_route);
}

static bool singleEntity_ShouldInterruptRoutedSource(source_routing_t * sources)
{
    bool routed_source_can_be_interrupted = !sources->have_interrupted_source && sources->have_routed_source && sources->routed_source.type == source_type_audio &&
                                            (sources->routed_source_context.audio == context_audio_is_playing) &&
                                            SingleEntityData_GetSourceState(sources->routed_source) != audio_router_state_to_be_interrupted;
    bool source_incoming = singleEntity_IsSourceIncoming(sources);

    return (singleEntity_IsChangingSource(sources) || (singleEntity_IsDisconnectingOnly(sources) && source_incoming)) && routed_source_can_be_interrupted;
}

static bool singleEntity_HasRoutedSourceBeenMarkedForInterruption(source_routing_t * sources)
{
    return !sources->have_interrupted_source && (SingleEntityData_GetSourceState(sources->routed_source) == audio_router_state_to_be_interrupted);
}

static bool singleEntity_DisconnectRoutedSource(source_routing_t * sources)
{
    DEBUG_LOG_VERBOSE("singleEntity_DisconnectRoutedSource disconnecting enum:source_type_t:%d, source=%d",
                      sources->routed_source.type, sources->routed_source.u.audio);

    audio_router_state_t routed_source_state = SingleEntityData_GetSourceState(sources->routed_source);

    if(routed_source_state != audio_router_state_connected && routed_source_state != audio_router_state_to_be_interrupted)
    {
        DEBUG_LOG_INFO("singleEntity_DisconnectRoutedSource cannot disconnect a source that isn't connected");
        Panic();
    }

    if(singleEntity_HasRoutedSourceBeenMarkedForInterruption(sources))
    {
        SingleEntityData_SetSourceState(sources->routed_source, audio_router_state_interrupting);
    }
    else
    {
        SingleEntityData_SetSourceState(sources->routed_source, audio_router_state_disconnecting);
    }

    return singleEntity_AttemptStableState(sources->routed_source);
}

static void singleEntity_InterruptSource(generic_source_t source_to_interrupt)
{
    DEBUG_LOG_FN_ENTRY("singleEntity_InterruptSource");
    if(source_to_interrupt.type == source_type_audio)
    {   
        AudioSources_Pause(source_to_interrupt.u.audio);
        SingleEntityData_SetSourceState(source_to_interrupt, audio_router_state_to_be_interrupted);
    }
}

static bool singleEntity_ShouldResumeInterruptedSource(source_routing_t * sources)
{
    bool only_interrupted_source_to_route = !sources->have_source_to_route && sources->have_routed_source && sources->have_interrupted_source;
    bool interrupted_source_matches_source_to_route = sources->have_source_to_route && sources->have_interrupted_source &&
                                                      SingleEntityData_AreSourcesSame(sources->source_to_route, sources->interrupted_source);
    bool interrupted_source_to_replace_inactive_source = singleEntity_HasRoutedSourceBeenRemoved(sources) && sources->have_interrupted_source;
    bool va_session_is_ongoing = sources->have_routed_source && (sources->routed_source.type == source_type_audio) &&
                                 (sources->routed_source_context.audio == context_audio_is_va_response);

    return (only_interrupted_source_to_route || interrupted_source_to_replace_inactive_source || interrupted_source_matches_source_to_route) &&
           !singleEntity_IsSourceIncoming(sources) &&
           !va_session_is_ongoing &&
           (SingleEntityData_GetSourceState(sources->interrupted_source) != audio_router_state_to_be_resumed);
}

static void singleEntity_ResumeSource(generic_source_t source_to_resume)
{
    DEBUG_LOG_FN_ENTRY("singleEntity_ResumeSource");
    if(source_to_resume.type == source_type_audio)
    {
        AudioSources_Play(source_to_resume.u.audio);
        SingleEntityData_SetSourceState(source_to_resume, audio_router_state_to_be_resumed);
    }
}

static bool singleEntity_HasInterruptedSourceBeenResumedElsewhere(source_routing_t * sources)
{
    bool interrupted_source_matches_source_to_route = sources->have_source_to_route && sources->have_interrupted_source &&
                                                      SingleEntityData_AreSourcesSame(sources->source_to_route, sources->interrupted_source);
    bool interrupted_source_is_playing = sources->have_interrupted_source && sources->interrupted_source.type == source_type_audio &&
                                         sources->interrupted_source_context.audio == context_audio_is_playing;

    return interrupted_source_matches_source_to_route && interrupted_source_is_playing;
}

static bool singleEntity_RefreshRoutedSource(source_routing_t * sources)
{
    bool stable = TRUE;

    DEBUG_LOG_FN_ENTRY("singleEntity_RefreshRoutedSource");

    if(singleEntity_IsDisconnectingOnly(sources) || singleEntity_IsChangingSource(sources) || singleEntity_HasRoutedSourceBeenRemoved(sources))
    {
        stable = singleEntity_DisconnectRoutedSource(sources);
        DEBUG_LOG("singleEntity_RefreshRoutedSource disconnected source, stable %d", stable);
    }

    if((singleEntity_IsConnectingOnly(sources) || singleEntity_IsChangingSource(sources)) && stable)
    {
        stable = singleEntity_ConnectSourceToRoute(sources);
    }

    return stable;
}

static void singleEntity_PopulateSourceContexts(source_routing_t * sources)
{
    if(sources->highest_priority_source.type == source_type_audio)
    {
        sources->highest_priority_source_context.audio = AudioSources_GetSourceContext(sources->highest_priority_source.u.audio);
        DEBUG_LOG("singleEntity_PopulateSourceContexts highest_priority_source enum:audio_source_provider_context_t:%d", sources->highest_priority_source_context.audio);
    }
    else if(sources->highest_priority_source.type == source_type_voice)
    {
        sources->highest_priority_source_context.voice = VoiceSources_GetSourceContext(sources->highest_priority_source.u.voice);
        DEBUG_LOG("singleEntity_PopulateSourceContexts highest_priority_source enum:voice_source_provider_context_t:%d", sources->highest_priority_source_context.voice);
    }

    if(sources->have_interrupted_source)
    {
        if(sources->interrupted_source.type == source_type_audio)
        {
            sources->interrupted_source_context.audio = AudioSources_GetSourceContext(sources->interrupted_source.u.audio);
            DEBUG_LOG("singleEntity_PopulateSourceContexts interrupted_source enum:audio_source_provider_context_t:%d", sources->interrupted_source_context.audio);
        }
        else if(sources->interrupted_source.type == source_type_voice)
        {
            sources->interrupted_source_context.voice = VoiceSources_GetSourceContext(sources->interrupted_source.u.voice);
            DEBUG_LOG("singleEntity_PopulateSourceContexts interrupted_source enum:voice_source_provider_context_t:%d", sources->interrupted_source_context.voice);
        }
    }

    if(sources->have_routed_source)
    {
        if(sources->routed_source.type == source_type_audio)
        {
            sources->routed_source_context.audio = AudioSources_GetSourceContext(sources->routed_source.u.audio);
            DEBUG_LOG("singleEntity_PopulateSourceContexts routed_source enum:audio_source_provider_context_t:%d", sources->routed_source_context.audio);
        }
        else if(sources->routed_source.type == source_type_voice)
        {
            sources->routed_source_context.voice = VoiceSources_GetSourceContext(sources->routed_source.u.voice);
            DEBUG_LOG("singleEntity_PopulateSourceContexts routed_source enum:voice_source_provider_context_t:%d", sources->routed_source_context.voice);
        }
    }
}

static source_routing_t singleEntity_GetSources(void)
{
    source_routing_t sources = {0};

    sources.highest_priority_source = Focus_GetFocusedGenericSourceForAudioRouting();
    sources.have_source_to_route = SingleEntityData_GetSourceToRoute(&sources.source_to_route);
    sources.have_routed_source = SingleEntityData_GetActiveSource(&sources.routed_source);
    sources.have_interrupted_source = SingleEntityData_GetInterruptedSource(&sources.interrupted_source);

    singleEntity_PopulateSourceContexts(&sources);

    return sources;
}

static void singleEntity_Update(void)
{
    if(singleEntity_RetryIfIntermediate())
    {
        source_routing_t sources = singleEntity_GetSources();

        if (singleEntity_HasInterruptedSourceBeenResumedElsewhere(&sources))
        {
            SingleEntityData_SetSourceState(sources.interrupted_source, audio_router_state_to_be_resumed);
        }

        singleEntity_PrintSources(&sources);

        if(singleEntity_ShouldInterruptRoutedSource(&sources))
        {
            singleEntity_InterruptSource(sources.routed_source);
        }
        else if(singleEntity_ShouldResumeInterruptedSource(&sources))
        {
            singleEntity_ResumeSource(sources.interrupted_source);
        }
        else
        {
            if(singleEntity_RefreshRoutedSource(&sources))
            {
                generic_source_t source = {0};
                /* Notify new sources that won't be routed */
                while(SingleEntityData_FindNewSource(&source))
                {
                    source_status_t status = AudioRouter_CommonSetSourceState(source, source_state_disconnected);
                    if(status != source_status_ready)
                    {
                        /* debug message */
                        Panic();
                    }
                    SingleEntityData_SetSourceState(source, audio_router_state_disconnected);
                }
            }
        }
    }
}

static void singleEntity_AddSource(generic_source_t source)
{
    DEBUG_LOG_FN_ENTRY("singleEntity_AddSource enum:source_type_t:%d, source=%d", source.type, (unsigned)source.u.audio);

    if(SingleEntityData_AddSource(source))
    {
        singleEntity_Update();
    }
}

static bool singleEntity_RemoveSource(generic_source_t source)
{
    bool status = FALSE;

    DEBUG_LOG_FN_ENTRY("singleEntity_RemoveSource enum:source_type_t:%d, source=%d", source.type, (unsigned)source.u.audio);

    if(SingleEntityData_RemoveSource(source))
    {
        status = TRUE;
        singleEntity_Update();
    }

    return status;
}

static bool singleEntity_GetAudioSourceForDevice(device_t device, generic_source_t* source)
{
    audio_source_t audio_source;

    DEBUG_LOG_FN_ENTRY("singleEntity_GetAudioSourceForDevice");

    audio_source = DeviceProperties_GetAudioSource(device);

    if(audio_source != audio_source_none)
    {
        source->type = source_type_audio;
        source->u.audio = audio_source;
        return TRUE;
    }
    return FALSE;
}

static bool singleEntity_GetVoiceSourceForDevice(device_t device, generic_source_t* source)
{
    /* TODO do this once there's the notion of multiple HFP sources */
    UNUSED(device);
    UNUSED(source);

    DEBUG_LOG_FN_ENTRY("singleEntity_GetVoiceSourceForDevice");

    return FALSE;
}

static bool singleEntity_IsDeviceInUse(device_t device)
{
    generic_source_t source = {0};

    bool in_use = FALSE;

    DEBUG_LOG_FN_ENTRY("singleEntity_IsDeviceInUse");

    if(singleEntity_GetAudioSourceForDevice(device, &source))
    {
        if(SingleEntityData_IsSourceActive(source))
        {
            in_use = TRUE;
        }
    }

    if(!in_use && singleEntity_GetVoiceSourceForDevice(device, &source))
    {
        if(SingleEntityData_IsSourceActive(source))
        {
            in_use = TRUE;
        }
    }

    if(in_use)
    {
        DEBUG_LOG_VERBOSE("singleEntity_IsDeviceInUse enum:source_type_t:%d, source=%d", source.type, source.u.audio);
    }

    return in_use;
}
