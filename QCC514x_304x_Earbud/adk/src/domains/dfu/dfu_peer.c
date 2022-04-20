/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_peer.c
\brief      Macros and Routines for establishing the DFU L2CAP Channel
*/

#ifdef INCLUDE_DFU_PEER

#include "dfu_peer.h"
#include <bluestack/l2cap_prim.h>
#include <util.h>
#include <service.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include "sdp.h"
#include "system_state.h"
#include "phy_state.h"
#include "bt_device.h"
#include <logging.h>
#include <connection_manager.h>
#include <message.h>
#include "dfu.h"
#include <panic.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include "dfu_config.h"


#ifdef INCLUDE_MIRRORING
#include "mirror_profile_protected.h"
#endif

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(dfu_peer_internal_messages_t)
LOGGING_PRESERVE_MESSAGE_ENUM(dfu_peer_messages_t)

/* Ensure message range is legal */
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(DFU_PEER_INTERNAL_MESSAGE_END)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(DFU_PEER, DFU_PEER_MESSAGE_END)

/* L2CAP Connection specific macros */
#define MAX_ATTRIBUTES 0x32
#define EXACT_MTU 672
#define MINIMUM_MTU 48

/* Re-try ACL link establishment on link loss 20 times until the link is established*/
#define ACL_CONNECT_RETRY_LIMIT 20

/* Re-try SDP Search 10 times until the link is established */
#define SDP_SEARCH_RETRY_LIMIT 10

/* Counter incremented during re-try of ACL link establishment on link loss or during SDP search */
uint16 acl_connect_attempts, sdp_connect_attempts = 0;

/* Macro to Check for the SDP status for which retry is needed */
#define SDP_STATUS(x) (x == sdp_no_response_data) || \
                      (x == sdp_con_disconnected) || \
                      (x == sdp_connection_error) || \
                      (x == sdp_search_data_error) || \
                      (x == sdp_search_busy) || \
                      (x == sdp_response_timeout_error) || \
                      (x == sdp_response_out_of_memory) || \
                      (x == sdp_connection_error_page_timeout) || \
                      (x == sdp_connection_error_rej_resources) || \
                      (x == sdp_connection_error_signal_timeout) || \
                      (x == sdp_search_unknown)


/* Macros to get Device Upgrade Peer Task information */
dfu_peer_task_data_t dfu_peer;
#define dfuPeer_GetTaskData()    (&dfu_peer)
#define dfuPeer_GetTask()  (&(dfuPeer_GetTaskData()->task))
#define dfuPeer_GetClientList()  (task_list_flexible_t *)(&(dfuPeer_GetTaskData()->client_list))

/* Macro to check if Device Upgrade Peer still in use */
#define dfuPeer_IsInUse() (dfu_peer.state > DFU_PEER_STATE_IDLE)


/*! Macro to make a message. */
#define MAKE_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! Check if the state is Disconnecting and message is NOT CL_L2CAP_DISCONNECT_CFM */

#define dfuPeer_IsStateDisconnecting(id, message) \
    (dfuPeer_GetState() == DFU_PEER_STATE_DISCONNECTING \
     && (id) != CL_L2CAP_DISCONNECT_CFM ? TRUE: FALSE)


static dfu_peer_state_t dfuPeer_GetState(void);
static void dfuPeer_ConnectL2cap(const bdaddr *bd_addr);

/******************************************************************************
 * General Definitions
 ******************************************************************************/

/*! \brief Notify Peer Device Connection Establishment (Peer Device Start) to Application
*/
static void dfuPeer_NotifyStart(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(dfuPeer_GetClientList()),
                           DFU_PEER_STARTED);
}

/*! \brief Notify Peer Device Disconnection to Application
*/
static void dfuPeer_NotifyDisconnect(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(dfuPeer_GetClientList()),
                           DFU_PEER_DISCONNECTED);
}

/*! \brief Send L2CAP peer connection failure to Upgrade Library
*/
static void dfuPeer_SendL2capConnectFailure(void)
{
    DEBUG_LOG("dfuPeer_SendL2capConnectFailure DFUPeer L2cap connection failure request");
    upgrade_peer_connect_state_t l2cap_status = UPGRADE_PEER_CONNECT_FAILED;
    UpgradePeerProcessDataRequest(UPGRADE_PEER_CONNECT_CFM,
                                 (uint8 *)&l2cap_status, sizeof(uint16));
}

/*! \brief Send L2CAP peer connection success to Upgrade Library
*/
static void dfuPeer_SendL2capConnectSuccess(void)
{
    upgrade_peer_connect_state_t l2cap_status = UPGRADE_PEER_CONNECT_SUCCESS;
#ifdef INCLUDE_MIRRORING
    DfuPeer_SetLinkPolicy(lp_active);
#endif
    /* WTF: Passing enum value, by taking address casting to uint8 and then passing sizeof(uint16)??? */
    UpgradePeerProcessDataRequest(UPGRADE_PEER_CONNECT_CFM,
                                 (uint8 *)&l2cap_status, sizeof(uint16));
}

