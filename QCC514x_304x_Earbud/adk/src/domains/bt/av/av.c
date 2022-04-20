/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AV State Machines (A2DP & AVRCP)

Main AV task.
*/

/* Only compile if AV defined */
#ifdef INCLUDE_AV

#include <a2dp.h>
#include <avrcp.h>
#include <panic.h>
#include <connection.h>
#include <ps.h>
#include <file.h>
#include <transform.h>
#include <feature.h>
#include <string.h>
#include <stdlib.h>

#include "app_task.h"
#include "audio_sources.h"
#include "av.h"
#include "av_config.h"
#include "av_instance.h"
#include "a2dp_profile.h"
#include "a2dp_profile_audio.h"
#include "a2dp_profile_sync.h"
#include "a2dp_profile_volume.h"
#include "avrcp_profile.h"
#include "avrcp_profile_config.h"
#include "avrcp_profile_volume_observer.h"
#include "avrcp_profile_browsing.h"
#include "kymera_latency_manager.h"
#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <focus_audio_source.h>
#include "system_state.h"
#include "link_policy.h"
#include <profile_manager.h>
#include "adk_log.h"
#include "ui.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "unexpected_message.h"
#include "multidevice.h"
#include <logging.h>
#include "kymera.h"
#include <ps_key_map.h>

#ifdef INCLUDE_APTX_ADAPTIVE
#include "a2dp_profile_caps_aptx_adaptive.h"
#endif

// this shouldn't be here
#include "state_proxy.h"

#include <connection_manager.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(av_avrcp_messages)
LOGGING_PRESERVE_MESSAGE_ENUM(av_avrcp_internal_messages)
LOGGING_PRESERVE_MESSAGE_ENUM(av_status_messages)
LOGGING_PRESERVE_MESSAGE_ENUM(av_ui_messages)


#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */

ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(AV_AVRCP, AV_AVRCP_MESSAGE_END)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(AV, AV_MESSAGE_END)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(AV_UI, AV_UI_MESSAGE_END)

#endif

/*! Macro for creating messages */
#define MAKE_AV_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! Code assertion that can be checked at run time. This will cause a panic. */
#define assert(x)   PanicFalse(x)


/*!< AV data structure */
avTaskData  app_av;

const av_context_provider_if_t *context_provider = NULL;

static void appAvHandleMessage(Task task, MessageId id, Message message);
/*! \brief Handle AV error

    Some error occurred in the Av state machine, to avoid the state machine
    getting stuck, drop connection and move to 'disconnected' state.
*/
static void appAvError(avTaskData *theAv, MessageId id, Message message)
{
    UNUSED(message); UNUSED(theAv);UNUSED(id);

#ifdef AV_DEBUG
    DEBUG_LOG("appAvError %p, state=%u, MESSAGE:0x%x", (void *)theAv, theAv->bitfields.state, id);
#else
    Panic();
#endif
}

Task appGetAvPlayerTask(void)
{
  return &app_av.task;
}

bool appAvHasAConnection(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst)
            if (appA2dpIsConnected(theInst) || appAvrcpIsConnected(theInst))
                return TRUE;
    }

    /* No AV connections */
    return FALSE;
}

/*! \brief Check A2DP links associated with av instance is disconnected.

    \return TRUE if there are no connected links. FALSE if the link
            associated with theInst has either an A2DP or an AVRCP connection.
*/
bool Av_InstanceIsDisconnected(avInstanceTaskData* theInst)
{
    if (theInst)
    {
        if (!appA2dpIsDisconnected(theInst) || !appAvrcpIsDisconnected(theInst))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/*! \brief Check all A2DP links are disconnected

    \return TRUE if there are no connected links. FALSE if any AV link
            has either an A2DP or an AVRCP connection.
*/
static bool appAvIsDisconnected(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst)
        {
            if (!appA2dpIsDisconnected(theInst) || !appAvrcpIsDisconnected(theInst))
                return FALSE;
        }
    }

    /* No AV connections */
    return TRUE;
}

bool Av_IsA2dpSinkStreaming(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && appA2dpIsSinkCodec(theInst) && appA2dpIsStreaming(theInst))
            return TRUE;
    }

    /* No AV connections */
    return FALSE;
}

bool Av_InstanceIsA2dpSinkStarted(avInstanceTaskData* av_instance)
{
    if (av_instance && appA2dpIsSinkCodec(av_instance) && appA2dpIsStarted((av_instance)->a2dp.state))
    {
        return TRUE;
    }
    /* No AV connections */
    return FALSE;
}

static bool av_InstanceIsPlayStatusActive(avInstanceTaskData* av_instance)
{
    if (av_instance && appA2dpIsSinkCodec(av_instance))
    {
        audio_source_t source = Av_GetSourceForInstance(av_instance);
        switch(AvGetCurrentContext(source))
        {
            case context_audio_is_streaming:
            case context_audio_is_playing:
                return TRUE;
            
            default:
                break;
        }
    }
    return FALSE;
}

bool appAvIsPlayStatusActive(void)
{
    avInstanceTaskData* av_instance;
    av_instance_iterator_t iterator;

    for_all_av_instances(av_instance, &iterator)
    {
        if(av_InstanceIsPlayStatusActive(av_instance))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*! \brief Check if A2DP connection associated with 
           theInst is connected

    \return TRUE if there is an AV that is connected as a sink
*/
bool Av_InstanceIsA2dpSinkConnected(avInstanceTaskData* theInst)
{
    if (theInst && appA2dpIsSinkCodec(theInst) && appA2dpIsConnected(theInst))
    {
        return TRUE;
    }
    /* No AV connections */
    return FALSE;
}

/*! \brief Check if A2DP is connected

    \return TRUE if there is an AV instance that is connected as a2dp sink
*/
bool Av_IsA2dpSinkConnected(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && Av_InstanceIsA2dpSinkConnected(theInst))
            return TRUE;
    }

    /* No AV connections */
    return FALSE;
}

/*! \brief Check if A2DP Source is connected

    \return TRUE if there is an AV instance that is connected in A2DP Source role
*/
bool Av_IsA2dpSourceConnected(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && appA2dpIsSourceCodec(theInst) && appA2dpIsConnected(theInst))
            return TRUE;
    }

    /* No AV connections */
    return FALSE;
}

static event_origin_t av_GetOrigin(avInstanceTaskData *theInst)
{
    event_origin_t origin = event_origin_local;
    if(theInst)
    {
        origin = (appDeviceIsPeer(&theInst->bd_addr) ? event_origin_peer : event_origin_external);
    }
    return origin;
}

static void av_StoreHandsetVolumeDeviceProperty(avInstanceTaskData *theInst)
{
    device_t device = BtDevice_GetDeviceForBdAddr(&theInst->bd_addr);

    if (device)
    {
        volume_t a2dp_volume = A2dpProfile_GetDefaultVolume();
        DeviceProperties_GetAudioVolume(device, a2dp_volume.config, &a2dp_volume);

        if (theInst->volume != a2dp_volume.value)
        {
            a2dp_volume.value = theInst->volume;
            DeviceProperties_SetAudioVolume(device, a2dp_volume);
        }
    }
    DEBUG_LOG("av_StoreHandsetVolumeDeviceProperty lap=%x vol=%u",
              theInst->bd_addr.lap, theInst->volume);
}

/*! \brief Get device volume

    \param bd_addr Pointer to read-only device BT address.
    \param[out] volume The device's volume
    \note The volume should only be requested for handsets.
*/
static bool av_GetVolumeForBdAddr(const bdaddr *bd_addr, uint8 *volume)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    if (device)
    {
        volume_t a2dp_volume = A2dpProfile_GetDefaultVolume();
        if (DeviceProperties_GetAudioVolume(device, a2dp_volume.config, &a2dp_volume))
        {
            *volume = a2dp_volume.value;
            return TRUE;
        }
    }

    return FALSE;
}

static bool av_CheckAndSetHandsetVolumeIfNeeded(avInstanceTaskData *theInst)
{
    bool is_handset = FALSE;
    if (appDeviceIsHandset(&theInst->bd_addr))
    {
        is_handset = TRUE;
        if ((theInst->volume) == VOLUME_UNSET)
        {
            uint8 volume;
            PanicFalse(av_GetVolumeForBdAddr(&theInst->bd_addr, &volume));

            DEBUG_LOG("av_CheckAndSetHandsetVolumeIfNeeded - Setting inst=%p, volume=%u", theInst, volume);

            /* Forward volume to other instance if AV connected */
            Volume_SendAudioSourceVolumeUpdateRequest(Av_GetSourceForInstance(theInst), av_GetOrigin(theInst), volume);

            theInst->volume = volume;
        }
    }
    return is_handset;
}

