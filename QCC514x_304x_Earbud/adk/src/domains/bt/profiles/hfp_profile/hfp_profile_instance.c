/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain HFP dynamic instance management.
*/

#include "hfp_profile_instance.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "hfp_profile_config.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_states.h"
#include "hfp_profile_telephony_control.h"
#include "hfp_profile_voice_source_link_prio_mapping.h"
#include "hfp_profile_volume.h"
#include "hfp_profile_volume_observer.h"
#include "telephony_messages.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "kymera.h"
#include "ui.h"
#include "hfp_profile.h"

#include <profile_manager.h>
#include <av.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <logging.h>
#include <mirror_profile.h>
#include <message.h>
#include <panic.h>
#include <ps.h>
#include <device_db_serialiser.h>

#include <stdio.h>

#define INDEX_OF_TOTAL_FRAME_COUNTER   2
#define INDEX_OF_ERROR_FRAME_COUNTER   3
#define NUMBER_OF_PARAMS              12

/*! Structure holding counts of previously read good aptx
 *  voice encoded frames and error frames. */
typedef struct
{
    /*! Correct encoded good frame counts */
    uint32 previous_good_frame_counts;

    /*! Error frame counts */
    uint32 previous_error_frame_counts;

} aptx_voice_frames_counts_info_t;

aptx_voice_frames_counts_info_t aptx_voice_frames_counts_info = {0, 0};

/* Local Function Prototypes */
static void hfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message);

#define HFP_MAX_NUM_INSTANCES 2

#define for_all_hfp_instances(hfp_instance, iterator) for(hfp_instance = HfpInstance_GetFirst(iterator); hfp_instance != NULL; hfp_instance = HfpInstance_GetNext(iterator))

static void HfpInstance_AddDeviceHfpInstanceToIterator(device_t device, void * iterator_data)
{
    hfpInstanceTaskData* hfp_instance = HfpProfileInstance_GetInstanceForDevice(device);

    if(hfp_instance)
    {
        hfp_instance_iterator_t * iterator = (hfp_instance_iterator_t *)iterator_data;
        iterator->instances[iterator->index] = hfp_instance;
        iterator->index++;
    }
}

hfpInstanceTaskData * HfpInstance_GetFirst(hfp_instance_iterator_t * iterator)
{
    memset(iterator, 0, sizeof(hfp_instance_iterator_t));

    DeviceList_Iterate(HfpInstance_AddDeviceHfpInstanceToIterator, iterator);

    iterator->index = 0;

    return iterator->instances[iterator->index];
}

hfpInstanceTaskData * HfpInstance_GetNext(hfp_instance_iterator_t * iterator)
{
    iterator->index++;

    if(iterator->index >= HFP_MAX_NUM_INSTANCES)
        return NULL;

    return iterator->instances[iterator->index];
}

static hfp_link_priority hfpProfileInstance_GetLinkForInstance(hfpInstanceTaskData * instance)
{
    hfp_link_priority link = hfp_invalid_link;

    PanicNull(instance);

    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    if (device != NULL)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(device);
        link = HfpLinkPriorityFromBdaddr(&addr);
    }

    DEBUG_LOG_VERBOSE(
        "hfpProfileInstance_GetLinkForInstance instance:%p enum:hfp_link_priority:%d",
        instance, link);

    return link;
}

/*! \brief Handle remote support features confirmation
*/
static void appHfpHandleClDmRemoteFeaturesConfirm(const CL_DM_REMOTE_FEATURES_CFM_T *cfm)
{
    tp_bdaddr bd_addr;
    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);

        PanicNull(instance);
        
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("appHfpHandleClDmRemoteFeaturesConfirm(%p) enum:hfpState:%d", instance, state);

        if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcDisconnectedOrDisconnecting(state))
        {
            if (cfm->status == hci_success)
            {
                uint16 features[PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE] = PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS;
                uint16 packets;
                uint16 index;

                /* Read local supported features to determine SCO packet types */
                PsFullRetrieve(PSKEY_LOCAL_SUPPORTED_FEATURES, &features, PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE);

                /* Get supported features that both HS & AG support */
                for (index = 0; index < PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE; index++)
                {
                    printf("%04x ", features[index]);
                    features[index] &= cfm->features[index];
                }
                printf("");

                /* Calculate SCO packets we should use */
                packets = sync_hv1;
                if (features[0] & 0x2000)
                    packets |= sync_hv3;
                if (features[0] & 0x1000)
                    packets |= sync_hv2;

                /* Only use eSCO for HFP 1.5+ */
                if (instance->profile == hfp_handsfree_profile)
                {
                    if (features[1] & 0x8000)
                        packets |= sync_ev3;
                    if (features[2] & 0x0001)
                        packets |= sync_ev4;
                    if (features[2] & 0x0002)
                        packets |= sync_ev5;
                    if (features[2] & 0x2000)
                    {
                        packets |= sync_2ev3;
                        if (features[2] & 0x8000)
                            packets |= sync_2ev5;
                    }
                    if (features[2] & 0x4000)
                    {
                        packets |= sync_3ev3;
                        if (features[2] & 0x8000)
                            packets |= sync_3ev5;
                    }
                }

                /* Update supported SCO packet types */
                instance->sco_supported_packets = packets;

                DEBUG_LOG("appHfpHandleClDmRemoteFeaturesConfirm(%p), SCO packets %x", instance, packets);
            }
        }
        else
        {
            HfpProfile_HandleError(instance, CL_DM_REMOTE_FEATURES_CFM, cfm);
        }
    }
}

