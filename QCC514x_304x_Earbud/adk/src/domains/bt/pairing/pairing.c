/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       pairing.c
\brief      Pairing task
*/

#include "pairing.h"
#include "pairing_plugin.h"
#include "pairing_config.h"
#include "pairing_protected.h"

#include "app_task.h"
#include "sdp.h"
#include "peer_signalling.h"
#include "system_state.h"
#include "adk_log.h"
#include "gatt_handler.h"
#include "device_properties.h"
#include "peer_link_keys.h"
#include "unexpected_message.h"

#include <device_db_serialiser.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <profile_manager.h>
#include <gatt.h>
#include <ui.h>

#include <panic.h>
#include <connection_abstraction.h>
#include <device.h>
#include <device_list.h>
#include <ps.h>
#include <string.h>
#include <stdio.h>
#include <logging.h>

/*! Check the message ranges are pemitted */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(PAIRING, PAIRING_MESSAGE_END)
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(PAIRING_INTERNAL_MESSAGE_END)

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(pairing_messages)
LOGGING_PRESERVE_MESSAGE_ENUM(pairing_internal_message_ids)

/*! Macro for simplifying creating messages */
#define MAKE_PAIRING_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! Macro to make message with variable length for array fields. */
#define MAKE_PAIRING_MESSAGE_WITH_LEN(TYPE, LEN) \
    TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + LEN);

#define PAIRING_IAC                     0x9E8B33

/*
 * Function Prototypes
 */
static void pairing_SetState(pairingTaskData *thePairing, pairingState state);

/*!< pairing task */
pairingTaskData pairing_task_data;

static unsigned pairing_GetCurrentContext(void)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    pairing_provider_context_t context = context_handset_pairing_idle;

    if (thePairing->state > PAIRING_STATE_IDLE)
    {
        context = context_handset_pairing_active;
    }
    return (unsigned)context;
}

static void pairing_ResetPendingBleAddress(void)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    
    BdaddrTypedSetEmpty(&thePairing->pending_ble_address);
}

static void pairing_ResetIoCapsRcvdBdAddr(void)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    BdaddrSetZero(&thePairing->device_io_caps_rcvd_bdaddr);
}


/*! \brief Notify clients of pairing activity. 

    \param status The activity to notify. This should be from the #pairingActivityStatus enum.
    \param device_addr The Bluetooth address associated with the status. In most cases NULL.
    \param permanent Is the passed device_addr permanent.
*/
static void pairing_MsgActivity(pairingActivityStatus status, const bdaddr* device_addr, bool permanent)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    MAKE_PAIRING_MESSAGE(PAIRING_ACTIVITY);

    message->status = status;
    message->user_initiated = thePairing->is_user_initiated;
    message->permanent = permanent;

    if (device_addr)
    {
        message->device_addr = *device_addr;
    }
    else
    {
        BdaddrSetZero(&message->device_addr);
    }

    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(PairingGetPairingActivity()), PAIRING_ACTIVITY, message);
}

static void pairing_EirSetupCompleteCallback(bool success)
{
    DEBUG_LOG("pairing_EirSetupCompleteCallback success=%d", success);

    if(success)
    {
        MessageSend(SystemState_GetTransitionTask(), PAIRING_INIT_CFM, NULL);
        pairing_SetState(PairingGetTaskData(), PAIRING_STATE_IDLE);
    }
    else
    {
        Panic();
    }
}

static void pairing_EnterInitialising(pairingTaskData *thePairing)
{
    UNUSED(thePairing);

    DEBUG_LOG("pairing_EnterInitialising");

    ScanManager_ConfigureEirData(pairing_EirSetupCompleteCallback);

}

/*! @brief Actions to take when leaving #PAIRING_STATE_INITIALISING. */
static void pairing_ExitInitialising(pairingTaskData *thePairing)
{
    UNUSED(thePairing);

    DEBUG_LOG("pairing_ExitInitialising");
}

static void pairing_EnterIdle(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_EnterIdle");

    BredrScanManager_InquiryScanRelease(&thePairing->task);
    BredrScanManager_PageScanRelease(&thePairing->task);

    if(thePairing->stop_task)
    {
        MessageSend(thePairing->stop_task, PAIRING_STOP_CFM, NULL);
        thePairing->stop_task = NULL;
    }

    /* reset IO caps received handset address so allow pairing for new handset */
    pairing_ResetIoCapsRcvdBdAddr();

    /* Unlock pairing and permit a pairing operation */
    thePairing->pairing_lock = 0;
}

static void pairing_ExitIdle(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitIdle");

    /* Lock pairing now that a pairing operation is underway */
    thePairing->pairing_lock = 1;
}

static void pairing_EnterDiscoverable(pairingTaskData *thePairing)
{
    static const uint32 iac_array[] = { PAIRING_IAC };

    DEBUG_LOG("pairing_EnterHandsetDiscoverable");

    /* No longer idle, starting a pairing operation so need to be connectable */
    BredrScanManager_PageScanRequest(&thePairing->task, SCAN_MAN_PARAMS_TYPE_FAST);

    ConnectionWriteInquiryAccessCode(&thePairing->task, iac_array, 1);

    /* Enable inquiry scan so handset can find us */
    BredrScanManager_InquiryScanRequest(&thePairing->task, SCAN_MAN_PARAMS_TYPE_FAST);

    /* Start pairing timeout */
    MessageCancelFirst(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
    if (thePairing->is_user_initiated && appConfigHandsetPairingTimeout())
        MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigHandsetPairingTimeout()));
    else if (!thePairing->is_user_initiated && appConfigAutoHandsetPairingTimeout())
        MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigAutoHandsetPairingTimeout()));

    /* Not initially pairing with BLE Handset */
    thePairing->pair_le_task = NULL;

    /* Cancel any pending messages which disables ble pairing */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_LE_PAIR_TIMEOUT);

    /* When pairing enters discoverable mode set permission to AllowAll to faciliate br/edr pairing. */
    thePairing->ble_permission = pairingBleAllowAll;

    /* Indicate pairing active */
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_ACTIVE);

    /* notify clients that pairing is in progress */
    pairing_MsgActivity(pairingActivityInProgress, NULL, FALSE);
}

static void pairing_ExitDiscoverable(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitDiscoverable");

    /* No longer discoverable, disable inquiry scan */
    BredrScanManager_InquiryScanRelease(&thePairing->task);

    /* Cancel pairing timeout */
    MessageCancelFirst(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);

    /* Stop pairing indication */
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_INACTIVE);
}


static void pairing_EnterPendingAuthentication(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_EnterPendingAuthentication");

    /* Cancel pairing timeout */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
    MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigAuthenticationTimeout()));

}

static void pairing_EnterLiPendingAuthentication(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_EnterLiPendingAuthentication");

    /* Cancel pairing timeout */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
    MessageSendLater(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND, 0, D_SEC(appConfigAuthenticationTimeout()));
}

