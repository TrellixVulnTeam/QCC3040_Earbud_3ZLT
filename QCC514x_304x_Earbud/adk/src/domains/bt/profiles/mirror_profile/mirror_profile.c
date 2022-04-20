/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to mirror ACL & eSCO connections.
*/

#include <stdlib.h>

#include <vm.h>
#include <message.h>

#include "av.h"
#include "av_instance.h"
#include "bt_device.h"
#include "connection_manager.h"
#include "device.h"
#include "device_list.h"
#include "device_properties.h"
#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "kymera.h"
#include "kymera_adaptation.h"
#include "peer_signalling.h"
#include "volume_messages.h"
#include "telephony_messages.h"
#include "a2dp_profile_sync.h"
#include "a2dp_profile_audio.h"
#include "audio_sync.h"
#include "voice_sources.h"
#include "qualcomm_connection_manager.h"
#include "focus_audio_source.h"
#include "focus_generic_source.h"
#include "key_sync.h"


#ifdef INCLUDE_MIRRORING

#include "mirror_profile.h"
#include "mirror_profile_signalling.h"
#include "mirror_profile_typedef.h"
#include "mirror_profile_marshal_typedef.h"
#include "mirror_profile_private.h"
#include "mirror_profile_mdm_prim.h"
#include "mirror_profile_sm.h"
#include "mirror_profile_audio_source.h"
#include "mirror_profile_voice_source.h"
#include "mirror_profile_peer_audio_sync_l2cap.h"
#include "mirror_profile_peer_mode_sm.h"
#include "timestamp_event.h"

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(MIRROR_PROFILE, MIRROR_PROFILE_MESSAGE_END)

#endif

#define mirrorProfile_IsHandsetConnected() appDeviceIsBredrHandsetConnected()

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(mirror_profile_msg_t)
LOGGING_PRESERVE_MESSAGE_TYPE(mirror_profile_internal_msg_t)

mirror_profile_task_data_t mirror_profile;

/*! \brief Reset mirror SCO connection state */
void MirrorProfile_ResetEscoConnectionState(void)
{
    uint8 volume = 0;
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForDevice(MirrorProfile_GetMirroredDevice());
    if (instance != NULL)
    {
        volume = appHfpGetVolume(instance);
    }

    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    sp->esco.conn_handle = MIRROR_PROFILE_CONNECTION_HANDLE_INVALID;
    sp->esco.codec_mode = hfp_codec_mode_none;
    sp->esco.wesco = 0;
    sp->esco.volume = volume;
}

/* \brief Set the local SCO audio volume */
void MirrorProfile_SetScoVolume(voice_source_t source, uint8 volume)
{
    mirror_profile_esco_t *esco = MirrorProfile_GetScoState();

    MIRROR_LOG("mirrorProfile_SetLocalVolume enum:voice_source_t:%d vol %u old_vol %u", source, volume, esco->volume);

    assert(!MirrorProfile_IsPrimary());

    if (volume != esco->volume)
    {
        esco->volume = volume;
        Volume_SendVoiceSourceVolumeUpdateRequest(source, event_origin_peer, volume);
    }
}

/*\! brief Set the local SCO codec params */
void MirrorProfile_SetScoCodec(hfp_codec_mode_t codec_mode)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();

    MIRROR_LOG("MirrorProfile_SetScoCodec codec_mode 0x%x", codec_mode);

    /* \todo Store the params as hfp params? That may actually be the best way w.r.t.
             handover as well? */
    sp->esco.codec_mode = codec_mode;
}

/*
    External notification helpers
*/

void MirrorProfile_SendAclConnectInd(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    MESSAGE_MAKE(ind, MIRROR_PROFILE_CONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    TaskList_MessageSend(sp->client_tasks, MIRROR_PROFILE_CONNECT_IND, ind);
}

void MirrorProfile_SendAclDisconnectInd(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    MESSAGE_MAKE(ind, MIRROR_PROFILE_DISCONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    /* \todo propagate disconnect reason */
    ind->reason = hci_error_unspecified;
    TaskList_MessageSend(sp->client_tasks, MIRROR_PROFILE_DISCONNECT_IND, ind);
}

void MirrorProfile_SendScoConnectInd(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    MESSAGE_MAKE(ind, MIRROR_PROFILE_ESCO_CONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    TaskList_MessageSend(sp->client_tasks, MIRROR_PROFILE_ESCO_CONNECT_IND, ind);
}

void MirrorProfile_SendScoDisconnectInd(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    MESSAGE_MAKE(ind, MIRROR_PROFILE_ESCO_DISCONNECT_IND_T);
    BdaddrTpFromBredrBdaddr(&ind->tpaddr, &sp->acl.bd_addr);
    /* \todo propagate disconnect reason */
    ind->reason = hci_error_unspecified;
    TaskList_MessageSend(sp->client_tasks, MIRROR_PROFILE_ESCO_DISCONNECT_IND, ind);
}

void MirrorProfile_SendA2dpStreamActiveInd(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    TaskList_MessageSendId(sp->client_tasks, MIRROR_PROFILE_A2DP_STREAM_ACTIVE_IND);
}

void MirrorProfile_SendA2dpStreamInactiveInd(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    TaskList_MessageSendId(sp->client_tasks, MIRROR_PROFILE_A2DP_STREAM_INACTIVE_IND);
}

static device_t mirrorProfile_GetTargetA2dpDevice(void)
{
    audio_source_t source;
    device_t target_device = NULL;
    generic_source_t routed_source = Focus_GetFocusedGenericSourceForAudioRouting();

    if (GenericSource_IsAudio(routed_source))
    {
       if (routed_source.u.audio == audio_source_a2dp_1 || routed_source.u.audio == audio_source_a2dp_2)
       {
           source = routed_source.u.audio;
           target_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_audio_source, &source, sizeof(source));
       }
    }

    MIRROR_LOG("mirrorProfile_GetTargetA2dpDevice focused_src=(enum:source_type_t:%d, %d) target_device=%p",
               routed_source.type, routed_source.u.audio, target_device);

    return target_device;
}

static bool mirrorProfile_UpdateTargetDevice(void)
{
    device_t target_device = NULL;
    generic_source_t routed_source = Focus_GetFocusedGenericSourceForAudioRouting();

    if (GenericSource_IsVoice(routed_source))
    {
        voice_source_t source;
        if (routed_source.u.voice == voice_source_hfp_1 || routed_source.u.voice == voice_source_hfp_2)
        {
           source = routed_source.u.voice;
           target_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(source));
        }
    }
    else if (GenericSource_IsAudio(routed_source))
    {
        audio_source_t source;
        if (routed_source.u.audio == audio_source_a2dp_1 || routed_source.u.audio == audio_source_a2dp_2)
        {
           source = routed_source.u.audio;
           target_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_audio_source, &source, sizeof(source));
        }
        else if(routed_source.u.audio == audio_source_le_audio_broadcast)
        {
            target_device = BtDevice_GetMruDevice();
        }
    }

    MIRROR_LOG("mirrorProfile_UpdateTargetDevice focused_src=(enum:source_type_t:%d, %d) target_device=0x%p",
               routed_source.type, routed_source.u.audio, target_device);

    MirrorProfile_SetTargetDevice(target_device);
    return (target_device != NULL);
}