/*! \brief Volume handling on AVRCP/A2DP disconnect
    \param theInst The disconnecting instance.
*/
static void av_VolumeHandleAvDisconnect(avInstanceTaskData *theInst)
{
    avTaskData *theAv = AvGetTaskData();

    if (appDeviceIsHandset(&theInst->bd_addr))
    {
        if (MessageCancelFirst(&theAv->task, AV_INTERNAL_VOLUME_STORE_REQ))
        {
            av_StoreHandsetVolumeDeviceProperty(theInst);
        }
    }
}
/*! \brief Handle incoming AVRCP connection

    AVRCP task has get an incoming AVRCP connection, we receive this message
    so that we can indicate if we'd like it for AV or not.  If A2DP is not connected
    then we say we don't want AVRCP.
*/
static void appAvHandleAvAvrcpConnectIndication(avTaskData *theAv, const AV_AVRCP_CONNECT_IND_T *ind)
{
    avInstanceTaskData *theInst;
    DEBUG_LOG("appAvHandleAvAvrcpConnectIndication");
    UNUSED(theAv);

    theInst = appAvInstanceFindFromBdAddr(&ind->bd_addr);
    if (theInst)
    {
        /* Reject connection if A2DP is disconnected, accept if A2DP connected or connecting */
        if (appA2dpIsDisconnected(theInst))
        {
            DEBUG_LOG("appAvHandleAvAvrcpConnectIndication, rejecting");

            /* Reject incoming connection */
            appAvAvrcpConnectResponse(&theAv->task, &theInst->av_task, &ind->bd_addr, ind->connection_id,
                                      ind->signal_id,  AV_AVRCP_REJECT);
        }
        else
        {
            DEBUG_LOG("appAvHandleAvAvrcpConnectIndication, accepting");

            /* Accept incoming connection */
            appAvAvrcpConnectResponse(&theAv->task, &theInst->av_task, &ind->bd_addr, ind->connection_id,
                                      ind->signal_id,  AV_AVRCP_ACCEPT);
        }
    }
}

/*! \brief Confirmation of AVRCP connection

*/
static void appAvInstanceHandleAvAvrcpConnectCfm(avInstanceTaskData *theInst, AV_AVRCP_CONNECT_CFM_T *cfm)
{
    assert(theInst == cfm->av_instance);
    {
        DEBUG_LOG("appAvInstanceHandleAvAvrcpConnectCfm(%p), status %d", theInst, cfm->status);

        if (cfm->status == avrcp_success)
        {
            /* Register for notifications to be sent to AV task */
            appAvrcpNotificationsRegister(theInst, av_plugin_interface.GetAvrcpEvents());

            av_CheckAndSetHandsetVolumeIfNeeded(theInst);

            /* Cancel outstanding connect later request since we are now connected */
            MessageCancelAll(&theInst->av_task, AV_INTERNAL_AVRCP_CONNECT_LATER_REQ);
        }
    }
}

/*! \brief Handle confirmation of AVRCP disconnection */
static void appAvInstanceHandleAvAvrcpDisconnectInd(avInstanceTaskData *theInst, AV_AVRCP_DISCONNECT_IND_T *ind)
{
    DEBUG_LOG("appAvInstanceHandleAvAvrcpDisconnectInd(%p), status %d", theInst, ind->status);
    av_VolumeHandleAvDisconnect(theInst);
    /* Cancel outstanding disconnect later request since we are now disconnected */
    MessageCancelAll(&theInst->av_task, AV_INTERNAL_AVRCP_DISCONNECT_LATER_REQ);
}

static void appAvInstanceHandleAvAvrcpPlayStatusChangedInd(avInstanceTaskData *theOtherInst, AV_AVRCP_PLAY_STATUS_CHANGED_IND_T *ind)
{
    /* Look in table to find connected instance */
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && (theInst != theOtherInst))
        {
            if (appAvIsAvrcpConnected(theInst) && appDeviceIsPeer(&theInst->bd_addr))
            {
                DEBUG_LOG("appAvInstanceHandleAvAvrcpPlayStatusChangedInd, send play status %u to %p", ind->play_status, theInst);
                appAvAvrcpPlayStatusNotification(theInst, ind->play_status);
            }
        }
    }
}

/*
 * Standard TWS:  Set volume from Slave to Master
 * Earbud (Slave) -> Earbud (Master) -> Phone
 */
static void appAvInstanceHandleAvAvrcpVolumeChangedInd(avInstanceTaskData *theInst, AV_AVRCP_VOLUME_CHANGED_IND_T *ind)
{
    assert(theInst == ind->av_instance);
    DEBUG_LOG("appAvInstanceHandleAvAvrcpVolumeChangedInd(%p), volume %u", (void *)theInst, ind->volume);
    /* Set volume and forward to phone if connected */
    Volume_SendAudioSourceVolumeUpdateRequest(Av_GetSourceForInstance(theInst), av_GetOrigin(theInst), ind->volume);
}

/*
 * TWS+: Set volume from phone to Earbud.
 * Phone -> Earbud (TWS+)
 *
 * Standard TWS:  Set volume from phone to Master, and from Master to Slave
 * Phone -> Earbud (Master) -> Earbud (Slave)
 */
static void appAvInstanceHandleAvAvrcpSetVolumeInd(avInstanceTaskData *theInst, AV_AVRCP_SET_VOLUME_IND_T *ind)
{
    assert(theInst == ind->av_instance);
    DEBUG_LOG("appAvInstanceHandleAvAvrcpSetVolumeInd(%p), volume %u", (void *)theInst, ind->volume);
    /* Set volume and forward to slave if connected */
    Volume_SendAudioSourceVolumeUpdateRequest(Av_GetSourceForInstance(theInst), av_GetOrigin(theInst), ind->volume);
}

void appAvInstanceHandleMessage(Task task, MessageId id, Message message)
{
    avInstanceTaskData *theInst = (avInstanceTaskData *)task;

    if (id >= AV_INTERNAL_AVRCP_BASE && id < AV_INTERNAL_AVRCP_TOP)
        appAvrcpInstanceHandleMessage(theInst, id, message);
    else if (id >= AV_INTERNAL_A2DP_BASE && id < AV_INTERNAL_A2DP_TOP)
        appA2dpInstanceHandleMessage(theInst, id, message);
    else if (id >= AVRCP_MESSAGE_BASE && id < AVRCP_MESSAGE_TOP)
        appAvrcpInstanceHandleMessage(theInst, id, message);
    else if (id >= A2DP_MESSAGE_BASE && id < A2DP_MESSAGE_TOP)
        appA2dpInstanceHandleMessage(theInst, id, message);
    else if (id >= AUDIO_SYNC_BASE && id < AUDIO_SYNC_TOP)
        appA2dpSyncHandleMessage(theInst, id, message);
    else
    {
        switch (id)
        {
            case AV_AVRCP_CONNECT_CFM:
                appAvInstanceHandleAvAvrcpConnectCfm(theInst, (AV_AVRCP_CONNECT_CFM_T *)message);
                break;

            case AV_AVRCP_DISCONNECT_IND:
                appAvInstanceHandleAvAvrcpDisconnectInd(theInst, (AV_AVRCP_DISCONNECT_IND_T *)message);
                break;

            case AV_AVRCP_SET_VOLUME_IND:
                appAvInstanceHandleAvAvrcpSetVolumeInd(theInst, (AV_AVRCP_SET_VOLUME_IND_T *)message);
                break;

            case AV_AVRCP_VOLUME_CHANGED_IND:
                appAvInstanceHandleAvAvrcpVolumeChangedInd(theInst, (AV_AVRCP_VOLUME_CHANGED_IND_T *)message);
                break;

            case AV_AVRCP_PLAY_STATUS_CHANGED_IND:
                appAvInstanceHandleAvAvrcpPlayStatusChangedInd(theInst, (AV_AVRCP_PLAY_STATUS_CHANGED_IND_T *)message);
                break;

            default:
                appAvError(AvGetTaskData(), id, message);
                break;
        }
    }
}

avInstanceTaskData *appAvInstanceFindA2dpState(const avInstanceTaskData *theInst,
                                               uint8 mask, uint8 expected)
{
    avInstanceTaskData* theOtherInst;
    av_instance_iterator_t iterator;

    PanicFalse(appAvIsValidInst((avInstanceTaskData*)theInst));

    /* Look in table to find entry with matching A2DP state */
    for_all_av_instances(theOtherInst, &iterator)
    {
        if (theOtherInst != NULL && theInst != theOtherInst)
        {
            if ((theOtherInst->a2dp.state & mask) == expected)
            {
                return theOtherInst;
            }
        }
    }

    /* No match found */
    return NULL;
}

