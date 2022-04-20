/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile
\brief      The voice source telephony control interface implementation for HFP sources
*/

#include "hfp_profile_telephony_control.h"

#include <hfp.h>
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stdlib.h>
#include <ui.h>

#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_states.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "voice_sources_telephony_control_interface.h"
#include "telephony_messages.h"

static void hfpProfile_IncomingCallAccept(voice_source_t source);
static void hfpProfile_IncomingCallReject(voice_source_t source);
static void hfpProfile_OngoingCallTerminate(voice_source_t source);
static void hfpProfile_OngoingCallTransferAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction);
static void hfpProfile_InitiateCallUsingNumber(voice_source_t source, phone_number_t number);
static void hfpProfile_InitiateVoiceDial(voice_source_t source);
static void hfpProfile_CallLastDialed(voice_source_t source);
static void hfpProfile_ToggleMicMute(voice_source_t source);
static unsigned hfpProfile_GetCurrentContext(voice_source_t source);
static void hfpProfile_TwcControl(voice_source_t source, voice_source_twc_control_t action);

static const voice_source_telephony_control_interface_t hfp_telephony_interface =
{
    .IncomingCallAccept = hfpProfile_IncomingCallAccept,
    .IncomingCallReject = hfpProfile_IncomingCallReject,
    .OngoingCallTerminate = hfpProfile_OngoingCallTerminate,
    .OngoingCallTransferAudio = hfpProfile_OngoingCallTransferAudio,
    .InitiateCallUsingNumber = hfpProfile_InitiateCallUsingNumber,
    .InitiateVoiceDial = hfpProfile_InitiateVoiceDial,
    .InitiateCallLastDialled = hfpProfile_CallLastDialed,
    .ToggleMicrophoneMute = hfpProfile_ToggleMicMute,
    .GetUiProviderContext = hfpProfile_GetCurrentContext,
    .TwcControl = hfpProfile_TwcControl
};

static void hfpProfile_SendMessageToInstance(hfpInstanceTaskData* instance, MessageId id, Message msg)
{
    Task task = HfpProfile_GetInstanceTask(instance);
    
    MessageSendConditionally(task, id, msg, HfpProfileInstance_GetLock(instance));
}

static void hfpProfile_IncomingCallAccept(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_IncomingCallAccept(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch(instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTED_INCOMING:
        {
            Telephony_NotifyCallAnswered(source);

            /* Send message into HFP state machine */
            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CALL_ACCEPT_REQ);
            message->instance = instance;
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_CALL_ACCEPT_REQ, message);
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_IncomingCallReject(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_IncomingCallReject(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                /* Play error tone to indicate we don't have a valid address */
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTED_INCOMING:
        {
            Telephony_NotifyCallRejected(source);

            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CALL_REJECT_REQ);
            message->instance = instance;
            /* Send message into HFP state machine */
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_CALL_REJECT_REQ, message);
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_OngoingCallTerminate(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_OngoingCallTerminate(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    switch (instance->state)
    {
        case HFP_STATE_DISCONNECTED:
        {
            /* Connect SLC */
            if (!HfpProfile_ConnectHandset())
            {
                Telephony_NotifyError(source);
                break;
            }
        }
        /* Fall through */

        case HFP_STATE_CONNECTED_INCOMING:
        case HFP_STATE_CONNECTED_OUTGOING:
        case HFP_STATE_CONNECTED_ACTIVE:
        {
            Telephony_NotifyCallTerminated(source);

            MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CALL_HANGUP_REQ);
            message->instance = instance;
            /* Send message into HFP state machine */
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_CALL_HANGUP_REQ, message);
        }
        break;

        default:
            break;
    }
}

static void hfpProfile_TransferCallAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_TransferCallAudio(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);
    
    hfpState state = appHfpGetState(instance);
    

    if(HfpProfile_StateIsSlcConnected(state))
    {
        HFP_INTERNAL_HFP_TRANSFER_REQ_T * message = (HFP_INTERNAL_HFP_TRANSFER_REQ_T*)PanicUnlessNew(HFP_INTERNAL_HFP_TRANSFER_REQ_T);

        message->direction = direction;
        message->source = source;
        hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_TRANSFER_REQ, message);

        Telephony_NotifyCallAudioTransferred(source);
        
        if(direction == voice_source_audio_transfer_to_ag || ((direction == voice_source_audio_transfer_toggle) && HfpProfile_IsScoActiveForInstance(instance)))
        {
            instance->bitfields.esco_disconnecting = TRUE;
        }
    }
}

static void hfpProfile_OngoingCallTransferAudio(voice_source_t source, voice_source_audio_transfer_direction_t direction)
{
    hfpProfile_TransferCallAudio(source, direction);
}