bool MirrorProfile_IsHandsetSwitchRequired(void)
{
    device_t target = MirrorProfile_GetTargetDevice();
    device_t mirrored = MirrorProfile_GetMirroredDevice();
    MIRROR_LOG("MirrorProfile_IsHandsetSwitchRequired target=0x%p mirrored=0x%p", target, mirrored);
    return (MirrorProfile_IsAclConnected() && target != mirrored);
}

/*
    Message handling functions
*/

/*! \brief Inspect profile and internal state and decide the target state. */
void MirrorProfile_SetTargetStateFromProfileState(void)
{
    mirror_profile_state_t target = MIRROR_PROFILE_STATE_DISCONNECTED;

    if (MirrorProfile_IsPrimary())
    {
        if (appPeerSigIsConnected() &&
            MirrorProfile_IsAudioSyncL2capConnected() &&
            mirrorProfile_IsHandsetConnected() &&
            MirrorProfile_IsQhsReady() &&
            mirrorProfile_UpdateTargetDevice() &&
            KeySync_IsDeviceInSync(MirrorProfile_GetTargetDevice()))
        {
            if (MirrorProfile_IsHandsetSwitchRequired())
            {
/* This requires new support in BTSS and appsP0 */
#ifdef MIRROR_PROFILE_ACL_SWITCH
                target = MIRROR_PROFILE_STATE_SWITCH;
#endif
            }
            else
            {
                if (MirrorProfile_IsAclConnected())
                {
                    device_t device = MirrorProfile_GetMirroredDevice();
                    voice_source_t voice_source = MirrorProfile_GetVoiceSource();
                    hfpInstanceTaskData *instance = HfpProfileInstance_GetInstanceForDevice(device);
                    /* SCO has higher priority than A2DP */
                    if (instance && HfpProfile_IsScoActiveForInstance(instance) &&
                        MirrorProfile_IsEscoMirroringEnabled() && MirrorProfile_IsVoiceSourceSupported(voice_source))
                    {
                        target = MIRROR_PROFILE_STATE_ESCO_CONNECTED;
                    }
                }


                if (target == MIRROR_PROFILE_STATE_DISCONNECTED)
                {
                    target = MIRROR_PROFILE_STATE_ACL_CONNECTED;
                    if (MirrorProfile_IsAclConnected() && MirrorProfile_IsA2dpMirroringEnabled())
                    {
                        if (mirrorProfile_GetMirroredAudioSyncState() == AUDIO_SYNC_STATE_READY)
                        {
                            target = MIRROR_PROFILE_STATE_A2DP_CONNECTED;
                        }
                        else if (mirrorProfile_GetMirroredAudioSyncState() == AUDIO_SYNC_STATE_ACTIVE)
                        {
                            target = MIRROR_PROFILE_STATE_A2DP_ROUTED;
                        }
                    }
                }
            }
        }

        MirrorProfile_SetTargetState(target);
    }
}

/*!  \brief Handle an APP_HFP_CONNECTED_IND.

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleAppHfpConnectedInd(const APP_HFP_CONNECTED_IND_T *ind)
{
    MIRROR_LOG("mirrorProfile_HandleAppHfpConnectedInd state 0x%x handset %u",
                MirrorProfile_GetState(), appDeviceIsHandset(&ind->bd_addr));

    MirrorProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle APP_HFP_DISCONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleAppHfpDisconnectedInd(const APP_HFP_DISCONNECTED_IND_T *ind)
{
    UNUSED(ind);
    MirrorProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AV_A2DP_CONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleAvA2dpConnectedInd(const AV_A2DP_CONNECTED_IND_T *ind)
{
    MIRROR_LOG("mirrorProfile_HandleAvA2dpConnectedInd state 0x%x", MirrorProfile_GetState());
    if (MirrorProfile_IsPrimary())
    {
        mirrorProfile_RegisterAudioSync(ind->av_instance);
    }

    /* Target state is updated on AUDIO_SYNC_STATE_IND */
}

/*! \brief Handle APP_HFP_VOLUME_IND

    Only Primary should receive this, because the Handset HFP must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleAppHfpVolumeInd(const APP_HFP_VOLUME_IND_T *ind)
{
    if (MirrorProfile_IsPrimary())
    {
        MirrorProfile_GetScoState()->volume = ind->volume;

        MIRROR_LOG("mirrorProfile_HandleAppHfpVolumeInd volume %u", ind->volume);

        MirrorProfile_SendHfpVolumeToSecondary(ind->source, ind->volume);
    }
}

/*! \brief Handle TELEPHONY_INCOMING_CALL

    Happens when a call is incoming, but before the SCO channel has been
    created.

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleTelephonyIncomingCall(void)
{
    /* Save time later by starting DSP now */
    appKymeraProspectiveDspPowerOn();
}

/*! \brief Handle TELEPHONY_CALL_ONGOING

    Happens when a call is outgoing, but before the SCO channel has been
    created.

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleTelephonyOutgoingCall(void)
{
    if (1 == BtDevice_GetNumberOfHandsetsConnectedOverBredr())
    {
        /* Prepare to mirror the SCO by exiting sniff on the peer link.
        This speeds up connecting the SCO mirror. The link is put back to sniff
        once the SCO mirror is connected or if the eSCO fails to connect. */
        mirrorProfilePeerMode_ActiveModePeriod(mirrorProfileConfig_PrepareForEscoMirrorActiveModeTimeout());
    }
    /* Save time later by starting DSP now */
    appKymeraProspectiveDspPowerOn();
}

/*! \brief Handle TELEPHONY_CALL_ENDED
*/
static void mirrorProfile_HandleTelephonyCallEnded(void)
{
}

