/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       a2dp_profile_sync.c
\brief      Coordination between the Sink & Source (aka forwarding) A2DP roles.
*/

/* Only compile if AV defined */
#ifdef INCLUDE_AV

#include <logging.h>
#include <message.h>
#include <panic.h>
#include <timestamp_event.h>

#include "av.h"
#include <av_instance.h>
#include "a2dp_profile.h"
#include "a2dp_profile_sync.h"
#include "a2dp_profile_config.h"
#include <device.h>
#include <device_properties.h>

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of this module at two levels. */
#define A2DP_SYNC_LOG        DEBUG_LOG
/*! \} */

/*! Code assertion that can be checked at run time        . This will cause a panic. */
#define assert(x) PanicFalse(x)

/*! Macro for simplifying creating messages */
#define MAKE_AV_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*
    Functions that implement the audio_sync_interface_t for an a2dp profile
*/

/*
    Helpers for sending AV_INTERNAL_A2DP_* messages to an AV instance.
*/

static void a2dpProfileSync_SendMediaStartRequest(avInstanceTaskData* theInst)
{
    if(a2dpIsSyncFlagSet(theInst, A2DP_SYNC_MEDIA_START_PENDING))
    {
        PanicFalse(A2dpMediaStartRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id));
        a2dpClearSyncFlag(theInst, A2DP_SYNC_MEDIA_START_PENDING);
    }
}

static void a2dpProfileSync_SendMediaStartResponse(avInstanceTaskData* theInst)
{
    if(a2dpIsSyncFlagSet(theInst, A2DP_SYNC_MEDIA_START_PENDING))
    {
        PanicFalse(A2dpMediaStartResponse(theInst->a2dp.device_id, theInst->a2dp.stream_id, TRUE));
        a2dpClearSyncFlag(theInst, A2DP_SYNC_MEDIA_START_PENDING);
    }
}

/*
    Handlers for AUDIO_SYNC_... messages
*/

static void appA2dpSyncHandleA2dpSyncConnectResponse(avInstanceTaskData *theInst,
                                                     const AUDIO_SYNC_CONNECT_RES_T *res)
{
    avA2dpState local_state = appA2dpGetState(theInst);

    if (((res->sync_id + 1) & 0xff) != theInst->a2dp.sync_counter)
    {
        /* This means whilst waiting for a sync response from the other instance,
        something else triggered the instance to exit the _SYNC state. So this sync
        response is late, and now irrelevant. */
        A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncConnectResponse(%p) late state(0x%x) sync_id(%d) sync_count(%d)",
            theInst, local_state, res->sync_id, theInst->a2dp.sync_counter);
        return;
    }

    /* This will cancel any responses sent 'later' to catch the other instance
       not responding in time. */
    PanicFalse(MessageCancelAll(&theInst->av_task, AUDIO_SYNC_CONNECT_RES) <= 1);

    A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncConnectResponse(%p) state(0x%x) sync_id(%d)",
               theInst, local_state, res->sync_id);

    switch (local_state)
    {
    case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        /* Accept media connection */
        A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncConnectResponse accepting A2dpMediaOpen device_id %u", theInst->a2dp.device_id);
        PanicFalse(A2dpMediaOpenResponse(theInst->a2dp.device_id, TRUE));
        /* The sync is complete, remain in this state waiting for the
            A2DP_MEDIA_OPEN_CFM. */
        break;

    default:
        appA2dpError(theInst, AUDIO_SYNC_CONNECT_RES, NULL);
        break;
    }
    theInst->a2dp.sync_counter++;
}