static void pairing_ExitPendingAuthentication(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitPendingAuthentication");

    /* Cancel pairing timeout */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
}

static void pairing_ExitLiPendingAuthentication(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_ExitLiPendingAuthentication");

    /* Cancel pairing timeout */
    MessageCancelAll(&thePairing->task, PAIRING_INTERNAL_TIMEOUT_IND);
}

/*! \brief Set Pairing FSM state

    Called to change state.  Handles calling the state entry and exit
    functions for the new and old states.
*/
static void pairing_SetState(pairingTaskData *thePairing, pairingState state)
{
    DEBUG_LOG_STATE("pairing_SetState, Current State = %d, New State = %d", thePairing->state, state);

    if(thePairing->state != state)
    {

        switch (thePairing->state)
        {
            case PAIRING_STATE_INITIALISING:
                pairing_ExitInitialising(thePairing);
                break;

            case PAIRING_STATE_IDLE:
                pairing_ExitIdle(thePairing);
                break;

            case PAIRING_STATE_DISCOVERABLE:
                pairing_ExitDiscoverable(thePairing);
                break;

            case PAIRING_STATE_PENDING_AUTHENTICATION:
                pairing_ExitPendingAuthentication(thePairing);
                break;

            case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
                pairing_ExitLiPendingAuthentication(thePairing);
                break;

            default:
                break;
        }

        /* Set new state */
        thePairing->state = state;

        /* Handle state entry functions */
        switch (state)
        {
            case PAIRING_STATE_INITIALISING:
                pairing_EnterInitialising(thePairing);
                break;

            case PAIRING_STATE_IDLE:
                pairing_EnterIdle(thePairing);
                break;

            case PAIRING_STATE_DISCOVERABLE:
                pairing_EnterDiscoverable(thePairing);
                break;

            case PAIRING_STATE_PENDING_AUTHENTICATION:
                pairing_EnterPendingAuthentication(thePairing);
                break;

            case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
                pairing_EnterLiPendingAuthentication(thePairing);
                break;

            default:
                break;
        }

        Ui_InformContextChange(ui_provider_handset_pairing, pairing_GetCurrentContext());
    }
    else /* thePairing->state == state */
    {
        Panic();
    }
}


/*! \brief Get Pairing FSM state

    Returns current state of the Pairing FSM.
*/
static pairingState pairing_GetState(pairingTaskData *thePairing)
{
    return thePairing->state;
}

static void pairing_SendNotReady(Task task)
{
    MAKE_PAIRING_MESSAGE(PAIRING_PAIR_CFM);
    message->status = pairingNotReady;
    MessageSend(task, PAIRING_PAIR_CFM, message);
}

static void pairing_Complete(pairingTaskData *thePairing, pairingStatus status, const bdaddr *bd_addr, bool permanent)
{
    DEBUG_LOG("pairing_Complete enum:pairingStatus:%d",status);

    if (thePairing->client_task)
    {
        MAKE_PAIRING_MESSAGE(PAIRING_PAIR_CFM);
        if (bd_addr)
        {
            message->device_bd_addr = *bd_addr;
        }
        else
        {
            DeviceDbSerialiser_Serialise();
        }
        message->status = status;
        MessageSend(thePairing->client_task, PAIRING_PAIR_CFM, message);
        /* Since the PAIRING_PAIR_CFM message has been sent to the client task
         * clear the task so that subsequent remote initiated pairing requests don't
         * get sent to the client */
        thePairing->client_task = NULL;
    }
    if (pairingSuccess == status)
    {
        /* When peer pairing, the peer pair LE module decides when to indicate
           completion to clients as the LE pairing is not the completion of the
           peer pairing process. */
        if (thePairing->device_type == DEVICE_TYPE_HANDSET)
        {
            Pairing_SendPairingCompleteMessageToClients();
        }
        /* notify clients that pairing succeeded */
        pairing_MsgActivity(pairingActivitySuccess, (bdaddr*)bd_addr, permanent);
    }
    else
    {
        if (status != pairingStopped)
        {
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_FAILED);
        }
    }

    /* notify clients that handset pairing is not in progress */
    pairing_MsgActivity(pairingActivityNotInProgress, NULL, FALSE);

    /* Once pairing completed(regardless of the status) RESET is_user_initiated flag */
    thePairing->is_user_initiated = FALSE;

    /* Move back to 'idle' state */
    pairing_SetState(thePairing, PAIRING_STATE_IDLE);
}


/*! \brief Set SCO forwarding supported features

    \param bd_addr Pointer to read-only device BT address to set featured for.
    \param supported_features Support featured for device.

static void pairing_SetScoForwardFeatures(const bdaddr *bd_addr, uint16 supported_features)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        DEBUG_LOG("appDeviceSetScoForwardFeatures, features %04x", supported_features);
        Device_SetPropertyU16(device, device_property_sco_fwd_features, supported_features);
    }
}
*/
static void pairing_UpdateLinkMode(const bdaddr *bd_addr, cl_sm_link_key_type key_type)
{
    /* Check if the key_type generated is p256. If yes then set the
    * attribute.mode to sink_mode_unknown. Once the encryption type is known, device
    * attributes will be updated accordingly with proper mode.
    * Update the device attributes with this information to be reused later.
    */
    switch (key_type)
    {
        case cl_sm_link_key_unauthenticated_p256:
        case cl_sm_link_key_authenticated_p256:
            DEBUG_LOG("pairing_UpdateLinkMode: link_mode UNKNOWN");
            appDeviceSetLinkMode(bd_addr, DEVICE_LINK_MODE_UNKNOWN);
            break;
        default:
            DEBUG_LOG("pairing_UpdateLinkMode: link_mode NO_SECURE_CONNECTION");
            appDeviceSetLinkMode(bd_addr, DEVICE_LINK_MODE_NO_SECURE_CONNECTION);
            break;
    }
}


/*! \brief Use link key for attached device to derive key for peer earbud.
 */
static void pairing_HandleClSmGetAuthDeviceConfirm(pairingTaskData* thePairing,
                                                     CL_SM_GET_AUTH_DEVICE_CFM_T *cfm)
{
    DEBUG_LOG("pairing_HandleClSmGetAuthDeviceConfirm %d", cfm->status);
    UNUSED(thePairing);

    if ((cfm->status == success) && (cfm->size_link_key == 8))
    {
        PeerLinkKeys_SendKeyToPeer(&cfm->bd_addr, cfm->size_link_key, cfm->link_key);
    }
    else
    {
        /* Failed to find link key data for device, we just added it
         * this is bad. */
        Panic();
    }
}