/*! \brief Handle Device Upgrade Peer Connected State
*/
static void dfuPeer_Connected(dfu_peer_state_t old_state)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    bool isPrimary = BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfuPeer_Connected");

    UpgradePeerSetConnectedStatus(TRUE);

    /* Peer devices are connected now. Set the device roles */
    DfuPeer_SetRole(isPrimary);

    if(the_dfu_peer->is_primary)
    {
        /*
         * In the post reboot DFU commit phase, now main role
         * (Primary/Secondary) are no longer fixed rather dynamically
         * selected by Topology using role selection. So if a role swap
         * occurs in the post reboot DFU commit phase.
         * (e.g. Primary on post reboot DFU commit phase becomes Secondary
         * In this scenario, the peer DFU L2CAP channel is established
         * by Old Primary and so is the necessary pre-condition is to have
         * SmCtx created inorder to send UPGRADE_HOST_IN_PROGRESS_RES to the
         * peer.
         * The following is not required, as post reboot DFU commit phase is the
         * last phase and these are insignificant for the DFU to successfully
         * progress to completion. This is mainly because
         * UPGRADE_HOST_IN_PROGRESS_RES is an unsolicited DFU response pdu sent
         * by the Host even without a preceding UPGRADE_HOST_IN_PROGRESS_IND.
         * (Refer: CS-00347923-SP, Table B-1 and Figure 3-1)
         *
         *      upgradePeerInfo->SmCtx->mResumePoint =
         *          upgradePeerInfo->UpgradePSKeys.upgradeResumePoint;
         *      upgradePeerInfo->SmCtx->isUpgrading = TRUE;
         *      UpgradePeerSetState(UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT);
         */
        UpgradePeerCtxInit();

        dfuPeer_SendL2capConnectSuccess();
    }
    else
    {
        /* For primary, the context will be set in app.
         * Since upg_host_type is used to decide the resume methodology and 
         * for PEB to SEB resume always uses GAIA resume methodology hence
         * it is set as GAIA context for secondary here.
         */
        Upgrade_SetHostType(UPGRADE_CONTEXT_GAIA);
        UpgradeTransportConnectRequest(dfuPeer_GetTask(),
                                       UPGRADE_DATA_CFM_ALL,
                                       UPGRADE_MAX_REQUEST_SIZE_NO_LIMIT);
    }
    /* Notify both Initiator and Peer App that Peer Upgrade has started.*/
    dfuPeer_NotifyStart();

    if(old_state == DFU_PEER_STATE_CONNECTING_LOCAL)
    {
        /* We have finished (successfully or not) attempting to connect, so
         * we can relinquish our lock on the ACL.  Bluestack will then close
         * the ACL when there are no more L2CAP connections */
        ConManagerReleaseAcl(&the_dfu_peer->peer_addr);
    }

    if(Dfu_GetRebootReason() == REBOOT_REASON_DFU_RESET &&
       Dfu_IsSilentCommitEnabled())
    {
        /* If peer is supported, commit new image here for silent commit option*/
        DEBUG_LOG("dfuPeer_Connected: UpgradeCommitConfirmForSilentCommit");
        UpgradeCommitConfirmForSilentCommit();

        /* Since both EBs will commit individually in case of silent commit,
         * UPGRADE_PEER_COMPLETE_IND will not be sent to primary from secondary
         * and DFU peer will not be disconnected.
         * So disconnect DFU peer channel specifically here.
         */
        if(the_dfu_peer->is_primary)
        {
            DfuPeer_InitiatePeerLinkDisconnection();
        }
    }
}

/*! \brief Handle Device Upgrade Peer Disconnected State
*/
static void dfuPeer_Disconnected(dfu_peer_state_t old_state)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    DEBUG_LOG("dfuPeer_Disconnected");

    UpgradePeerSetConnectedStatus(FALSE);

    if(old_state == DFU_PEER_STATE_CONNECTED ||
       old_state == DFU_PEER_STATE_DISCONNECTING)
    {
        if (the_dfu_peer->is_primary)
        {
            UpgradePeerProcessDataRequest(UPGRADE_PEER_DISCONNECT_IND, NULL, 0);
        }
        else
        {
            UpgradeTransportDisconnectRequest();
        }
    }
    else
    {
        /* If silent commit command is recieved, do not send L2CAP connect
         * failure for this scenario which will cause DFU abort.
         */
        if(!Dfu_IsSilentCommitEnabled())
        {
            dfuPeer_SendL2capConnectFailure();
        }
    }

    /*
     * Reset the state variables of peer (those that are maintained here and
     * by library), as these can be inappropriate when DFU has ended/aborted
     * owing to a handover.
     * This is needed because currently DFU data is not marshalled during
     * an handover.
     */
    the_dfu_peer->is_primary = FALSE;
    UpgradePeerResetStateInfo();

    /* Disconnect process is completed. Moved to IDLE state to accept new
     * connection.
     */
    the_dfu_peer->state = DFU_PEER_STATE_IDLE;

    /* Notify both Initiator and Peer App that Peers are disconnected.*/
    dfuPeer_NotifyDisconnect();

    /* If silent commit is enabled, and if DFU peer disconnects (because of
     * handover, not because of link loss), reconnect DFU peer for commit phase.
     */
    if(Dfu_IsSilentCommitEnabled() && !UpgradePeerIsLinkLoss() && !UpgradePeerIsBlocked())
    {
        bool is_primary = BtDevice_IsMyAddressPrimary();
        DfuPeer_SetRole(is_primary);

        if(is_primary)
        {
            /* Initiate DFU peer link from primary device */
            DfuPeer_InitiateLinkToPeer();
        }
    }
}


/*! \brief Handle Device Upgrade Peer Disconnecting State
*/
static void dfuPeer_Disconnecting(void)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    DEBUG_LOG("dfuPeer_Disconnecting");

    ConnectionL2capDisconnectRequest(dfuPeer_GetTask(),
                                     the_dfu_peer->link_sink);
}

/*! \brief Handle Device Upgrade Peer Initialise State
*/
static void dfuPeer_Initialise(void)
{
    DEBUG_LOG("dfuPeer_Initialise");

    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    /* Register a Protocol/Service Multiplexor (PSM) that will be
       used for this application. The same PSM is used at both
       ends. */
    ConnectionL2capRegisterRequest(&the_dfu_peer->task, L2CA_PSM_INVALID, 0);
}

/*! \brief Handle Device Upgrade Peer Idle State
*/
static void dfuPeer_Idle(void)
{
    DEBUG_LOG("dfuPeer_Idle");

    /* Send messge to forward to the app to unblock initialisation */
    MessageSend(SystemState_GetTransitionTask(), DFU_PEER_INIT_CFM, NULL);

    /* Initialize Upgrade Peer DFU.
     * The primary upgrade key size is 18 words. For peer upgrade currently
     * 6 words need to be used. Same UPGRADE_CONTEXT_KEY PSKey is used
     * with offset UPGRADE_PEER_CONTEXT_OFFSET
     */
    UpgradePeerInit(dfuPeer_GetTask(), UPGRADE_CONTEXT_KEY,
            UPGRADE_PEER_CONTEXT_OFFSET);
    UpgradeSetPeerDfuSupport(TRUE);
}