avInstanceTaskData *appAvInstanceFindAvrcpForPassthrough(audio_source_t source)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && appAvrcpIsConnected(theInst) && source == Av_GetSourceForInstance(theInst))
        {
            if (appA2dpIsSinkCodec(theInst))
                return theInst;
            else if (appDeviceIsHandset(&theInst->bd_addr))
                return theInst;
        }
    }

    /* No sink A2DP instance, if there's just one AVRCP, send passthrough on that instance */
    avInstanceTaskData *thePassthoughInstance = NULL;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && appAvrcpIsConnected(theInst))
        {
            /* If no previous instance, remember this one for passthrough */
            if (!thePassthoughInstance)
                thePassthoughInstance = theInst;
            else
            {
                /* Second instance found, can't do passthrough, so exit loop */
                thePassthoughInstance = NULL;
                break;
            }
        }
    }

    return thePassthoughInstance;
}

avInstanceTaskData *Av_InstanceFindFromDevice(device_t device)
{
    return AvInstance_GetInstanceForDevice(device);
}

device_t Av_FindDeviceFromInstance(avInstanceTaskData* av_instance)
{
    return Av_GetDeviceForInstance(av_instance);
}

avInstanceTaskData *appAvInstanceFindFromBdAddr(const bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    return AvInstance_GetInstanceForDevice(device);
}

/*! \brief Cancel any queued AVRCP disconnect requests

    \param[in] av_inst Instance to cancel queud AVRCP disconnect requests for.
*/
static void appAvAvrcpCancelQueuedDisconnectRequests(avInstanceTaskData *av_inst)
{
    Task task = &av_inst->av_task;
    MessageCancelAll(task, AV_INTERNAL_AVRCP_DISCONNECT_REQ);
    MessageCancelAll(task, AV_INTERNAL_AVRCP_DISCONNECT_LATER_REQ);
}

static void appAvAvrcpConnectLaterRequest(avInstanceTaskData *theInst, uint32 delay)
{
    Task task = &theInst->av_task;

    /* Cancel any queued disconnect requests */
    appAvAvrcpCancelQueuedDisconnectRequests(theInst);

    MessageSendLater(task, AV_INTERNAL_AVRCP_CONNECT_LATER_REQ, NULL, delay);
    DEBUG_LOG("appAvAvrcpConnectLaterRequest(0x%x) delay=%d", theInst, delay);
}

/*! \brief Cancel any queued AVRCP connect requests

    \param[in] av_inst Instance to cancel queued AVRCP connect requests for.
*/
static void appAvAvrcpCancelQueuedConnectRequests(avInstanceTaskData *av_inst)
{
    Task task = &av_inst->av_task;

    if (MessageCancelAll(task, AV_INTERNAL_AVRCP_CONNECT_REQ))
    {
        /* Call ConManagerReleaseAcl to decrement the reference count on the
           ACL that was added when ConManagerCreateAcl was called in
           appAvAvrcpConnectRequest for the queued connect request. */
        ConManagerReleaseAcl(&av_inst->bd_addr);
    }
    MessageCancelAll(task, AV_INTERNAL_AVRCP_CONNECT_LATER_REQ);
}

static void appAvAvrcpDisconnectLaterRequest(avInstanceTaskData *theInst, uint32 delay)
{
    Task task = &theInst->av_task;

    /* Cancel any queued connect requests */
    appAvAvrcpCancelQueuedConnectRequests(theInst);

    MessageSendLater(task, AV_INTERNAL_AVRCP_DISCONNECT_LATER_REQ, NULL, delay);
    DEBUG_LOG("appAvAvrcpDisconnectLaterRequest(0x%x) delay=%d", theInst, delay);
}

typedef struct audio_source_search_data
{
    /*! The audio source associated with the device to find */
    audio_source_t source_to_find;
    /*! Set to TRUE if a device with the source is found */
    bool source_found;
} audio_source_search_data_t;

static void av_SearchForDeviceWithAudioSource(device_t device, void * data)
{
    audio_source_search_data_t *search_data = data;

    if (DeviceProperties_GetAudioSource(device) == search_data->source_to_find)
    {
        deviceType device_type = BtDevice_GetDeviceType(device);
        if (device_type == DEVICE_TYPE_HANDSET || device_type == DEVICE_TYPE_SINK)
        {
            search_data->source_found = TRUE;
        }
    }
}

static void av_AllocateAudioSourceToDevice(avInstanceTaskData *theInst)
{
    audio_source_search_data_t search_data = {audio_source_a2dp_1, FALSE};
    device_t device = Av_FindDeviceFromInstance(theInst);
    PanicFalse(device != NULL);

    /* Find a free audio source */
    DeviceList_Iterate(av_SearchForDeviceWithAudioSource, &search_data);
    if (search_data.source_found)
    {
        /* If a2dp_1 has been allocated, try to allocate a2dp_2 */
        search_data.source_to_find = audio_source_a2dp_2;
        search_data.source_found = FALSE;
        DeviceList_Iterate(av_SearchForDeviceWithAudioSource, &search_data);
    }
    if (!search_data.source_found)
    {
        /* A free audio_source exists, allocate it to the device with the instance. */
        DeviceProperties_SetAudioSource(device, search_data.source_to_find);
        DEBUG_LOG_VERBOSE("Av_AllocateAudioSourceToDevice inst=%08x enum:audio_source_t:%d",
                          theInst, search_data.source_to_find);
    }
    else
    {
        /* It should be impossible to have connected the A2DP profile if we have already
           two connected audio sources for A2DP, this may indicate a handle was leaked. */
        Panic();
    }
}

static bool av_PopulateContextFromProviders(audio_source_t source, audio_source_provider_context_t *context)
{
    if (context_provider && context_provider->PopulateSourceContext)
    {
        return context_provider->PopulateSourceContext(source, context);
    }
    return FALSE;
}

static audio_source_provider_context_t av_GetA2dpContext(audio_source_t source)
{
    audio_source_provider_context_t context = context_audio_disconnected;

    avInstanceTaskData* av_instance = Av_GetInstanceForHandsetSource(source);

    if(av_instance)
    {
        if (!Av_InstanceIsDisconnected(av_instance))
        {
            if(Av_InstanceIsA2dpSinkStarted(av_instance))
            {
                if(Av_IsInstancePlaying(av_instance))
                {
                    context = context_audio_is_playing;
                }
                else if(Av_IsInstancePaused(av_instance))
                {
                    context = context_audio_is_paused;
                }
                else
                {
                    context = context_audio_is_streaming;
                }
            }
            else if (Av_InstanceIsA2dpSinkConnected(av_instance))
            {
                context = context_audio_connected;
            }
        }
    }

    return context;
}

static profile_manager_disconnected_ind_reason_t av_ConvertA2dpDisconnectReason(avA2dpDisconnectReason a2dp_reason)
{
    profile_manager_disconnected_ind_reason_t reason;
    switch (a2dp_reason)
    {
    case AV_A2DP_DISCONNECT_NORMAL:
        reason = profile_manager_disconnected_normal;
        break;
    case AV_A2DP_DISCONNECT_LINKLOSS:
        reason = profile_manager_disconnected_link_loss;
        break;
    case AV_A2DP_DISCONNECT_LINK_TRANSFERRED:
        reason = profile_manager_disconnected_link_transfer;
        break;
    case AV_A2DP_DISCONNECT_ERROR:
    default:
        reason = profile_manager_disconnected_error;
        break;
    }
    return reason;
}