/*! \brief Resends CL_SM_AUTHENTICATE_CFM if device data base is full.

    It is expected that when a new device is paired and device list is full,
    CL_SM_AUTH_DEVICE_DELETED_IND message will be sent indicating that the connection library
    have deleted oldest handset to make space for just paired one.
    However, CL_SM_AUTH_DEVICE_DELETED_IND may be received after CL_SM_AUTHENTICATE_CFM.
    In such case CL_SM_AUTHENTICATE_CFM is send again to the pairing component without processing,
    to allow for reception of CL_SM_AUTH_DEVICE_DELETED_IND and synchronisation of device data base with PDL.

    Note: In case CL_SM_AUTHENTICATE_CFM is received for a known device and the device list is full, we will not be resending
    of CL_SM_AUTHENTICATE_CFM.

    \param cfm Payload of CL_SM_AUTHENTICATE_CFM message.

    \return TRUE if device data base was full and CL_SM_AUTHENTICATE_CFM have been re-sent.
*/
static bool pairing_ResendAuthenticateCfmIfDeviceDbFull(const CL_SM_AUTHENTICATE_CFM_T *cfm)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    bool was_full = FALSE;

    if(BtDevice_IsFull() && !BtDevice_isKnownBdAddr(&cfm->bd_addr))
    {
        CL_SM_AUTHENTICATE_CFM_T *new_cfm = PanicUnlessMalloc(sizeof(CL_SM_AUTHENTICATE_CFM_T));
        DEBUG_LOG_VERBOSE("pairing_ResendAuthenticateCfm device list full, re-sending message");
        memcpy(new_cfm, cfm, sizeof(CL_SM_AUTHENTICATE_CFM_T));
        MessageSend(&thePairing->task, CL_SM_AUTHENTICATE_CFM, new_cfm);
        was_full = TRUE;
    }

    return was_full;
}

/*! \brief Handle authentication confirmation

    The firmware has indicated that authentication with the remote device
    has completed, we should only receive this message in the pairing states.

    If we're pairing with a peer earbud, delete any previous pairing and then
    update the TWS+ service record with the address of the peer earbud.

    If we're pairing with a handset start SDP search to see if phone supports
    TWS+.
*/
static void pairing_HandleClSmAuthenticateConfirm(const CL_SM_AUTHENTICATE_CFM_T *cfm)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG_FN_ENTRY("pairing_HandleClSmAuthenticateConfirm,enum:pairingState:%d, enum:authentication_status:%d, enum:cl_sm_link_key_type:%d bonded %d addr=%04x %02x %06x",
                        pairing_GetState(thePairing),
                        cfm->status,
                        cfm->key_type,
                        cfm->bonded,
                        cfm->bd_addr.nap,
                        cfm->bd_addr.uap,
                        cfm->bd_addr.lap);

    Pairing_PluginPairingComplete();

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
            if (cfm->status == auth_status_fail &&
                !BdaddrIsSame(&thePairing->device_io_caps_rcvd_bdaddr, &cfm->bd_addr))
            {
                /* Getting here suggests that multiple AGs trying to pair to us.
                Except first AG, all other AG's IO caps get rejected in LI_PENDING_AUTHENTICATION. 
                This results AG to abort the pairing and sending the authentication failure(apps
                receives CL_SM_AUTHENTICATE_CFM(auth_status_fail)).
                Just swallow the CL_SM_AUTHENTICATE_CFM(auth_status_fail) message and wait for 
                CL_SM_AUTHENTICATE_CFM(auth_status_success) from AG whose IO caps have been accepted.*/
                DEBUG_LOG_VERBOSE("pairing_HandleClSmAuthenticateConfirm, authentication failed ignore");
                break;
            }
        /* Fall-through */
        case PAIRING_STATE_PENDING_AUTHENTICATION:
            if (cfm->status == auth_status_success)
            {
                bool expected_ble = BtDevice_ResolvedBdaddrIsSame(&cfm->bd_addr, &thePairing->pending_ble_address);
                if(expected_ble)
                {
                    DEBUG_LOG("pairing_HandleClSmAuthenticateConfirm, BLE Device");
                }
                else
                {
                    /* Check if device db is full. If it is send CL_SM_AUTHENTICATE_CFM to itself and quit */
                    if(pairing_ResendAuthenticateCfmIfDeviceDbFull(cfm))
                    {
                        return;
                    }
                }

                BtDevice_PrintAllDevices();

                if (cfm->bonded)
                {
                    /* Update the device link mode based on the key type */
                    pairing_UpdateLinkMode(&cfm->bd_addr, cfm->key_type);

                    /* Wait for TWS version, store BT address of authenticated device */
                    thePairing->device_to_pair_with_bdaddr = cfm->bd_addr;

                    if(!expected_ble) /* Don't consider ble complete until CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND */
                    {
                        /* Send confirmation to main task */
                        pairing_Complete(thePairing, pairingSuccess, &cfm->bd_addr, TRUE);
                        /* Allow ble connections only with paired device after br/edr pairing. */
                        thePairing->ble_permission = pairingBleOnlyPairedDevices;
                        MessageSendLater(&thePairing->task, 
                                         PAIRING_INTERNAL_LE_PAIR_TIMEOUT, NULL,
                                         D_SEC(appConfigLePairingDisableTimeout()));
                    }
                }
            }
            else
            {
                /* Send confirmation with error to main task */
                pairing_Complete(thePairing, pairingAuthenticationFailed, &cfm->bd_addr, FALSE);
                /*  Do not allow ble connections when br/edr pairing is failed. */
                thePairing->ble_permission = pairingBleDisallowed;
            }
            break;

        default:
            if (cfm->status == auth_status_success)
            {
                DEBUG_LOG("pairing_HandleClSmAuthenticateConfirm, unexpected authentication");
            }
            break;
    }
}


/*! \brief Handle authorisation request for unknown device

    This function is called to handle an authorisation request for a device the
    connection library did not know about.

    If we are expecting a connection, notify the connection library if we accept
    or reject the connection.

    \note The response from the function is purely based on the state of the 
          pairing module. That is, TRUE is returned only if we are expecting
          a connection.

    \param[in] ind The authorise indication to process

    \return TRUE
*/
static bool pairing_HandleClSmAuthoriseIndication(const CL_SM_AUTHORISE_IND_T *ind)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    bool    handled = FALSE;
    bool    accepted = TRUE;

    DEBUG_LOG("pairing_HandleClSmAuthoriseIndication, state %d, protocol %d, channel %d, incoming %d",
                   pairing_GetState(thePairing), ind->protocol_id, ind->channel, ind->incoming);

    /*! \todo BLE: The pairing module doesn't handle LE connections at present */
    if (ind->protocol_id == protocol_le_l2cap)
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication Ignore. LE L2CAP");
    }
    else if ((ind->protocol_id == protocol_l2cap) && (ind->channel == 1))
    {
        /* SDP L2CAP, always allow */
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication Accept. SDP");

        handled = TRUE;
    }
    else if (appDeviceIsPeer(&ind->bd_addr))
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication Ignore. Peer");

        /* Connection is to peer, so nothing to do with handset pairing. */
    }
    else if (pairing_GetState(thePairing) == PAIRING_STATE_DISCOVERABLE)
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication Accept. Not peer and we are looking for devices.");

        /* Pairing in progress and it is early enough to cancel it
         * cancel it and accept the connection */
        Pairing_PairStop(NULL);

        handled = TRUE;
    }
    else if (pairing_GetState(thePairing) == PAIRING_STATE_PENDING_AUTHENTICATION || 
             pairing_GetState(thePairing) == PAIRING_STATE_LI_PENDING_AUTHENTICATION)
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication Reject. Awaiting pairing.");

        /* Pairing in progress, but too late to cancel the pairing,
         * refuse the connection */
        accepted = FALSE;
        handled = TRUE;
    }
    else
    {
        DEBUG_LOG("pairing_HandleClSmAuthoriseIndication Ignore (default action)");
    }

    if (handled)
    {
        ConnectionSmAuthoriseResponse(&ind->bd_addr, ind->protocol_id, ind->channel, ind->incoming, accepted);
    }
    return handled;
}