/*! \brief Handle Device Upgrade Peer Sdp Search State
*/
static void dfuPeer_SdpSearch(void)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    bdaddr peer_bd_addr;

    DEBUG_LOG("dfuPeer_SdpSearch");
    if(!appDeviceGetPeerBdAddr(&peer_bd_addr))
    {
        /* Disconnect the l2cap connection and put the device state to disconnected */
        the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
        dfuPeer_SendL2capConnectFailure();
        return;
    }

    /* Perform SDP search */
    ConnectionSdpServiceSearchAttributeRequest(&the_dfu_peer->task,
                                               &peer_bd_addr, MAX_ATTRIBUTES,
                         appSdpGetDFUPeerServiceSearchRequestSize(),
                         appSdpGetDFUPeerServiceSearchRequest(),
                         appSdpGetDFUPeerAttributeSearchRequestSize(),
                         appSdpGetDFUPeerAttributeSearchRequest());
}

/*! \brief Get the Device Upgrade Peer FSM state

    Called to get current Device Upgrade Peer FSM state
*/
static dfu_peer_state_t dfuPeer_GetState(void)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    return the_dfu_peer->state;
}

/*! \brief Set the Upgrade Peer FSM state

    Called to change state. Handles calling the state entry and exit
    functions for the new and old states.
*/
static void dfuPeer_SetState(dfu_peer_state_t state)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    dfu_peer_state_t old_state = the_dfu_peer->state;

    DEBUG_LOG("dfuPeer_SetState enum:dfu_peer_state_t:%d -> enum:dfu_peer_state_t:%d", old_state, state);

    /* Set new state */
    the_dfu_peer->state = state;

    /* Handle state functions */
    switch (state)
    {
        case DFU_PEER_STATE_INITIALISE:
            dfuPeer_Initialise();
            break;
        case DFU_PEER_STATE_IDLE:
            dfuPeer_Idle();
            break;
        case DFU_PEER_STATE_DISCONNECTED:
            dfuPeer_Disconnected(old_state);
            break;
        case DFU_PEER_STATE_CONNECTING_ACL:
            DEBUG_LOG("dfuPeer_SetState DFUPeerConnectingAcl");
            break;
        case DFU_PEER_STATE_CONNECTING_SDP_SEARCH:
            dfuPeer_SdpSearch();
            break;
        case DFU_PEER_STATE_CONNECTING_LOCAL:
            DEBUG_LOG("dfuPeer_SetState DFUPeerConnectingLocal");
            break;
        case DFU_PEER_STATE_CONNECTING_REMOTE:
            DEBUG_LOG("dfuPeer_SetState DFUPeerConnectingRemote");
            break;
        case DFU_PEER_STATE_CONNECTED:
            dfuPeer_Connected(old_state);
            break;
        case DFU_PEER_STATE_DISCONNECTING:
            dfuPeer_Disconnecting();
            break;
        default:
            break;
    }
}

/*! \brief Disconnect the l2cap connection when device upgrade error is called and put the state to disconnected
*/
static void dfuPeer_Error(MessageId id)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    UNUSED(id);
    DEBUG_LOG("dfuPeer_Error, state %u, id MESSAGE:0x%x", dfuPeer_GetState(), id);

    the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
    dfuPeer_SendL2capConnectFailure();
}

/*! \brief Request for initiating the L2CAP connection with peer device
*/
static void dfuPeer_Startup(const bdaddr *peer_addr)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    MAKE_MESSAGE(DFU_PEER_INTERNAL_STARTUP_REQ);
    message->peer_addr = *peer_addr;
    MessageSend(&the_dfu_peer->task,
                DFU_PEER_INTERNAL_STARTUP_REQ, message);
}

/*! \brief Request to create a L2CAP connection with the Peer device

    Called to create a L2CAP connection with the Peer device for device upgrade
*/
static void dfuPeer_HandleInternalStartupRequest(DFU_PEER_INTERNAL_STARTUP_REQ_T *req)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    DEBUG_LOG("dfuPeer_HandleInternalStartupRequest, state %u, bdaddr %04x,%02x,%06lx",
               dfuPeer_GetState(),
               req->peer_addr.nap, req->peer_addr.uap, req->peer_addr.lap);

    switch (dfuPeer_GetState())
    {
        case DFU_PEER_STATE_CONNECTING_ACL:
        case DFU_PEER_STATE_CONNECTING_SDP_SEARCH:
        {
            /* Check if ACL is now up */
            if (ConManagerIsConnected(&req->peer_addr))
            {
                DEBUG_LOG("dfuPeer_HandleInternalStartupRequest, ACL connected");

                /* Initiate if we created the ACL, or was previously rejected */
                if (ConManagerIsAclLocal(&req->peer_addr) ||
                    BdaddrIsSame(&the_dfu_peer->peer_addr,
                                 &req->peer_addr))
                {
                    if(dfuPeer_GetState() == DFU_PEER_STATE_CONNECTING_ACL)
                    {
                        DEBUG_LOG("dfuPeer_HandleInternalStartupRequest, ACL locally initiated");

                        /* Store address of peer */
                        the_dfu_peer->peer_addr = req->peer_addr;

                        /* Begin the search for the peer signalling SDP record */
                        dfuPeer_SetState(DFU_PEER_STATE_CONNECTING_SDP_SEARCH);
                    }
                    else
                    {
                        sdp_connect_attempts += 1;
                        if(sdp_connect_attempts <= SDP_SEARCH_RETRY_LIMIT)
                        {
                            /* Try SDP Search again */
                            DEBUG_LOG("dfuPeer_HandleInternalStartupRequest SDP Search retrying again, %d", sdp_connect_attempts);
                            dfuPeer_SetState(DFU_PEER_STATE_CONNECTING_SDP_SEARCH);
                        }
                        else
                        {
                            /* Peer Earbud doesn't support Device Upgrade service */
                            DEBUG_LOG("dfuPeer_HandleInternalStartupRequest Device Upgrade SDP Service unsupported, go to disconnected state");
                            dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
                        }
                    }
                }
                else
                {
                    DEBUG_LOG("dfuPeer_HandleInternalStartupRequest, ACL remotely initiated");

                    /* Not locally initiated ACL, move to 'Disconnected' state */
                    dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
                }
            }
            else
            {
                if(acl_connect_attempts >= ACL_CONNECT_RETRY_LIMIT)
                {
                    DEBUG_LOG("dfuPeer_HandleInternalStartupRequest, ACL failed to open, giving up");
                    /* ACL failed to open, move to 'Disconnected' state */
                    dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
                }
                else
                {
                    acl_connect_attempts++;
                    DEBUG_LOG("dfuPeer_HandleInternalStartupRequest ACL Connection retrying again, %d", acl_connect_attempts);
                     /* Post message back to ourselves, blocked on creating ACL */
                    MAKE_MESSAGE(DFU_PEER_INTERNAL_STARTUP_REQ);
                    message->peer_addr = req->peer_addr;
                    MessageSendConditionally(&the_dfu_peer->task,
                          DFU_PEER_INTERNAL_STARTUP_REQ, message,
                          ConManagerCreateAcl(&req->peer_addr));
                    return;
                }
            }
        }
        break;
        case DFU_PEER_STATE_IDLE:
        {
            DEBUG_LOG("dfuPeer_HandleInternalStartupRequest, ACL not connected, attempt to open ACL");

            /* Post message back to ourselves, blocked on creating ACL */
            MAKE_MESSAGE(DFU_PEER_INTERNAL_STARTUP_REQ);
            message->peer_addr = req->peer_addr;
            MessageSendConditionally(&the_dfu_peer->task,
                         DFU_PEER_INTERNAL_STARTUP_REQ,
                         message, ConManagerCreateAcl(&req->peer_addr));

            /* Wait in 'Connecting ACL' state for ACL to open */
            dfuPeer_SetState(DFU_PEER_STATE_CONNECTING_ACL);
            return;
        }

        case DFU_PEER_STATE_CONNECTED:
            /* Already connected, just ignore startup request */
            break;

        default:
            dfuPeer_Error(DFU_PEER_INTERNAL_STARTUP_REQ);
            break;
    }

    /* Cancel any other startup requests */
    MessageCancelAll(&the_dfu_peer->task,
                     DFU_PEER_INTERNAL_STARTUP_REQ);
}


