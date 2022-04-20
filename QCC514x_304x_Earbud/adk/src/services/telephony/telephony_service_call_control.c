/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Telephony Service Call Control
*/

#include "telephony_service.h"
#include "telephony_service_call_control.h"

#include <voice_sources.h>

#include <focus_voice_source.h>
#include <panic.h>
#include <logging.h>

static const unsigned held_remaining_contexts[] = 
{
    context_voice_call_held
};

static const unsigned active_contexts[] = 
{
    context_voice_in_call,
    context_voice_in_call_with_incoming,
    context_voice_in_call_with_outgoing,
    context_voice_in_call_with_held,
    context_voice_in_multiparty_call
};

static const unsigned incoming_outgoing_or_held_contexts[] = 
{
    context_voice_ringing_incoming,
    context_voice_ringing_outgoing,
    context_voice_in_call_with_incoming,
    context_voice_in_call_with_outgoing,
    context_voice_in_call_with_held,
    context_voice_call_held
};

static bool telephonyService_FindHeldCallRemainingExcludingSource(voice_source_t* source, voice_source_t source_to_exclude)
{
    *source = source_to_exclude;
    return Focus_GetVoiceSourceInContexts(ui_provider_telephony, source, held_remaining_contexts);
}

static bool telephonyService_FindHeldCallRemaining(voice_source_t* source)
{
    return telephonyService_FindHeldCallRemainingExcludingSource(source, voice_source_none);
}

static bool telephonyService_FindActiveCallExcludingSource(voice_source_t* source, voice_source_t source_to_exclude)
{
    *source = source_to_exclude;
    return Focus_GetVoiceSourceInContexts(ui_provider_telephony, source, active_contexts);
}

bool TelephonyService_FindActiveCall(voice_source_t* source)
{
    return telephonyService_FindActiveCallExcludingSource(source, voice_source_none);
}

bool TelephonyService_FindIncomingOutgoingOrHeldCall(voice_source_t* source)
{
    *source = voice_source_none;
    return Focus_GetVoiceSourceInContexts(ui_provider_telephony, source, incoming_outgoing_or_held_contexts);
}

static void telephonyService_HangUpActiveCallOnOtherSource(voice_source_t source_to_exclude)
{
    voice_source_t source;
    
    if(telephonyService_FindActiveCallExcludingSource(&source, source_to_exclude))
    {
        VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_ag);
        TelephonyService_HangUpCall(source);
    }
}

static void telephonyService_HoldActiveCallOnOtherSource(voice_source_t source_to_exclude)
{
    voice_source_t source;
    
    if(telephonyService_FindActiveCallExcludingSource(&source, source_to_exclude))
    {
        TelephonyService_HoldCall(source);
    }
}

static void telephonyService_ResumeHeldCallOnOtherSource(voice_source_t source_to_exclude)
{
    voice_source_t source;
    
    if(telephonyService_FindHeldCallRemainingExcludingSource(&source, source_to_exclude))
    {
        TelephonyService_ResumeCall(source);
    }
}

void TelephonyService_ResumeHighestPriorityHeldCallRemaining(void)
{
    voice_source_t source;
    if(telephonyService_FindHeldCallRemaining(&source))
    {
        DEBUG_LOG("telephonyService_ResumeHighestPriorityHeldCallRemaining enum:voice_source_t:%d",source);
        TelephonyService_ResumeCall(source);
    }
    else
    {
        DEBUG_LOG("telephonyService_ResumeHighestPriorityHeldCallRemaining no held calls remaining");
    }
}

void TelephonyService_HangUpCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_ringing_outgoing:
        case context_voice_in_call:
        {
            VoiceSources_TerminateOngoingCall(source);
            break;
        }
        
        case context_voice_in_call_with_outgoing:
        case context_voice_in_call_with_held:
        case context_voice_in_multiparty_call:
        {
            VoiceSources_TwcControl(source, voice_source_release_active_accept_other);
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_HangUpCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}

void TelephonyService_AnswerCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    
    telephonyService_HangUpActiveCallOnOtherSource(source);
    
    switch(context)
    {
        case context_voice_ringing_incoming:
        {
            VoiceSources_AcceptIncomingCall(source);
            break;
        }
        
        case context_voice_in_call_with_incoming:
        {
            VoiceSources_TwcControl(source, voice_source_release_active_accept_other);
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_AnswerCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}

void TelephonyService_RejectCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_ringing_incoming:
        {
            VoiceSources_RejectIncomingCall(source);
            break;
        }
        
        case context_voice_in_call_with_incoming:
        case context_voice_call_held:
        {
            VoiceSources_TwcControl(source, voice_source_release_held_reject_waiting);
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_RejectCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}

void TelephonyService_CycleToNextCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_ringing_incoming:
        {
            telephonyService_HoldActiveCallOnOtherSource(source);
            VoiceSources_AcceptIncomingCall(source);
            break;
        }
        
        case context_voice_in_call_with_incoming:
        case context_voice_in_call_with_outgoing:
        case context_voice_in_call_with_held:
        {
            /* Two calls on one handset, use TWC as normal */
            VoiceSources_TwcControl(source, voice_source_hold_active_accept_other);
            break;
        }
        
        case context_voice_call_held:
        {
            /* One call on each handset, hold active and resume held */
            telephonyService_HoldActiveCallOnOtherSource(source);
            TelephonyService_ResumeCall(source);
            break;
        }
        
        case context_voice_in_call:
        {
            /* One call on each handset, hold active and resume held */
            TelephonyService_HoldCall(source);
            telephonyService_ResumeHeldCallOnOtherSource(source);
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_CycleToNextCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}

void TelephonyService_JoinCalls(voice_source_t source, telephony_join_calls_action_t action)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_in_call_with_incoming:
        case context_voice_in_call_with_outgoing:
        case context_voice_in_call_with_held:
        {
            if(action == telephony_join_calls_and_leave)
            {
                VoiceSources_TwcControl(source, voice_source_join_calls_and_hang_up);
            }
            else
            {
                VoiceSources_TwcControl(source, voice_source_add_held_to_multiparty);
            }
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_JoinCalls enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}

void TelephonyService_HoldCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_in_call:
        {
            VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_ag);
            VoiceSources_TwcControl(source, voice_source_hold_active_accept_other);
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_HoldCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}

void TelephonyService_ResumeCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_call_held:
        {
            VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_hfp);
            VoiceSources_TwcControl(source, voice_source_hold_active_accept_other);
            break;
        }
        
        default:
        {
            DEBUG_LOG_INFO("TelephonyService_ResumeCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
            break;
        }
    }
}