/*! \brief Handle IO capabilities request

    This function is called when the remote device wants to know the devices IO
    capabilities.  If we are pairing we respond indicating the headset has no input
    or ouput capabilities, if we're not pairing then just reject the request.
*/
static void pairing_HandleClSmIoCapabilityReqIndication(const CL_SM_IO_CAPABILITY_REQ_IND_T *ind)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    pairing_io_capability_rsp_t response =
    {
        .io_capability = cl_sm_reject_request,
        .mitm = mitm_not_required,
        .bonding = TRUE,
        .key_distribution = KEY_DIST_NONE,
        .oob_data = oob_data_none,
        .oob_hash_c = NULL,
        .oob_rand_r = NULL
    };

    DEBUG_LOG_FN_ENTRY("pairing_HandleClSmIoCapabilityReqIndication state=%d type=%d trans=%d sm_over_BREDR=%d addr=%04x %02x %06x",
                        pairing_GetState(thePairing), ind->tpaddr.taddr.type,
                        ind->tpaddr.transport,
                        ind->sm_over_bredr,
                        ind->tpaddr.taddr.addr.nap,
                        ind->tpaddr.taddr.addr.uap,
                        ind->tpaddr.taddr.addr.lap);

    if(Pairing_PluginHandleIoCapabilityRequest(ind, &response))
    {
        DEBUG_LOG_VERBOSE("pairing_HandleClSmIoCapabilityReqIndication handled by plugin");

        if(ind->tpaddr.transport == TRANSPORT_BLE_ACL)
        {
            thePairing->pending_ble_address = ind->tpaddr.taddr;
        }
        
        /* If pairing is in progress, then do not accept the bonding request from second handset */
        if((pairing_GetState(thePairing) == PAIRING_STATE_DISCOVERABLE) || (pairing_GetState(thePairing) == PAIRING_STATE_IDLE))
        {
             pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
        }
        else
        {
            response.bonding = FALSE;
        }
        /* When Fast pair plugin is activated mark the device to be paired as HANDSET */
        if(response.bonding)
        {
            thePairing->device_type = DEVICE_TYPE_HANDSET;
        }
    }
    else
    {
        if(ind->tpaddr.transport == TRANSPORT_BLE_ACL)
        {
            DEBUG_LOG_VERBOSE("pairing_HandleClSmIoCapabilityReqIndication Handling BLE pairing. Addr %06x",ind->tpaddr.taddr.addr.lap);

            switch (pairing_GetState(thePairing))
            {
                case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
                    /* Only accept IO caps from the AG whose IO caps accepted in 
                    discoverable state. Reject the IO caps from other AGs */
                    if(BdaddrIsSame(&thePairing->device_io_caps_rcvd_bdaddr, &ind->tpaddr.taddr.addr))
                    {
                        response.io_capability = cl_sm_io_cap_no_input_no_output;
                        response.key_distribution = KEY_DIST_RESPONDER_ENC_CENTRAL |
                                                    KEY_DIST_RESPONDER_ID |
                                                    KEY_DIST_INITIATOR_ENC_CENTRAL |
                                                    KEY_DIST_INITIATOR_ID;
                    }
                    break;

                case PAIRING_STATE_PENDING_AUTHENTICATION:
                    response.io_capability = cl_sm_io_cap_no_input_no_output;
                    response.key_distribution = KEY_DIST_RESPONDER_ENC_CENTRAL |
                                                KEY_DIST_RESPONDER_ID |
                                                KEY_DIST_INITIATOR_ENC_CENTRAL |
                                                KEY_DIST_INITIATOR_ID;
                    break;

                case PAIRING_STATE_DISCOVERABLE:
                case PAIRING_STATE_IDLE:
                {
                    bool random_address = (ind->tpaddr.taddr.type == TYPED_BDADDR_RANDOM);
                    bool known_address = BtDevice_isKnownBdAddr(&ind->tpaddr.taddr.addr);

                    DEBUG_LOG_VERBOSE("pairing_HandleClSmIoCapabilityReqIndication. random:%d ble_permission:%d sm_over_bredr:%d known_address:%d",
                                        random_address,
                                        thePairing->ble_permission,
                                        ind->sm_over_bredr,
                                        known_address);

                    /* Clear expected address for BLE pairing. We should not have a crossover
                       but in any case, safest to remove any pending */
                    pairing_ResetPendingBleAddress();

                        /* BLE pairing is not allowed */
                    if (thePairing->ble_permission == pairingBleDisallowed)
                    {
                        DEBUG_LOG_VERBOSE("pairing_HandleClSmIoCapabilityReqIndication. BLE pairing not allowed");
                        response.bonding = FALSE;
                        /* 
                        If the pairing state is not moved to PAIRING_STATE_LI_PENDING_AUTHENTICATION, pairing state would still
                        be in IDLE state and when CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T is received later, pairing_Complete is 
                        called with pairing_failed as status and again the pairing state will be moved to IDLE leading to a panic.
                        */
                        pairing_SetState(thePairing, PAIRING_STATE_LI_PENDING_AUTHENTICATION);
                        break;
                    }

                    /* Eliminate this being a valid Cross Transport Key derivation(CTKD) */
                    if (ind->sm_over_bredr && !known_address)
                    {
                        DEBUG_LOG_VERBOSE("pairing_HandleClSmIoCapabilityReqIndication. SMP pairing over BREDR");
                        break;
                    }

                    response.io_capability = cl_sm_io_cap_no_input_no_output;

                    response.key_distribution = (KEY_DIST_RESPONDER_ENC_CENTRAL |
                                                 KEY_DIST_RESPONDER_ID |
                                                 KEY_DIST_INITIATOR_ENC_CENTRAL |
                                                 KEY_DIST_INITIATOR_ID);

                    thePairing->pending_ble_address = ind->tpaddr.taddr;
                    /* IO caps received from AG in this state means pairing with
                    this handset is completed.*/
                    thePairing->device_io_caps_rcvd_bdaddr = ind->tpaddr.taddr.addr;

                    /* pairing started locally, IO caps from AG is received 
                    wait for authentication in LI_PENDING_AUTHENTICATION state.*/
                    pairing_SetState(thePairing, PAIRING_STATE_LI_PENDING_AUTHENTICATION);
                }
                break;

            default:
                break;
            }
        }
        else
        {
            DEBUG_LOG_VERBOSE("pairing_HandleClSmIoCapabilityReqIndication handling BREDR pairing");
            switch (pairing_GetState(thePairing))
            {
                case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
                    /* Only accept IO caps from the AG whose IO caps accepted in 
                    discoverable state. Reject the IO caps from other AGs */
                    if(BdaddrIsSame(&thePairing->device_io_caps_rcvd_bdaddr, &ind->tpaddr.taddr.addr))
                    {
                        response.io_capability = cl_sm_io_cap_no_input_no_output;
                    }
                    break;

                case PAIRING_STATE_PENDING_AUTHENTICATION:
                    response.io_capability = cl_sm_io_cap_no_input_no_output;
                    break;

                case PAIRING_STATE_DISCOVERABLE:
                    response.io_capability = cl_sm_io_cap_no_input_no_output;
                    /* IO caps received from AG in this state means pairing with
                    this handset is completed.*/
                    thePairing->device_io_caps_rcvd_bdaddr = ind->tpaddr.taddr.addr;

                    /* pairing started locally, IO caps from AG is received 
                    wait for authentication in LI_PENDING_AUTHENTICATION state.*/
                    pairing_SetState(thePairing, PAIRING_STATE_LI_PENDING_AUTHENTICATION);
                    break;

                default:
                    break;
            }
        }
    }

    DEBUG_LOG("pairing_HandleClSmIoCapabilityReqIndication, accept %d device_io_caps_rcvd_bdaddr %04x,%02x,%06lx",
                (response.io_capability != cl_sm_reject_request),
                thePairing->device_io_caps_rcvd_bdaddr.nap,
                thePairing->device_io_caps_rcvd_bdaddr.uap,
                thePairing->device_io_caps_rcvd_bdaddr.lap);

    ConnectionSmIoCapabilityResponse(&ind->tpaddr,
                                     response.io_capability,
                                     response.mitm,
                                     response.bonding,
                                     response.key_distribution,
                                     response.oob_data,
                                     response.oob_hash_c,
                                     response.oob_rand_r);
}