/******************************************************************************
 * Handlers for upgrade peer channel L2CAP messages
 ******************************************************************************/

/*! \brief Initiate an L2CAP connection request to the peer
*/
static void dfuPeer_ConnectL2cap(const bdaddr *bd_addr)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    bdaddr peer_bd_addr;

    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
            L2CAP_AUTOPT_SEPARATOR,
        /* Flow & Error Control Mode. */
            L2CAP_AUTOPT_FLOW_MODE,
        /* Set to Basic mode with no fallback mode */
            BKV_16_FLOW_MODE( FLOW_MODE_BASIC, 0 ),
        /* Local MTU exact value (incoming). */
            L2CAP_AUTOPT_MTU_IN,
        /*  Exact MTU for this L2CAP connection - 672. */
            EXACT_MTU,
        /* Remote MTU Minumum value (outgoing). */
            L2CAP_AUTOPT_MTU_OUT,
        /*  Minimum MTU accepted from the Remote device. */
            MINIMUM_MTU,
         /* Local Flush Timeout  - Accept Non-default Timeout*/
            L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(DEFAULT_L2CAP_FLUSH_TIMEOUT,0),

        /* Configuration Table must end with a terminator. */
            L2CAP_AUTOPT_TERMINATOR
    };

    DEBUG_LOG("dfuPeer_ConnectL2cap");

    /* If the below scenarios fails, then send connect failure to upgrade library */
    if(!appDeviceGetPeerBdAddr(&peer_bd_addr) ||
       !BdaddrIsSame(bd_addr, &peer_bd_addr))
    {
        the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
        dfuPeer_SendL2capConnectFailure();
        return;
    }

    ConnectionL2capConnectRequest(&the_dfu_peer->task,
                                  bd_addr,
                                  the_dfu_peer->local_psm,
                                  the_dfu_peer->remote_psm,
                                  CONFTAB_LEN(l2cap_conftab),
                                  l2cap_conftab);
}

static void dfuPeer_ProcessL2CAPDataOnConnection(void)
{
    dfu_peer_task_data_t *the_dfu_peer =  dfuPeer_GetTaskData();
    Source source = the_dfu_peer->link_source;

    if(SourceBoundary(source))
    {
        DEBUG_LOG("dfuPeer_ProcessL2CAPDataOnConnection data available.");
        /*
         * There is data in the L2CAP source to be processed.
         * Trigger an internal message to procees it
         */
        MessageSend(&the_dfu_peer->task, DFU_PEER_INTERNAL_MESSAGE_MORE_DATA,
                        NULL);
    }
}

/*! \brief Handle result of L2CAP PSM registration request
*/
static void dfuPeer_HandleL2capRegisterCfm(const CL_L2CAP_REGISTER_CFM_T *cfm)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    DEBUG_LOG("dfuPeer_HandleL2capRegisterCfm, status %u, psm %u",
                cfm->status, cfm->psm);

    /* We have registered the PSM used for SCO forwarding links with
       connection manager, now need to wait for requests to process
       an incoming connection or make an outgoing connection. */
    if (success == cfm->status)
    {
        /* Keep a copy of the registered L2CAP PSM, maybe useful later */
        the_dfu_peer->local_psm = cfm->psm;

        /* Copy and update SDP record */
        uint8 *record = PanicUnlessMalloc(appSdpGetDFUPeerServiceRecordSize());
        memcpy(record, appSdpGetDFUPeerServiceRecord(),
                       appSdpGetDFUPeerServiceRecordSize());

        /* Write L2CAP PSM into service record */
        appSdpSetDFUPeerPsm(record, cfm->psm);

        /* Register service record */
        ConnectionRegisterServiceRecord(dfuPeer_GetTask(),
                         appSdpGetDFUPeerServiceRecordSize(), record);
    }
    else
    {
     /* Since the L2CAP PSM registration failed, we are moving the state
      * to DISCONNECTED, and won't accept any L2CAP channel creation request
      */
        DEBUG_LOG("dfuPeer_HandleL2capRegisterCfm, failed to register L2CAP PSM");
        the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
    }
}

/*! \brief Handle result of the SDP service record registration request
*/
static void dfuPeer_HandleClSdpRegisterCfm(const CL_SDP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG("dfuPeer_HandleClSdpRegisterCfm, status %d", cfm->status);

    if (cfm->status == sds_status_success)
    {
        /* Move to 'idle' state */
        dfuPeer_SetState(DFU_PEER_STATE_IDLE);
    }
    else
    {
     /* Since the SDP service record registration failed, we are moving the state
      * to DISCONNECTED, and won't accept any L2CAP channel creation request
      */
        dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

        DEBUG_LOG("dfuPeer_HandleL2capRegisterCfm, failed to register L2CAP PSM");
        the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
    }
}