void appAvInstanceA2dpConnected(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appAvInstanceA2dpConnected");

    /* Set link supervision timeout 5 seconds */
    //ConnectionSetLinkSupervisionTimeout(appAvGetSink(theInst), 0x1F80);

    /* Update most recent connected device */
    appDeviceUpdateMruDevice(&theInst->bd_addr);

    /* If A2DP was initiated by us, or AVRCP has already been brought up by someone else */
    if (theInst->a2dp.bitfields.local_initiated || appAvrcpIsConnected(theInst))
    {
        DEBUG_LOG("appAvInstanceA2dpConnected, locally initiated, connecting AVRCP");
        appAvAvrcpConnectRequest(&theInst->av_task, &theInst->bd_addr);
    }
    else if ((appAvrcpGetState(theInst) == AVRCP_STATE_DISCONNECTING) || appAvrcpIsDisconnected(theInst))
    {
        DEBUG_LOG("appAvInstanceA2dpConnected, remotely initiated");
        appAvAvrcpConnectLaterRequest(theInst, appConfigAvrcpConnectDelayAfterRemoteA2dpConnectMs());
    }

    /* If A2DP connection is made to the handset before AVRCP, set the system volume based
     on the stored handset device volume. If the new A2DP connection is to the peer, AVRCP
     will also connect and av_VolumeHandleAvrcpConnect will handle volume setup. */
    av_CheckAndSetHandsetVolumeIfNeeded(theInst);

    /* Tell clients we have connected */
    MAKE_AV_MESSAGE(AV_A2DP_CONNECTED_IND);
    message->av_instance = theInst;
    message->bd_addr = theInst->bd_addr;
    message->local_initiated = theInst->a2dp.bitfields.local_initiated;
    appAvSendStatusMessage(AV_A2DP_CONNECTED_IND, message, sizeof(*message));

    /* If this is completing a connect request, send confirmation for this device */
    if (!ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&AvGetTaskData()->a2dp_connect_request_clients),
                                           &theInst->bd_addr, profile_manager_success,
                                           profile_manager_a2dp_profile, profile_manager_connect))
    {
        /* otherwise provide indication to the Profile Manager */
        ProfileManager_GenericConnectedInd(profile_manager_a2dp_profile, &theInst->bd_addr);
    }
}

void appAvInstanceA2dpDisconnected(avInstanceTaskData *theInst)
{
    bool wasNotified = FALSE;

    DEBUG_LOG("appAvInstanceA2dpDisconnected");

    appAvAvrcpDisconnectLaterRequest(theInst, D_SEC(2));

    av_VolumeHandleAvDisconnect(theInst);

    if (theInst->a2dp.bitfields.disconnect_reason == AV_A2DP_CONNECT_FAILED)
    {
        wasNotified = ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&AvGetTaskData()->a2dp_connect_request_clients),
                                          &theInst->bd_addr, profile_manager_failed,
                                          profile_manager_a2dp_profile, profile_manager_connect);
    }
    else
    {
        wasNotified = ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&AvGetTaskData()->a2dp_disconnect_request_clients),
                                          &theInst->bd_addr,
                                          profile_manager_success,
                                          profile_manager_a2dp_profile, profile_manager_disconnect);
    }
    if (!wasNotified)
    {
        profile_manager_disconnected_ind_reason_t reason = av_ConvertA2dpDisconnectReason(theInst->a2dp.bitfields.disconnect_reason);
        ProfileManager_GenericDisconnectedInd(profile_manager_a2dp_profile, &theInst->bd_addr, reason);
    }

    /* Tell clients we have disconnected */
    MAKE_AV_MESSAGE(AV_A2DP_DISCONNECTED_IND);
    message->av_instance = theInst;
    message->bd_addr = theInst->bd_addr;
    message->reason = theInst->a2dp.bitfields.disconnect_reason;
    appAvSendStatusMessage(AV_A2DP_DISCONNECTED_IND, message, sizeof(*message));
}

void appAvInstanceAvrcpConnected(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appAvInstanceAvrcpConnected");

    /* Update power table */
    appLinkPolicyUpdatePowerTable(&theInst->bd_addr);

    appAvInstanceStartMediaPlayback(theInst);

    /* Tell clients we have connected */
    MAKE_AV_MESSAGE(AV_AVRCP_CONNECTED_IND);
    message->av_instance = theInst;
    message->bd_addr = theInst->bd_addr;
    message->sink = AvrcpGetSink(theInst->avrcp.avrcp);
    appAvSendStatusMessage(AV_AVRCP_CONNECTED_IND, message, sizeof(*message));

    /* If this is completing a connect request, send confirmation for this device */
    if (!ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&AvGetTaskData()->avrcp_connect_request_clients),
                                      &theInst->bd_addr, profile_manager_success,
                                      profile_manager_avrcp_profile, profile_manager_connect))
    {
        /* otherwise provide indication to the Profile Manager */
        ProfileManager_GenericConnectedInd(profile_manager_avrcp_profile, &theInst->bd_addr);
    }
}

void appAvInstanceAvrcpDisconnected(avInstanceTaskData *theInst, bool is_disconnect_request)
{
    bool wasNotified = FALSE;
    DEBUG_LOG("appAvInstanceAvrcpDisconnected is_disconnect_request %d",is_disconnect_request);

    if (!is_disconnect_request)
    {
        wasNotified = ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&AvGetTaskData()->avrcp_connect_request_clients),
                                          &theInst->bd_addr, profile_manager_failed,
                                          profile_manager_avrcp_profile, profile_manager_connect);
    }
    else
    {
        wasNotified = ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&AvGetTaskData()->avrcp_disconnect_request_clients),
                                          &theInst->bd_addr,
                                          profile_manager_success,
                                          profile_manager_avrcp_profile, profile_manager_disconnect);
    }

    if (!wasNotified)
    {
        ProfileManager_GenericDisconnectedInd(profile_manager_avrcp_profile, &theInst->bd_addr, profile_manager_disconnected_normal);
    }


    /* Tell clients we have disconnected */
    MAKE_AV_MESSAGE(AV_AVRCP_DISCONNECTED_IND);
    message->av_instance = theInst;
    message->bd_addr = theInst->bd_addr;
    appAvSendStatusMessage(AV_AVRCP_DISCONNECTED_IND, message, sizeof(*message));
}

bool appAvIsAvrcpConnected(avInstanceTaskData *theInst)
{
    return appAvrcpIsValidClient(theInst, &theInst->av_task);
}

audio_source_provider_context_t AvGetCurrentContext(audio_source_t source)
{
    audio_source_provider_context_t provider_context;

    if (av_PopulateContextFromProviders(source, &provider_context))
    {
        return provider_context;
    }

    return av_GetA2dpContext(source);
}

avInstanceTaskData *appAvInstanceCreate(const bdaddr *bd_addr, const av_callback_interface_t * plugin_interface)
{
    avTaskData *theAv = AvGetTaskData();
    avInstanceTaskData* av_inst = NULL;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    /* Return null if the device doesn't exist */
    if (device == NULL)
    {
        DEBUG_LOG_ERROR("appAvInstanceCreate device not found (%04x:%02x:%06x)", bd_addr->nap, bd_addr->uap, bd_addr->lap);
        return av_inst;
    }

    /* Panic if we have a duplicate av_instance somehow */
    av_inst = AvInstance_GetInstanceForDevice(device);
    PanicNotNull(av_inst);

    /* Allocate new instance */
    av_inst = PanicUnlessNew(avInstanceTaskData);
    AvInstance_SetInstanceForDevice(device, av_inst);
    av_inst->av_callbacks = plugin_interface;

    DEBUG_LOG("appAvInstanceCreate %p", av_inst);

    /* Initialise instance */
    appA2dpInstanceInit(av_inst, theAv->suspend_state);
    appAvrcpInstanceInit(av_inst);

    /* Default to unset volume. The volume will be set when A2DP or AVRCP connects. */
    av_inst->volume = VOLUME_UNSET;

    /* Set up task handler */
    av_inst->av_task.handler = appAvInstanceHandleMessage;

    /* Set Bluetooth address of remote device */
    av_inst->bd_addr = *bd_addr;
    av_inst->avrcp_reject_pending = FALSE;

    /* Initially not synced to another AV instance */
    appA2dpSyncInitialise(av_inst);

    if (!appDeviceIsPeer(bd_addr))
    {
        av_AllocateAudioSourceToDevice(av_inst);
    }

    AvInstance_RegisterMediaControlInterfaceForInstance(av_inst);

    /* Register to receive kymera events(e.g: KYMERA_LOW_LATENCY_STATE_CHANGED_IND) */
    Kymera_ClientRegister(AvGetTask());

    /* Tell clients we have created new instance */
    appAvSendStatusMessage(AV_CREATE_IND, NULL, 0);

    /* Return pointer to new instance */
    return av_inst;
}

/*! \brief Check whether there is an A2DP lock set

    \return TRUE is there is and A2DP lock pending, else FALSE

    \note If the lock is set, this function also sends a conditional message to
    trigger destruction of the AV instance when the lock is cleared. Therefore,
    this function should be used with caution.
*/
static bool appAvA2dpLockPending(avInstanceTaskData *theInst)
{
    bool result = FALSE;

    if (theInst && (appA2dpGetLock(theInst) & APP_A2DP_AUDIO_STOP_LOCK))
    {
        uint16 *lock_addr = &appA2dpGetLock(theInst);
        DEBUG_LOG("appAvA2dpLockPending(%p) %d", theInst, *lock_addr);
        MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ, NULL, lock_addr);
        result = TRUE;
    }
    return result;
}