static void appA2dpSyncHandleA2dpSyncPrepareResponse(avInstanceTaskData *theInst,
                                                     const AUDIO_SYNC_PREPARE_RES_T *res)
{
    avA2dpState local_state = appA2dpGetState(theInst);

    if (((res->sync_id + 1) & 0xff) != theInst->a2dp.sync_counter)
    {
        if(!a2dpIsSyncFlagSet(theInst, A2DP_SYNC_PREPARE_RESPONSE_PENDING) && !a2dpIsSyncFlagSet(theInst, A2DP_SYNC_MEDIA_START_PENDING))
        {
            /* This means whilst waiting for a sync response from the other instance,
            something else triggered the instance to exit the _SYNC state. So this sync
            response is late, but we still need to complete the sequence */
            A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncPrepareResponse(%p) late state(enum:avA2dpState:%d) sync_id(%d) sync_count(%d) reason (enum:audio_sync_reason_t:%d)",
                           theInst, local_state, res->sync_id, theInst->a2dp.sync_counter, res->reason);
            
            return;
        }
    }
    
    A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncPrepareResponse(%p) state(enum:avA2dpState:%d) sync_id(%d) reason (enum:audio_sync_reason_t:%d)",
                   theInst, local_state, res->sync_id, res->reason);

    switch (local_state)
    {
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        {
            if(res->reason != audio_sync_success)
            {
                a2dpProfileSync_SendMediaStartResponse(theInst);
            }
        }
        break;
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        {
            if(res->reason != audio_sync_success)
            {
                a2dpProfileSync_SendMediaStartRequest(theInst);

                /* If the A2DP instance is acting in the source role, without the benefit of
                 * the Audio Router to eventually trigger ACTIVATE_IND, then a2dp_profile
                 * should set the media start audio sync flag to true */
                if (appA2dpIsSourceCodec(theInst) && res->reason == audio_sync_not_required)
                {
                    theInst->a2dp.bitfields.local_media_start_audio_sync_complete = TRUE;
                }
            }
        }
        break;
        
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        break;

        default:
            appA2dpError(theInst, AUDIO_SYNC_PREPARE_RES, NULL);
        break;
    }
    
    if(res->reason != audio_sync_timeout)
    {
        a2dpClearSyncFlag(theInst, A2DP_SYNC_PREPARE_RESPONSE_PENDING);

        if(theInst->a2dp.source_state != source_state_disconnected)
        {
            appA2dpSetAudioStartLockBit(theInst);
        }

        if(res->reason != audio_sync_rejected)
        {
            /* Don't set prepared flag on rejection so that prepare 
               stage is repeated before the source is routed */
            a2dpSetSyncFlag(theInst, A2DP_SYNC_PREPARED);
        }

        AvSendAudioConnectedStatusMessage(theInst, AV_A2DP_AUDIO_CONNECTED);
        theInst->a2dp.sync_counter++;
    }

}


static void appA2dpSyncHandleA2dpSyncActivateResponse(avInstanceTaskData *theInst,
                                                      const AUDIO_SYNC_ACTIVATE_RES_T *res)
{
    avA2dpState local_state = appA2dpGetState(theInst);

    if (((res->sync_id + 1) & 0xff) != theInst->a2dp.sync_counter)
    {
        /* This means whilst waiting for a sync response from the other instance,
        something else triggered the instance to exit the _SYNC state. So this sync
        response is late, and now irrelevant. */
        A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncActivateResponse(%p) late state(0x%x) sync_id(%d) sync_count(%d)",
                       theInst, local_state, res->sync_id, theInst->a2dp.sync_counter);
        return;
    }

    /* This will cancel any responses sent 'later' to catch the other instance
       not responding in time. */
    PanicFalse(MessageCancelAll(&theInst->av_task, AUDIO_SYNC_ACTIVATE_RES) <= 1);

    A2DP_SYNC_LOG("appA2dpSyncHandleA2dpSyncActivateResponse(%p) state(0x%x) sync_id(%d)",
                   theInst, local_state, res->sync_id);
    /* Set the flag as received the AUDIO_SYNC_CONNECT_RES */
    theInst->a2dp.bitfields.local_media_start_audio_sync_complete = TRUE;
    DEBUG_LOG("appA2dpSyncHandleA2dpSyncActivateResponse: local_media_start_audio_sync_complete %d", theInst->a2dp.bitfields.local_media_start_audio_sync_complete);

    switch (local_state)
    {
    case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
    {
        /* Start streaming request */
        a2dpProfileSync_SendMediaStartRequest(theInst);
        /* The sync is complete, remain in this state waiting for the
            A2DP_MEDIA_START_CFM. */
        break;
    }
    case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
    {
        TimestampEvent(TIMESTAMP_EVENT_A2DP_START_RSP);
        a2dpProfileSync_SendMediaStartResponse(theInst);

        /* The sync is complete, remain in this state waiting for the
            A2DP_MEDIA_START_CFM. */
        break;
    }

    case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
    case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
    case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        break;

    default:
        appA2dpError(theInst, AUDIO_SYNC_ACTIVATE_RES, NULL);
        break;
    }
    
    theInst->a2dp.sync_counter++;
}

/*! \brief Initialise a audio_sync_t instance for use with an a2dp profile instance */
void appA2dpSyncInitialise(avInstanceTaskData *theInst)
{
    /* No client registered initially with this AV instance. */
    memset(&theInst->a2dp.sync_if, 0, sizeof(audio_sync_t));
}