static void hfpProfile_InitiateCallUsingNumber(voice_source_t source, phone_number_t number)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_InitiateCallUsingNumber(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    if(HfpProfile_StateIsSlcDisconnected(instance->state))
    {
        /* Connect SLC */
        if (!HfpProfile_ConnectHandset())
        {
            Telephony_NotifyError(voice_source_hfp_1);
            return;
        }
    }
    
    if(HfpProfile_StateIsSlcConnected(instance->state))
    {
        HFP_INTERNAL_NUMBER_DIAL_REQ_T * message= (HFP_INTERNAL_NUMBER_DIAL_REQ_T *)PanicNull(calloc(1,sizeof(HFP_INTERNAL_NUMBER_DIAL_REQ_T)+number.number_of_digits-1));
        message->length = number.number_of_digits;
        message->instance = instance;
        memmove(message->number, number.digits, number.number_of_digits);
        hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_NUMBER_DIAL_REQ, message);

        Telephony_NotifyCallInitiatedUsingNumber(source);
    }
}

static void hfpProfile_InitiateVoiceDial(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_InitiateVoiceDial(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    if(HfpProfile_StateIsSlcDisconnected(instance->state))
    {
        /* Connect SLC */
        if (!HfpProfile_ConnectHandset())
        {
            Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
            return;
        }
    }
    
    if(HfpProfile_StateIsSlcConnectedOrConnecting(instance->state))
    {
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_VOICE_DIAL_REQ);
        message->instance = instance;
        /* Send message into HFP state machine */
        hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);
    }
}

/*! \brief Attempt last number redial

    Initiate last number redial, attempt to connect SLC first if not currently
    connected.
*/
static void hfpProfile_CallLastDialed(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_CallLastDialed(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    if(HfpProfile_StateIsSlcDisconnected(instance->state))
    {
        /* Connect SLC */
        if (!HfpProfile_ConnectHandset())
        {
            Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
            return;
        }
    }
    
    if(HfpProfile_StateIsSlcConnectedOrConnecting(instance->state))
    {
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ);
        message->instance = instance;
        /* Send message into HFP state machine */
        hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ, message);
    }
}

static void hfpProfile_ToggleMicMute(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_ToggleMicMute(%p) enum:voice_source_t:%d", instance, source);

    PanicNull(instance);

    if(HfpProfile_StateHasActiveCall(appHfpGetState(instance)))
    {
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_MUTE_REQ);

        /* Send message into HFP state machine */
        message->mute = !HfpProfile_IsMicrophoneMuted(instance);
        message->instance = instance;
        hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_MUTE_REQ, message);
    }
}

/*! \brief provides hfp (telephony) current context to ui module

    \param[in]  void

    \return     current context of hfp module.
*/
static unsigned hfpProfile_GetCurrentContext(voice_source_t source)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    if (instance == NULL)
        return context_voice_disconnected;

    switch(appHfpGetState(instance))
    {
        case HFP_STATE_DISCONNECTING:
        case HFP_STATE_DISCONNECTED:
        case HFP_STATE_CONNECTING_LOCAL:
        case HFP_STATE_CONNECTING_REMOTE:
            return context_voice_disconnected;
        
        case HFP_STATE_CONNECTED_IDLE:
            return context_voice_connected;
        
        case HFP_STATE_CONNECTED_OUTGOING:
            return context_voice_ringing_outgoing;
            
        case HFP_STATE_CONNECTED_INCOMING:
            return context_voice_ringing_incoming;
            
        case HFP_STATE_CONNECTED_ACTIVE:
            return context_voice_in_call;
            
        case HFP_STATE_CONNECTED_ACTIVE_WITH_INCOMING:
            return context_voice_in_call_with_incoming;
            
        case HFP_STATE_CONNECTED_ACTIVE_WITH_OUTGOING:
            return context_voice_in_call_with_outgoing;
            
        case HFP_STATE_CONNECTED_ACTIVE_WITH_HELD:
            return context_voice_in_call_with_held;
            
        case HFP_STATE_CONNECTED_HELD:
            return context_voice_call_held;
            
        case HFP_STATE_CONNECTED_MULTIPARTY:
            return context_voice_in_multiparty_call;
        
        case HFP_STATE_NULL:
        default:
            return BAD_CONTEXT;
    }
}

static void hfpProfile_TwcControl(voice_source_t source, voice_source_twc_control_t action)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_TwcControl(%p) enum:voice_source_t:%d, enum:voice_source_twc_control_t:%d", instance, source, action);

    PanicNull(instance);
    
    switch(action)
    {
        case voice_source_release_held_reject_waiting:
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_RELEASE_WAITING_REJECT_INCOMING_REQ, NULL);
            break;
            
        case voice_source_release_active_accept_other:
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_ACCEPT_WAITING_RELEASE_ACTIVE_REQ, NULL);
            break;
            
        case voice_source_hold_active_accept_other:
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_ACCEPT_WAITING_HOLD_ACTIVE_REQ, NULL);
            break;
            
        case voice_source_add_held_to_multiparty:
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_ADD_HELD_TO_MULTIPARTY_REQ, NULL);
            break;
            
        case voice_source_join_calls_and_hang_up:
            hfpProfile_SendMessageToInstance(instance, HFP_INTERNAL_HFP_JOIN_CALLS_AND_HANG_UP, NULL);
            break;
            
        default:
            Panic();
            break;
    }
}

const voice_source_telephony_control_interface_t * HfpProfile_GetTelephonyControlInterface(void)
{
    return &hfp_telephony_interface;
}