void appAvInstanceDestroy(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appAvInstanceDestroy(%p)", theInst);
    device_t device = Av_GetDeviceForInstance(theInst);

    PanicNull(device);

    /* Destroy instance only both state machines are disconnected and there is no A2DP lock pending */
    if (appA2dpIsDisconnected(theInst) && appAvrcpIsDisconnected(theInst) && !appAvA2dpLockPending(theInst))
    {
        DEBUG_LOG("appAvInstanceDestroy(%p) permitted", theInst);

        /* Check there are no A2DP & AVRCP profile library instances */
        PanicFalse(theInst->a2dp.device_id == INVALID_DEVICE_ID);

        PanicNotNull(theInst->avrcp.avrcp);

        /* Cancel all audio sync messages */
        appA2dpSyncUnregister(theInst);

        /* Clear client lists */
        if (theInst->avrcp.client_list)
        {
            TaskList_Destroy(theInst->avrcp.client_list);
            theInst->avrcp.client_list = NULL;
        }


        /* Flush any messages still pending delivery */
        MessageFlushTask(&theInst->av_task);

        /* Clear entry and free instance */
        AvInstance_SetInstanceForDevice(device, NULL);
        free(theInst);

        DeviceProperties_RemoveAudioSource(device);

        if (appAvIsDisconnected())
        {
            /* Unregister to stop receiving kymera events */
            Kymera_ClientUnregister(AvGetTask());
        }

        /* Tell clients we have destroyed instance */
        appAvSendStatusMessage(AV_DESTROY_IND, NULL, 0);
        return;
    }
    else
    {
        DEBUG_LOG("appAvInstanceDestroy(%p) A2DP (%d) or AVRCP (%d) not disconnected, or A2DP Lock Pending",
                   theInst, !appA2dpIsDisconnected(theInst), !appAvrcpIsDisconnected(theInst));
    }
}

/*! \brief Return AV instance for A2DP sink

    This function walks through the AV instance table looking for the
    first instance which is a connected sink that can use the
    specified codec.

    \param codec_type   Codec to look for

    \return Pointer to AV information for a connected source,NULL if none
        was found
*/
avInstanceTaskData *appAvGetA2dpSink(avCodecType codec_type)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        /* Note that the sink SEID is also know when in state
           A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC. This is required because the
           audio sync code sends AUDIO_SYNC_CONNECT_IND when the remote device
           initiates media channel connection and A2DP is therefore in the state
           A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC. Audio sync clients can request
           media connect parameters at this point, so accessing instance is
           required when in this state.
        */
        if (theInst && (appA2dpIsStateConnectedMedia(theInst->a2dp.state) ||
                        theInst->a2dp.state == A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC))
        {
            switch (codec_type)
            {
                case AV_CODEC_ANY:
                    if (appA2dpIsSinkCodec(theInst))
                        return theInst;
                    break;

                case AV_CODEC_TWS:
                    if (appA2dpIsSinkTwsCodec(theInst))
                        return theInst;
                    break;

                case AV_CODEC_NON_TWS:
                    if (appA2dpIsSinkNonTwsCodec(theInst))
                        return theInst;
                    break;
            }
        }
    }

    /* No sink found so return NULL */
    return NULL;
}

avInstanceTaskData *appAvGetA2dpSource(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && appA2dpIsStateConnectedMedia(theInst->a2dp.state) && appA2dpIsSourceCodec(theInst))
            return theInst;
    }

    /* No sink found so return NULL */
    return NULL;
}

/*! \brief Entering `Initialising A2DP` state

    This function is called when the AV state machine enters
    the 'Initialising A2DP' state, it calls the A2dpInit() function
    to initialise the A2DP profile library and register the SEPs.
*/

static void appAvEnterInitialisingA2dp(avTaskData *theAv)
{
    av_plugin_interface.InitialiseA2dp(&theAv->task);
}

/*! \brief Entering `Initialising AVRCP` state

    This function is called when the AV state machine enters
    the 'Initialising AVRCP' state, it calls the AvrcpInit() function
    to initialise the AVRCP profile library.
*/
static void appAvEnterInitialisingAvrcp(avTaskData *theAv)
{
    DEBUG_LOG("appAvEnterInitialisingAvrcp");

    /* Go ahead and initialise the AVRCP library */
    AvrcpInit(&theAv->task, av_plugin_interface.GetAvrcpConfig());
}
/*! \brief Set AV FSM state

    Called to change state.  Handles calling the state entry and exit
    functions for the new and old states.
*/
static void appAvSetState(avTaskData *theAv, avState state)
{
    DEBUG_LOG("appAvSetState(%d)", state);

    /* Set new state */
    theAv->bitfields.state = state;

    /* Handle state entry functions */
    switch (state)
    {
        case AV_STATE_INITIALISING_A2DP:
            appAvEnterInitialisingA2dp(theAv);
            break;

        case AV_STATE_INITIALISING_AVRCP:
            appAvEnterInitialisingAvrcp(theAv);
            break;

        default:
            break;
    }

    /* Set new state */
    theAv->bitfields.state = state;
}

/*! \brief Set AV FSM state

    Returns current state of the AV FSM.
*/
static avState appAvGetState(avTaskData *theAv)
{
    return theAv->bitfields.state;
}

static void appAvHandleA2dpInitConfirm(avTaskData *theAv, const A2DP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("appAvHandleA2dpInitConfirm");

    /* Check if A2DP initialised successfully */
    if (cfm->status == a2dp_success)
    {
        /* Move to 'Initialising AVRCP' state */
        appAvSetState(theAv, AV_STATE_INITIALISING_AVRCP);
    }
    else
        Panic();
}

static void appAvHandleAvrcpInitConfirm(avTaskData *theAv, const AVRCP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("appAvHandleAvrcpInitConfirm");

    /* Check if AVRCP successfully initialised */
    if (cfm->status == avrcp_success)
    {
        /* Tell main application task we have initialised */
        MessageSend(SystemState_GetTransitionTask(), AV_INIT_CFM, NULL);

        /* Change to 'idle' state */
        appAvSetState(theAv, AV_STATE_IDLE);
    }
    else
        Panic();
}

/*! \brief Handle indication of change in a connection status.

    Some phones will disconnect the ACL without closing any L2CAP/RFCOMM
    connections, so we check the ACL close reason code to determine if this
    has happened.

    If the close reason code was not link-loss and we have an AV profiles
    on that link, mark it as detach pending, so that we can gracefully handle
    the L2CAP or RFCOMM disconnection that will follow shortly.
 */
static void appAvHandleConManagerConnectionInd(CON_MANAGER_CONNECTION_IND_T *ind)
{
    avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&ind->bd_addr);
    if (theInst)
    {
        if (!ind->connected && !ind->ble)
        {
            if (ind->reason != hci_error_conn_timeout)
            {
                DEBUG_LOG("appAvHandleConManagerConnectionInd, detach pending");
                theInst->detach_pending = TRUE;
            }
        }
    }
}

static void initAvVolume(audio_source_t source)
{
    AudioSources_RegisterVolume(source, A2dpProfile_GetAudioSourceVolumeInterface());
    AudioSources_RegisterObserver(source, AvrcpProfile_GetObserverInterface());
}

void Av_SetupForPrimaryRole(void)
{
    DEBUG_LOG("Av_SetupForPrimaryRole");
    AudioSources_RegisterAudioInterface(audio_source_a2dp_1, A2dpProfile_GetHandsetSourceAudioInterface());
    AudioSources_RegisterAudioInterface(audio_source_a2dp_2, A2dpProfile_GetHandsetSourceAudioInterface());

    AudioSources_RegisterMediaControlInterface(audio_source_a2dp_1, AvrcpProfile_GetMediaControlInterface());
    AudioSources_RegisterMediaControlInterface(audio_source_a2dp_2, AvrcpProfile_GetMediaControlInterface());
}

void Av_SetupForSecondaryRole(void)
{
}


static void av_CancelQueuedA2dpDisconnectRequests(const bdaddr* bd_addr)
{
    avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(bd_addr);
    if (theInst != NULL)
    {
        MessageCancelAll(&theInst->av_task, AV_INTERNAL_A2DP_DISCONNECT_REQ);
    }
}

static void av_CancelQueuedAvrcpDisconnectRequests(const bdaddr* bd_addr)
{
    avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(bd_addr);
    if (theInst != NULL)
    {
        appAvAvrcpCancelQueuedDisconnectRequests(theInst);
    }
}

/*! \brief Initiate an AV connection to a Bluetooth address

    AV connections are started with an A2DP connection.

    \param bd_addr Address to connect to
 */