/*! \brief Handle APP_HFP_SCO_CONNECTING_SYNC_IND */
static void mirrorProfile_HandleAppHfpScoConnectingSyncInd(const APP_HFP_SCO_CONNECTING_SYNC_IND_T *ind)
{
    bool immediate_response = FALSE;
    if (MirrorProfile_IsAudioSyncL2capConnected() && MirrorProfile_IsEscoMirroringEnabled())
    {
        if (MirrorProfile_GetMirroredDevice() == ind->device)
        {
            /* Already mirroring this device so accept immediately */
            immediate_response = TRUE;
        }
        else
        {
            Task task = MirrorProfile_GetTask();
            uint16 *lock = MirrorProfile_GetScoSyncLockAddr();
            uint32 timeout = mirrorProfileConfig_ScoConnectingSyncTimeout();
            /* Mirroring another device. The mirror profile will switch to mirror
               the ACL of this device and then clear the ScoSync lock. Clearing
               this lock will cause the conditional message below to be delivered
               which calls back to HFP profile to accept the SCO connection.
               If something goes wrong during this process, the _TIMEOUT message
               will be delivered and the SCO will be accepted regardless of the
               whether the mirroring is prepared for the SCO connection. */
            MESSAGE_MAKE(msg, MIRROR_PROFILE_INTERNAL_SCO_SYNC_RSP_T);
            msg->device = ind->device;
            MessageSendConditionally(task, MIRROR_PROFILE_INTERNAL_SCO_SYNC_RSP, msg, lock);
            MessageSendLater(task, MIRROR_PROFILE_INTERNAL_SCO_SYNC_TIMEOUT, NULL, timeout);
            MirrorProfile_SetScoSyncLock();
            MirrorProfile_SetTargetStateFromProfileState();
        }
    }
    else
    {
        immediate_response = TRUE;
    }

    if (immediate_response)
    {
        HfpProfile_ScoConnectingSyncResponse(ind->device, MirrorProfile_GetTask(), TRUE);
    }
}

/*! \brief Handle APP_HFP_SCO_CONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleAppHfpScoConnectedInd(void)
{
    MIRROR_LOG("mirrorProfile_HandleAppHfpScoConnectedInd");

    MirrorProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle APP_HFP_SCO_DISCONNECTED_IND

    Only Primary should receive this, because the Handset must always
    be connected to the Primary.
*/
static void mirrorProfile_HandleAppHfpScoDisconnectedInd(void)
{
    MIRROR_LOG("mirrorProfile_HandleAppHfpScoDisconnectedInd");
    /* When SCO disconnects we want to change the target state, but we don't
       need to initiate a disconnection since we expect the SCO mirror to be
       disconnected automatically by the firmware. Therefore, set the delay kick
       flag to stop the SM from initiating the disconnect immediately. A
       disconnect indication will arrive from the firmware during the delay. */
       MirrorProfile_SetDelayKick();
       MirrorProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle PEER_SIG_CONNECTION_IND

    Both Primary and Secondary will receive this when the peer signalling
    channel is connected and disconnected.
*/
static void mirrorProfile_HandlePeerSignallingConnectionInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    UNUSED(ind);
    MirrorProfile_SetTargetStateFromProfileState();
    if (ind->status != peerSigStatusConnected)
    {
        MirrorProfile_ClearStreamChangeLock();
    }
}