static void pairing_HandleClSmRemoteIoCapabilityIndication(const CL_SM_REMOTE_IO_CAPABILITY_IND_T *ind)
{
    DEBUG_LOG("pairing_HandleClSmRemoteIoCapabilityIndication %lx auth:%x io:%x oib:%x", ind->tpaddr.taddr.addr.lap,
                                                                       ind->authentication_requirements,
                                                                       ind->io_capability,
                                                                       ind->oob_data_present);

    if(Pairing_PluginHandleRemoteIoCapability(ind))
    {
        DEBUG_LOG("pairing_HandleClSmRemoteIoCapabilityIndication handled by plugin");
    }
}

/*! \brief Handle request to start pairing with a device. */
static void pairing_HandleInternalPairRequest(pairingTaskData *thePairing, PAIR_REQ_T *req)
{
    DEBUG_LOG("pairing_HandleInternalPairRequest, state %d", pairing_GetState(thePairing));

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_IDLE:
        {
            /* Store client task */
            thePairing->client_task = req->client_task;

            /* Store address of device to pair with, 0 we should go discoverable */
            thePairing->device_to_pair_with_bdaddr = req->addr;

            /* Store if pairing was initiated by user or not */
            thePairing->is_user_initiated = req->is_user_initiated ? TRUE : FALSE;

            thePairing->device_type = DEVICE_TYPE_HANDSET;

            /* no address, go discoverable for inquiry process */
            if (BdaddrIsZero(&req->addr))
            {
                /* Move to 'discoverable' state to start inquiry & page scan */
                pairing_SetState(thePairing, PAIRING_STATE_DISCOVERABLE);
            }
            else
            {
                ConnectionSmAuthenticate(appGetAppTask(), &req->addr, appConfigAuthenticationTimeout());
                pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
            }
        }
        break;

        default:
        {
            pairing_SendNotReady(req->client_task);
        }
        break;
    }
}

static void pairing_HandleInternalPairLePeerRequest(pairingTaskData *thePairing, PAIR_LE_PEER_REQ_T *req)
{
    DEBUG_LOG("pairing_HandleInternalPairLePeerRequest, state %d server %d", pairing_GetState(thePairing), req->le_peer_server);

    if(pairing_GetState(thePairing) == PAIRING_STATE_IDLE)
    {
        /* Store client task */
        thePairing->client_task = req->client_task;

        thePairing->pending_ble_address = req->typed_addr;

        BdaddrSetZero(&thePairing->device_to_pair_with_bdaddr);

        thePairing->pairing_lock = TRUE;

        thePairing->device_type = DEVICE_TYPE_EARBUD;
        /* Set ble pairing permission to AllowAll so that peers can pair with each other. */
        thePairing->ble_permission = pairingBleAllowAll;
        if(req->le_peer_server)
        {
            ConnectionDmBleSecurityReq(&thePairing->task,
                                        &req->typed_addr,
                                        ble_security_authenticated_bonded,
                                        ble_connection_slave_directed);
        }
        pairing_SetState(thePairing, PAIRING_STATE_PENDING_AUTHENTICATION);
    }
    else
    {
        pairing_SendNotReady(req->client_task);
    }
}

static void pairing_HandleInternalTimeoutIndications(pairingTaskData *thePairing)
{
    DEBUG_LOG("pairing_HandleInternalTimeoutIndications");

    switch (pairing_GetState(thePairing))
    {
        case PAIRING_STATE_DISCOVERABLE:
        case PAIRING_STATE_PENDING_AUTHENTICATION:
        case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
            /* Send confirmation with error to main task */
            pairing_Complete(thePairing, pairingTimeout, NULL, FALSE);
            break;

        default:
            break;
    }
}

/*! \brief Send client thst pairing can not be stopped now. */
static void pairing_SendInProgress(Task task)
{
    MAKE_PAIRING_MESSAGE(PAIRING_STOP_CFM);
    message->status = pairingInProgress;
    MessageSend(task, PAIRING_STOP_CFM, message);
}