static void Av_A2dpConnectWithBdAddr(bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        task_list_t * req_task_list = TaskList_GetBaseTaskList(&AvGetTaskData()->a2dp_connect_request_clients);
        appAvA2dpConnectFlags connect_flags = A2DP_CONNECT_MEDIA;
        avTaskData *theAv = AvGetTaskData();

        av_CancelQueuedA2dpDisconnectRequests(bd_addr);

        ProfileManager_AddToNotifyList(req_task_list, device);
        if (theAv->play_on_connect && (!appDeviceIsPeer(bd_addr)))
        {
            connect_flags |= A2DP_START_MEDIA_PLAYBACK;
            theAv->play_on_connect = FALSE;
        }

        if (!appAvA2dpConnectRequest(bd_addr, connect_flags))
        {
            /* If A2DP is already connected send a connect cfm */
            ProfileManager_NotifyConfirmation(req_task_list, bd_addr, profile_manager_success,
                                              profile_manager_a2dp_profile, profile_manager_connect);
        }
    }
}

static void Av_A2dpDisconnectWithBdAddr(bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        task_list_t * req_task_list = TaskList_GetBaseTaskList(&AvGetTaskData()->a2dp_disconnect_request_clients);
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(bd_addr);

        ProfileManager_AddToNotifyList(req_task_list, device);
        if (!appAvA2dpDisconnectRequest(theInst))
        {
            /* If A2DP is already disconnected send a disconnect cfm */
            ProfileManager_NotifyConfirmation(req_task_list, bd_addr, profile_manager_success,
                                              profile_manager_a2dp_profile, profile_manager_disconnect);
        }
    }
}

static void Av_AvrcpConnectRequest(bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        task_list_t * req_task_list = TaskList_GetBaseTaskList(&AvGetTaskData()->avrcp_connect_request_clients);

        av_CancelQueuedAvrcpDisconnectRequests(bd_addr);

        ProfileManager_AddToNotifyList(req_task_list, device);
        if (!appAvAvrcpConnectRequest(&profile_manager.dummy_task, bd_addr))
        {
            /* If AVRCP is already connected send a connect cfm */
            ProfileManager_NotifyConfirmation(req_task_list, bd_addr, profile_manager_success,
                                              profile_manager_avrcp_profile, profile_manager_connect);
        }
    }
}

static void Av_AvrcpDisconnectRequest(bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        task_list_t * req_task_list = TaskList_GetBaseTaskList(&AvGetTaskData()->avrcp_disconnect_request_clients);
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(bd_addr);

        ProfileManager_AddToNotifyList(req_task_list, device);
        if (!appAvAvrcpDisconnectRequest(&profile_manager.dummy_task, theInst))
        {
            /* If AVRCP is already disconnected send a disconnect cfm */
            ProfileManager_NotifyConfirmation(req_task_list, bd_addr, profile_manager_success,
                                              profile_manager_avrcp_profile, profile_manager_disconnect);
        }
    }
}

bool appAvInit(Task init_task)
{
    avTaskData *theAv = AvGetTaskData();

    UNUSED(init_task);

    av_plugin_interface.Initialise();

    /* Set up task handler */
    theAv->task.handler = appAvHandleMessage;

    /* Initialise state */
    theAv->suspend_state = 0;
    theAv->bitfields.state = AV_STATE_NULL;
    appAvSetState(theAv, AV_STATE_INITIALISING_A2DP);

    /* Initialise client lists */
    TaskList_Initialise(&theAv->av_status_client_list);
    TaskList_Initialise(&theAv->av_ui_client_list);
    TaskList_Initialise(&theAv->avrcp_client_list);
    appAvAvrcpClientRegister(&theAv->task, 0);

    /* Create lists for connection/disconnection requests */
    TaskList_WithDataInitialise(&theAv->a2dp_connect_request_clients);
    TaskList_WithDataInitialise(&theAv->a2dp_disconnect_request_clients);
    TaskList_WithDataInitialise(&theAv->avrcp_connect_request_clients);
    TaskList_WithDataInitialise(&theAv->avrcp_disconnect_request_clients);

    /* Register to receive notifications of (dis)connections */
    ConManagerRegisterConnectionsClient(&theAv->task);

    Av_SetupForPrimaryRole();
    initAvVolume(audio_source_a2dp_1);
    initAvVolume(audio_source_a2dp_2);

    ProfileManager_RegisterProfile(profile_manager_a2dp_profile, Av_A2dpConnectWithBdAddr, Av_A2dpDisconnectWithBdAddr);
    ProfileManager_RegisterProfile(profile_manager_avrcp_profile, Av_AvrcpConnectRequest, Av_AvrcpDisconnectRequest);

    /* Register a2dp feature with Bandwidth Manager for high priority to throttle other lower priority features */
    PanicFalse(BandwidthManager_RegisterFeature(BANDWIDTH_MGR_FEATURE_A2DP_LL, high_bandwidth_manager_priority, NULL));
    PanicFalse(BandwidthManager_RegisterFeature(BANDWIDTH_MGR_FEATURE_A2DP_HIGH_BW, high_bandwidth_manager_priority, NULL));

    return TRUE;
}

void appAvAvrcpClientRegister(Task client_task, uint8 interests)
{
    UNUSED(interests);

    /* Add client task to list */
    TaskList_AddTask(&AvGetTaskData()->avrcp_client_list, client_task);
}

void appAvStatusClientRegister(Task client_task)
{
    TaskList_AddTask(&AvGetTaskData()->av_status_client_list, client_task);
}

void appAvStatusClientUnregister(Task client_task)
{
    TaskList_RemoveTask(&AvGetTaskData()->av_status_client_list, client_task);
}

void appAvSendStatusMessage(MessageId id, void *msg, size_t size)
{
    TaskList_MessageSendWithSize(&AvGetTaskData()->av_status_client_list, id, msg, size);
}

void AvSendAudioConnectedStatusMessage(avInstanceTaskData* av_instance, MessageId id)
{
    audio_source_t source  = Av_GetSourceForInstance(av_instance);
    
    if(source != audio_source_none)
    {
        MESSAGE_MAKE(message, AV_A2DP_AUDIO_CONNECT_MESSAGE_T);
        message->audio_source = source;
        appAvSendStatusMessage(id, (void *)message, sizeof(AV_A2DP_AUDIO_CONNECT_MESSAGE_T));
    }
}

void appAvUiClientRegister(Task client_task)
{
    TaskList_AddTask(&AvGetTaskData()->av_ui_client_list, client_task);
}

void appAvSendUiMessage(MessageId id, void *msg, size_t size)
{
    TaskList_MessageSendWithSize(&AvGetTaskData()->av_ui_client_list, id, msg, size);
}

void appAvSendUiMessageId(MessageId id)
{
     appAvSendUiMessage(id, NULL, 0);
}

bool appAvA2dpConnectRequest(const bdaddr *bd_addr, appAvA2dpConnectFlags a2dp_flags)
{
    avInstanceTaskData *theInst;

    /* Check if AV instance to this device already exists */
    theInst = appAvInstanceFindFromBdAddr(bd_addr);
    if (theInst == NULL)
    {
        /* No AV instance for this device, so create new instance */
        theInst = appAvInstanceCreate(bd_addr, &av_plugin_interface);
    }
    else
    {
        /* Make sure there's no pending destroy message */
        MessageCancelAll(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);
        MessageCancelAll(&theInst->av_task, AV_INTERNAL_AVRCP_DESTROY_REQ);
    }

    /* Now check we have an AV instance */
    if (theInst)
    {
        /* Check A2DP is not already connected */
        if (!appA2dpIsConnected(theInst))
        {
            /* Send AV_INTERNAL_A2DP_CONNECT_REQ to start A2DP connection */
            MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_CONNECT_REQ);

            DEBUG_LOG("appAvA2dpConnectRequest A2DP, %p, %x %x %lx",
                         (void *)theInst, bd_addr->nap, bd_addr->uap, bd_addr->lap);

            /* Send message to newly created AV instance to connect A2DP */
            message->num_retries = 2;
            message->flags = (unsigned)a2dp_flags;
            MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ);
            MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ, message,
                                     ConManagerCreateAcl(&theInst->bd_addr));

            return TRUE;
        }
    }

    return FALSE;
}