/*! \brief Handle encrypt confirmation
*/
static void appHfpHandleClDmEncryptConfirmation(const CL_SM_ENCRYPT_CFM_T *cfm)
{
    tp_bdaddr bd_addr;
    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);

        PanicNull(instance);
        
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("appHfpHandleClDmEncryptConfirmation(%p) enum:hfpState:%d encypted=%d", instance, state, cfm->encrypted);

        if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
        {
            /* Store encrypted status */
            instance->bitfields.encrypted = cfm->encrypted;

            /* Check if SCO is now encrypted (or not) */
            HfpProfile_CheckEncryptedSco(instance);
        }
        else
        {
            HfpProfile_HandleError(instance, CL_SM_ENCRYPT_CFM, cfm);
        }
    }
}

/*! \brief Handle connect HFP SLC request
*/
static void appHfpHandleInternalHfpConnectRequest(const HFP_INTERNAL_HFP_CONNECT_REQ_T *req)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&req->addr);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpConnectRequest(%p), enum:hfpState:%d %04x,%02x,%06lx",
              instance, state, req->addr.nap, req->addr.uap, req->addr.lap);

    if(HfpProfile_StateIsSlcDisconnected(state))
    {
        if (ConManagerIsConnected(&req->addr))
        {
            /* Store connection flags */
            instance->bitfields.flags = req->flags;

            /* Store AG Bluetooth Address and profile type */
            instance->ag_bd_addr = req->addr;
            instance->profile = req->profile;

            /* Move to connecting local state */
            appHfpSetState(instance, HFP_STATE_CONNECTING_LOCAL);
        }
        else
        {
            DEBUG_LOG("appHfpHandleInternalHfpConnectRequest, no ACL %x,%x,%lx",
                       req->addr.nap, req->addr.uap, req->addr.lap);

            /* Set disconnect reason */
            instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;

            /* Move to 'disconnected' state */
            appHfpSetState(instance, HFP_STATE_DISCONNECTED);

            HfpProfileInstance_Destroy(instance);
        }
    }
    else if(HfpProfile_StateIsSlcDisconnecting(state))
    {
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CONNECT_REQ);

        /* repost the connect message pending final disconnection of the profile
         * via the lock */
        message->addr = req->addr;
        message->profile = req->profile;
        message->flags = req->flags;
        MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 HfpProfileInstance_GetLock(instance));
    }
    else if(HfpProfile_StateIsSlcConnectedOrConnecting(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpConnectRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CONNECT_REQ, req);
    }
}

/*! \brief Handle disconnect HFP SLC request
*/
static void appHfpHandleInternalHfpDisconnectRequest(const HFP_INTERNAL_HFP_DISCONNECT_REQ_T *req)
{
    hfpState state = appHfpGetState(req->instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpDisconnectRequest enum:hfpState:%d", state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        /* Move to disconnecting state */
        appHfpSetState(req->instance, HFP_STATE_DISCONNECTING);
    }
    else if(HfpProfile_StateIsSlcDisconnectedOrDisconnecting(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpDisconnectRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(req->instance, HFP_INTERNAL_HFP_DISCONNECT_REQ, req);
    }
}

/*! \brief Handle last number redial request
*/
static void appHfpHandleInternalHfpLastNumberRedialRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpLastNumberRedialRequest enum:hfpState:%d", state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        if (instance->profile == hfp_headset_profile)
        {
            /* Send button press */
            HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
        else
        {
            HfpDialLastNumberRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpLastNumberRedialRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ, NULL);
    }
}