/*! \brief Extract the remote PSM value from a service record returned by a SDP service search
*/
static bool dfuPeer_GetL2capPSM(const uint8 *begin,
                                       const uint8 *end, uint16 *psm, uint16 id)
{
    ServiceDataType type;
    Region record, protocols, protocol, value;
    record.begin = begin;
    record.end   = end;

    while (ServiceFindAttribute(&record, id, &type, &protocols))
        if (type == sdtSequence)
            while (ServiceGetValue(&protocols, &type, &protocol))
            if (type == sdtSequence
               && ServiceGetValue(&protocol, &type, &value)
               && type == sdtUUID
               && RegionMatchesUUID32(&value, (uint32)UUID16_L2CAP)
               && ServiceGetValue(&protocol, &type, &value)
               && type == sdtUnsignedInteger)
            {
                *psm = (uint16)RegionReadUnsigned(&value);
                return TRUE;
            }

    return FALSE;
}

/*! \brief Handle the result of a SDP service attribute search
*/
static void dfuPeer_HandleClSdpServiceSearchAttributeCfm(const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    DEBUG_LOG("dfuPeer_HandleClSdpServiceSearchAttributeCfm, status %d",
              cfm->status);

    switch (dfuPeer_GetState())
    {
        case DFU_PEER_STATE_CONNECTING_SDP_SEARCH:
        {
            /* Find the PSM in the returned attributes */
            if (cfm->status == sdp_response_success)
            {
                if (dfuPeer_GetL2capPSM(cfm->attributes,
                                                 cfm->attributes +
                                                 cfm->size_attributes,
                   &the_dfu_peer->remote_psm, saProtocolDescriptorList))
                {
                    DEBUG_LOG("dfuPeer_HandleClSdpServiceSearchAttributeCfm, peer psm 0x%x", the_dfu_peer->remote_psm);

                    /* Initate outgoing peer L2CAP connection */
                    dfuPeer_ConnectL2cap(&the_dfu_peer->peer_addr);
                    dfuPeer_SetState(DFU_PEER_STATE_CONNECTING_LOCAL);
                }
                else
                {
                    /* No PSM found, malformed SDP record on peer? */
                    DEBUG_LOG("dfuPeer_HandleClSdpServiceSearchAttributeCfm, malformed SDP record");
                    dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
                }
            }
            /* Check if retry needed for valid status */
            else if (SDP_STATUS(cfm->status))
            {
                MAKE_MESSAGE(DFU_PEER_INTERNAL_STARTUP_REQ);
                message->peer_addr = cfm->bd_addr;
                /* Try the SDP Search again after 1 sec */
                MessageSendLater(&the_dfu_peer->task,
                    DFU_PEER_INTERNAL_STARTUP_REQ, message, D_SEC(1));
            }
            else
            {
                /* SDP search failed */
                DEBUG_LOG("dfuPeer_HandleClSdpServiceSearchAttributeCfm, SDP search failed");
                dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
            }
        }
        break;

        default:
        {
            DEBUG_LOG("dfuPeer_HandleClSdpServiceSearchAttributeCfm, unexpected state %d status %d", dfuPeer_GetState(), cfm->status);
         /* Silently ignore, not the end of the world */
        }
        break;
    }
}

/*! \brief Handle a L2CAP connection request that was initiated by the remote peer device
*/
static void dfuPeer_HandleL2capConnectInd(const CL_L2CAP_CONNECT_IND_T *ind)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    bool accept = FALSE;

    DEBUG_LOG("dfuPeer_HandleL2capConnectInd, state %u, psm %u",
                dfuPeer_GetState(), ind->psm);

    /* If the PSM doesn't macthes, then send l2cap failure message to upgrade
     * library and put the device in disconnected state
     */
    if(!(ind->psm == the_dfu_peer->local_psm))
    {
        the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
        dfuPeer_SendL2capConnectFailure();
        return;
    }

    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR,
        /* Local Flush Timeout  - Accept Non-default Timeout*/
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(DEFAULT_L2CAP_FLUSH_TIMEOUT,0),
        L2CAP_AUTOPT_TERMINATOR
    };

    switch (dfuPeer_GetState())
    {
        case DFU_PEER_STATE_IDLE:
        {
            /* only accept Peer connections from paired peer devices. */
            if (appDeviceIsPeer(&ind->bd_addr))
            {
                DEBUG_LOG("dfuPeer_HandleL2capConnectInd, accepted");

                dfuPeer_SetState(DFU_PEER_STATE_CONNECTING_REMOTE);

                /* Accept connection */
                accept = TRUE;
            }
            else
            {
                DEBUG_LOG("dfuPeer_HandleL2capConnectInd, rejected, unknown peer");
            }
        }
        break;

        default:
        {
            DEBUG_LOG("dfuPeer_HandleL2capConnectInd, rejected, state %u",
                       dfuPeer_GetState());
        }
        break;
    }

    /* Send a response accepting or rejcting the connection. */
    ConnectionL2capConnectResponse(&the_dfu_peer->task,     /* The client task. */
                                   accept,                 /* Accept/reject the connection. */
                                   ind->psm,               /* The local PSM. */
                                   ind->connection_id,     /* The L2CAP connection ID.*/
                                   ind->identifier,        /* The L2CAP signal identifier. */
                                   CONFTAB_LEN(l2cap_conftab),
                                   l2cap_conftab);          /* The configuration table. */
}