bool appAvAvrcpConnectRequest(Task client_task, const bdaddr *bd_addr)
{
    avInstanceTaskData *theInst;

    /* Check if AV instance to this device already exists */
    theInst = appAvInstanceFindFromBdAddr(bd_addr);
    if (theInst == NULL)
    {
        /* No AV instance for this device, so create new instance */
        theInst = appAvInstanceCreate(bd_addr, &av_plugin_interface);
    }
    else
    {
        /* Make sure there's no pending disconnect/destroy message */
        MessageCancelAll(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);
        MessageCancelAll(&theInst->av_task, AV_INTERNAL_AVRCP_DESTROY_REQ);
        if (client_task == &theInst->av_task)
        {
            MessageCancelAll(&theInst->av_task, AV_INTERNAL_AVRCP_DISCONNECT_LATER_REQ);
        }
    }

    /* Now check we have an AV instance */
    if (theInst)
    {
        /* Send AV_INTERNAL_AVRCP_CONNECT_REQ to start AVRCP connection */
        MAKE_AV_MESSAGE(AV_INTERNAL_AVRCP_CONNECT_REQ);

        DEBUG_LOG("appAvAvrcpConnectRequest AVRCP, %p, %x %x %lx",
                     (void *)theInst, bd_addr->nap, bd_addr->uap, bd_addr->lap);

        /* Send message to newly created AV instance to connect AVRCP */
        message->client_task = client_task;
        MessageCancelFirst(&theInst->av_task, AV_INTERNAL_AVRCP_CONNECT_REQ);
        MessageSendConditionally(&theInst->av_task, AV_INTERNAL_AVRCP_CONNECT_REQ, message,
                                 ConManagerCreateAcl(&theInst->bd_addr));

        return TRUE;
    }

    return FALSE;
}

void appAvAvrcpConnectResponse(Task ind_task, Task client_task, const bdaddr *bd_addr,
                               uint16 connection_id, uint16 signal_id, avAvrcpAccept accept)
{
    avInstanceTaskData *av_inst;

    /* Get AV instance for this device */
    av_inst = appAvInstanceFindFromBdAddr(bd_addr);
    if (av_inst)
    {
        MAKE_AV_MESSAGE(AV_INTERNAL_AVRCP_CONNECT_RES);
        message->ind_task = ind_task;
        message->client_task = client_task;
        message->accept = accept;
        message->connection_id = connection_id;
        message->signal_id = signal_id;
        MessageSend(&av_inst->av_task, AV_INTERNAL_AVRCP_CONNECT_RES, message);
    }
    else
    {
        Panic();
    }
}

bool appAvA2dpDisconnectRequest(avInstanceTaskData *av_inst)
{
    if (av_inst)
    {
        MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_DISCONNECT_REQ);
        message->flags = 0;
        PanicFalse(appAvIsValidInst(av_inst));
        MessageSendConditionally(&av_inst->av_task, AV_INTERNAL_A2DP_DISCONNECT_REQ,
                                 message, &appA2dpGetLock(av_inst));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

bool appAvAvrcpDisconnectRequest(Task client_task, avInstanceTaskData *av_inst)
{
    if (av_inst)
    {
        /* Cancel any queued connect messages and release the lock */
        appAvAvrcpCancelQueuedConnectRequests(av_inst);

        MAKE_AV_MESSAGE(AV_INTERNAL_AVRCP_DISCONNECT_REQ);
        message->client_task = client_task;
        PanicFalse(appAvIsValidInst(av_inst));
        MessageSendConditionally(&av_inst->av_task, AV_INTERNAL_AVRCP_DISCONNECT_REQ,
                                 message, &appAvrcpGetLock(av_inst));

        DEBUG_LOG("appAvAvrcpDisconnectRequest(0x%x)", client_task);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void appAvStreamingSuspend(avSuspendReason reason)
{
    avTaskData *theAv = AvGetTaskData();
    unsigned suspend_state_pre = theAv->suspend_state;
    DEBUG_LOG("appAvStreamingSuspend(0x%x, 0x%x)", suspend_state_pre, reason);

    /* Update suspend state for any newly created AV instances */
    theAv->suspend_state |= reason;

    /* Only send suspend messages if the suspend state has changed */
    if (theAv->suspend_state != suspend_state_pre)
    {
        avInstanceTaskData* av_inst;
        av_instance_iterator_t iterator;

        for_all_av_instances(av_inst, &iterator)
        {
            if (av_inst != NULL)
            {
                MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ);

                /* Send message to AV instance */
                message->reason = reason;
                MessageSendConditionally(&av_inst->av_task, AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ,
                                         message, &appA2dpGetLock(av_inst));
            }
        }
    }
}

void appAvStreamingResume(avSuspendReason reason)
{
    avTaskData *theAv = AvGetTaskData();
    unsigned suspend_state_pre = theAv->suspend_state;
    DEBUG_LOG("appAvStreamingResume(0x%x, 0x%x)", suspend_state_pre, reason);

    /* Update suspend state for any newly created AV instances */
    theAv->suspend_state &= ~reason;

    /* Only send resume messages if the suspend state has changed */
    if (theAv->suspend_state != suspend_state_pre)
    {
        avInstanceTaskData* av_inst;
        av_instance_iterator_t iterator;

        for_all_av_instances(av_inst, &iterator)
        {
            if (av_inst != NULL)
            {
                MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_RESUME_MEDIA_REQ);

                /* Send message to AV instance */
                message->reason = reason;
                MessageSendConditionally(&av_inst->av_task, AV_INTERNAL_A2DP_RESUME_MEDIA_REQ,
                                        message, &appA2dpGetLock(av_inst));
            }
        }
    }
}

bool appAvIsValidInst(avInstanceTaskData* theInst)
{
    avInstanceTaskData* av_instance;
    av_instance_iterator_t iterator;

    for_all_av_instances(av_instance, &iterator)
    {
        if (theInst == av_instance)
            return TRUE;
    }
    return FALSE;
}

/*! \brief Schedules media playback if in correct AV state and flag is set.
    \param  theInst The AV instance.
    \return TRUE if media play is scheduled, otherwise FALSE.
 */
bool appAvInstanceStartMediaPlayback(avInstanceTaskData *theInst)
{
        
    if (appA2dpIsConnectedMedia(theInst) && appAvIsAvrcpConnected(theInst))
    {
        if (theInst->a2dp.bitfields.flags & A2DP_START_MEDIA_PLAYBACK)
        {
            DEBUG_LOG("appAvInstanceStartMediaPlayback(%p)", theInst);
            theInst->a2dp.bitfields.flags &= ~A2DP_START_MEDIA_PLAYBACK;
            MessageSendLater(&theInst->av_task, AV_INTERNAL_AVRCP_PLAY_REQ, NULL,
                             appConfigHandoverMediaPlayDelay());
            return TRUE;
        }
    }
    return FALSE;
}

#ifdef INCLUDE_LATENCY_MANAGER
static void appAvHandleLowLatencyStateChangeInd(KYMERA_LOW_LATENCY_STATE_CHANGED_IND_T *message)
{
    DEBUG_LOG("appAvHandleLowLatencyStateChangeInd: enum:ll_stream_state_t:state[%d]", message->state);
    /* Inform Bandwidth Manager about start usuage of bandwidth for a2dp LL streaming.
     * This might make Bandwidth Manager to cause other lower priority features to
     * throttle their bandwidth usuage */
    if ((message->state == LOW_LATENCY_STREAM_ACTIVE) && (Av_IsA2dpSinkStreaming()))
    {
        BandwidthManager_FeatureStart(BANDWIDTH_MGR_FEATURE_A2DP_LL);
    }
    /* Inform Bandwidth Manager about stop usuage of bandwidth for a2dp LL streaming.
     * If any other lower priority features are throttling their bandwidth usage,
     * Bandwidth Manager informs them to resume actual(un-throttle) usage of their bandwidth */
    else if(message->state == LOW_LATENCY_STREAM_INACTIVE)
    {
        BandwidthManager_FeatureStop(BANDWIDTH_MGR_FEATURE_A2DP_LL);
    }
}

static void appAvHandleHighBandwidthStateChangeInd(KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND_T *message)
{
    /* Inform Bandwidth Manager that the high bandwidth streaming mode is active */
    if (message->state == HIGH_BANDWIDTH_STREAM_ACTIVE)
    {
        BandwidthManager_FeatureStart(BANDWIDTH_MGR_FEATURE_A2DP_HIGH_BW);
    }
    /* Inform Bandwidth Manager that the high bandwidth streaming mode is no longer active */
    else if(message->state == HIGH_BANDWIDTH_STREAM_INACTIVE)
    {
        BandwidthManager_FeatureStop(BANDWIDTH_MGR_FEATURE_A2DP_HIGH_BW);
    }
}
#endif

/*! \brief Message Handler

    This function is the main message handler for the AV module, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
void appAvHandleMessage(Task task, MessageId id, Message message)
{
    avTaskData *theAv = (avTaskData *)task;
    avState state = appAvGetState(theAv);

    /* Handle kymera event messages */
    switch(id)
    {
        case KYMERA_LOW_LATENCY_STATE_CHANGED_IND:
        {
#ifdef INCLUDE_LATENCY_MANAGER
            appAvHandleLowLatencyStateChangeInd((KYMERA_LOW_LATENCY_STATE_CHANGED_IND_T*)message);
#endif
        }
        return;
        case KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND:
        {
#ifdef INCLUDE_LATENCY_MANAGER
            appAvHandleHighBandwidthStateChangeInd((KYMERA_HIGH_BANDWIDTH_STATE_CHANGED_IND_T*)message);
#endif
        }
        return;
        case KYMERA_AANC_ED_ACTIVE_TRIGGER_IND:
        case KYMERA_AANC_ED_INACTIVE_TRIGGER_IND:
        case KYMERA_AANC_QUIET_MODE_TRIGGER_IND:
        case KYMERA_AANC_ED_ACTIVE_CLEAR_IND:
        case KYMERA_AANC_ED_INACTIVE_CLEAR_IND:
        case KYMERA_AANC_QUIET_MODE_CLEAR_IND:
            return;
    }

    /* Handle connection manager messages */
    switch (id)
    {
        case CON_MANAGER_CONNECTION_IND:
            appAvHandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T *)message);
            return;
    }

    /* Handle A2DP messages */
    switch (id)
    {
        case A2DP_INIT_CFM:
        {
            switch (state)
            {
                case AV_STATE_INITIALISING_A2DP:
                    appAvHandleA2dpInitConfirm(theAv, (A2DP_INIT_CFM_T *)message);
                    return;
                default:
                    UnexpectedMessage_HandleMessage(id);
                    return;
            }
        }

        case AVRCP_INIT_CFM:
        {
            switch (state)
            {
                case AV_STATE_INITIALISING_AVRCP:
                    appAvHandleAvrcpInitConfirm(theAv, (AVRCP_INIT_CFM_T *)message);
                    return;
                default:
                    UnexpectedMessage_HandleMessage(id);
                    return;
            }
        }

        case A2DP_SIGNALLING_CONNECT_IND:
        {
            switch (state)
            {
                case AV_STATE_IDLE:
                    appA2dpSignallingConnectIndicationNew(theAv, (A2DP_SIGNALLING_CONNECT_IND_T *)message);
                    return;
                default:
                    appA2dpRejectA2dpSignallingConnectIndicationNew(theAv, (A2DP_SIGNALLING_CONNECT_IND_T *)message);
                    return;
            }
        }

        case AVRCP_CONNECT_IND:
        {
            switch (state)
            {
                case AV_STATE_IDLE:
                    appAvrcpHandleAvrcpConnectIndicationNew(theAv, (AVRCP_CONNECT_IND_T *)message);
                    return;
                default:
                    appAvrcpRejectAvrcpConnectIndicationNew(theAv, (AVRCP_CONNECT_IND_T *)message);
                    return;
            }
        }

        case AVRCP_BROWSE_CONNECT_IND:
        {
            AvrcpBrowsing_HandleBrowseConnectInd((AVRCP_BROWSE_CONNECT_IND_T *)message);
            return;
        }

        case AV_INTERNAL_VOLUME_STORE_REQ:
        {
            Av_UpdateStoredVolumeForFocussedHandset();
            return;
        }

        case AV_AVRCP_CONNECT_IND:
            appAvHandleAvAvrcpConnectIndication(theAv, (AV_AVRCP_CONNECT_IND_T *)message);
            return;

        case AV_INTERNAL_A2DP_UNROUTED_PREVENT_REPEAT:
            DEBUG_LOG_VERBOSE("AV_INTERNAL_A2DP_UNROUTED_PREVENT_REPEAT expired");
            break;

        default:
            appAvError(theAv, id, message);
            return;
    }
}

