/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief  Application domain HFP dynamic instance management.
*/

#include "aghfp_profile_instance.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile.h"
#include "aghfp_profile_telephony_control.h"
#include "bt_device.h"
#include "device_properties.h"
#include "aghfp.h"
#include "logging.h"
#include "aghfp_profile_audio.h"

#include "connection_manager.h"
#include <device_list.h>
#include <panic.h>
#include <sink.h>

static aghfpTaskData AghfpData = {NULL, 0};
static void aghfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message);

static void AgHfpInstance_AddDeviceHfpInstanceToIterator(device_t device, void * iterator_data)
{
    aghfpInstanceTaskData* aghfp_instance = AghfpProfileInstance_GetInstanceForDevice(device);

    if(aghfp_instance)
    {
        aghfp_instance_iterator_t * iterator = (aghfp_instance_iterator_t *)iterator_data;
        iterator->instances[iterator->index] = aghfp_instance;
        iterator->index++;
    }
}

aghfpInstanceTaskData * AghfpInstance_GetFirst(aghfp_instance_iterator_t * iterator)
{
    memset(iterator, 0, sizeof(aghfp_instance_iterator_t));

    DeviceList_Iterate(AgHfpInstance_AddDeviceHfpInstanceToIterator, iterator);

    iterator->index = 0;

    return iterator->instances[iterator->index];
}

aghfpInstanceTaskData * AghfpInstance_GetNext(aghfp_instance_iterator_t * iterator)
{
    iterator->index++;

    if(iterator->index >= AGHFP_MAX_NUM_INSTANCES)
        return NULL;

    return iterator->instances[iterator->index];
}

static void aghfpProfileInstance_SetInstanceForDevice(device_t device, aghfpInstanceTaskData* instance)
{
    PanicFalse(Device_SetProperty(device, device_property_aghfp_instance, &instance, sizeof(aghfpInstanceTaskData *)));
}

static void aghfpProfileInstance_InitTaskData(aghfpInstanceTaskData* instance)
{
    /* Set up instance task handler */
    instance->task.handler = aghfpProfile_InstanceHandleMessage;

    /* By default, assume remote device supports all HFP standard packet types.
       This is modified when the remote features are read from the device after
       connection. */
    instance->sco_supported_packets = sync_all_sco | sync_ev3 | sync_2ev3 | sync_all_esco;
    /* Move to disconnected state */
    instance->state = AGHFP_STATE_DISCONNECTED;

    PanicNull(AghfpData.aghfp_lib_instance);
    instance->aghfp = AghfpData.aghfp_lib_instance;
    instance->sco_sink = NULL;
    instance->slc_sink = NULL;
    instance->wesco = 0;
    instance->tesco = 0;
    instance->bitfields.call_setup = aghfp_call_setup_none;
    instance->bitfields.call_status = aghfp_call_none;
    instance->bitfields.in_band_ring = FALSE;
    instance->bitfields.caller_id_active_host = FALSE;
    instance->bitfields.caller_id_active_remote = FALSE;
    instance->network_operator = NULL;
    instance->bitfields.call_hold = aghfp_call_held_none;
    AghfpProfileCallList_Initialise(&instance->call_list);
}

/*! \brief Handle connect HFP SLC request
*/
static void aghfpProfileInstance_HandleInternalHfpConnectRequest(const AGHFP_INTERNAL_HFP_CONNECT_REQ_T *req)
{
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForBdaddr(&req->addr);

    PanicNull(instance);

    aghfpState state = AghfpProfile_GetState(instance);

    DEBUG_LOG("aghfpProfileInstance_HandleInternalHfpConnectRequest(%p), enum:aghfpState:%d %04x,%02x,%06lx",
              instance, state, req->addr.nap, req->addr.uap, req->addr.lap);

    switch (state)
    {
        case AGHFP_STATE_DISCONNECTED:
        {
            /* Check ACL is connected */
            if (ConManagerIsConnected(&req->addr))
            {
                instance->hf_bd_addr = req->addr;
                AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTING_LOCAL);
            }
            else
            {
                DEBUG_LOG("aghfpProfileInstance_HandleInternalHfpConnectRequest, no ACL %x,%x,%lx",
                           req->addr.nap, req->addr.uap, req->addr.lap);

                /* Move to 'disconnected' state */
                AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);

                AghfpProfileInstance_Destroy(instance);
            }
        }
        return;
    default:
        DEBUG_LOG("Attempting to connect to profile in invalid state. State enum:aghfpState:%d", state);
        return;
    }
}