/*! \brief Handle voice dial request
*/
static void appHfpHandleInternalHfpVoiceDialRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpVoiceDialRequest(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        if (instance->profile == hfp_headset_profile)
        {
            HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
        else
        {
            HfpVoiceRecognitionEnableRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                             instance->bitfields.voice_recognition_request = TRUE);
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpVoiceDialRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_VOICE_DIAL_REQ, NULL);
    }
}

/*! \brief Handle voice dial disable request
*/
static void appHfpHandleInternalHfpVoiceDialDisableRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpVoiceDialDisableRequest(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        if (instance->profile == hfp_headset_profile)
        {
            HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
        else
        {
           HfpVoiceRecognitionEnableRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                            instance->bitfields.voice_recognition_request = FALSE);
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpVoiceDialDisableRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ, NULL);
    }
}

static void appHfpHandleInternalNumberDialRequest(HFP_INTERNAL_NUMBER_DIAL_REQ_T * message)
{
    hfpState state = appHfpGetState(message->instance);
    DEBUG_LOG("appHfpHandleInternalNumberDialRequest(%p) enum:hfpState:%d", message->instance, state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        if (message->instance->profile == hfp_headset_profile)
        {
            HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(message->instance));
        }
        else
        {
            HfpDialNumberRequest(hfpProfileInstance_GetLinkForInstance(message->instance),
                                 message->length, message->number);
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalNumberDialRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(message->instance, HFP_INTERNAL_NUMBER_DIAL_REQ, NULL);
    }
}

/*! \brief Handle accept call request
*/
static void appHfpHandleInternalHfpCallAcceptRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallAcceptRequest(%p) enum:hfpState:%d", instance, state);

    if(state == HFP_STATE_CONNECTED_INCOMING)
    {
        if (instance->profile == hfp_headset_profile)
        {
            HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
        else
        {
            HfpCallAnswerRequest(hfpProfileInstance_GetLinkForInstance(instance), TRUE);
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpCallAcceptRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CALL_ACCEPT_REQ, NULL);
    }
}

/*! \brief Handle reject call request
*/
static void appHfpHandleInternalHfpCallRejectRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallRejectRequest(%p) enum:hfpState:%d", instance, state);

    if(state == HFP_STATE_CONNECTED_INCOMING)
    {
        if (instance->profile == hfp_headset_profile)
        {
            Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
        }
        else
        {
            HfpCallAnswerRequest(hfpProfileInstance_GetLinkForInstance(instance), FALSE);
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpCallRejectRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CALL_REJECT_REQ, NULL);
    }
}

/*! \brief Handle hangup call request
*/
static void appHfpHandleInternalHfpCallHangupRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallHangupRequest(%p) enum:hfpState:%d", instance, state);

    if(state == HFP_STATE_CONNECTED_ACTIVE || state == HFP_STATE_CONNECTED_OUTGOING)
    {
        if (instance->profile == hfp_headset_profile)
        {
            HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
        else
        {
            HfpCallTerminateRequest(hfpProfileInstance_GetLinkForInstance(instance));
        }
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpCallHangupRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CALL_HANGUP_REQ, NULL);
    }
}

/*! \brief Handle mute/unmute request
*/
static void appHfpHandleInternalHfpMuteRequest(const HFP_INTERNAL_HFP_MUTE_REQ_T *req)
{
    hfpState state = appHfpGetState(req->instance);

    DEBUG_LOG("appHfpHandleInternalHfpMuteRequest(%p) enum:hfpState:%d", req->instance, state);

    if(HfpProfile_StateHasActiveCall(state))
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(req->instance);
        if (req->mute)
        {
            Telephony_NotifyMicrophoneMuted(source);
        }
        else
        {
            Telephony_NotifyMicrophoneUnmuted(source);
        }

        /* Set mute flag */
        req->instance->bitfields.mute_active = req->mute;

        /* Re-configure audio chain */
        appKymeraScoMicMute(req->mute);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpMuteRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(req->instance, HFP_INTERNAL_HFP_MUTE_REQ, NULL);
    }
}