static avrcp_play_status Av_GetInstancePlayStatus(avInstanceTaskData *av_instance)
{
    avrcp_play_status status;

    if (av_instance->avrcp.play_status != avrcp_play_status_error)
    {
        status = av_instance->avrcp.play_status;
    }
    else
    {
        status = av_instance->avrcp.play_hint;
    }
    return status;
}

bool Av_IsInstancePlaying(avInstanceTaskData* theInst)
{
    bool playing = FALSE;
    if (theInst && appA2dpIsSinkCodec(theInst) && appA2dpIsConnectedMedia(theInst))
    {
        switch (Av_GetInstancePlayStatus(theInst))
        {
            case avrcp_play_status_playing:
            case avrcp_play_status_fwd_seek:
            case avrcp_play_status_rev_seek:
                playing = TRUE;
                break;
            default:
                break;
        }
    }
    return playing;
}

bool Av_IsPlaying(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;
    bool playing = FALSE;

    for_all_av_instances(theInst, &iterator)
    {
        if (Av_IsInstancePlaying(theInst))
        {
            playing = TRUE;
            break;
        }
    }
    return playing;
}

bool Av_IsInstancePaused(avInstanceTaskData* theInst)
{
    bool paused = FALSE;
    if (theInst && appA2dpIsSinkCodec(theInst) && appA2dpIsConnectedMedia(theInst))
    {
        switch (Av_GetInstancePlayStatus(theInst))
        {
            case avrcp_play_status_stopped:
            case avrcp_play_status_paused:
                paused = TRUE;
            default:
                break;
        }
    }
    return paused;
}

bool Av_IsPaused(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;
    bool paused = TRUE;

    for_all_av_instances(theInst, &iterator)
    {
        if (!Av_IsInstancePaused(theInst))
        {
            paused = FALSE;
            break;
        }
    }
    return paused;
}

/*! \brief Set the play status if the real status is not known

    The AV should know whether we are playing audio, based on AVRCP
    status messages. This information can be missing, in which case
    this function allows you to set a status. It won't override
    a known status.

    \param theInst  the AV instance which should take the hint
    \param status   The status hint to be used
 */
void appAvHintPlayStatus(avInstanceTaskData *theInst, avrcp_play_status status)
{
    if (theInst && appA2dpIsSinkCodec(theInst) && appA2dpIsConnectedMedia(theInst))
    {
        if (theInst->avrcp.play_status == avrcp_play_status_error)
        {
            theInst->avrcp.play_hint = status;
        }
    }
}

void appAvPlayOnHandsetConnection(bool play)
{
    avTaskData *theAv = AvGetTaskData();

    theAv->play_on_connect = play;
}


void appAvConfigStore(void)
{
    /* Cancel any pending messages */
    MessageCancelFirst(appGetAvPlayerTask(), AV_INTERNAL_VOLUME_STORE_REQ);

    /* Store configuration after 5 seconds */
    MessageSendLater(appGetAvPlayerTask(), AV_INTERNAL_VOLUME_STORE_REQ, 0, D_SEC(5));
}

static void av_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group = AV_UI_MESSAGE_GROUP);
    appAvUiClientRegister(task);
}

void Av_ReportChangedLatency(void)
{
    avInstanceTaskData* theInst;
    av_instance_iterator_t iterator;

    for_all_av_instances(theInst, &iterator)
    {
        if (theInst && appA2dpIsSinkCodec(theInst) && appA2dpIsConnectedMedia(theInst))
        {
            uint8 seid = theInst->a2dp.current_seid;
            uint32 latency = Kymera_LatencyManagerGetLatencyForSeidInUs(seid);
            A2dpMediaAvSyncDelayRequest(theInst->a2dp.device_id,
                                        seid,
                                        latency / 100);
            DEBUG_LOG("Av_ReportChangedLatency %dus", latency);
        }
    }
}

void Av_UpdateStoredVolumeForFocussedHandset(void)
{
    audio_source_t source = audio_source_none;
    if (Focus_GetAudioSourceForContext(&source))
    {
        avInstanceTaskData * theInst = Av_GetInstanceForHandsetSource(source);
        if (theInst != NULL)
        {
            av_StoreHandsetVolumeDeviceProperty(theInst);
        }
    }
}

bool Av_RegisterContextProvider(const av_context_provider_if_t *provider_if)
{
    if (context_provider)
    {
        return FALSE;
    }

    context_provider = provider_if;
    return TRUE;
}

void Av_ResetPlayStatus(avInstanceTaskData* av_instance)
{
    av_instance->avrcp.play_status = avrcp_play_status_error;
    av_instance->avrcp.play_hint = avrcp_play_status_error;
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(AV_UI, av_RegisterMessageGroup, NULL);

#endif