/*! \brief Handle request to send ring alert to HF
*/
static void aghfpProfileInstance_HandleInternalHfpRingRequest(AGHFP_INTERNAL_HFP_RING_REQ_T * req)
{
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForBdaddr(&req->addr);

    PanicNull(instance);

    DEBUG_LOG("aghfpProfileInstance_HandleInternalHfpRingRequest(%p)", instance);

    aghfpState state = AghfpProfile_GetState(instance);

    switch (state)
    {
    case AGHFP_STATE_CONNECTED_INCOMING:
        AghfpSendRingAlert(instance->aghfp);

        if(instance->bitfields.caller_id_active_host && instance->bitfields.caller_id_active_remote)
        {
            AghfpSendCallerId(instance->aghfp,
                              instance->clip.clip_type,
                              instance->clip.size_clip_number,
                              instance->clip.clip_number,
                              0,
                              NULL);
        }

        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RING_REQ);
            message->addr = instance->hf_bd_addr;
            MessageSendLater(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RING_REQ, message, D_SEC(RING_PERIOD_IN_SECONDS));
        }

        MAKE_AGHFP_MESSAGE(APP_AGHFP_SCO_INCOMING_RING_IND);
        message->bd_addr = instance->hf_bd_addr;
        TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetStatusNotifyList()), APP_AGHFP_SCO_INCOMING_RING_IND, message);

        break;
    default:
        DEBUG_LOG("Wrong state for ring requesting. State enum:aghfpState:%d", state);
    }
}

/*! \brief Handle request to accept the incoming call
*/
static void aghfpProfileInstance_HandleInternalCallAccept(AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ_T* req)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileInstance_HandleInternalCallAccept(%p)", req->instance);

    aghfpState state = AghfpProfile_GetState(req->instance);

    if (AGHFP_STATE_CONNECTED_INCOMING == state)
    {
        AghfpProfileCallList_AnswerIncomingCall(req->instance->call_list);
        req->instance->bitfields.call_status = aghfp_call_active;
        AghfpProfile_SetState(req->instance, AGHFP_STATE_CONNECTED_ACTIVE);
    }
    else if (req->instance->bitfields.call_setup == aghfp_call_setup_incoming)
    {   
        req->instance->bitfields.call_setup = aghfp_call_setup_none;

        if (req->instance->bitfields.call_status == aghfp_call_active)
        {
            AghfpProfileCallList_HoldActiveCall(req->instance->call_list);
            req->instance->bitfields.call_hold = aghfp_call_held_active;
        }
        else
        {
            req->instance->bitfields.call_status = aghfp_call_active;
        }

        if (req->instance->bitfields.call_hold == aghfp_call_held_remaining)
        {
            req->instance->bitfields.call_hold = aghfp_call_held_active;
        }

        AghfpProfileCallList_AnswerIncomingCall(req->instance->call_list);

        if (req->instance->bitfields.call_hold == aghfp_call_held_active)
        {
            AghfpSendCallHeldIndicator(req->instance->aghfp, req->instance->bitfields.call_hold);
        }
        AghfpSendCallSetupIndicator(req->instance->aghfp, req->instance->bitfields.call_setup);
    }
}

/*! \brief Handle request to reject the incoming call
*/
static void aghfpProfileInstance_HandleInternalCallReject(AGHFP_INTERNAL_HFP_CALL_REJECT_REQ_T* req)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileInstance_HandleInternalCallReject(%p)", req->instance);

    aghfpState state = AghfpProfile_GetState(req->instance);

    AghfpProfileCallList_RejectIncomingCall(req->instance->call_list);

    if (AGHFP_STATE_CONNECTED_INCOMING == state)
    {
        req->instance->bitfields.call_status = aghfp_call_none;
        AghfpProfile_SetState(req->instance, AGHFP_STATE_CONNECTED_IDLE);
    }
    else if (req->instance->bitfields.call_setup == aghfp_call_setup_incoming
        && req->instance->bitfields.call_status == aghfp_call_active)
    {
        req->instance->bitfields.call_setup = aghfp_call_setup_none;
    }
    else if (req->instance->bitfields.call_setup == aghfp_call_setup_incoming
            && req->instance->bitfields.call_status != aghfp_call_active)
    {
        AghfpProfileInstance_Destroy(req->instance);
    }
}