static hfp_audio_transfer_direction hfpProfile_GetVoiceSourceHfpDirection(voice_source_audio_transfer_direction_t direction)
{
    if ((direction < voice_source_audio_transfer_to_hfp) || (direction > voice_source_audio_transfer_toggle))
    {
        DEBUG_LOG_ERROR("hfpProfile_GetVoiceSourceHfpDirection Invalid direction");
        Panic();
    }

    if(direction == voice_source_audio_transfer_to_hfp)
    {
        return hfp_audio_to_hfp;
    }
    else if(direction == voice_source_audio_transfer_to_ag)
    {
        return hfp_audio_to_ag;
    }
    else
    {
        return hfp_audio_transfer;
    }
}


/*! \brief Handle audio transfer request
*/
static void appHfpHandleInternalHfpTransferRequest(const HFP_INTERNAL_HFP_TRANSFER_REQ_T *req)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(req->source);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpTransferRequest state enum:hfpState:%u direction enum:voice_source_audio_transfer_direction_t:%d",
               state,
               req->direction);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        HfpAudioTransferRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                hfpProfile_GetVoiceSourceHfpDirection(req->direction),
                                instance->sco_supported_packets  ^ sync_all_edr_esco,
                                NULL);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpTransferRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_TRANSFER_REQ, NULL);
    }
}

/*! \brief Handle HSP incoming call timeout

    We have had a ring indication for a while, so move back to 'connected
    idle' state.
*/
static void appHfpHandleHfpHspIncomingTimeout(hfpInstanceTaskData* instance)
{
    DEBUG_LOG("appHfpHandleHfpHspIncomingTimeout(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            /* Move back to connected idle state */
            appHfpSetState(instance, HFP_STATE_CONNECTED_IDLE);
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HSP_INCOMING_TIMEOUT, NULL);
            return;
    }
}

static void appHfpHandleInternalOutOfBandRingtoneRequest(hfpInstanceTaskData * instance)
{
    if(HfpProfile_StateHasIncomingCall(appHfpGetState(instance)))
    {
        Telephony_NotifyCallIncomingOutOfBandRingtone(HfpProfileInstance_GetVoiceSourceForInstance(instance));

        MESSAGE_MAKE(msg, HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ_T);
        msg->instance = instance;
        MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ);
        MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ, msg, D_SEC(5));
    }
}

static void appHfpHandleInternalReleaseWaitingRejectIncomingRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalReleaseWaitingRejectIncomingRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasActiveAndIncomingCall(state) || HfpProfile_StateHasHeldCall(state))
    {
        hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
        HfpCallHoldActionRequest(link, hfp_chld_release_held_reject_waiting, 0);
    }
}

static void appHfpHandleInternalAcceptWaitingReleaseActiveRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalAcceptWaitingReleaseActiveRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasEstablishedCall(state))
    {
        hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
        HfpCallHoldActionRequest(link, hfp_chld_release_active_accept_other, 0);
    }
}

static void appHfpHandleInternalAcceptWaitingHoldActiveRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalAcceptWaitingHoldActiveRequest %p enum:hfpState:%d", instance, state);
    
    /* Allow in active or held call states to enable putting an active call on hold or resuming held call */
    if(HfpProfile_StateHasEstablishedCall(state))
    {
        hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
        HfpCallHoldActionRequest(link, hfp_chld_hold_active_accept_other, 0);
    }
}

static void appHfpHandleInternalAddHeldToMultipartyRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalAddHeldToMultipartyRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasMultipleCalls(state))
    {
        hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
        HfpCallHoldActionRequest(link, hfp_chld_add_held_to_multiparty, 0);
    }
}

static void appHfpHandleInternalJoinCallsAndHangUpRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalJoinCallsAndHangUpRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasMultipleCalls(state))
    {
        hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
        HfpCallHoldActionRequest(link, hfp_chld_join_calls_and_hang_up, 0);
    }
}

static void hfpProfileInstance_InitTaskData(hfpInstanceTaskData* instance)
{
    /* Set up instance task handler */
    instance->task.handler = hfpProfile_InstanceHandleMessage;

    /* By default, assume remote device supports all HFP standard packet types.
       This is modified when the remote features are read from the device after
       connection. */
    instance->sco_supported_packets = sync_all_sco | sync_ev3 | sync_2ev3;

    /* Initialise state */
    instance->sco_sink = 0;
    instance->hfp_lock = 0;
    instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
    instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;
    instance->bitfields.call_accepted = FALSE;
    instance->codec = hfp_wbs_codec_mask_none;
    instance->wesco = 0;
    instance->tesco = 0;
    instance->qce_codec_mode_id = CODEC_MODE_ID_UNSUPPORTED;

    /* Move to disconnected state */
    instance->state = HFP_STATE_DISCONNECTED;
}