/*! \brief Handle request to stop pairing. */
static void pairing_HandleInternalPairStopReq(pairingTaskData* thePairing, PAIRING_INTERNAL_PAIR_STOP_REQ_T* req)
{
    pairingState state = pairing_GetState(thePairing);
    DEBUG_LOG("pairing_HandleInternalPairStopReq enum:pairingState:%d", state);

    switch(state)
    {
        case PAIRING_STATE_IDLE:
            if(req->client_task)
            {
                MessageSend(req->client_task, PAIRING_STOP_CFM, NULL);
            }
            break;

        case PAIRING_STATE_PENDING_AUTHENTICATION:
        case PAIRING_STATE_LI_PENDING_AUTHENTICATION:
            /* Pairing already in progress. Cannot be stopped by user.
             * Either pairing completes or gets timed out */
            if(req->client_task)
            {
                pairing_SendInProgress(req->client_task);
            }
            break;

        case PAIRING_STATE_DISCOVERABLE:
            if(thePairing->stop_task)
            {
                Panic();
            }

            thePairing->stop_task = req->client_task;

            /* just send complete message with stopped status, there is an auto
            * transition back to idle after sending the message */
            pairing_Complete(thePairing, pairingStopped, NULL, FALSE);
            break;

        default:
            break;
    }
}

static void pairing_HandleClSmAddAuthDeviceConfirm(CL_SM_ADD_AUTH_DEVICE_CFM_T *cfm)
{
    DEBUG_LOG("pairing_HandlePeerSigPairHandsetConfirm %d", cfm->status);

    /* Set event indicating we've received a handset link-key.
       Can be used by client to decide to connect to the handset. */
    pairing_MsgActivity(pairingActivityLinkKeyReceived, NULL, FALSE);

    /* Tell the peer that the link key has been set ok */
    PeerLinkKeys_SendKeyResponseToPeer(&cfm->bd_addr, (cfm->status == success));
}


/*! \brief Handler for CL_SM_SEC_MODE_CONFIG_CFM.

    Handle Security mode confirmation */
static void pairing_HandleClSmSecurityModeConfigConfirm(const CL_SM_SEC_MODE_CONFIG_CFM_T *cfm)
{
    DEBUG_LOG("pairing_HandleClSmSecurityModeConfigConfirm");

    /* Check if setting security mode was successful */
    if (!cfm->success)
        Panic();
}


/*  Handle Pin code indication

    The firmware has indicated that it requires the pin code,
    pin code pairing is not supported.
*/
static void pairing_HandleClPinCodeIndication(const CL_SM_PIN_CODE_IND_T *ind)
{
    DEBUG_LOG("pairing_HandleClPinCodeIndication - not supported");

    /* Respond to the PIN code request with empty pin code */
    ConnectionSmPinCodeResponse(&ind->taddr, 0, (uint8 *)"");
}


/*  Handle the user passkey confirmation.

    This function is called to handle confirmation for the user that the passkey
    matches, since the headset doesn't have a display we just send a reject
    immediately.
*/
static void pairing_HandleClSmUserConfirmationReqIndication(const CL_SM_USER_CONFIRMATION_REQ_IND_T *ind)
{
    pairing_user_confirmation_rsp_t response;

    DEBUG_LOG("pairing_HandleClSmUserConfirmationReqIndication");

    if(!Pairing_PluginHandleUserConfirmationRequest(ind, &response))
    {
        response = pairing_user_confirmation_reject;
    }

    if(response != pairing_user_confirmation_wait)
    {
        bool accept = response != pairing_user_confirmation_reject;
        ConnectionSmUserConfirmationResponse(&ind->tpaddr, accept);
    }
}


/*! \brief Handler for CL_SM_USER_PASSKEY_REQ_IND.
*/
static void pairing_HandleClSmUserPasskeyReqIndication(const CL_SM_USER_PASSKEY_REQ_IND_T *ind)
{
    DEBUG_LOG("pairing_HandleClSmUserPasskeyReqIndication");

    ConnectionSmUserPasskeyResponse(&ind->tpaddr, TRUE, 0);
}

/*! \brief Handler for CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T . */
static void pairing_HandleClSmBleSimplePairingCompleteInd(const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T *ind)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    
    bool current_request = BdaddrTypedIsSame(&thePairing->pending_ble_address, &ind->tpaddr.taddr);
    bool any_pending = !BdaddrTypedIsEmpty(&thePairing->pending_ble_address);
    bool permanent = !BdaddrTypedIsEmpty(&ind->permanent_taddr);
    
    pairingBlePermission permission = thePairing->ble_permission;
    pairingStatus pairing_status = pairingFailed;

    DEBUG_LOG_FN_ENTRY("pairing_HandleClSmBleSimplePairingCompleteInd Any:%d Matches:%d Permission %d flags 0x%x status %d",
                        any_pending, current_request, permission, ind->flags, ind->status);

    Pairing_PluginPairingComplete();

    if (current_request && thePairing->pair_le_task != NULL)
    {
        /*  Indicate pairing status to client and remain in Discoverable state  */
        MAKE_PAIRING_MESSAGE(PAIRING_PAIR_CFM);

        DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd: LE handset status %d", ind->status);

        if (ind->status == success)
        {
            message->status = pairingSuccess;
        }
        else
        {
            message->status = pairingFailed;
        }

        message->device_bd_addr = ind->tpaddr.taddr.addr;
        MessageSend(thePairing->pair_le_task, PAIRING_PAIR_CFM, message);

        thePairing->pair_le_task = NULL;
        thePairing->ble_permission = pairingBleDisallowed;
    }
    else
    {
        if (any_pending && current_request && permission > pairingBleDisallowed)
        {
            bool is_paired = ConnectionSmGetAttributeNowReq(0, TYPED_BDADDR_PUBLIC, &ind->permanent_taddr.addr, 0, NULL);
            
            DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd is_paired:%d Permanent:%d",
                        is_paired, permanent);

            bool permitted =    (permission == pairingBleOnlyPairedDevices && is_paired)
                             || (permission == pairingBleAllowOnlyResolvable && permanent)
                             || (permission == pairingBleAllowAll);

            DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd Paired:%d Permanent:%d Permitted:%d",
                        is_paired, permanent, permitted);

            if (permitted)
            {
                if (ind->tpaddr.taddr.type == TYPED_BDADDR_PUBLIC)
                {
                    /* If remote device is on public address, set device privacy mode for it.
                     * Ideally, this is done by reading particular GATT charactertistics
                     * (RPA address only characteristics) before setting device privacy mode
                     * for it.
                     */
                    ConnectionDmUlpSetPrivacyModeReq(&ind->tpaddr.taddr, privacy_mode_device);
                }
            }
            else
            {
                /* Disconnect the link and delete device keys */
                uint16 cid = GattGetCidForBdaddr(&ind->tpaddr.taddr);

                if (cid && cid != INVALID_CID)
                {
                    DEBUG_LOG("pairing_HandleClSmBleSimplePairingCompleteInd disconnect GATT cid %d",cid);
                    GattDisconnectRequest(cid);
                }

                if (permanent)
                {
                    ConnectionSmDeleteAuthDeviceReq(ind->permanent_taddr.type, &ind->permanent_taddr.addr);
                }
                else
                {
                    ConnectionSmDeleteAuthDeviceReq(ind->tpaddr.taddr.type, &ind->tpaddr.taddr.addr);
                }
            }

            /* Pairing is accepted if status is success. */
            if (ind->status == success && permitted)
            {
                pairing_status = pairingSuccess;
            }
            pairing_ResetPendingBleAddress();
            /* Disallow ble pairing after peer pairing. ble pairing will be enabled once the pairing module enters discoverable mode.
               Disallow ble pairing once ble pairing from a ble app is done.
            */
            thePairing->ble_permission = pairingBleDisallowed;
        }
        else
        {
            DEBUG_LOG("Ble Pairing Disallowed");
        }

    }
    
    if(permanent)
    {
        pairing_Complete(thePairing, pairing_status, &ind->permanent_taddr.addr, permanent);
    }
    else
    {
        pairing_Complete(thePairing, pairing_status, &ind->tpaddr.taddr.addr, permanent);
    }
}