/*! \brief Handle request to end an ongoing call
*/
static void aghfpProfileInstance_HandleInternalCallHangup(AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ_T* req)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileInstance_HandleInternalCallHangup(%p)", req->instance);

    aghfpState state = AghfpProfile_GetState(req->instance);

    AghfpProfileCallList_TerminateActiveCall(req->instance->call_list);

    if (AGHFP_STATE_CONNECTED_ACTIVE == state)
    {
        if (req->instance->bitfields.call_hold == aghfp_call_held_none)
        {
            req->instance->bitfields.call_status = aghfp_call_none;
            AghfpProfile_SetState(req->instance, AGHFP_STATE_CONNECTED_IDLE);
        }
        else
        {
            if (req->instance->bitfields.call_hold == aghfp_call_held_active)
            {
                req->instance->bitfields.call_hold = aghfp_call_held_remaining;
                AghfpSendCallHeldIndicator(req->instance->aghfp, req->instance->bitfields.call_hold);
            }
        }
    }
    else if (req->instance->bitfields.call_status == aghfp_call_active)
    {
        if (req->instance->bitfields.call_hold == aghfp_call_held_none)
        {
            AghfpProfileInstance_Destroy(req->instance);
        }
        else
        {
            req->instance->bitfields.call_hold = aghfp_call_held_remaining;
        }
    }
}

/*! \brief Handle request to disconnect SLC
*/
static void aghfpProfileInstance_HandleInternalDisconnect(AGHFP_INTERNAL_HFP_DISCONNECT_REQ_T* req)
{
    DEBUG_LOG("aghfpProfileInstance_HandleInternalDisconnect(%p)", req->instance);

    switch (AghfpProfile_GetState(req->instance))
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        {
            AghfpProfile_SetState(req->instance, AGHFP_STATE_DISCONNECTING);
        }
        return;

        case AGHFP_STATE_DISCONNECTED:
            return;

        default:
            return;
    }
}

static void aghfpProfileInstance_HandleInternalVoiceDialRequest(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ_T* req)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileInstance_HandleInternalVoiceDialRequest");

    if (AghfpProfile_GetState(req->instance) == AGHFP_STATE_CONNECTED_IDLE)
    {
        AghfpProfile_SetState(req->instance, AGHFP_STATE_CONNECTED_OUTGOING);
    }
}

static void aghfpProfileInstance_HandleInternalHoldActiveCall(AGHFP_INTERNAL_HFP_HOLD_CALL_REQ_T* req)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileInstance_HandleInternalHoldActiveCall");

    aghfpState state = AghfpProfile_GetState(req->instance);

    if (req->instance->bitfields.call_status == aghfp_call_active &&
        req->instance->bitfields.call_hold == aghfp_call_held_none)
    {
        AghfpProfileCallList_HoldActiveCall(req->instance->call_list);

        req->instance->bitfields.call_hold = aghfp_call_held_remaining;

        if (state == AGHFP_STATE_CONNECTED_ACTIVE)
        {
            AghfpSendCallHeldIndicator(req->instance->aghfp, req->instance->bitfields.call_hold);
        }
    }
}

static void aghfpProfileInstance_HandleInternalReleaseHeldCall(AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ_T* req)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileInstance_HandleInternalReleaseHeldCall");

    aghfpState state = AghfpProfile_GetState(req->instance);
    bool end_active_call;

    if (req->instance->bitfields.call_hold != aghfp_call_held_none)
    {
        AghfpProfileCallList_TerminateHeldCall(req->instance->call_list);

        /* If there was only a call being held and no active call */
        end_active_call = (req->instance->bitfields.call_hold == aghfp_call_held_remaining);

        req->instance->bitfields.call_hold = aghfp_call_held_none;

        if (state == AGHFP_STATE_CONNECTED_ACTIVE && !end_active_call)
        {
            /* Active call still in progress - update HF that the held call has been terminated */
            AghfpSendCallHeldIndicator(req->instance->aghfp, req->instance->bitfields.call_hold);
        }
        else if(state == AGHFP_STATE_CONNECTED_ACTIVE && end_active_call)
        {
            /* Active call not in progress - return to connected idle state */
            AghfpProfile_SetState(req->instance, AGHFP_STATE_CONNECTED_IDLE);
        }
        else if(end_active_call)
        {
            /* Not connected to HF and no calls to track so destroy the instance */
            AghfpProfileInstance_Destroy(req->instance);
        }
    }
}