static void hfpProfile_BlockHandsetForSwb(const bdaddr *bd_addr)
{
    device_t device  = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        flags |= DEVICE_FLAGS_SWB_NOT_SUPPORTED;
        Device_SetPropertyU16(device, device_property_flags, flags);
        DeviceDbSerialiser_SerialiseDevice(device);
        DEBUG_LOG("hfpProfile_BlockHandsetForSwb:Handset blocked for swb");
    }
}

static bool appHfpAptxVoiceCurrentGoodFramesCountIsSameAsPreviousGoodFramesCount(uint32 current_good_frame_counts)
{
    return ((current_good_frame_counts - aptx_voice_frames_counts_info.previous_good_frame_counts) == 0);
}

static bool appHfpAptxVoiceErrorFramesCountIncreasedFromPreviousErrorFramesCount(uint32 current_error_frame_counts)
{
     return ((current_error_frame_counts - aptx_voice_frames_counts_info.previous_error_frame_counts) > 0);
}

static bool appHfpAptxVoiceFrameCountersAreNotOk(uint32 current_good_frame_counts, uint32 current_error_frame_counts)
{
    bool aptx_voice_frame_counters_not_ok = FALSE;

    if (appHfpAptxVoiceCurrentGoodFramesCountIsSameAsPreviousGoodFramesCount(current_good_frame_counts) &&
        appHfpAptxVoiceErrorFramesCountIncreasedFromPreviousErrorFramesCount(current_error_frame_counts))
    {
        aptx_voice_frame_counters_not_ok = TRUE;
    }
    return aptx_voice_frame_counters_not_ok;
}

static bool appHfpHandleNoAudioInSwbCall(get_status_data_t * operator_status)
{
    /* OPR_SCO_RECEIVE operator decodes aptX voice packets in swb call.In no audio swb call scenario,
     * good frames count will not increase but error frames count will keep increasing.Good frames count
     * is calculated by substracting error frames count(fourth word in operator status data) from total
     * frame counts(third word in operator status data). */

      bool swb_call_no_audio = FALSE;
      uint32 current_total_frame_counts = operator_status->value[INDEX_OF_TOTAL_FRAME_COUNTER];
      uint32 current_error_frame_counts = operator_status->value[INDEX_OF_ERROR_FRAME_COUNTER];

      uint32 current_good_frame_counts = current_total_frame_counts - current_error_frame_counts;

    if(appHfpAptxVoiceFrameCountersAreNotOk(current_good_frame_counts, current_error_frame_counts))
    {
        swb_call_no_audio = TRUE;
    }

    aptx_voice_frames_counts_info.previous_good_frame_counts = current_good_frame_counts;
    aptx_voice_frames_counts_info.previous_error_frame_counts = current_error_frame_counts;

    DEBUG_LOG("appHfpHandleNoAudioInSwbCall:%d",swb_call_no_audio);
    return swb_call_no_audio;
}

void HfpProfileInstance_ResetAptxVoiceFrameCounts(void)
{
    aptx_voice_frames_counts_info.previous_good_frame_counts = 0;
    aptx_voice_frames_counts_info.previous_error_frame_counts = 0;
}

static void appHfpRecheckAptxVoicePacketsCounterAfterSometime(HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ_T * msg)
{
    MAKE_HFP_MESSAGE(HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ);
    message->instance = msg->instance;
    MessageSendLater(&(message->instance->task), HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ, message, HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS);
}

static bool appHfpSwbCallActive(hfp_call_state call_state, uint16 qce_codec_mode_id)
{
    return ((call_state == hfp_call_state_active) && HfpProfile_HandsetSupportsSuperWideband(qce_codec_mode_id));
}

void HfpProfileInstance_StartCheckingAptxVoicePacketsCounterImmediatelyIfSwbCallActive(void)
{
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        if(appHfpSwbCallActive(instance->bitfields.call_state, instance->qce_codec_mode_id))
        {
            DEBUG_LOG("HfpProfileInstance_StartCheckingAptxVoicePacketsCounterImmediatelyIfSwbCallActive:(%p)Handover in swb call",instance);
            MAKE_HFP_MESSAGE(HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ);
            message->instance = instance;
            MessageSend(&(message->instance->task), HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ, message);
            return;
        }
    }
}