static void pairing_handleDmBleSecurityCfm(const CL_DM_BLE_SECURITY_CFM_T *ble_security_cfm)
{
    DEBUG_LOG("peerPairLe_handleDmBleSecurityCfm: enum:pairingState:%d enum:ble_security_status:%d lap=%06x",
              PairingGetTaskData()->state,
              ble_security_cfm->status, ble_security_cfm->taddr.addr.lap);
}

bool Pairing_HandleConnectionLibraryMessages(MessageId id,Message message, bool already_handled)
{
    pairingTaskData *thePairing = PairingGetTaskData();
    bool handled = TRUE;

    /* Check for messages that can be handled in any state */
    switch (id)
    {
        /* This can be used to initiate pairing */
        case CL_SM_IO_CAPABILITY_REQ_IND:
            if (!already_handled)
            {
                pairing_HandleClSmIoCapabilityReqIndication((CL_SM_IO_CAPABILITY_REQ_IND_T *)message);
            }
            else
            {
                handled = FALSE;
            }
            return handled;

        case CL_SM_REMOTE_IO_CAPABILITY_IND:
            if (!already_handled)
            {
                pairing_HandleClSmRemoteIoCapabilityIndication((CL_SM_REMOTE_IO_CAPABILITY_IND_T *)message);
            }
            else
            {
                handled = FALSE;
            }
            return handled;

        default:
            break;
    }

    /* Only handle connection library messages when pairing is considered active.

       This is a safeguard that has the added benefit of reducing processing when
       not active.

       The safeguard is against unexpected messages which could cause a state change.
       This can happen when a crossover with the device being paired occurs, a command 
       is sent at the same time as a command from the other device.

       The known effect of this can be receipt of two CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND
       messages. */
    if (pairing_GetState(thePairing) <= PAIRING_STATE_IDLE)
    {
        DEBUG_LOG_VERBOSE("Pairing_HandleConnectionLibraryMessages MSG:0x%x when not active enum:pairingState:%d",
                            message, pairing_GetState(thePairing));

        return FALSE;
    }

    switch (id)
    {
        case CL_SM_SEC_MODE_CONFIG_CFM:
            pairing_HandleClSmSecurityModeConfigConfirm((CL_SM_SEC_MODE_CONFIG_CFM_T *)message);
            break;

        case CL_SM_PIN_CODE_IND:
            pairing_HandleClPinCodeIndication((CL_SM_PIN_CODE_IND_T *)message);
            break;

        case CL_SM_AUTHENTICATE_CFM:
            if (!already_handled)
            {
                pairing_HandleClSmAuthenticateConfirm((CL_SM_AUTHENTICATE_CFM_T *)message);
            }
            else
            {
                handled = FALSE;
            }
            break;

        case CL_SM_AUTHORISE_IND:
            return pairing_HandleClSmAuthoriseIndication((CL_SM_AUTHORISE_IND_T *)message);

        case CL_SM_USER_CONFIRMATION_REQ_IND:
            pairing_HandleClSmUserConfirmationReqIndication((CL_SM_USER_CONFIRMATION_REQ_IND_T *)message);
            break;

        case CL_SM_USER_PASSKEY_REQ_IND:
            pairing_HandleClSmUserPasskeyReqIndication((CL_SM_USER_PASSKEY_REQ_IND_T *)message);
            break;

        case CL_SM_USER_PASSKEY_NOTIFICATION_IND:
        case CL_SM_KEYPRESS_NOTIFICATION_IND:
        case CL_DM_WRITE_APT_CFM:
            /* These messages are associated with pairing, although as
               indications they required no handling */
            break;

        case CL_DM_LINK_SUPERVISION_TIMEOUT_IND:
            break;

        case CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND:
            pairing_HandleClSmBleSimplePairingCompleteInd((const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T *)message);
            break;

        case CL_DM_BLE_SECURITY_CFM:
            pairing_handleDmBleSecurityCfm((CL_DM_BLE_SECURITY_CFM_T*)message);
            break;

        default:
            handled = FALSE;
            break;

    }
    return handled;
}


/*! \brief Message Handler

    This function is the main message handler for the pairing module.
*/
static void pairing_HandleMessage(Task task, MessageId id, Message message)
{
    pairingTaskData *thePairing = (pairingTaskData *) task;

    switch (id)
    {
        case PAIRING_INTERNAL_PAIR_REQ:
            pairing_HandleInternalPairRequest(thePairing, (PAIR_REQ_T *)message);
            break;

        case PAIRING_INTERNAL_LE_PEER_PAIR_REQ:
            pairing_HandleInternalPairLePeerRequest(thePairing, (PAIR_LE_PEER_REQ_T *)message);
            break;

        case PAIRING_INTERNAL_TIMEOUT_IND:
            pairing_HandleInternalTimeoutIndications(thePairing);
            break;

        case PAIRING_INTERNAL_PAIR_STOP_REQ:
            pairing_HandleInternalPairStopReq(thePairing, (PAIRING_INTERNAL_PAIR_STOP_REQ_T *)message);
            break;

        case PAIRING_INTERNAL_LE_PAIR_TIMEOUT:
            /* After a configured timeout to disable le pairing expires, disallow le pairing */
            thePairing->ble_permission = pairingBleDisallowed;
            DEBUG_LOG("Ble pairing disallowed. ble_permission : %d", thePairing->ble_permission);
            break;

        case CL_DM_WRITE_INQUIRY_MODE_CFM:
        case CL_DM_WRITE_INQUIRY_ACCESS_CODE_CFM:
            break;

        case CL_SM_GET_AUTH_DEVICE_CFM:
            pairing_HandleClSmGetAuthDeviceConfirm(thePairing, (CL_SM_GET_AUTH_DEVICE_CFM_T*)message);
            break;
        case CL_SM_ADD_AUTH_DEVICE_CFM:
            pairing_HandleClSmAddAuthDeviceConfirm((CL_SM_ADD_AUTH_DEVICE_CFM_T*)message);
            break;

        case CL_SM_AUTHENTICATE_CFM:
            pairing_HandleClSmAuthenticateConfirm((CL_SM_AUTHENTICATE_CFM_T*)message);
            break;

        case CL_DM_BLE_SECURITY_CFM:
            pairing_handleDmBleSecurityCfm((CL_DM_BLE_SECURITY_CFM_T*)message);
            break;

        default:
            UnexpectedMessage_HandleMessage(id);
            break;
    }
}