device_t AghfpProfileInstance_FindDeviceFromInstance(aghfpInstanceTaskData* instance)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_aghfp_instance, &instance, sizeof(aghfpInstanceTaskData *));
}

typedef struct voice_source_search_data
{
    /*! The voice source associated with the device to find */
    voice_source_t source_to_find;
    /*! Set to TRUE if a device with the source is found */
    bool source_found;
} voice_source_search_data_t;

static void aghfpProfileInstance_SearchForHandsetWithVoiceSource(device_t device, void * data)
{
    voice_source_search_data_t *search_data = data;
    if ((DeviceProperties_GetVoiceSource(device) == search_data->source_to_find))
    {
        search_data->source_found = TRUE;
    }
}

static voice_source_t aghfpProfileInstance_AllocateVoiceSourceToDevice(aghfpInstanceTaskData *instance)
{
    voice_source_search_data_t search_data = {voice_source_hfp_1, FALSE};
    device_t device = AghfpProfileInstance_FindDeviceFromInstance(instance);
    PanicFalse(device != NULL);

    /* Find a free voice source */
    DeviceList_Iterate(aghfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    if (search_data.source_found)
    {
        /* If hfp_1 has been allocated, try to allocate hfp_2 */
        search_data.source_found = FALSE;
        search_data.source_to_find = voice_source_hfp_2;
        DeviceList_Iterate(aghfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    }
    if (!search_data.source_found)
    {
        /* A free audio_source exists, allocate it to the device with the instance. */
        DeviceProperties_SetVoiceSource(device, search_data.source_to_find);
        DEBUG_LOG_VERBOSE("aghfpProfileInstance_AllocateVoiceSourceToDevice inst(%p) device=%p enum:voice_source_t:%d",
                          instance, device, search_data.source_to_find);
    }
    else
    {
        /* It should be impossible to have connected the HFP profile if we have already
           two connected voice sources for HFP, this may indicate a handle was leaked. */
        Panic();
    }

    return search_data.source_to_find;
}

static void aghfpProfileInstance_IncrementInstanceCount(void)
{
    AghfpData.num_of_instances += 1;
}

static void aghfpProfileInstance_DecrementInstanceCount(void)
{
    if (AghfpData.num_of_instances == 0)
    {
        Panic();
    }

    AghfpData.num_of_instances -= 1;
}


void AghfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    VoiceSources_RegisterAudioInterface(voice_source, AghfpProfile_GetAudioInterface());
    VoiceSources_RegisterTelephonyControlInterface(voice_source, AghfpProfile_GetTelephonyControlInterface());
}

void AghfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    VoiceSources_DeregisterTelephonyControlInterface(voice_source);
}

aghfpInstanceTaskData * AghfpProfileInstance_Create(const bdaddr *bd_addr, bool allocate_source)
{
    voice_source_t new_source = voice_source_none;

    DEBUG_LOG_FN_ENTRY("AghfpProfileInstance_Create");

    PanicFalse(!AghfpProfileInstance_ReachedMaxInstances());

    device_t device = PanicNull(BtDevice_GetDeviceForBdAddr(bd_addr));

    /* Panic if we have a duplicate instance somehow */
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForDevice(device);
    PanicNotNull(instance);

    /* Allocate new instance */
    instance = PanicUnlessNew(aghfpInstanceTaskData);
    memset(instance, 0 , sizeof(*instance));
    aghfpProfileInstance_SetInstanceForDevice(device, instance);

    DEBUG_LOG("AghfpProfileInstance_Create(%p) device=%p", instance, device);

    /* Initialise instance */
    aghfpProfileInstance_InitTaskData(instance);

    /* Set Bluetooth address of remote device */
    instance->hf_bd_addr = *bd_addr;

     if(allocate_source)
     {
         new_source = aghfpProfileInstance_AllocateVoiceSourceToDevice(instance);
         AghfpProfileInstance_RegisterVoiceSourceInterfaces(new_source);
     }

     aghfpProfileInstance_IncrementInstanceCount();

    /* Return pointer to new instance */
    return instance;
}