static void appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter(HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ_T * msg)
{
    get_status_data_t * operator_status = Kymera_GetOperatorStatusDataInScoChain(OPR_SCO_RECEIVE, NUMBER_OF_PARAMS);

    /* It may be possible that sco chain is not loaded yet and we may be trying to read OPR_SCO_RECEIVE
     * status data earlier which will fail. So we will read it again after HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS delay. */

    if (operator_status == NULL)
    {
        appHfpRecheckAptxVoicePacketsCounterAfterSometime(msg);
        return;
    }

    DEBUG_LOG_VERBOSE("appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter, result=%d, num of params=%d",
                       operator_status->result,operator_status->number_of_params);

    for(uint8 index=0;index<operator_status->number_of_params;index++)
    {
       DEBUG_LOG("%d ",operator_status->value[index]);
    }

    if (appHfpHandleNoAudioInSwbCall(operator_status))
    {
         DEBUG_LOG_VERBOSE("appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter:No Swb Audio Detected.Disconnecting SLC.");
         hfpProfile_BlockHandsetForSwb(&(msg->instance->ag_bd_addr));

         /* Disconnect slc to re negotiate hfp codec again and will reconnect once slc disconnect is complete. */
         msg->instance->bitfields.reconnect_handset = TRUE;
         hfpProfile_Disconnect(&(msg->instance->ag_bd_addr));
    }
    else
    {
         DEBUG_LOG_VERBOSE("appHfpHandleInternalHfpMonitorAptxVoicePacketsCountere:Swb packets ok.");
         appHfpRecheckAptxVoicePacketsCounterAfterSometime(msg);
    }

    free(operator_status);
}