/*! \brief Handle a L2CAP connection request that was initiated by the remote
    peer device. This is called for both local and remote initiated
    L2CAP requests
*/
static void dfuPeer_HandleL2capConnectCfm(const CL_L2CAP_CONNECT_CFM_T *cfm)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    DEBUG_LOG("dfuPeer_HandleL2capConnectCfm, status %u", cfm->status);

    switch (dfuPeer_GetState())
    {
        case DFU_PEER_STATE_CONNECTING_LOCAL:
        case DFU_PEER_STATE_CONNECTING_REMOTE:
            /* If connection was succesful, get sink, attempt to enable wallclock and move
             * to connected state */
            if (l2cap_connect_success == cfm->status)
            {
                DEBUG_LOG("dfuPeer_HandleL2capConnectCfm, connected, conn ID %u, flush remote %u",
                          cfm->connection_id, cfm->flush_timeout_remote);


                the_dfu_peer->link_sink = cfm->sink;
                the_dfu_peer->link_source = StreamSourceFromSink(cfm->sink);

                MessageStreamTaskFromSink(the_dfu_peer->link_sink,
                                          &the_dfu_peer->task);
                MessageStreamTaskFromSource(the_dfu_peer->link_source,
                                          &the_dfu_peer->task);

                /* Disconnect the l2cap connection if Sink Configuration fails*/
                if(!SinkConfigure(the_dfu_peer->link_sink,
                                         VM_SINK_MESSAGES, VM_MESSAGES_ALL)||
                   !SourceConfigure(the_dfu_peer->link_source,
                                           VM_SOURCE_MESSAGES, VM_MESSAGES_ALL))
                {
                    the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
                    dfuPeer_SendL2capConnectFailure();
                    return;
                }

                dfuPeer_SetState(DFU_PEER_STATE_CONNECTED);

                /*
                 * In some scenarios, MESSAGE_MORE_DATA from streams may be
                 * missed if stream configuration at connection setup is delayed.
                 * To compensate for the missed MESSAGE_MORE_DATA, rescan the
                 * source buffer after connection setup and if data is available
                 * request to process it.
                 *
                 * This scenario can occur where the DFU pdu (i.e. SYNC_REQ)
                 * is immediately sent on DFU L2CAP channel establishment but
                 * the peer has missed MESSAGE_MORE_DATA as the confirmation
                 * for DFU L2CAP channel establishment arrived late.
                 * Even though this specific to the receiver of SYNC_REQ, its
                 * fine to commonly rescan the source buffer.
                 */
                 dfuPeer_ProcessL2CAPDataOnConnection();

            }
            else
            {
                /* Connection failed, if no more pending connections, return to disconnected state */
                if (cfm->status >= l2cap_connect_failed)
                {
                    DEBUG_LOG("dfuPeer_HandleL2capConnectCfm, failed, go to disconnected state");
                    dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
                }
                /* Pending connection, return, will get another message in a bit */
                else
                {
                    DEBUG_LOG("dfuPeer_HandleL2capConnectCfm, L2CAP connection is Pending");
                    return;
                }
            }
            break;

        default:
            /* Connect confirm receive not in connecting state, connection must have failed */
            if(l2cap_connect_success != cfm->status)
            {
                the_dfu_peer->state = DFU_PEER_STATE_DISCONNECTED;
                dfuPeer_SendL2capConnectFailure();
            }
            DEBUG_LOG("dfuPeer_HandleL2capConnectCfm, failed");
            break;
    }
}

/*! \brief Handle a L2CAP disconnect initiated by the remote peer
*/
static void dfuPeer_HandleL2capDisconnectInd(const CL_L2CAP_DISCONNECT_IND_T *ind)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    DEBUG_LOG("dfuPeer_HandleL2capDisconnectInd, status %u", ind->status);

    /* Always send reponse */
    ConnectionL2capDisconnectResponse(ind->identifier, ind->sink);

    /* Ensure source is empty so that stream can be destroyed */
    StreamConnectDispose(StreamSourceFromSink(ind->sink));

    /* Only change state if sink matches */
    if (ind->sink == the_dfu_peer->link_sink)
    {
        /* Print if there is a link loss. */
        if (ind->status == l2cap_disconnect_link_loss &&
                           !BdaddrIsZero(&the_dfu_peer->peer_addr))
        {
            DEBUG_LOG("dfuPeer_HandleL2capDisconnectInd, link-loss");
            /* Store the peer DFU L2CAP disconnection reason */
            UpgradePeerStoreDisconReason(upgrade_peer_l2cap_link_loss);
        }

        dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
    }
}

/*! \brief Handle a L2CAP disconnect confirmation
*/
static void dfuPeer_HandleL2capDisconnectCfm(const CL_L2CAP_DISCONNECT_CFM_T *cfm)
{
    UNUSED(cfm);
    DEBUG_LOG("dfuPeer_HandleL2capDisconnectCfm, status %u", cfm->status);

    /* Ensure source is empty so that stream can be destroyed */
    StreamConnectDispose(StreamSourceFromSink(cfm->sink));

    /* Move to DISCONNECTED  state if we're in the disconnecting state */
    if (dfuPeer_GetState() == DFU_PEER_STATE_DISCONNECTING)
    {
        dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTED);
    }
}


/*! \brief Claim the requested number of octets from a sink
*/
static uint8 *dfuPeer_ClaimSink(Sink sink, uint16 size)
{
    uint8 *dest = SinkMap(sink);
    uint16 available = SinkSlack(sink);
    uint16 claim_size = 0;
    uint16 already_claimed = SinkClaim(sink, 0);
    uint16 offset = 0;

    if (size > available)
    {
        DEBUG_LOG("dfuPeer_ClaimSink attempt to claim too much %u", size);
        return NULL;
    }

    /* We currently have to claim an extra margin in the sink for bytes the
     * marshaller will add, describing the marshalled byte stream. This can
     * accumulate in the sink, so use them up first. */
    if (already_claimed < size)
    {
        claim_size = size - already_claimed;
        offset = SinkClaim(sink, claim_size);
    }
    else
    {
        offset = already_claimed;
    }

    if ((NULL == dest) || (offset == 0xffff))
    {
        DEBUG_LOG("dfuPeer_ClaimSink SinkClaim returned Invalid Offset");
        return NULL;
    }

    return (dest + offset - already_claimed);
}

/*! \brief Send upgrade peer data packets to peer device
*/
static void dfuPeer_L2capSendRequest(uint8 *data_in, uint16 data_size)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    Sink sink = the_dfu_peer->link_sink;

    uint8 *data = dfuPeer_ClaimSink(sink, data_size );
    if (data)
    {
        DEBUG_LOG_VERBOSE("dfuPeer_L2capSendRequest, claimed %u bytes", data_size);
        DEBUG_LOG_DATA_V_VERBOSE(data_in, data_size);
        memcpy(data,data_in,data_size);
        SinkFlush(sink, data_size);
    }
    else
        DEBUG_LOG_ERROR("dfuPeer_L2capSendRequest, failed to claim %u bytes", data_size);
}