void AghfpProfileInstance_Destroy(aghfpInstanceTaskData *instance)
{
    DEBUG_LOG("AghfpProfileInstance_Destroy(%p)", instance);
    device_t device = AghfpProfileInstance_FindDeviceFromInstance(instance);

    PanicNull(device);

    /* Destroy instance only if state machine is disconnected and there is no lock pending */
    if (AghfpProfile_IsDisconnected(instance))
    {
        DEBUG_LOG("AghfpProfileInstance_Destroy(%p) permitted", instance);

        MessageFlushTask(&instance->task);

        aghfpProfileInstance_SetInstanceForDevice(device, NULL);
        free(instance);

        if (instance->network_operator)
        {
            free(instance->network_operator);
        }

        AghfpProfileCallList_Destroy(&instance->call_list);

        voice_source_t source = DeviceProperties_GetVoiceSource(device);
        DeviceProperties_RemoveVoiceSource(device);

        AghfpProfileInstance_DeregisterVoiceSourceInterfaces(source);

        aghfpProfileInstance_DecrementInstanceCount();

    }
    else
    {
        DEBUG_LOG("AghfpProfileInstance_Destroy(%p) HFP (%d) not disconnected",
                   instance, !AghfpProfile_IsDisconnected(instance));
    }

}

aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForAghfp(AGHFP *aghfp)
{
    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;
    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->aghfp == aghfp)
        {
            return instance;
        }
    }

    return instance;
}

aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForDevice(device_t device)
{
    aghfpInstanceTaskData** pointer_to_instance;
    size_t size_pointer_to_instance;

    if(device && Device_GetProperty(device, device_property_aghfp_instance, (void**)&pointer_to_instance, &size_pointer_to_instance))
    {
        PanicFalse(size_pointer_to_instance == sizeof(aghfpInstanceTaskData*));
        return *pointer_to_instance;
    }
    DEBUG_LOG_VERBOSE("AghfpProfileInstance_GetInstanceForDevice device=%p has no device_property_hfp_instance", device);
    return NULL;
}

aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForBdaddr(const bdaddr *bd_addr)
{
    aghfpInstanceTaskData* instance = NULL;
    device_t device = NULL;

    PanicNull((void *)bd_addr);

    device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device != NULL)
    {
        instance = AghfpProfileInstance_GetInstanceForDevice(device);
    }

    return instance;
}

/*! \brief Set HFP lock */
void AghfpProfileInstance_SetLock(aghfpInstanceTaskData* instance, uint16 lock)
{
    instance->aghfp_lock = lock;
}

/*! \brief Get HFP lock */
uint16 * AghfpProfileInstance_GetLock(aghfpInstanceTaskData* instance)
{
    return &instance->aghfp_lock;
}