/*! \brief Handle AV_AVRCP_CONNECTED_IND */
static void mirrorProfile_HandleAvAvrcpConnectedInd(void)
{
    MirrorProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AV_AVRCP_DISCONNECTED_IND */
static void mirrorProfile_HandleAvAvrcpDisconnectedInd(void)
{
    MirrorProfile_SetTargetStateFromProfileState();
}

static void mirrorProfile_SendAudioSyncConnectRes(const AUDIO_SYNC_CONNECT_IND_T *ind, uint16* lock)
{
    MESSAGE_MAKE(rsp, AUDIO_SYNC_CONNECT_RES_T);
    rsp->sync_id = ind->sync_id;
    MessageCancelAll(ind->task, AUDIO_SYNC_CONNECT_RES);
    MessageSendConditionally(ind->task, AUDIO_SYNC_CONNECT_RES, rsp, lock);
}

/*! \brief Handle AUDIO_SYNC_CONNECT_IND_T
    \param ind The message.
*/
static void MirrorProfile_HandleAudioSyncConnectInd(const AUDIO_SYNC_CONNECT_IND_T *ind)
{
    audio_source_t mirrored_source = MirrorProfile_GetAudioSource();

    mirrorProfile_SetAudioSyncState(ind->source_id, AUDIO_SYNC_STATE_CONNECTED);

    if(mirrored_source == ind->source_id)
    {
        if(MirrorProfile_StoreAudioSourceParameters(ind->source_id))
        {
            mirrorProfile_SendAudioSyncConnectRes(ind, MirrorProfile_GetStreamChangeLockAddr());
            MirrorProfile_SetStreamChangeLock();

            MIRROR_LOG("MirrorProfile_HandleAudioSyncConnectInd");

            MirrorProfile_SendA2dpStreamContextToSecondaryRequestResponse();
            MirrorProfile_SetTargetStateFromProfileState();
        }
        else
        {
            MIRROR_LOG("MirrorProfile_HandleAudioSyncConnectInd invalid audio source parameters");
        }
    }
    else
    {
        MIRROR_LOG("MirrorProfile_HandleAudioSyncConnectInd for enum:audio_source_t:%d, mirroring enum:audio_source_t:%d", ind->source_id, mirrored_source);
        MirrorProfile_SetTargetStateFromProfileState();
        mirrorProfile_SendAudioSyncConnectRes(ind, NULL);
    }
}

/*! \brief Handle AUDIO_SYNC_PREPARE_IND_T
    \param ind The message.
*/
static void MirrorProfile_HandleAudioSyncPrepareInd(const AUDIO_SYNC_PREPARE_IND_T *ind)
{
    audio_source_t mirrored_source = MirrorProfile_GetAudioSource();
    bool reply_immediately = FALSE;
    audio_sync_reason_t reason = audio_sync_success;

    if(!MirrorProfile_IsAudioSyncL2capConnected())
    {
        /* No earbud connection, respond immediately so that
           audio routing can continue without waiting */
        reply_immediately = TRUE;
        reason = audio_sync_not_required;
    }

    MIRROR_LOG("MirrorProfile_HandleAudioSyncPrepareInd enum:mirror_profile_audio_sync_l2cap_state_t:%d", MirrorProfile_GetAudioSyncL2capState()->l2cap_state);

    if(mirrored_source == ind->source_id)
    {
        if(MirrorProfile_StoreAudioSourceParameters(ind->source_id))
        {
            if (MirrorProfile_GetStreamChangeLock())
            {
                MIRROR_LOG("MirrorProfile_HandleAudioSyncPrepareInd already changing stream");
            }
            else
            {
                /* The context is sent to the secondary with the state set to
                AUDIO_SYNC_STATE_CONNECTED, not AUDIO_SYNC_STATE_READY.
                This ensures that the secondary reports the correct
                correct mirror_profile_a2dp_start_mode_t. */
                mirrorProfile_SetAudioSyncState(ind->source_id, AUDIO_SYNC_STATE_CONNECTED);
                MirrorProfile_SendA2dpStreamContextToSecondary();
                MirrorProfile_ClearStreamChangeLock();
            }
        }
        else
        {
            MIRROR_LOG("MirrorProfile_HandleAudioSyncPrepareInd invalid audio source parameters");
        }
    }
    else
    {
        /* If the preparing device is not going to trigger a switch of target
           device, then reply immediately */
        device_t mirrored = MirrorProfile_GetMirroredDevice();
        device_t target_device = mirrorProfile_GetTargetA2dpDevice();

        if((mirrored == target_device) || (target_device == NULL))
        {
            /* We have chosen not to mirror this source so respond immediately */
            reply_immediately = TRUE;
            reason = audio_sync_rejected;
        }
        MIRROR_LOG("MirrorProfile_HandleAudioSyncPrepareInd for enum:audio_source_t:%d, mirroring enum:audio_source_t:%d", ind->source_id, mirrored_source);
    }
    mirrorProfile_StoreAudioSyncPrepareState(ind->source_id, ind->task, ind->sync_id);
    mirrorProfile_SetAudioSyncState(ind->source_id, AUDIO_SYNC_STATE_READY);
    MirrorProfile_SetTargetStateFromProfileState();

    if (reply_immediately)
    {
        MIRROR_LOG("MirrorProfile_HandleAudioSyncPrepareInd immediate response enum:audio_sync_reason_t:%d", reason);
        mirrorProfile_SendAudioSyncPrepareRes(ind->source_id, reason);
    }
}

/*! \brief Handle AUDIO_SYNC_ACTIVATE_IND_T
    \param ind The message.
*/
static void MirrorProfile_HandleAudioSyncActivateInd(const AUDIO_SYNC_ACTIVATE_IND_T *ind)
{
    audio_source_t mirrored_source = MirrorProfile_GetAudioSource();

    if(mirrored_source == ind->source_id)
    {
        if(MirrorProfile_StoreAudioSourceParameters(ind->source_id))
        {
            MIRROR_LOG("MirrorProfile_HandleAudioSyncActivateInd");
        }
        else
        {
            MIRROR_LOG("MirrorProfile_HandleAudioSyncActivateInd invalid audio source parameters");
        }
    }
    else
    {
        MIRROR_LOG("MirrorProfile_HandleAudioSyncActivateInd for enum:audio_source_t:%d, mirroring enum:audio_source_t:%d", ind->source_id, mirrored_source);
    }
    mirrorProfile_StoreAudioSyncActivateState(ind->source_id, ind->task, ind->sync_id);
    mirrorProfile_SetAudioSyncState(ind->source_id, AUDIO_SYNC_STATE_ACTIVE);
    MirrorProfile_SetTargetStateFromProfileState();
    mirrorProfile_SendAudioSyncActivateRes(ind->source_id);
}

/*! \brief Handle AUDIO_SYNC_STATE_IND_T
    \param ind The message.

    The only state of interest here is disconnected, since other states are
    indicated in other sync messages.
*/
static void MirrorProfile_HandleAudioSyncStateInd(const AUDIO_SYNC_STATE_IND_T *ind)
{
    audio_source_t mirrored_source = MirrorProfile_GetAudioSource();
    MIRROR_LOG("MirrorProfile_HandleAudioSyncStateInd enum:audio_source_t:%d enum:audio_sync_state_t:%d", ind->source_id, ind->state);

    mirrorProfile_SetAudioSyncState(ind->source_id, ind->state);

    if(mirrored_source == ind->source_id)
    {
        switch (ind->state)
        {
            case AUDIO_SYNC_STATE_DISCONNECTED:
                mirrorProfile_SetAudioSyncState(ind->source_id, AUDIO_SYNC_STATE_DISCONNECTED);
                mirrorProfile_StoreAudioSyncPrepareState(ind->source_id, NULL, 0);
                mirrorProfile_StoreAudioSyncActivateState(ind->source_id, NULL, 0);
            break;
            case AUDIO_SYNC_STATE_CONNECTED:
            case AUDIO_SYNC_STATE_ACTIVE:
                MirrorProfile_StoreAudioSourceParameters(ind->source_id);
            break;
            case AUDIO_SYNC_STATE_READY:
            break;
        }
        MirrorProfile_SendA2dpStreamContextToSecondary();
    }
    else
    {
        MIRROR_LOG("MirrorProfile_HandleAudioSyncStateInd mirroring enum:audio_source_t:%d", mirrored_source);
    }

    MirrorProfile_SetTargetStateFromProfileState();
}

/*! \brief Handle AUDIO_SYNC_CODEC_RECONFIGURED_IND_T
    \param ind The message.
*/
static void MirrorProfile_HandleAudioSyncReconfiguredInd(const AUDIO_SYNC_CODEC_RECONFIGURED_IND_T *ind)
{
    audio_source_t mirrored_source = MirrorProfile_GetAudioSource();
    if(mirrored_source == ind->source_id)
    {
        if(MirrorProfile_StoreAudioSourceParameters(ind->source_id))
        {
            MirrorProfile_SendA2dpStreamContextToSecondary();
        }
        else
        {
            MIRROR_LOG("MirrorProfile_HandleAudioSyncReconfiguredInd invalid audio source parameters");
        }
    }
    else
    {
        MIRROR_LOG("MirrorProfile_HandleAudioSyncReconfiguredInd for enum:audio_source_t:%d, mirroring enum:audio_source_t:%d", ind->source_id, mirrored_source);
    }
}

/*! \brief Handle QHS link establishing between buds or QHS start timeout */
static void MirrorProfile_HandleQhsReadyOrFailed(void)
{
    MirrorProfile_SetQhsReady();
    MirrorProfile_SetTargetStateFromProfileState();
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_QHS_START_TIMEOUT);
}

static void MirrorProfile_HandleQhsConnectedInd(const QCOM_CON_MANAGER_QHS_CONNECTED_T * message)
{
    if(appDeviceIsPeer(&message->bd_addr))
    {
        MirrorProfile_HandleQhsReadyOrFailed();
    }
}

static void MirrorProfile_HandleScoSyncRsp(const MIRROR_PROFILE_INTERNAL_SCO_SYNC_RSP_T *msg)
{
    DEBUG_LOG("MirrorProfile_HandleScoSyncRsp");
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_PROFILE_INTERNAL_SCO_SYNC_TIMEOUT);
    /* Accept SCO connection */
    HfpProfile_ScoConnectingSyncResponse(msg->device, MirrorProfile_GetTask(), TRUE);
}