/*! \brief Message Handler for a specific HFP Instance

    This function is the main message handler for the HFP instance, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void hfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message)
{
    DEBUG_LOG("hfpProfile_InstanceHandleMessage id 0x%x", id);

    /* Handle internal messages */
    switch (id)
    {
        case HFP_INTERNAL_HSP_INCOMING_TIMEOUT:
            appHfpHandleHfpHspIncomingTimeout(((HFP_INTERNAL_HSP_INCOMING_TIMEOUT_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CONNECT_REQ:
            appHfpHandleInternalHfpConnectRequest((HFP_INTERNAL_HFP_CONNECT_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_DISCONNECT_REQ:
            appHfpHandleInternalHfpDisconnectRequest((HFP_INTERNAL_HFP_DISCONNECT_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ:
            appHfpHandleInternalHfpLastNumberRedialRequest(((HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_VOICE_DIAL_REQ:
            appHfpHandleInternalHfpVoiceDialRequest(((HFP_INTERNAL_HFP_VOICE_DIAL_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ:
            appHfpHandleInternalHfpVoiceDialDisableRequest(((HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CALL_ACCEPT_REQ:
            appHfpHandleInternalHfpCallAcceptRequest(((HFP_INTERNAL_HFP_CALL_ACCEPT_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CALL_REJECT_REQ:
            appHfpHandleInternalHfpCallRejectRequest(((HFP_INTERNAL_HFP_CALL_REJECT_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_CALL_HANGUP_REQ:
            appHfpHandleInternalHfpCallHangupRequest(((HFP_INTERNAL_HFP_CALL_HANGUP_REQ_T *)message)->instance);
            return;

        case HFP_INTERNAL_HFP_MUTE_REQ:
            appHfpHandleInternalHfpMuteRequest((HFP_INTERNAL_HFP_MUTE_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_TRANSFER_REQ:
            appHfpHandleInternalHfpTransferRequest((HFP_INTERNAL_HFP_TRANSFER_REQ_T *)message);
            return;

        case HFP_INTERNAL_NUMBER_DIAL_REQ:
            appHfpHandleInternalNumberDialRequest((HFP_INTERNAL_NUMBER_DIAL_REQ_T *)message);
            return;

        case HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ:
              appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter((HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ_T *)message);
              return;

        case HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ:
            appHfpHandleInternalOutOfBandRingtoneRequest(((HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ_T *)message)->instance);
            return;
        
        case HFP_INTERNAL_HFP_RELEASE_WAITING_REJECT_INCOMING_REQ:
            appHfpHandleInternalReleaseWaitingRejectIncomingRequest(hfpInstanceFromTask(task));
            return;
            
        case HFP_INTERNAL_HFP_ACCEPT_WAITING_RELEASE_ACTIVE_REQ:
            appHfpHandleInternalAcceptWaitingReleaseActiveRequest(hfpInstanceFromTask(task));
            return;
    
        case HFP_INTERNAL_HFP_ACCEPT_WAITING_HOLD_ACTIVE_REQ:
            appHfpHandleInternalAcceptWaitingHoldActiveRequest(hfpInstanceFromTask(task));
            return;
            
        case HFP_INTERNAL_HFP_ADD_HELD_TO_MULTIPARTY_REQ:
            appHfpHandleInternalAddHeldToMultipartyRequest(hfpInstanceFromTask(task));
            return;
        
        case HFP_INTERNAL_HFP_JOIN_CALLS_AND_HANG_UP:
            appHfpHandleInternalJoinCallsAndHangUpRequest(hfpInstanceFromTask(task));
            return;

    }

    /* Handle connection library messages */
    switch (id)
    {
        case CL_DM_REMOTE_FEATURES_CFM:
            appHfpHandleClDmRemoteFeaturesConfirm((CL_DM_REMOTE_FEATURES_CFM_T *)message);
            return;

        case CL_SM_ENCRYPT_CFM:
            appHfpHandleClDmEncryptConfirmation((CL_SM_ENCRYPT_CFM_T *)message);
            return;
   }
}

device_t HfpProfileInstance_FindDeviceFromInstance(hfpInstanceTaskData* instance)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_hfp_instance, &instance, sizeof(hfpInstanceTaskData *));
}

static void hfpProfileInstance_SetInstanceForDevice(device_t device, hfpInstanceTaskData* instance)
{
    PanicFalse(Device_SetProperty(device, device_property_hfp_instance, &instance, sizeof(hfpInstanceTaskData *)));
}

typedef struct voice_source_search_data
{
    /*! The voice source associated with the device to find */
    voice_source_t source_to_find;
    /*! Set to TRUE if a device with the source is found */
    bool source_found;
} voice_source_search_data_t;

static void hfpProfileInstance_SearchForHandsetWithVoiceSource(device_t device, void * data)
{
    voice_source_search_data_t *search_data = data;
    if ((DeviceProperties_GetVoiceSource(device) == search_data->source_to_find) &&
        (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET))
    {
        search_data->source_found = TRUE;
    }
}

static voice_source_t hfpProfileInstance_AllocateVoiceSourceToDevice(hfpInstanceTaskData *instance)
{
    voice_source_search_data_t search_data = {voice_source_hfp_1, FALSE};
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    PanicFalse(device != NULL);

    /* Find a free voice source */
    DeviceList_Iterate(hfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    if (search_data.source_found)
    {
        /* If hfp_1 has been allocated, try to allocate hfp_2 */
        search_data.source_found = FALSE;
        search_data.source_to_find = voice_source_hfp_2;
        DeviceList_Iterate(hfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    }
    if (!search_data.source_found)
    {
        /* A free audio_source exists, allocate it to the device with the instance. */
        DeviceProperties_SetVoiceSource(device, search_data.source_to_find);
        DEBUG_LOG_VERBOSE("hfpProfileInstance_AllocateVoiceSourceToDevice inst(%p) device=%p enum:voice_source_t:%d",
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

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForDevice(device_t device)
{
    hfpInstanceTaskData** pointer_to_instance;
    size_t size_pointer_to_instance;

    if(device && Device_GetProperty(device, device_property_hfp_instance, (void**)&pointer_to_instance, &size_pointer_to_instance))
    {
        PanicFalse(size_pointer_to_instance == sizeof(hfpInstanceTaskData*));
        return *pointer_to_instance;
    }
    DEBUG_LOG_VERBOSE("HfpProfileInstance_GetInstanceForDevice device=%p has no device_property_hfp_instance", device);
    return NULL;
}

/*! \brief Get HFP lock */
uint16 * HfpProfileInstance_GetLock(hfpInstanceTaskData* instance)
{
    return &instance->hfp_lock;
}

/*! \brief Set HFP lock */
void HfpProfileInstance_SetLock(hfpInstanceTaskData* instance, uint16 lock)
{
    instance->hfp_lock = lock;
}

/*! \brief Is HFP SCO/ACL encrypted */
bool HfpProfileInstance_IsEncrypted(hfpInstanceTaskData* instance)
{
    return instance->bitfields.encrypted;
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForBdaddr(const bdaddr *bd_addr)
{
    hfpInstanceTaskData* instance = NULL;
    device_t device = NULL;

    PanicNull((void *)bd_addr);

    device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device != NULL)
    {
        instance = HfpProfileInstance_GetInstanceForDevice(device);
    }

    return instance;
}

device_t HfpProfileInstance_FindDeviceFromVoiceSource(voice_source_t source)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(voice_source_t));
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForSource(voice_source_t source)
{
    hfpInstanceTaskData* instance = NULL;

    if (source != voice_source_none)
    {
        device_t device = HfpProfileInstance_FindDeviceFromVoiceSource(source);

        if (device != NULL)
        {
            instance = HfpProfileInstance_GetInstanceForDevice(device);
        }
    }

    DEBUG_LOG_V_VERBOSE("HfpProfileInstance_GetInstanceForSource(%p) enum:voice_source_t:%d",
                         instance, source);

    return instance;
}

voice_source_t HfpProfileInstance_GetVoiceSourceForInstance(hfpInstanceTaskData * instance)
{
    voice_source_t source = voice_source_none;

    PanicNull(instance);

    device_t device = BtDevice_GetDeviceForBdAddr(&instance->ag_bd_addr);
    if (device)
    {
        source = DeviceProperties_GetVoiceSource(device);
    }

    return source;
}

void HfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    /* Register voice source interfaces implementated by HFP profile */
    VoiceSources_RegisterAudioInterface(voice_source, HfpProfile_GetAudioInterface());
    VoiceSources_RegisterTelephonyControlInterface(voice_source, HfpProfile_GetTelephonyControlInterface());
    VoiceSources_RegisterObserver(voice_source, HfpProfile_GetVoiceSourceObserverInterface());
}

void HfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    //VoiceSources_RegisterAudioInterface(source, NULL);
    VoiceSources_DeregisterTelephonyControlInterface(voice_source);
    //VoiceSources_RegisterVolume(source, NULL);
    VoiceSources_DeregisterObserver(voice_source, HfpProfile_GetVoiceSourceObserverInterface());
}

hfpInstanceTaskData * HfpProfileInstance_Create(const bdaddr *bd_addr, bool allocate_source)
{
    voice_source_t new_source = voice_source_none;

    DEBUG_LOG_FN_ENTRY("HfpProfileInstance_Create");

    device_t device = PanicNull(BtDevice_GetDeviceForBdAddr(bd_addr));

    /* Panic if we have a duplicate instance somehow */
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForDevice(device);
    PanicNotNull(instance);

    /* Allocate new instance */
    instance = PanicUnlessNew(hfpInstanceTaskData);
    memset(instance, 0 , sizeof(*instance));
    hfpProfileInstance_SetInstanceForDevice(device, instance);

    DEBUG_LOG("HfpProfileInstance_Create(%p) device=%p", instance, device);

    /* Initialise instance */
    hfpProfileInstance_InitTaskData(instance);

    /* Set Bluetooth address of remote device */
    instance->ag_bd_addr = *bd_addr;

    /* initialise the routed state */
    instance->source_state = source_state_disconnected;

    if (appDeviceIsHandset(bd_addr))
    {
        if(allocate_source)
        {
            new_source = hfpProfileInstance_AllocateVoiceSourceToDevice(instance);
            HfpProfileInstance_RegisterVoiceSourceInterfaces(new_source);
        }
    }
    else
    {
        Panic(); /* Unexpected device type */
    }

    /* Return pointer to new instance */
    return instance;
}

/*! \brief Destroy HFP instance

    This function should only be called if the instance no longer has HFP connected.
    If HFP is still connected, the function will silently fail.

    The function will panic if the instance is not valid, or if the instance
    is already destroyed.

    \param  instance The instance to destroy

*/
void HfpProfileInstance_Destroy(hfpInstanceTaskData *instance)
{
    DEBUG_LOG("HfpProfileInstance_Destroy(%p)", instance);
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);

    PanicNull(device);

    /* Destroy instance only if state machine is disconnected. */
    if (HfpProfile_IsDisconnected(instance))
    {
        DEBUG_LOG("HfpProfileInstance_Destroy(%p) permitted", instance);

        /* Flush any messages still pending delivery */
        MessageFlushTask(&instance->task);

        /* Clear entry and free instance */
        hfpProfileInstance_SetInstanceForDevice(device, NULL);
        free(instance);

        voice_source_t source = DeviceProperties_GetVoiceSource(device);
        DeviceProperties_RemoveVoiceSource(device);

        /* Deregister voice source interfaces that were implementated by the instance. */
        HfpProfileInstance_DeregisterVoiceSourceInterfaces(source);
    }
    else
    {
        DEBUG_LOG("HfpProfileInstance_Destroy(%p) HFP (%d) not disconnected, or HFP Lock Pending",
                   instance, !HfpProfile_IsDisconnected(instance));
    }
}