/*! \brief Handle remote support features confirmation
*/
static void aghfpProfile_HandleClDmRemoteFeaturesConfirm(const CL_DM_REMOTE_FEATURES_CFM_T *cfm)
{
    tp_bdaddr bd_addr;
    aghfpInstanceTaskData * instance = NULL;
    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        instance = AghfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);
    }

    PanicNull(instance);

    DEBUG_LOG("aghfpProfile_HandleClDmRemoteFeaturesConfirm(%p)", instance);

    switch (AghfpProfile_GetState(instance))
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_DISCONNECTED:
        {
            if (cfm->status == hci_success)
            {
                uint16 features[PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE] = PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS;
                uint16 packets;
                uint16 index;

                /* Get supported features that both HS & AG support */
                for (index = 0; index < PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE; index++)
                {
                    features[index] &= cfm->features[index];
                }

                /* Calculate SCO packets we should use */
                packets = sync_hv1;
                if (features[0] & REMOTE_FEATURE_HV3)
                    packets |= sync_hv3;
                if (features[0] & REMOTE_FEATURE_HV2)
                    packets |= sync_hv2;

                if (features[1] & REMOTE_FEATURE_EV3)
                    packets |= sync_ev3;
                if (features[2] & REMOTE_FEATURE_EV4)
                    packets |= sync_ev4;
                if (features[2] & REMOTE_FEATURE_EV5)
                    packets |= sync_ev5;
                if (features[2] & REMOTE_FEATURE_2EV3)
                {
                    packets |= sync_2ev3;
                    if (features[2] & REMOTE_FEATURE_2EV5)
                        packets |= sync_2ev5;
                }

                if (features[2] & REMOTE_FEATURE_3EV3)
                {
                    packets |= sync_3ev3;
                    if (features[2] & REMOTE_FEATURE_3EV5)
                        packets |= sync_3ev5;
                }

                instance->sco_supported_packets = packets;

                DEBUG_LOG("aghfpProfile_HandleClDmRemoteFeaturesConfirm(%p), SCO packets %x", instance, packets);
            }
        }
        return;

        default:
            DEBUG_LOG_ERROR("aghfpProfile_HandleClDmRemoteFeaturesConfirm: error retrieving supported remote features");
            return;
    }
}

/*! \brief Message Handler for a specific AGHFP Instance

    This function is the main message handler for the AGHFP instance, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void aghfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
    case AGHFP_INTERNAL_HFP_CONNECT_REQ:
        aghfpProfileInstance_HandleInternalHfpConnectRequest((AGHFP_INTERNAL_HFP_CONNECT_REQ_T *)message);
        return;
    case AGHFP_INTERNAL_HFP_RING_REQ:
        aghfpProfileInstance_HandleInternalHfpRingRequest((AGHFP_INTERNAL_HFP_RING_REQ_T *)message);
        return;
    case AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ:
        aghfpProfileInstance_HandleInternalCallAccept((AGHFP_INTERNAL_HFP_CALL_ACCEPT_REQ_T*)message);
        return;
    case AGHFP_INTERNAL_HFP_CALL_REJECT_REQ:
        aghfpProfileInstance_HandleInternalCallReject((AGHFP_INTERNAL_HFP_CALL_REJECT_REQ_T*)message);
        return;
    case AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ:
        aghfpProfileInstance_HandleInternalCallHangup((AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ_T*)message);
        return;
    case AGHFP_INTERNAL_HFP_DISCONNECT_REQ:
        aghfpProfileInstance_HandleInternalDisconnect((AGHFP_INTERNAL_HFP_DISCONNECT_REQ_T*)message);
        return;
    case AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ:
        aghfpProfileInstance_HandleInternalVoiceDialRequest((AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ_T*)message);
        return;
    case AGHFP_INTERNAL_HFP_HOLD_CALL_REQ:
        aghfpProfileInstance_HandleInternalHoldActiveCall((AGHFP_INTERNAL_HFP_HOLD_CALL_REQ_T*)message);
        break;
   case AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ:
        aghfpProfileInstance_HandleInternalReleaseHeldCall((AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ_T*)message);
        break;
    }

    /* Handle connection library messages */
    switch (id)
    {
        case CL_DM_REMOTE_FEATURES_CFM:
            aghfpProfile_HandleClDmRemoteFeaturesConfirm((CL_DM_REMOTE_FEATURES_CFM_T *)message);
            return;
    }
}

voice_source_t AghfpProfileInstance_GetVoiceSourceForInstance(aghfpInstanceTaskData * instance)
{
    voice_source_t source = voice_source_none;

    PanicNull(instance);

    device_t device = BtDevice_GetDeviceForBdAddr(&instance->hf_bd_addr);
    if (device)
    {
        source = DeviceProperties_GetVoiceSource(device);
    }

    return source;
}

void AghfpProfileInstance_SetAghfp(AGHFP *aghfp)
{
    PanicNotNull(AghfpData.aghfp_lib_instance);
    AghfpData.aghfp_lib_instance = aghfp;
}

bool AghfpProfileInstance_ReachedMaxInstances(void)
{
    return (AghfpData.num_of_instances == AGHFP_MAX_NUM_INSTANCES);
}