bool Pairing_Init(Task init_task)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("Pairing_Init");

    /* Set up task handler */
    thePairing->task.handler = pairing_HandleMessage;

    TaskList_InitialiseWithCapacity(PairingGetClientList(), PAIRING_CLIENT_TASK_LIST_INIT_CAPACITY);

    /* Initialise state */
    thePairing->state = PAIRING_STATE_NULL;
    thePairing->pairing_lock = 1;
    pairing_SetState(thePairing, PAIRING_STATE_INITIALISING);
    /* Do not enabled LE pairing always since it is a potential security issue. */
    thePairing->ble_permission = pairingBleDisallowed;
    thePairing->stop_task = NULL;
    TaskList_InitialiseWithCapacity(PairingGetPairingActivity(), PAIRING_ACTIVITY_LIST_INIT_CAPACITY);

    Pairing_PluginInit();

    /* register pairing as a client of the peer signalling task, it means
     * we will may get PEER_SIG_CONNECTION_IND messages if the signalling
     * channel becomes available again to retry sending the link key */
    appPeerSigClientRegister(&thePairing->task);

    Ui_RegisterUiProvider(ui_provider_handset_pairing, pairing_GetCurrentContext);

    return TRUE;
}

/*! \brief Pair with a device, where inquiry scanning is required. */
void Pairing_Pair(Task client_task, bool is_user_initiated)
{
    MAKE_PAIRING_MESSAGE(PAIR_REQ);
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_Pair");

    message->client_task = client_task;
    message->is_user_initiated = is_user_initiated ? TRUE : FALSE;

    BdaddrSetZero(&message->addr);
    MessageSendConditionally(&thePairing->task, PAIRING_INTERNAL_PAIR_REQ,
                             message, &thePairing->pairing_lock);
}

/*! \brief Pair with a device where the address is already known.
 */
void Pairing_PairAddress(Task client_task, bdaddr* device_addr)
{
    MAKE_PAIRING_MESSAGE(PAIR_REQ);
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_PairAddress");

    message->client_task = client_task;
    message->addr = *device_addr;
    message->is_user_initiated = FALSE;

    MessageSendConditionally(&thePairing->task, PAIRING_INTERNAL_PAIR_REQ,
                             message, &thePairing->pairing_lock);
}

/*! \brief Pair with le peer device. This is a temporary function until
           discovery is removed from the pairing module.
 */
void Pairing_PairLePeer(Task client_task, typed_bdaddr* device_addr, bool server)
{
    MAKE_PAIRING_MESSAGE(PAIR_LE_PEER_REQ);
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_PairLePeer state %d", pairing_GetState(thePairing));

    message->client_task = client_task;
    message->le_peer_server = server;

    if(device_addr)
    {
        message->typed_addr = *device_addr;
    }
    else
    {
        BdaddrTypedSetEmpty(&message->typed_addr);
    }

    MessageSendConditionally(&thePairing->task, PAIRING_INTERNAL_LE_PEER_PAIR_REQ,
                             message, &thePairing->pairing_lock);
}

void Pairing_PairLeAddress(Task client_task, const typed_bdaddr* device_addr)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_PairLeAddress state %d addr %04x %02x %06x",
              pairing_GetState(thePairing),
              device_addr->addr.nap,
              device_addr->addr.uap,
              device_addr->addr.lap);

    /*  Only supported in Discoverable state
     *  Pairing plugin handles handset pairing so don't interfere if one is set up */
    if ((pairing_GetState(thePairing) == PAIRING_STATE_DISCOVERABLE) && !Pairing_PluginIsRegistered())
    {

        thePairing->pair_le_task = client_task;
        thePairing->pending_ble_address = *device_addr;


        ConnectionDmBleSecurityReq(&thePairing->task,
                        device_addr,
                        ble_security_authenticated_bonded,
                        ble_connection_slave_directed);
    }
    else
    {
        pairing_SendNotReady(client_task);
    }
}

/*! @brief Stop a pairing.
 */
void Pairing_PairStop(Task client_task)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    MAKE_PAIRING_MESSAGE(PAIRING_INTERNAL_PAIR_STOP_REQ);

    DEBUG_LOG("Pairing_PairStop");

    message->client_task = client_task;

    MessageSend(&thePairing->task, PAIRING_INTERNAL_PAIR_STOP_REQ, message);
}

/* Register to receive #PAIRING_ACTIVITY messages. */
void Pairing_ActivityClientRegister(Task task)
{
    DEBUG_LOG_FN_ENTRY("Pairing_ActivityClientRegister");

    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(PairingGetPairingActivity()), task);
}

/* Unregister from #PAIRING_ACTIVITY messages. */
void Pairing_ActivityClientUnregister(Task task)
{
    DEBUG_LOG_FN_ENTRY("Pairing_ActivityClientUnregister");

    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(PairingGetPairingActivity()), task);
}

/*! brief Add a device to the PDL. */
void Pairing_AddAuthDevice(const bdaddr* address, const uint16 key_length, const uint16* link_key)
{
    pairingTaskData *thePairing = PairingGetTaskData();

    DEBUG_LOG("Pairing_AddAuthDevice");

    /* Store the link key in the PDL */
    ConnectionSmAddAuthDevice(&thePairing->task,
                              address,
                              FALSE,
                              TRUE,
                              cl_sm_link_key_unauthenticated_p192, /* TODO is this right? */
                              key_length,
                              link_key);
}

static void pairing_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == PAIRING_MESSAGE_GROUP);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), task);
}

/*
 * TEST FUNCTIONS
 */
void Pairing_SetLinkTxReqd(void)
{
    bdaddr handset_addr;

    DEBUG_LOG("Setting handset link key TX is required");
    appDeviceGetHandsetBdAddr(&handset_addr);
    BtDevice_SetHandsetLinkKeyTxReqd(&handset_addr, TRUE);
}

void Pairing_ClearLinkTxReqd(void)
{
    bdaddr handset_addr;

    DEBUG_LOG("Pairing_ClearLinkTxReqd");
    appDeviceGetHandsetBdAddr(&handset_addr);
    BtDevice_SetHandsetLinkKeyTxReqd(&handset_addr, FALSE);
}

void Pairing_SendPairingCompleteMessageToClients(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(PairingGetClientList()), PAIRING_COMPLETE);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(PAIRING, pairing_RegisterMessageGroup, NULL);

