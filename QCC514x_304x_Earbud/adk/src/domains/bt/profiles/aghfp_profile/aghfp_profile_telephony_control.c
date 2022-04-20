/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   aghfp_profile
\brief      The voice source telephony control interface implementation for AGHFP sources
*/

#include "aghfp_profile_telephony_control.h"
#include "aghfp_profile.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile_instance.h"
#include "aghfp_profile_private.h"

#include "logging.h"
#include "panic.h"
#include "telephony_messages.h"
#include "voice_sources.h"
#include "ui.h"


static void aghfpProfile_IncomingCallAccept(voice_source_t source);
static void aghfpProfile_IncomingCallReject(voice_source_t source);
static void aghfpProfile_OngoingCallTerminate(voice_source_t source);
static void aghfpProfile_OngoingCallTransferAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction);
static void aghfpProfile_InitiateCallUsingNumber(voice_source_t source, phone_number_t number);
static void aghfpProfile_InitiateVoiceDial(voice_source_t source);
static void aghfpProfile_CallLastDialed(voice_source_t source);
static void aghfpProfile_ToggleMicMute(voice_source_t source);
static unsigned aghfpProfile_GetCurrentContext(voice_source_t source);

static const voice_source_telephony_control_interface_t aghfp_telephony_interface =
{
    .IncomingCallAccept = aghfpProfile_IncomingCallAccept,
    .IncomingCallReject = aghfpProfile_IncomingCallReject,
    .OngoingCallTerminate = aghfpProfile_OngoingCallTerminate,
    .OngoingCallTransferAudio = aghfpProfile_OngoingCallTransferAudio,
    .InitiateCallUsingNumber = aghfpProfile_InitiateCallUsingNumber,
    .InitiateVoiceDial = aghfpProfile_InitiateVoiceDial,
    .InitiateCallLastDialled = aghfpProfile_CallLastDialed,
    .ToggleMicrophoneMute = aghfpProfile_ToggleMicMute,
    .GetUiProviderContext = aghfpProfile_GetCurrentContext
};

static void aghfpProfile_IncomingCallAccept(voice_source_t source)
{
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG_FN_ENTRY("aghfpProfile_IncomingCallAccept(%p) enum:voice_source_t:%d", instance, source);

    if(!instance)
    {
        DEBUG_LOG("aghfpProfile_IncomingCallAccept: No available AGHFP instance");
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
    case AGHFP_STATE_DISCONNECTED:
    case AGHFP_STATE_CONNECTED_INCOMING:
    case AGHFP_STATE_CONNECTED_ACTIVE:
    {
        /* Send message into HFP state machine */
        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ);
        message->instance = instance;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ,
                                 message, AghfpProfileInstance_GetLock(instance));
    }
    break;
    default:
        DEBUG_LOG("aghfpProfile_IncomingCallAccept: Unhandled state enum:aghfpState:%d", state);
    }
}

static void aghfpProfile_IncomingCallReject(voice_source_t source)
{
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("aghfpProfile_IncomingCallReject(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch(instance->state)
    {
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_DISCONNECTED:
        {
            /* Send message into HFP state machine */
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_CALL_REJECT_REQ);
            message->instance = instance;
            MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_CALL_REJECT_REQ,
                                     message, AghfpProfileInstance_GetLock(instance));
        }
        break;

        default:
            break;
    }
}

static void aghfpProfile_OngoingCallTerminate(voice_source_t source)
{
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("agHfpProfile_OngoingCallTerminate(%p) enum:voice_source_t:%d", instance, source);

    if (!instance)
    {
        DEBUG_LOG_WARN("aghfpProfile_OngoingCallTerminate: No aghfpInstanceTaskData instance found");
        return;
    }

    /* Send message into HFP state machine */
    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ);
    message->instance = instance;
    MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ,
                             message, AghfpProfileInstance_GetLock(instance));

}

static void aghfpProfile_OngoingCallTransferAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction)
{
    UNUSED(source);
    UNUSED(direction);
}

static void aghfpProfile_InitiateCallUsingNumber(voice_source_t source, phone_number_t number)
{
    UNUSED(source);UNUSED(number);
    DEBUG_LOG("Not supported");
}

static void aghfpProfile_InitiateVoiceDial(voice_source_t source)
{
    UNUSED(source);
    DEBUG_LOG("Not supported");
}

static void aghfpProfile_CallLastDialed(voice_source_t source)
{
    UNUSED(source);
    DEBUG_LOG("Not supported");
}

static void aghfpProfile_ToggleMicMute(voice_source_t source)
{
    UNUSED(source);
    DEBUG_LOG("Not supported");
}

static unsigned aghfpProfile_GetCurrentContext(voice_source_t source)
{
    voice_source_provider_context_t context = BAD_CONTEXT;

    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if (instance == NULL)
        return context_voice_disconnected;

    switch(AghfpProfile_GetState(instance))
    {
    case AGHFP_STATE_NULL:
        break;
    case AGHFP_STATE_DISCONNECTING:
    case AGHFP_STATE_DISCONNECTED:
    case AGHFP_STATE_CONNECTING_LOCAL:
    case AGHFP_STATE_CONNECTING_REMOTE:

        context = context_voice_disconnected;
        break;
    case AGHFP_STATE_CONNECTED_IDLE:
        context = context_voice_connected;
        break;
    case AGHFP_STATE_CONNECTED_OUTGOING:
        context = context_voice_ringing_outgoing;
        break;
    case AGHFP_STATE_CONNECTED_INCOMING:
        context = context_voice_ringing_incoming;
        break;
    case AGHFP_STATE_CONNECTED_ACTIVE:
        context = context_voice_in_call;
        break;
    default:
        break;
    }
    return (unsigned)context;

}

const voice_source_telephony_control_interface_t * AghfpProfile_GetTelephonyControlInterface(void)
{
    return &aghfp_telephony_interface;
}