/*! \brief Process incoming upgrade peer data packets
*/
static void dfuPeer_L2capProcessData(void)
{
    uint16 size;
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    Source source = the_dfu_peer->link_source;

    if (the_dfu_peer->is_primary)
    {
        while((size = SourceBoundary(source)) != 0)
        {
            DEBUG_LOG_VERBOSE("dfuPeer_L2capProcessData, primary, size %u", size);
            uint8 *data = (uint8 *)SourceMap(source);
            DEBUG_LOG_DATA_V_VERBOSE(data, size);
            UpgradePeerProcessDataRequest(UPGRADE_PEER_GENERIC_MSG, data, size);
            SourceDrop(source, size);
        }
    }
    else
    {
        /* Upgrade transport can get disconnected in middle of data transfer due to handover
           so, make sure its connected. */
        if ((size = SourceBoundary(source)) != 0 && !the_dfu_peer->processing && UpgradeTransportInUse())
        {
            DEBUG_LOG_VERBOSE("dfuPeer_L2capProcessData, secondary, size %u", size);
            uint8 *data = (uint8 *)SourceMap(source);
            DEBUG_LOG_DATA_V_VERBOSE(data, size);
            UpgradeProcessDataRequest(size, data);
            the_dfu_peer->processing = TRUE;
        }
    }
}

/*! \brief Handle MessageMoreData request from peer device
*/
static void dfuPeer_HandleMMD(const MessageMoreData *mmd)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    if (mmd->source == the_dfu_peer->link_source)
    {
        dfuPeer_L2capProcessData();
    }
    else
    {
        DEBUG_LOG("dfuPeer_HandleMMD MMD received that doesn't match a link");
    }
}



/*! \brief Handle MessageMoreSpace request from peer device
*/
static void dfuPeer_HandleMMS(const MessageMoreSpace* mms)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    if (mms->sink == the_dfu_peer->link_sink)
    {
        UpgradePeerProcessDataRequest(UPGRADE_PEER_DATA_SEND_CFM, NULL, 0);
    }
    else
    {
        DEBUG_LOG("dfuPeer_HandleMMS MMS received that doesn't match a link");
    }
}

/*! \brief Handle UPGRADE_TRANSPORT_DATA_CFM from upgrade library
*/
static void dfuPeer_HandleTransportDataCfm(const UPGRADE_TRANSPORT_DATA_CFM_T* cfm)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();
    DEBUG_LOG("dfuPeer_HandleTransportDataCfm, status %d, packet type %d", cfm->status, cfm->packet_type);

    if (!the_dfu_peer->is_primary)
    {
        DEBUG_LOG("dfuPeer_HandleTransportDataCfm, secondary, dropping %u bytes from source", cfm->size_data);
        Source source = the_dfu_peer->link_source;
        SourceDrop(source, cfm->size_data);
        the_dfu_peer->processing = FALSE;
    }
    else
        DEBUG_LOG("dfuPeer_HandleTransportDataCfm, primary");


    dfuPeer_L2capProcessData();
}