static void MirrorProfile_HandleScoSyncTimeout(void)
{
    DEBUG_LOG("MirrorProfile_HandleScoSyncTimeout");
    MirrorProfile_ClearScoSyncLock();
}



static void mirrorProfile_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* Notifications from other bt domain modules */
    case CON_MANAGER_TP_DISCONNECT_IND:
        mirrorProfile_HandleTpConManagerDisconnectInd((const CON_MANAGER_TP_DISCONNECT_IND_T *)message);
        break;

    case CON_MANAGER_TP_CONNECT_IND:
        mirrorProfile_HandleTpConManagerConnectInd((const CON_MANAGER_TP_CONNECT_IND_T *)message);
        break;

    case APP_HFP_CONNECTED_IND:
        mirrorProfile_HandleAppHfpConnectedInd((const APP_HFP_CONNECTED_IND_T *)message);
        break;

    case APP_HFP_DISCONNECTED_IND:
        mirrorProfile_HandleAppHfpDisconnectedInd((const APP_HFP_DISCONNECTED_IND_T *)message);
        break;

    case APP_HFP_SCO_INCOMING_RING_IND:
        /* \todo Use this as a trigger to send a ring command to Secondary */
        break;

    case APP_HFP_SCO_INCOMING_ENDED_IND:
        /* \todo Use this as a trigger to send a stop ring command to Secondary */
        break;

    case APP_HFP_VOLUME_IND:
        mirrorProfile_HandleAppHfpVolumeInd((const APP_HFP_VOLUME_IND_T *)message);
        break;

    case APP_HFP_SCO_CONNECTING_SYNC_IND:
        mirrorProfile_HandleAppHfpScoConnectingSyncInd(message);
        break;

    case APP_HFP_SCO_CONNECTED_IND:
        mirrorProfile_HandleAppHfpScoConnectedInd();
        break;

    case APP_HFP_SCO_DISCONNECTED_IND:
        mirrorProfile_HandleAppHfpScoDisconnectedInd();
        break;

    case AV_A2DP_CONNECTED_IND:
        mirrorProfile_HandleAvA2dpConnectedInd((const AV_A2DP_CONNECTED_IND_T *)message);
        break;

    case AV_A2DP_DISCONNECTED_IND:
        break;

    case AV_AVRCP_CONNECTED_IND:
        mirrorProfile_HandleAvAvrcpConnectedInd();
        break;

    case AV_AVRCP_DISCONNECTED_IND:
        mirrorProfile_HandleAvAvrcpDisconnectedInd();
        break;

    case TELEPHONY_INCOMING_CALL:
        mirrorProfile_HandleTelephonyIncomingCall();
        break;

    case TELEPHONY_CALL_ONGOING:
        mirrorProfile_HandleTelephonyOutgoingCall();
        break;

    case TELEPHONY_CALL_ENDED:
        mirrorProfile_HandleTelephonyCallEnded();
        break;

    /* Internal mirror_profile messages */
    case MIRROR_INTERNAL_DELAYED_KICK:
        MirrorProfile_SmKick();
        break;

    case MIRROR_INTERNAL_SET_TARGET_STATE:
        MirrorProfile_SetTargetState(((const MIRROR_INTERNAL_SET_TARGET_STATE_T *)message)->target_state);
        break;

    case MIRROR_INTERNAL_KICK_TARGET_STATE:
        MirrorProfile_SetTargetStateFromProfileState();
        break;

    case MIRROR_INTERNAL_PEER_LINK_POLICY_IDLE_TIMEOUT:
        MirrorProfile_PeerLinkPolicyHandleIdleTimeout();
        break;
    case MIRROR_PROFILE_INTERNAL_SCO_SYNC_RSP:
        MirrorProfile_HandleScoSyncRsp(message);
        break;

    case MIRROR_PROFILE_INTERNAL_SCO_SYNC_TIMEOUT:
        MirrorProfile_HandleScoSyncTimeout();
        break;

    /* MDM prims from firmware */
    case MESSAGE_BLUESTACK_MDM_PRIM:
        MirrorProfile_HandleMessageBluestackMdmPrim((const MDM_UPRIM_T *)message);
        break;

    /* Peer Signalling messages */
    case PEER_SIG_CONNECTION_IND:
        mirrorProfile_HandlePeerSignallingConnectionInd((const PEER_SIG_CONNECTION_IND_T *)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
        MirrorProfile_HandlePeerSignallingMessage((const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
        MirrorProfile_HandlePeerSignallingMessageTxConfirm((const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
        break;

    /* Connection library messages */
    case CL_L2CAP_REGISTER_CFM:
        MirrorProfile_HandleClL2capRegisterCfm((const CL_L2CAP_REGISTER_CFM_T *)message);
        break;

    case CL_SDP_REGISTER_CFM:
        MirrorProfile_HandleClSdpRegisterCfm((const CL_SDP_REGISTER_CFM_T *)message);
        break;

    case CL_L2CAP_CONNECT_IND:
        MirrorProfile_HandleL2capConnectInd((const CL_L2CAP_CONNECT_IND_T *)message);
        break;

    case CL_L2CAP_CONNECT_CFM:
        MirrorProfile_HandleL2capConnectCfm((const CL_L2CAP_CONNECT_CFM_T *)message);
        break;

    case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
        MirrorProfile_HandleClSdpServiceSearchAttributeCfm((const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
        break;

    case CL_L2CAP_DISCONNECT_IND:
        MirrorProfile_HandleL2capDisconnectInd((const CL_L2CAP_DISCONNECT_IND_T *)message);
        break;

    case CL_L2CAP_DISCONNECT_CFM:
        MirrorProfile_HandleL2capDisconnectCfm((const CL_L2CAP_DISCONNECT_CFM_T *)message);
        break;
    case QCOM_CON_MANAGER_QHS_CONNECTED:
        MirrorProfile_HandleQhsConnectedInd((const QCOM_CON_MANAGER_QHS_CONNECTED_T *) message);
        break;

    case MIRROR_INTERNAL_QHS_START_TIMEOUT:
         /* QHS link didn't establish */
        MirrorProfile_HandleQhsReadyOrFailed();
        break;

    case MIRROR_INTERNAL_PEER_ENTER_SNIFF:
        mirrorProfile_HandlePeerEnterSniff();
        break;

    case KEY_SYNC_DEVICE_COMPLETE_IND:
        MirrorProfile_SetTargetStateFromProfileState();
        break;


    default:
        MIRROR_LOG("mirrorProfile_MessageHandler: Unhandled id MESSAGE:mirror_profile_internal_msg_t:0x%x", id);
        break;
    }
}

static void mirrorProfile_AudioSyncMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* Audio sync messages */
    case AUDIO_SYNC_CONNECT_IND:
        MirrorProfile_HandleAudioSyncConnectInd((const AUDIO_SYNC_CONNECT_IND_T *)message);
        break;

    case AUDIO_SYNC_PREPARE_IND:
        MirrorProfile_HandleAudioSyncPrepareInd((const AUDIO_SYNC_PREPARE_IND_T *)message);
        break;

    case AUDIO_SYNC_ACTIVATE_IND:
        MirrorProfile_HandleAudioSyncActivateInd((const AUDIO_SYNC_ACTIVATE_IND_T *)message);
        break;

    case AUDIO_SYNC_STATE_IND:
        MirrorProfile_HandleAudioSyncStateInd((const AUDIO_SYNC_STATE_IND_T *)message);
        break;

    case AUDIO_SYNC_CODEC_RECONFIGURED_IND:
        MirrorProfile_HandleAudioSyncReconfiguredInd((const AUDIO_SYNC_CODEC_RECONFIGURED_IND_T *)message);
        break;

    default:
        break;
    }
}

/*! \brief Send an audio_sync_msg_t internally.

    The audio_sync_msg_t messages do not need to be sent conditionally as the
    handling of the message can only modify the target state.
*/
static void mirrorProfile_SyncSendAudioSyncMessage(audio_sync_t *sync_inst, MessageId id, Message message)
{
    Task task = &sync_inst->task;
    PanicFalse(MessageCancelAll(task, id) <= 1);
    MessageSendConditionally(task, id, message, &MirrorProfile_GetLock());
}

bool MirrorProfile_Init(Task task)
{
    memset(&mirror_profile, 0, sizeof(mirror_profile));
    mirror_profile.task_data.handler = mirrorProfile_MessageHandler;
    mirror_profile.state = MIRROR_PROFILE_STATE_DISCONNECTED;
    mirror_profile.target_state = MIRROR_PROFILE_STATE_DISCONNECTED;
    mirror_profile.acl.conn_handle = MIRROR_PROFILE_CONNECTION_HANDLE_INVALID;
    mirror_profile.esco.conn_handle = MIRROR_PROFILE_CONNECTION_HANDLE_INVALID;
    mirror_profile.esco.volume = 0; //appHfpGetVolume();
    mirror_profile.init_task = task;
    mirror_profile.client_tasks = TaskList_Create();
    mirror_profile.audio_sync.local_psm = 0;
    mirror_profile.audio_sync.remote_psm = 0;
    mirror_profile.audio_sync.sdp_search_attempts = 0;
    mirror_profile.audio_sync.l2cap_state = MIRROR_PROFILE_STATE_AUDIO_SYNC_L2CAP_NONE;
    mirror_profile.enable_esco_mirroring = TRUE;
    mirror_profile.enable_a2dp_mirroring = TRUE;

    /* Register a Protocol/Service Multiplexor (PSM) that will be
       used for this application. The same PSM is used at both
       ends. */
    ConnectionL2capRegisterRequest(MirrorProfile_GetTask(), L2CA_PSM_INVALID, 0);

    /* Register for notifications when devices and/or profiles connect
       or disconnect. */
    ConManagerRegisterTpConnectionsObserver(cm_transport_bredr, MirrorProfile_GetTask());
    HfpProfile_RegisterStatusClient(MirrorProfile_GetTask());
    appAvStatusClientRegister(MirrorProfile_GetTask());
    Telephony_RegisterForMessages(MirrorProfile_GetTask());
    QcomConManagerRegisterClient(MirrorProfile_GetTask());

    /* Register a channel for peer signalling */
    appPeerSigMarshalledMsgChannelTaskRegister(MirrorProfile_GetTask(),
                                            PEER_SIG_MSG_CHANNEL_MIRROR_PROFILE,
                                            mirror_profile_marshal_type_descriptors,
                                            NUMBER_OF_MIRROR_PROFILE_MARSHAL_TYPES);

    /* Register for peer signaling notifications */
    appPeerSigClientRegister(MirrorProfile_GetTask());

    HfpProfile_SetScoConnectingSyncTask(MirrorProfile_GetTask());

    KeySync_RegisterListener(MirrorProfile_GetTask());

    /* Now wait for MDM_REGISTER_CFM */
    return TRUE;
}


/* \brief Inform mirror profile of current device Primary/Secondary role.

    todo : A Primary <-> Secondary role switch should only be allowed
    when the state machine is in a stable state. This will be more important
    when the handover logic is implemented.
*/
void MirrorProfile_SetRole(bool primary)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();


    if (!primary)
    {
        /* Take ownership of the A2DP source (mirror) when becoming secondary */
        AudioSources_RegisterAudioInterface(audio_source_a2dp_1, MirrorProfile_GetAudioInterface());
        AudioSources_RegisterAudioInterface(audio_source_a2dp_2, MirrorProfile_GetAudioInterface());

        AudioSources_RegisterMediaControlInterface(audio_source_a2dp_1, MirrorProfile_GetMediaControlInterface());
        AudioSources_RegisterMediaControlInterface(audio_source_a2dp_2, MirrorProfile_GetMediaControlInterface());

        /* Register voice source interface */
        VoiceSources_RegisterAudioInterface(voice_source_hfp_1, MirrorProfile_GetVoiceInterface());
        VoiceSources_RegisterAudioInterface(voice_source_hfp_2, MirrorProfile_GetVoiceInterface());

        VoiceSources_RegisterTelephonyControlInterface(voice_source_hfp_1, MirrorProfile_GetTelephonyControlInterface());
        VoiceSources_RegisterTelephonyControlInterface(voice_source_hfp_2, MirrorProfile_GetTelephonyControlInterface());


        /* Clear delayed kicks when becoming secondary. This avoids the state
           machine being kicked in the secondary role resulting in panic */
        MessageCancelAll(MirrorProfile_GetTask(), MIRROR_INTERNAL_DELAYED_KICK);
    }

    sp->is_primary = primary;
    MIRROR_LOG("MirrorProfile_SetRole primary %u", sp->is_primary);
}

/* \brief Get the SCO sink associated with the mirror eSCO link. */
Sink MirrorProfile_GetScoSink(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    return StreamScoSink(sp->esco.conn_handle);
}

void MirrorProfile_Connect(Task task, const bdaddr *peer_addr)
{
    if(peer_addr)
    {
        DEBUG_LOG("MirrorProfile_Connect - startup");

        mirror_profile_task_data_t *mirror_inst = MirrorProfile_Get();
        mirror_inst->is_primary = TRUE;
        MirrorProfile_CreateAudioSyncL2capChannel(task, peer_addr);
    }
    else
    {
        DEBUG_LOG("MirrorProfile_Connect - Peer address is NULL");
        Panic();
    }
}

void MirrorProfile_Disconnect(Task task)
{
    DEBUG_LOG("MirrorProfile_Disconnect");

    MirrorProfile_CloseAudioSyncL2capChannel(task);
}

void MirrorProfile_ClientRegister(Task client_task)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    TaskList_AddTask(sp->client_tasks, client_task);
}

void MirrorProfile_ClientUnregister(Task client_task)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    TaskList_RemoveTask(sp->client_tasks, client_task);
}