/*! \brief Get the audio_sync_state_t for a given avA2dpState. */
audio_sync_state_t appA2dpSyncGetAudioSyncState(avInstanceTaskData *theInst)
{
    audio_sync_state_t audio_state = AUDIO_SYNC_STATE_DISCONNECTED;
    
    avA2dpState a2dp_state = appA2dpGetState(theInst);

    DEBUG_LOG("appA2dpSyncGetAudioSyncState enum:avA2dpState:%d", a2dp_state);

    if (appA2dpIsStateConnectedMediaStreaming(a2dp_state))
    {
        if(theInst->a2dp.source_state == source_state_connected)
        {
            audio_state = AUDIO_SYNC_STATE_ACTIVE;
        }
        else
        {
            audio_state = AUDIO_SYNC_STATE_READY;
        }
    }
    else if (appA2dpIsStateConnectedMedia(a2dp_state))
    {
        audio_state = AUDIO_SYNC_STATE_CONNECTED;
    }

    return audio_state;
}

void appA2dpSyncRegister(avInstanceTaskData *theInst, const audio_sync_t *sync_if)
{
    DEBUG_LOG("appA2dpSyncRegister(%p)", (void *)theInst);

    theInst->a2dp.sync_if = *sync_if;

    /* Notify the current state to the synchronised instance. */
    a2dpProfileSync_SendStateIndication(theInst, appA2dpSyncGetAudioSyncState(theInst));
}

void appA2dpSyncUnregister(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpSyncUnregister theInst %p is_valid %d", theInst, appAvIsValidInst(theInst));

    if (appAvIsValidInst(theInst))
    {
        AudioSync_CancelQueuedMessages(&theInst->a2dp.sync_if);
        memset(&theInst->a2dp.sync_if, 0, sizeof(audio_sync_t));
    }
}

/*! \brief Handler function for all audio_sync_msg_t messages */
void appA2dpSyncHandleMessage(avInstanceTaskData *theInst, MessageId id, Message message)
{
    switch (id)
    {
    case AUDIO_SYNC_CONNECT_RES:
        appA2dpSyncHandleA2dpSyncConnectResponse(theInst, (const AUDIO_SYNC_CONNECT_RES_T *)message);
        break;
        
    case AUDIO_SYNC_PREPARE_RES:
        appA2dpSyncHandleA2dpSyncPrepareResponse(theInst, (const AUDIO_SYNC_PREPARE_RES_T *)message);
        break;

    case AUDIO_SYNC_ACTIVATE_RES:
        appA2dpSyncHandleA2dpSyncActivateResponse(theInst, (const AUDIO_SYNC_ACTIVATE_RES_T *)message);
        break;

    default:
        A2DP_SYNC_LOG("appA2dpSyncHandleMessage unhandled msg id MESSAGE:0x%x", id);
        break;
    }
}

void a2dpProfileSync_SendConnectIndication(avInstanceTaskData *av_instance)
{
    Task av_task = &av_instance->av_task;
    uint8 sync_id = av_instance->a2dp.sync_counter++;
    struct audio_sync_t *sync_inst = &av_instance->a2dp.sync_if;
    
    AudioSync_ConnectIndication(sync_inst, av_task, Av_GetSourceForInstance(av_instance),
                                av_instance->a2dp.current_seid, sync_id);
}

void a2dpProfileSync_SendPrepareIndication(avInstanceTaskData *av_instance)
{
    Task av_task = &av_instance->av_task;
    uint8 sync_id = av_instance->a2dp.sync_counter++;
    struct audio_sync_t *sync_inst = &av_instance->a2dp.sync_if;
    
    MessageCancelAll(&av_instance->av_task, AUDIO_SYNC_PREPARE_RES);
    a2dpSetSyncFlag(av_instance, A2DP_SYNC_PREPARE_RESPONSE_PENDING);
    
    AudioSync_PrepareIndication(sync_inst, av_task, Av_GetSourceForInstance(av_instance),
                                av_instance->a2dp.current_seid, sync_id);
}

void a2dpProfileSync_SendActiveIndication(avInstanceTaskData *av_instance)
{
    Task av_task = &av_instance->av_task;
    uint8 sync_id = av_instance->a2dp.sync_counter++;
    struct audio_sync_t *sync_inst = &av_instance->a2dp.sync_if;
    
    /* Cancel any pending prepare response/timeout */
    MessageCancelAll(&av_instance->av_task, AUDIO_SYNC_PREPARE_RES);
    
    AudioSync_ActivateIndication(sync_inst, av_task, Av_GetSourceForInstance(av_instance),
                                 av_instance->a2dp.current_seid, sync_id);
}

void a2dpProfileSync_SendStateIndication(avInstanceTaskData* av_instance, audio_sync_state_t state)
{
    struct audio_sync_t *sync_inst = &av_instance->a2dp.sync_if;
    AudioSync_StateIndication(sync_inst, Av_GetSourceForInstance(av_instance), state, av_instance->a2dp.current_seid);
}


#endif /* INCLUDE_AV */