/*! \brief Message Handler

    This function is the main message handler for Upgrade Peer, every
    message is handled in it's own seperate handler function.
*/
static void dfuPeer_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    /* If the state is Disconnecting, then reject all messages except
     * CL_L2CAP_DISCONNECT_CFM.
     */
    if(dfuPeer_IsStateDisconnecting(id, message))
        return;

    switch (id)
    {
       /* Connection library messages */
        case CL_L2CAP_REGISTER_CFM:
            dfuPeer_HandleL2capRegisterCfm(
                                      (const CL_L2CAP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_REGISTER_CFM:
            dfuPeer_HandleClSdpRegisterCfm(
                                        (const CL_SDP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            dfuPeer_HandleClSdpServiceSearchAttributeCfm(
                        (const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
            return;

        case CL_L2CAP_CONNECT_IND:
            dfuPeer_HandleL2capConnectInd(
                                       (const CL_L2CAP_CONNECT_IND_T *)message);
            break;

        case CL_L2CAP_CONNECT_CFM:
            dfuPeer_HandleL2capConnectCfm(
                                       (const CL_L2CAP_CONNECT_CFM_T *)message);
            break;

        case CL_L2CAP_DISCONNECT_IND:
            dfuPeer_HandleL2capDisconnectInd(
                                    (const CL_L2CAP_DISCONNECT_IND_T *)message);
            break;

        case CL_L2CAP_DISCONNECT_CFM:
            dfuPeer_HandleL2capDisconnectCfm(
                                    (const CL_L2CAP_DISCONNECT_CFM_T *)message);
            break;


        /* Internal Upgrade Peer Messages */
        case DFU_PEER_INTERNAL_STARTUP_REQ:
            dfuPeer_HandleInternalStartupRequest(
                         (DFU_PEER_INTERNAL_STARTUP_REQ_T *)message);
            break;

        /* Transport Data Messages */
        case UPGRADE_PEER_DATA_IND:
            {
                UPGRADE_PEER_DATA_IND_T *pdu = (UPGRADE_PEER_DATA_IND_T*)message;
                dfuPeer_L2capSendRequest(pdu->data, pdu->size_data);
                break;
            }
        case UPGRADE_TRANSPORT_DATA_IND:
            {
                UPGRADE_TRANSPORT_DATA_IND_T *pdu = (UPGRADE_TRANSPORT_DATA_IND_T*)message;
                dfuPeer_L2capSendRequest(pdu->data, pdu->size_data);
                break;
            }

        /* Peer-Connect/Disconnect Messages */
         case UPGRADE_PEER_CONNECT_REQ:
            DfuPeer_InitiateLinkToPeer();
            break;

        case UPGRADE_PEER_DISCONNECT_REQ:
            DfuPeer_InitiatePeerLinkDisconnection();
            break;

        case UPGRADE_PEER_END_DATA_TRANSFER:
            Dfu_SetPeerDataTransferStatus(DFU_PEER_DATA_TRANSFER_COMPLETED);
            break;

        case DFU_PEER_INTERNAL_MESSAGE_MORE_DATA:
            dfuPeer_L2capProcessData();
            break;

        /* Sink Data Message*/
        case MESSAGE_MORE_DATA:
            dfuPeer_HandleMMD((const MessageMoreData*)message);
            break;

        case MESSAGE_MORE_SPACE:
            dfuPeer_HandleMMS((const MessageMoreSpace*)message);
            break;

        case UPGRADE_TRANSPORT_DATA_CFM:
            dfuPeer_HandleTransportDataCfm((const UPGRADE_TRANSPORT_DATA_CFM_T* )message);
            break;

        case MESSAGE_SOURCE_EMPTY:
        case UPGRADE_PEER_DATA_CFM:
            break;

        default:
            DEBUG_LOG("dfuPeer_HandleMessage. UNHANDLED Message MESSAGE:dfu_peer_internal_messages_t:0x%x. State %d",
                      id, dfuPeer_GetState());
            break;
    }
}

bool DfuPeer_EarlyInit(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("DfuPeer_EarlyInit");

    TaskList_InitialiseWithCapacity(dfuPeer_GetClientList(), PEER_UPGRADE_CLIENT_LIST_INIT_CAPACITY);

    return TRUE;
}

/*! \brief Initialise Device upgrade peer task

    Called at start up to initialise the Device upgrade peer task
*/
bool DfuPeer_Init(Task init_task)
{
    dfu_peer_task_data_t *theDFUPeer = dfuPeer_GetTaskData();

    UNUSED(init_task);

    /* Set up task handler */
    theDFUPeer->task.handler = dfuPeer_HandleMessage;

    dfuPeer_SetState(DFU_PEER_STATE_INITIALISE);
    Dfu_SetPeerDataTransferStatus(DFU_PEER_DATA_TRANSFER_NOT_STARTED);

    return TRUE;
}

/*! \brief Initiate Peer connect request

    Called when UPGRADE_PEER_CONNECT_REQ msg is received from upgrade library
*/
void DfuPeer_InitiateLinkToPeer(void)
{
     dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

     /* Set the connect attempts variable to zero */
     acl_connect_attempts = sdp_connect_attempts = 0;

    DEBUG_LOG("DfuPeer_InitiateLinkToPeer is_primary:%d", the_dfu_peer->is_primary);
    DEBUG_LOG("DfuPeer_InitiateLinkToPeer state:%d", the_dfu_peer->state);

    /* Don't try the link connection if another connection is in progress */
    if(the_dfu_peer->state != DFU_PEER_STATE_IDLE)
    {
        dfuPeer_SendL2capConnectFailure();
        DEBUG_LOG("DfuPeer_InitiateLinkToPeer A connection is already in progress");
        return;
    }
    if (appDeviceGetPeerBdAddr(&(the_dfu_peer->peer_addr)))
    {
        DEBUG_LOG("DfuPeer_InitiateLinkToPeer DFUPeerConnect");

        /* Initiate the L2CAP connection with peer device */
        dfuPeer_Startup(&the_dfu_peer->peer_addr);
    }
    else
    {
        DEBUG_LOG("DfuPeer_InitiateLinkToPeer No peer earbud paired");
    }
}

/*! \brief Disconnect the L2CAP connection with Peer device

    Called to disconnect the L2CAP connection with Peer device and change the
    state of the device
*/
void DfuPeer_InitiatePeerLinkDisconnection(void)
{
    dfu_peer_state_t state = dfuPeer_GetState();
    DEBUG_LOG("DfuPeer_InitiatePeerLinkDisconnection, state enum:dfu_peer_state_t:%u",
               state);

    switch (state)
    {
        case DFU_PEER_STATE_CONNECTED:
        case DFU_PEER_STATE_CONNECTING_ACL:
        case DFU_PEER_STATE_CONNECTING_SDP_SEARCH:
        case DFU_PEER_STATE_CONNECTING_LOCAL:
        case DFU_PEER_STATE_CONNECTING_REMOTE:
             dfuPeer_SetState(DFU_PEER_STATE_DISCONNECTING);
             break;
        default:
             break;
    }
}

/*! \brief Check if Abort is trigerred and device upgrade peer still in use.
    Return TRUE if abort is initiated and device upgrade peer still in use,
    else FALSE.

    \note This API will be used only for Primary device. Since that device is the
    starting point of a new DFU after abort.
*/
bool DfuPeer_StillInUseAfterAbort(void)
{
    if(UpgradePeerIsPeerDFUAborted() && dfuPeer_IsInUse())
        return TRUE;
    else
        return FALSE;
}

/*! \brief Register any Client with DFU Peer for messages
*/
void DfuPeer_ClientRegister(Task tsk)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(dfuPeer_GetClientList()), tsk);
}

void DfuPeer_SetRole(bool role)
{
    dfu_peer_task_data_t *the_dfu_peer = dfuPeer_GetTaskData();

    /* Global non-heap allocated taskdata safely assessed without NULL checks.*/
    the_dfu_peer->is_primary = role;
    DEBUG_LOG("DfuPeer_SetRole is_primary:%d", the_dfu_peer->is_primary);

    /* Set the role in upgrade peer lib */
    UpgradePeerSetRole(role);

    /* Set the function pointer for host communication */
    Dfu_UpgradeHostRspSwap(role);
}

void DfuPeer_SetLinkPolicy(lp_power_mode mode)
{
#ifdef INCLUDE_MIRRORING
    /*Need to be done only if peer dfu process is there*/
    if(UpgradePeerIsStarted())
    {
        if(mode == lp_active)
        {
            /*Check for active a2dp/eSCO mirroring*/
            if((!MirrorProfile_IsA2dpActive()) && (!MirrorProfile_IsEscoActive()))
            {
                DEBUG_LOG("DfuPeer_SetLinkPolicy  setting peer link active");
                /*Keep the peer link policy to active so as to improve the speed, even for in-case DFU */
                MirrorProfile_UpdatePeerLinkPolicyDfu(mode);
            }
        }
        else
        {
            DEBUG_LOG("DfuPeer_SetLinkPolicy setting peer link sniff");
            MirrorProfile_UpdatePeerLinkPolicyDfu(mode);
        }      
    }
#else
    UNUSED(mode);
#endif
}

void DfuPeer_ProcessHostMsg(upgrade_peer_msg_t msg_id, uint8 action)
{
    upgrade_action_status_t action_status = (upgrade_action_status_t)action;
    UpgradePeerProcessHostMsg(msg_id, action_status);
}

void DfuPeer_StoreMd5(uint32 md5_checksum)
{
    /* Store MD5 for Peer Upgrade */
    UpgradePeerStoreMd5(md5_checksum);
}

void DfuPeer_CtxInit(void)
{
    /* Initialize the peer context for peer device, because
     * UPGRADE_PEER_TRANSFER_COMPLETE_RES will be sent to both the
     * devices upgrade peer librabry.
     */
    UpgradePeerCtxInit();
}

void DfuPeer_CancelDFU(void)
{
    UpgradePeerCancelDFU();
}

#endif