bool MirrorProfile_IsConnected(void)
{
    return (MirrorProfile_IsAclConnected() || MirrorProfile_IsEscoConnected());
}

bool MirrorProfile_IsCisMirroringConnected(void)
{
    return 0;
}

bool MirrorProfile_IsEscoActive(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    return (SinkIsValid(StreamScoSink(sp->esco.conn_handle)));
}

bool MirrorProfile_IsA2dpActive(void)
{
    return MirrorProfile_IsA2dpConnected();
}

uint16 MirrorProfile_GetMirrorAclHandle(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    return sp->acl.conn_handle;
}

/*
    Test only functions
*/
void MirrorProfile_Destroy(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    TaskList_Destroy(sp->client_tasks);
}

mirror_profile_a2dp_start_mode_t MirrorProfile_GetA2dpStartMode(void)
{
    mirror_profile_a2dp_start_mode_t mode = MIRROR_PROFILE_A2DP_START_PRIMARY_UNSYNCHRONISED;
    bool sync_start;

    /* When we are in Q2Q mode, audio playback on the primary and secondary will inherently be in sync,
     * so we can just return Q2Q mode here
     */
    if (Kymera_IsQ2qModeEnabled())
    {
        DEBUG_LOG("MirrorProfile_GetA2dpStartMode mirror mode enum:mirror_profile_a2dp_start_mode_t:%d",mode);
        return MIRROR_PROFILE_A2DP_START_Q2Q_MODE;
    }

    switch (MirrorProfile_GetState())
    {
        case MIRROR_PROFILE_STATE_A2DP_CONNECTING:
        case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
        case MIRROR_PROFILE_STATE_A2DP_ROUTED:
            sync_start = TRUE;
            break;
        default:
            /* Also start synchronised if transitioning between handsets */
            sync_start = MirrorProfile_IsHandsetSwitchRequired();
            break;
    }

    if (MirrorProfile_IsPrimary())
    {
        if (sync_start)
        {
            /* If the mirrored instance is already streaming, the audio will
            be started in sync with the secondary by unmuting the audio
            stream at the same instant. The secondary sends a message to the
            primary defining the unmute instant. */
            avInstanceTaskData *av_inst = AvInstance_GetInstanceForDevice(MirrorProfile_GetMirroredDevice());
            if (av_inst && appA2dpIsStreaming(av_inst))
            {
                mode = MIRROR_PROFILE_A2DP_START_PRIMARY_SYNC_UNMUTE;
            }
            else
            {
                mode = MIRROR_PROFILE_A2DP_START_PRIMARY_SYNCHRONISED;
            }
        }
        else
        {
            mode = MIRROR_PROFILE_A2DP_START_PRIMARY_UNSYNCHRONISED;
        }
    }
    else
    {
        audio_sync_state_t sync_state = mirrorProfile_GetMirroredAudioSyncState();

        switch (sync_state)
        {
            case AUDIO_SYNC_STATE_READY:
            case AUDIO_SYNC_STATE_CONNECTED:
            {
                if (sync_start)
                {
                    mode = MIRROR_PROFILE_A2DP_START_SECONDARY_SYNCHRONISED;
                }
                else
                {
                    mode = MIRROR_PROFILE_A2DP_START_SECONDARY_SYNC_UNMUTE;
                }
            }
            break;

            case AUDIO_SYNC_STATE_ACTIVE:
                mode = MIRROR_PROFILE_A2DP_START_SECONDARY_SYNC_UNMUTE;
            break;

            default:
                DEBUG_LOG_WARN("MirrorProfile_GetA2dpStartMode Unexpected a2dp state enum:audio_sync_state_t:%d",
                                sync_state);
            break;
        }
    }
    DEBUG_LOG("MirrorProfile_GetA2dpStartMode mirror mode enum:mirror_profile_a2dp_start_mode_t:%d",mode);
    return mode;
}

bool MirrorProfile_ShouldEscoAudioStartSynchronously(voice_source_t source)
{
    if (MirrorProfile_IsSecondary())
    {
        return TRUE;
    }
    else if (MirrorProfile_IsAclConnected() &&
             MirrorProfile_IsEscoMirroringEnabled() &&
             (MirrorProfile_GetVoiceSource() == source))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

Sink MirrorProfile_GetA2dpAudioSyncTransportSink(void)
{
    return MirrorProfile_GetAudioSyncL2capState()->link_sink;
}

Source MirrorProfile_GetA2dpAudioSyncTransportSource(void)
{
    return MirrorProfile_GetAudioSyncL2capState()->link_source;
}

/*! \brief Request mirror_profile to Enable Mirror Esco.

    This should only be called from the Primary device.
*/
void MirrorProfile_EnableMirrorEsco(void)
{
    DEBUG_LOG("MirrorProfile_EnableMirrorEsco, State(0x%x)", MirrorProfile_GetState());
    if (!mirror_profile.enable_esco_mirroring)
    {
        mirror_profile.enable_esco_mirroring = TRUE;
        MirrorProfile_SetTargetStateFromProfileState();
    }
}

/*! \brief Request mirror_profile to Disable Mirror Esco.

    This should only be called from the Primary device.
*/
void MirrorProfile_DisableMirrorEsco(void)
{
    DEBUG_LOG("MirrorProfile_DisableMirrorEsco, State(0x%x)", MirrorProfile_GetState());
    if (mirror_profile.enable_esco_mirroring)
    {
        mirror_profile.enable_esco_mirroring = FALSE;
        MirrorProfile_SetTargetStateFromProfileState();
    }
}

void MirrorProfile_EnableMirrorA2dp(void)
{
    DEBUG_LOG("MirrorProfile_EnableMirrorA2dp, State(0x%x)", MirrorProfile_GetState());
    mirror_profile.enable_a2dp_mirroring = TRUE;
    MirrorProfile_SetTargetStateFromProfileState();
}

void MirrorProfile_DisableMirrorA2dp(void)
{
    DEBUG_LOG("MirrorProfile_DisableMirrorA2dp, State(0x%x)", MirrorProfile_GetState());
    mirror_profile.enable_a2dp_mirroring = FALSE;
    MirrorProfile_SetTargetStateFromProfileState();
}

uint16 MirrorProfile_GetMirrorState(void)
{
    return MirrorProfile_GetState();
}

uint32 MirrorProfile_GetExpectedPeerLinkTransmissionTime(void)
{
    return MirrorProfilePeerLinkPolicy_GetExpectedTransmissionTime();
}

bool MirrorProfile_IsVoiceSourceSupported(voice_source_t source)
{
    source_defined_params_t source_params = {0};
    bool mirroring_supported = TRUE;

    /* The local HFP SCO should already have been connected up to the point
       where we know the type (SCO/eSCO) and eSCO connection paramters. */
    if (VoiceSources_GetConnectParameters(source, &source_params))
    {
        voice_connect_parameters_t *voice_params = (voice_connect_parameters_t *)source_params.data;
        assert(source_params.data_length == sizeof(voice_connect_parameters_t));

        /* Mirroring is not supported for:
            SCO links (tesco == 0)
            eSCO links using HV3 packets (tesco == 6) */
        if (voice_params->tesco <= 6)
        {
            mirroring_supported = FALSE;
        }

        VoiceSources_ReleaseConnectParameters(source, &source_params);
    }

    DEBUG_LOG("MirrorProfile_IsVoiceSourceSupported supported %d", mirroring_supported);
    return mirroring_supported;
}

bdaddr * MirrorProfile_GetMirroredDeviceAddress(void)
{
    return &(MirrorProfile_Get()->acl.bd_addr);
}

void mirrorProfile_SetAudioSyncState(audio_source_t source, audio_sync_state_t state)
{
    switch (source)
    {
        case audio_source_a2dp_1:
            MirrorProfile_GetA2dpState()->state[0] = state;
            break;
        case audio_source_a2dp_2:
            MirrorProfile_GetA2dpState()->state[1] = state;
            break;
        case audio_source_none:
            DEBUG_LOG("mirrorProfile_SetAudioSyncState audio_source_none");
            break;
        default:
            Panic();
            break;
    }
}

audio_sync_state_t mirrorProfile_GetMirroredAudioSyncState(void)
{
    audio_source_t asource;
    if (MirrorProfile_IsPrimary())
    {
        asource = MirrorProfile_GetAudioSource();
        if (asource != audio_source_none)
        {
            focus_t focus = Focus_GetFocusForAudioSource(asource);
            if (focus != focus_foreground)
            {
                /* The A2DP audio source is not foreground so ignore it */
                asource = audio_source_none;
            }
        }
    }
    else
    {
        asource = MirrorProfile_GetA2dpState()->audio_source;
    }

    switch (asource)
    {
        case audio_source_none:
            return AUDIO_SYNC_STATE_DISCONNECTED;
        case audio_source_a2dp_1:
            return MirrorProfile_GetA2dpState()->state[0];
        case audio_source_a2dp_2:
            return MirrorProfile_GetA2dpState()->state[1];
        default:
            Panic();
            return AUDIO_SYNC_STATE_DISCONNECTED;
    }
}

uint8 mirrorProfile_GetMirroredAudioVolume(void)
{
    audio_source_t source = MirrorProfile_GetAudioSource();

    if(source != audio_source_none)
    {
        return AudioSources_GetVolume(source).value;
    }

    return 0;
}

unsigned mirrorProfile_audioSourceToIndex(audio_source_t source)
{
    switch (source)
    {
        case audio_source_a2dp_1:
            return 0;
        case audio_source_a2dp_2:
            return 1;
        default:
            Panic();
            return 0;
    }
}

void mirrorProfile_StoreAudioSyncPrepareState(audio_source_t source, Task task, uint16 id)
{
    unsigned index = mirrorProfile_audioSourceToIndex(source);
    MirrorProfile_GetA2dpState()->prepare_state[index].id = id;
    MirrorProfile_GetA2dpState()->prepare_state[index].task = task;
}

void mirrorProfile_StoreAudioSyncActivateState(audio_source_t source, Task task, uint16 id)
{
    unsigned index = mirrorProfile_audioSourceToIndex(source);
    MirrorProfile_GetA2dpState()->activate_state[index].id = id;
    MirrorProfile_GetA2dpState()->activate_state[index].task = task;
}

void mirrorProfile_SendAudioSyncPrepareRes(audio_source_t source, audio_sync_reason_t reason)
{
    unsigned index = mirrorProfile_audioSourceToIndex(source);
    Task task = MirrorProfile_GetA2dpState()->prepare_state[index].task;
    if (task)
    {
        MESSAGE_MAKE(rsp, AUDIO_SYNC_PREPARE_RES_T);
        rsp->sync_id = MirrorProfile_GetA2dpState()->prepare_state[index].id;
        rsp->reason = reason;
        MessageSend(task, AUDIO_SYNC_PREPARE_RES, rsp);
        MirrorProfile_GetA2dpState()->prepare_state[index].task = NULL;
    }
}

void mirrorProfile_SendAudioSyncActivateRes(audio_source_t source)
{
    unsigned index = mirrorProfile_audioSourceToIndex(source);
    Task task = MirrorProfile_GetA2dpState()->activate_state[index].task;
    if (task)
    {
        MESSAGE_MAKE(rsp, AUDIO_SYNC_ACTIVATE_RES_T);
        rsp->sync_id = MirrorProfile_GetA2dpState()->activate_state[index].id;
        MessageSend(task, AUDIO_SYNC_ACTIVATE_RES, rsp);
        MirrorProfile_GetA2dpState()->activate_state[index].task = NULL;
    }
}

void mirrorProfile_RegisterAudioSync(avInstanceTaskData *av_inst)
{
    audio_sync_t sync = {{mirrorProfile_AudioSyncMessageHandler},
                         mirrorProfile_SyncSendAudioSyncMessage};
    appA2dpSyncRegister(av_inst, &sync);
}

bool MirrorProfile_IsRolePrimary(void)
{
    return MirrorProfile_IsPrimary();
}

audio_source_t MirrorProfile_GetAudioSource(void)
{
    return DeviceProperties_GetAudioSource(MirrorProfile_GetAclState()->device);
}

voice_source_t MirrorProfile_GetVoiceSource(void)
{
    return DeviceProperties_GetVoiceSource(MirrorProfile_GetAclState()->device);
}

uint16 MirrorProfile_GetLeAudioUnicastContext(void)
{
    return 0;
}

#endif /* INCLUDE_MIRRORING */
