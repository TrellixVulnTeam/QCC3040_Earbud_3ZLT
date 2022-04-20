/*!
\copyright  Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu.c
\brief      Device firmware upgrade management.

Over the air upgrade is managed from this file.
*/

#ifdef INCLUDE_DFU

#include "dfu.h"

#include "system_state.h"
#include "adk_log.h"
#include "phy_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "device_db_serialiser.h"

#include <charger_monitor.h>
#include <system_state.h>

#include <vmal.h>
#include <panic.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <upgrade.h>
#include <ps.h>
#include <gatt_connect.h>
#include <gatt_handler.h>
#include <gatt_server_gatt.h>
#include <connection_manager.h>
#include <device_list.h>
#include <connection_manager_list.h>

#ifdef INCLUDE_DFU_PEER
#include "dfu_peer.h"
#include "bt_device.h"
#include <app/message/system_message.h>
#include <peer_signalling.h>
#include <tws_topology_config.h>
#include <mirror_profile.h>
#include <handover_profile.h>
#include <handset_service.h>
#endif

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(DFU, DFU_MESSAGE_END)

#endif

/*!< Task information for UPGRADE support */
dfu_task_data_t app_dfu;

/*! Identifiers for messages used internally by the DFU module */
typedef enum dfu_internal_messages_t
{
    DFU_INTERNAL_BASE = INTERNAL_MESSAGE_BASE ,

    DFU_INTERNAL_CONTINUE_HASH_CHECK_REQUEST,

#ifdef INCLUDE_DFU_PEER
    DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE,

    DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED,
#endif

    /*! This must be the final message */
    DFU_INTERNAL_MESSAGE_END
};
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(DFU_INTERNAL_MESSAGE_END)

LOGGING_PRESERVE_MESSAGE_ENUM(dfu_internal_messages_t)
LOGGING_PRESERVE_MESSAGE_TYPE(dfu_messages_t)

/* The upgrade libraries use of partitions is not relevant to the
   partitions as used on devices targetted by this application.

   As it is not possible to pass 0 partitions in the Init function
   use a simple entry */
static const UPGRADE_UPGRADABLE_PARTITION_T logicalPartitions[]
                    = {UPGRADE_PARTITION_SINGLE(0x1000,DFU)
                      };

/*! Maximum size of buffer used to hold the variant string
    supplied by the application. 6 chars, plus NULL terminator */
#define VARIANT_BUFFER_SIZE (7)

static void dfu_MessageHandler(Task task, MessageId id, Message message);
static void dfu_GattConnect(gatt_cid_t cid);
static void dfu_GattDisconnect(gatt_cid_t cid);
static void dfu_GetVersionInfo(dfu_VersionInfo *ver_info);
static void dfu_SetGattServiceUpdateFlags(void);
#ifdef INCLUDE_DFU_PEER
static void dfu_PeerEraseStartTx(void);
static void dfu_PeerSetContextTx(upgrade_context_t context);
#endif

static const gatt_connect_observer_callback_t dfu_gatt_connect_callback =
{
    .OnConnection = dfu_GattConnect,
    .OnDisconnection = dfu_GattDisconnect
};

static void dfu_NotifyStartedNeedConfirm(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_REQUESTED_TO_CONFIRM);
}


static void dfu_NotifyStartedWithInProgress(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_REQUESTED_IN_PROGRESS);
}


static void dfu_NotifyActivity(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_ACTIVITY);
}


static void dfu_NotifyStart(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_STARTED);
}

#ifdef INCLUDE_DFU_PEER
static void dfu_NotifyPreStart(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_PRE_START);
}

static void dfu_StartPeerDfu(void)
{
    if(UPGRADE_PEER_IS_PRIMARY)
    {
        if((!UPGRADE_PEER_IS_STARTED) || (UPGRADE_PEER_IS_LINK_LOSS))
        {
            DEBUG_LOG("dfu_StartPeerDfu: Setting up peer upgrade");
            UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_IGNORE);
            /* Since concurrent peer dfu about to start, set the status here
             * This will be used to decide whether to delay the hash checking
             * after primary dfu completion or go ahead with hash checking.
             */
             Dfu_SetPeerDataTransferStatus(DFU_PEER_DATA_TRANSFER_STARTED);
        }
    }
}
#endif

static void dfu_NotifyCompleted(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_COMPLETED);
}

static void dfu_NotifyAbort(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_CLEANUP_ON_ABORT);
}

static void dfu_NotifyAborted(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_ABORTED);
}

static void dfu_NotifyReadyforSilentCommit(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_READY_FOR_SILENT_COMMIT);
}

static void dfu_NotifyReadyToReboot(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_READY_TO_REBOOT);
}

/*************************************************************************
    Provide the logical partition map.

    For earbuds this is initially hard coded, but may come from other
    storage in time.
*/
static void dfu_GetLogicalPartitions(const UPGRADE_UPGRADABLE_PARTITION_T **partitions, uint16 *count)
{
    uint16 num_partitions = sizeof(logicalPartitions)/sizeof(logicalPartitions[0]);
    *partitions = logicalPartitions;
    *count = num_partitions;
}

/*************************************************************************
    Get the variant Id from the firmware and convert it into a variant
    string that can be passed to UpgradeInit.

    This function allocates a buffer for the string which must be freed
    after the call to UpgradeInit.
*/
static void dfu_GetVariant(char *variant, size_t length)
{
    int i = 0;
    char chr;
    uint32 product_id;

    PanicFalse(length >= VARIANT_BUFFER_SIZE);

    product_id = VmalVmReadProductId();
    if (product_id == 0)
    {
        variant[0] = '\0';
        return;
    }

    /* The product Id is encoded as two ascii chars + 4 integers in BCD format. */

    /* The ascii chars may be undefined or invalid (e.g. '\0').
       If so, don't include them in the variant string. */
    chr = (char)((product_id >> 8) & 0xFF);
    if (isalnum(chr))
        variant[i++] = chr;

    chr = (char)(product_id & 0xFF);
    if (isalnum(chr))
        variant[i++] = chr;

    sprintf(&variant[i], "%04X", ((uint16)((product_id >> 16) & 0xFFFF)));
}


/********************  PUBLIC FUNCTIONS  **************************/


bool Dfu_EarlyInit(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("Dfu_EarlyInit");

    TaskList_InitialiseWithCapacity(Dfu_GetClientList(), THE_DFU_CLIENT_LIST_INIT_CAPACITY);

    return TRUE;
}

/*! Initialisation point for the over the air support in the upgrade library.
 *
 */
bool Dfu_Init(Task init_task)
{
    dfu_task_data_t *the_dfu=Dfu_GetTaskData();
    uint16 num_partitions;
    const UPGRADE_UPGRADABLE_PARTITION_T *logical_partitions;
    char variant[VARIANT_BUFFER_SIZE];
    dfu_VersionInfo ver_info;
    UNUSED(init_task);

    dfu_GetVersionInfo(&ver_info);

    GattConnect_RegisterObserver(&dfu_gatt_connect_callback);

    the_dfu->dfu_task.handler = dfu_MessageHandler;
    the_dfu->reboot_permission_required = FALSE;

#ifdef INCLUDE_DFU_PEER
    /*
     * Register to use marshalled message channel with DFU domain for Peer DFU
     * messages.
     */
    appPeerSigMarshalledMsgChannelTaskRegister(Dfu_GetTask(),
        PEER_SIG_MSG_CHANNEL_DFU,
        dfu_peer_sig_marshal_type_descriptors,
        NUMBER_OF_DFU_PEER_SIG_MARSHAL_TYPES);

    /* Register for peer signaling notifications */
    appPeerSigClientRegister(Dfu_GetTask());

    ConManagerRegisterConnectionsClient(Dfu_GetTask());

    /* Register for connect / disconnect events from mirror profile */
    MirrorProfile_ClientRegister(Dfu_GetTask());

    HandoverProfile_ClientRegister(Dfu_GetTask());
#endif

    dfu_GetVariant(variant, sizeof(variant));

    dfu_GetLogicalPartitions(&logical_partitions, &num_partitions);

    /* Allow storage of info at end of (SINK_UPGRADE_CONTEXT_KEY) */
    UpgradeInit(Dfu_GetTask(), UPGRADE_CONTEXT_KEY, UPGRADE_LIBRARY_CONTEXT_OFFSET,
            logical_partitions,
            num_partitions,
            UPGRADE_INIT_POWER_MANAGEMENT,
            variant,
            upgrade_perm_always_ask,
            &ver_info.upgrade_ver,
            ver_info.config_ver);

    return TRUE;
}


bool Dfu_HandleSystemMessages(MessageId id, Message message, bool already_handled)
{
    switch (id)
    {
        case MESSAGE_IMAGE_UPGRADE_ERASE_STATUS:
        case MESSAGE_IMAGE_UPGRADE_COPY_STATUS:
        case MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS:
        case MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS:
        {
            Task upg = Dfu_GetTask();

            upg->handler(upg, id, message);
            return TRUE;
        }
    }
    return already_handled;
}

static void dfu_ForwardInitCfm(const UPGRADE_INIT_CFM_T *cfm)
{
    UPGRADE_INIT_CFM_T *copy = PanicUnlessNew(UPGRADE_INIT_CFM_T);
    *copy = *cfm;

    MessageSend(SystemState_GetTransitionTask(), UPGRADE_INIT_CFM, copy);
}

static void dfu_HandleRestartedInd(const UPGRADE_RESTARTED_IND_T *restart)
{
    /* This needs to base its handling on the reason in the message,
       for instance upgrade_reconnect_not_required is a hint that errr,
       reconnect isn't a priority. */

    DEBUG_LOG("dfu_HandleRestartedInd 0x%x", restart->reason);
    switch (restart->reason)
    {
        case upgrade_reconnect_not_required:
            /* No need to reconnect, not even sure why we got this */
            break;

        case upgrade_reconnect_required_for_confirm:
            Dfu_SetRebootReason(REBOOT_REASON_DFU_RESET);
            dfu_NotifyStartedNeedConfirm();
#ifndef INCLUDE_DFU_PEER
            /* If peer is NOT supported, commit new image here for silent commit option*/
            if(UpgradeIsSilentCommitEnabled())
            {
                DEBUG_LOG("dfu_HandleRestartedInd: UpgradeCommitConfirmForSilentCommit");
                UpgradeCommitConfirmForSilentCommit();
            }
#endif
            break;

        case upgrade_reconnect_recommended_as_completed:
        case upgrade_reconnect_recommended_in_progress:
            /*
            * Remember the reset reason, in order to progress an DFU 
            * if abruptly reset.
            */
            Dfu_SetRebootReason(REBOOT_REASON_ABRUPT_RESET);
            dfu_NotifyStartedWithInProgress();
            break;
    }
}


static void dfu_HandleUpgradeStatusInd(const UPGRADE_STATUS_IND_T *sts)
{
    dfu_NotifyActivity();

    switch (sts->state)
    {
        case upgrade_state_idle:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. idle(%d)",sts->state);
            break;

        case upgrade_state_downloading:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. downloading(%d)",sts->state);
            break;

        case upgrade_state_commiting:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. commiting(%d)",sts->state);
            break;

        case upgrade_state_done:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. done(%d)",sts->state);
            dfu_NotifyCompleted();

            /* To Do: remove when merging GAA resume changes because context gets cleared
               as a part of upgrade pskey */
            Upgrade_SetContext(UPGRADE_CONTEXT_UNUSED);

#ifdef INCLUDE_DFU_PEER
            Dfu_SetPeerDataTransferStatus(DFU_PEER_DATA_TRANSFER_NOT_STARTED);
#endif
            break;

        default:
            DEBUG_LOG_ERROR("dfu_HandleUpgradeStatusInd. Unexpected state %d",sts->state);
            Panic();
            break;
    }
}

static void dfu_HandleUpgradeOperationInd(const UPGRADE_OPERATION_IND_T *operation)
{
    DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd. Ops enum:upgrade_ops_t:%d", operation->ops);
    switch (operation->ops)
    {
        case upgrade_ops_store_peer_md5:
        {
#ifdef INCLUDE_DFU_PEER
            if(UPGRADE_PEER_IS_SUPPORTED)
            {
                uint32 md5_checksum = UpgradeGetMD5Checksum();
                DfuPeer_StoreMd5(md5_checksum);
            }
#endif
        }
            break;

        case upgrade_ops_trnsfr_complt_res_send_to_peer:
        {
#ifdef INCLUDE_DFU_PEER
            /* Send UPGRADE_PEER_TRANSFER_COMPLETE_RES message to 
             * upgrade_peer library of both the devices
             * since the action set with this message will be required
             * during the dynamic role commit phase.
             */
            if(UPGRADE_PEER_IS_SUPPORTED)
            {
                uint8 is_silent_commit = operation->action;
                upgrade_action_status_t upgrade_action = 
                             is_silent_commit? UPGRADE_SILENT_COMMIT : UPGRADE_CONTINUE;

                DfuPeer_ProcessHostMsg(UPGRADE_PEER_TRANSFER_COMPLETE_RES, (uint8)upgrade_action);

                /* Check if it is an interactive commit on primary */
                if(!is_silent_commit && UPGRADE_PEER_IS_PRIMARY)
                {

                    /* Disconnect the handset. Since DFU reboot is dealyed, not
                     * checking for disconnect indication assuming it will get
                     * disconnected.
                     * TODO: Use reboot_permission_required and DFU_READY_TO_REBOOT
                     * in EB SM to disconnect peer and handset and then reboot.
                     */
                    HandsetService_DisconnectAll(Dfu_GetTask());
                }
            }
#endif
        }
            break;
        case upgrade_ops_send_silent_commit_ind_to_host:
        {
#ifdef INCLUDE_DFU_PEER
            if(!UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_CONNECTED)
            {
                UpgradeSendReadyForSilentCommitInd();
            }
#else
            UpgradeSendReadyForSilentCommitInd();
#endif
        }
            break;
        case upgrade_ops_check_peer_during_commit:
        {
#ifdef INCLUDE_DFU_PEER
            /* Verify that peer is connected otherwise, we should just abort rather than waiting. */

            if(UPGRADE_PEER_IS_SUPPORTED && !UPGRADE_PEER_IS_CONNECTED)
            {
                DEBUG_LOG("dfu_HandleUpgradeOperationInd Fatal error, UPGRADE_PEER_IS_CONNECTED %d",UPGRADE_PEER_IS_CONNECTED);
                UpgradeFatalErrorAppNotReady();
            }

            if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
            {
                DfuPeer_ProcessHostMsg(UPGRADE_PEER_COMMIT_CFM, operation->action);
            }
#endif
        }
            break;
        case upgrade_ops_init_peer_context:
        {
#ifdef INCLUDE_DFU_PEER
            if(!UPGRADE_PEER_IS_PRIMARY)
            {
                DfuPeer_CtxInit();
            }
#endif
        }
            break;
        case upgrade_ops_notify_early_erase:
        {
#ifdef INCLUDE_DFU_PEER
            /* Do not send UPGRADE_START_PEER_ERASE_IND, if peers are not connected */
            if(UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_BLOCKED)
            {
                /*
                 * Notify application that DFU is about to start so that DFU timers
                 * can be cancelled to avoid false DFU timeouts owing to actual
                 * DFU start indication being deferred as DFU erase was ongoing.
                 */
                dfu_NotifyPreStart();
                dfu_PeerEraseStartTx();
             }
#endif
        }
            break;
        case upgrade_ops_delay_prim_commit:
        {
            /* For Earbuds Case*/
            /* Primary EB should wait for the peer to first complete the commit
             * so any error occured on secondary EB can be handled. eg., if SEB
             * has been reset before receiving the commit confirm, then it will
             * be in the sync state so, we should just abort. On success, SEB
             * will send the Upgrade complete indication. After this, PEB can commit. */
#ifdef INCLUDE_DFU_PEER
            if(!UPGRADE_PEER_IS_PRIMARY)
#endif
                UpgradeSmCommitConfirmYes();
        }
            break;

        case upgrade_ops_send_host_in_progress_ind:
        {
#ifdef INCLUDE_DFU_PEER
            if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
            {
                if(UPGRADE_PEER_IS_COMMIT_CONTINUE && UpgradeSmIsStateCommitHostContinue())
                {
                    UpgradeSmSendHostInProgressInd(TRUE, TRUE);
                }
                else
                {
                    UpgradeSmSendHostInProgressInd(TRUE, FALSE);
                }
            }
            else
#endif
            {
                UpgradeSmSendHostInProgressInd(FALSE, FALSE);
            }
        }
            break;

        case upgrade_ops_check_peer_commit:
        {
#ifdef INCLUDE_DFU_PEER
            /* If Peer upgrade is running then we will wait for
             * UPGRADE_PEER_COMMIT_REQ to proceed, else proceed further.
             */
            if(!(UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_COMMITED))
            {
                UpgradeSmHandleCommitVerifyProceed();
            }
#else
            UpgradeSmHandleCommitVerifyProceed();
#endif
        }
            break;

        case upgrade_ops_cancel_peer_dfu:
        {
#ifdef INCLUDE_DFU_PEER
            /* Cancel the Peer DFU request */
            if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
                DfuPeer_CancelDFU();
#endif
        }
            break;

        case upgrade_ops_relay_peer_in_prog_ind:
        {
#ifdef INCLUDE_DFU_PEER
            /*
             * In the post reboot DFU commit phase, now main role
             * (Primary/Secondary) are no longer fixed rather dynamically
             * selected by Topology using role selection. So if a role swap
             * occurs in the post reboot DFU commit phase.
             * (e.g. Primary on post reboot DFU commit phase becomes Secondary
             * In this scenario, the peer DFU L2CAP channel is established
             * by Old Primary and as a result UPGRADE_PEER_IS_STARTED won't be
             * satisfied on the New Primary.)
             *
             * Since the DFU domain communicates the main role on peer
             * signalling channel establishment which is established earlier
             * then handset connection, the necessary and sufficient pre-
             * conditions to relay UPGRADE_PEER_IN_PROGRESS_RES are as follows:
             * - firstly on the role as Primary and
             * - secondly on the peer DFU channel being setup. If peer DFU
             * channel isn't setup then defer relaying UPGRADE_PEER_IN_PROGRESS_RES
             * to the peer. Peer DFU channel is established post peer signalling
             * channel establishment.
             */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                if(UPGRADE_PEER_IS_CONNECTED)
                {
                    DfuPeer_ProcessHostMsg(UPGRADE_PEER_IN_PROGRESS_RES, operation->action);
                    UpgradeSmHandleInProgressInd(FALSE, ZERO_DURATION, operation->action);
                }
                else
                {
                    UpgradeSmHandleInProgressInd(TRUE, UPGRADE_PEER_POLL_INTERVAL, operation->action);
                }
            }
            else
#endif
            {
                UpgradeSmHandleInProgressInd(TRUE, ZERO_DURATION, operation->action);
            }
        }
            break;
        case upgrade_ops_handle_notify_host_of_commit:
        {
#ifdef INCLUDE_DFU_PEER
            /*
             * Notify the Host of commit and upgrade completion only
             * when peer is done with its commit and upgrade completion.
             *
             * Poll for peer upgrade completion at fixed intervals
             * (less frequently) before notify the Host of its commit and
             * upgrade completion.
             */
            if(UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_ENDED)
            {
                UpgradeSmHandleNotifyHostOfCommit(TRUE, UPGRADE_PEER_POLL_INTERVAL);
            }
            else
#endif
            {
                UpgradeSmHandleNotifyHostOfCommit(FALSE, ZERO_DURATION);
            }
        }
            break;

        case upgrade_ops_handle_hash_check_request:
        {
#ifdef INCLUDE_DFU_PEER
            dfu_task_data_t *the_dfu = Dfu_GetTaskData();
            if(UPGRADE_PEER_IS_BLOCKED || UPGRADE_PEER_IS_PRIMARY)
            {
                DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd. Hash Checking After Sec Data transfer is over");
                MessageSendConditionally(Dfu_GetTask(), DFU_INTERNAL_CONTINUE_HASH_CHECK_REQUEST, NULL,
                                    (uint16 *)&the_dfu->peerDataTransferStatus);
            }
            else
#endif
            {
                MessageSend(Dfu_GetTask(), DFU_INTERNAL_CONTINUE_HASH_CHECK_REQUEST, NULL);
            }
        }
            break;

        case upgrade_ops_notify_host_of_upgrade_complete:
        {
            bool is_silent_commit = UpgradeIsSilentCommitEnabled();
            bool is_primary = FALSE;
            /*
             * Notify the Host of commit and upgrade completion only
             * when peer is done with its commit and upgrade completion.
             *
             * Poll for peer upgrade completion at fixed intervals
             * (less frequently) before notify the Host of its commit and
             * upgrade completion.
             */
            /*
             * For silent commit, no notification to host is required so 
             * proceed with completion in else part
             */
#ifdef INCLUDE_DFU_PEER
            is_primary = UPGRADE_PEER_IS_PRIMARY;
            if (!is_silent_commit && UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_ENDED)
            {
                UpgradeSmHandleNotifyHostOfComplete(is_silent_commit, UPGRADE_PEER_POLL_INTERVAL, is_primary);
            }
            else
#endif
            {
                UpgradeSmHandleNotifyHostOfComplete(is_silent_commit, ZERO_DURATION, is_primary);
            }

        }
            break;

        case upgrade_ops_abort_post_transfer_complete:
        {
#ifdef INCLUDE_DFU_PEER
            if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
            {
                /* Host has aborted th DFU, inform peer device as well */
                DfuPeer_ProcessHostMsg(UPGRADE_PEER_TRANSFER_COMPLETE_RES, (uint8)UPGRADE_ABORT);
            }
            else
#endif
            {
                UpgradeSMHandleAbort();
            }
        }
            break;

        case upgrade_ops_permit_reboot_on_condition:
        {
#ifdef INCLUDE_DFU_PEER
            /* For silent commit, do not wait for confirmation from app (DFU domain)
             */
            if(!UpgradeIsSilentCommitEnabled() && UPGRADE_PEER_IS_PRIMARY)
            {
                /*
                 * Trigger reboot after DFU domain confirms that its ok
                 * to reboot.
                 */
                UpgradeSMSetPermission(upgrade_perm_always_ask);
            }
#endif
        }
            break;

        case upgrade_ops_handle_abort:
        {
#ifdef INCLUDE_DFU_PEER
            /* If peer upgrade is supported then inform UpgradePeer lib as well */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                DfuPeer_ProcessHostMsg(UPGRADE_PEER_ABORT_REQ, (uint8)UPGRADE_ABORT);
            }
#endif
        }
            break;

        case upgrade_ops_internal_handle_post_vldtn_msg_rcvd:
        {
#ifdef INCLUDE_DFU_PEER
            /* Send UPGRADE_HOST_TRANSFER_COMPLETE_IND once standalone upgrade
             * is done.
             */
            if(!UPGRADE_PEER_IS_PRIMARY)
#endif
            {
                /* this check is for: when link loss between peers 
                 * then do not send UPGRADE_HOST_TRANSFER_COMPLETE_IND, wait until
                 * connection is back
                 */
#ifdef INCLUDE_DFU_PEER
                if(UpgradePeerIsBlocked())
                {
                    uint16 * peer_connection_status = UpgradePeerGetPeersConnectionStatus();
                    /* it's a link loss between peers, wait for peer connection to come back */
                    DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd, waiting for peers connection to come back");
                    UpgradeSMWaitForPeerConnection(peer_connection_status);
                }
                else
#endif
                {
                    UpgradeSMHandleImageCopyStatus(TRUE);
                }
            }

        }
            break;

        case upgrade_ops_reset_peer_current_state:
        {
#ifdef INCLUDE_DFU_PEER
            /* Set the currentState of UpgradePSKeys and SmCtx to default value
             * after role switch since the new secondary don't need this
             * information at this stage. Moreover, if it is not set, then
             * during subsequent DFU, the currentState incorrect value can
             * lead to not starting the peer DFU.
             */
            UpgradePeerResetCurState();
#endif
        }
            break;

        case upgrade_ops_handle_post_vldtn_msg_rcvd:
        {
#ifdef INCLUDE_DFU_PEER
            /* If Peer DFU is supported, then start the DFU of Peer Device */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                if(!UPGRADE_PEER_IS_STARTED)
                {
                    bool start_peer_dfu;
                    /* Setup Peer connection for DFU once image upgarde copy is
                     * completed successfully.
                     */
                    start_peer_dfu = UpgradeSMHandleImageCopyStatusForPrim();
                    if(start_peer_dfu && UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_REQUIRED)== FALSE)
                    {
                        /* TODO: An error has occured, fail the DFU */
                        DEBUG_LOG_ERROR("dfu_HandleUpgradeOperationInd: An error has occured");
                    }
                }
            }
            /* Send UPGRADE_HOST_TRANSFER_COMPLETE_IND later once upgrade is
             * done during standalone DFU.
             */
            else
#endif
            {
                UpgradeSMHandleImageCopyStatus(FALSE);
            }
        }
            break;

        case upgrade_ops_save_peer_pskeys:
        {
            /* Save the  state data in upgrade peer pskey */
#ifdef INCLUDE_DFU_PEER
            UpgradePeerSavePSKeys();
#endif
        }
            break;

            case upgrade_ops_clear_peer_pskeys:
            {
                /* Clear the  upgrade peer pskey */
#ifdef INCLUDE_DFU_PEER
                UpgradePeerClearPSKeys();
#endif
            }
                break;

            case upgrade_ops_handle_csr_valid_done_req_not_received:
            {
#ifdef INCLUDE_DFU_PEER
                if(!UPGRADE_PEER_IS_PRIMARY)
                {
                    UpgradeSMHandleValidDoneReqNotReceived();
                }
#endif
            }
                break;

            case upgrade_ops_clean_up_on_abort:
            {
#ifdef INCLUDE_DFU_PEER
                if(UpgradePeerIsSecondary())
                {
                    DEBUG_LOG("dfu_HandleUpgradeOperationInd UPGRADE_HOST_ERRORWARN_RES received from Peer");
                    UpgradeSMAbort();
                    UpgradeCleanupOnAbort();
                }
#endif
            }
                break;

           case upgrade_ops_handle_upgrade_partition_init:
           {
                DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd: upgrade_ops_handle_upgrade_partition_init, Upgrade Host type is enum:upgrade_context_t:%d", Upgrade_GetHostType());

                if(Upgrade_GetHostType()== UPGRADE_CONTEXT_GAA_OTA && UpgradePartialUpdateInterrupted())
                {
#ifdef INCLUDE_DFU_PEER
                    if(!BtDevice_IsMyAddressPrimary())
                    {
                        DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd: upgrade_ops_handle_upgrade_partition_init, UPGRADE_CONTEXT_GAA_OTA / Interrupted / Not Primary EB");
                        UpgradePartitionDataInitHelper(FALSE);
                    }
                    else
#endif
                    {
                        DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd: upgrade_ops_handle_upgrade_partition_init, UPGRADE_CONTEXT_GAA_OTA / Interrupted / Primary EB or HS");
                        UpgradePartitionDataInitHelper(TRUE);
                    }
                }
                else
                {
                    DEBUG_LOG_INFO("dfu_HandleUpgradeOperationInd : upgrade_ops_handle_upgrade_partition_init, NOT GAA / Not Interrupted ");
                    UpgradePartitionDataInitHelper(FALSE);
                }
           }
                break;
        default:
            DEBUG_LOG_ERROR("dfu_HandleUpgradeOperationInd. Unexpected state %d",operation->ops);
            break;
    }
}

static void dfu_HandleUpgradeTransportNotification(const UPGRADE_NOTIFY_TRANSPORT_STATUS_T *notification)
{
    DEBUG_LOG_INFO("dfu_HandleUpgradeTransportNotification. Status: %d", notification->status);
    switch (notification->status)
    {
        case upgrade_notify_transport_connect:
        {
#ifdef INCLUDE_DFU_PEER
            if(BtDevice_IsMyAddressPrimary())
#endif
            {
                /* Set QOS to low latency over BLE connection if it is not set.
                 */
                Dfu_RequestQOS();
            }
        }
            break;

        case upgrade_notify_transport_disconnect:
        {
            /* Release QOS which was earlier requested over BLE connection,
             * and set using Dfu_RequestQOS() call.
             */
            Dfu_ReleaseQOS();
        }
            break;

        default:
            DEBUG_LOG_ERROR("dfu_HandleUpgradeTransportNotification. Unexpected state %d",notification->status);
            break;
    }
}

static void dfu_SwapImage(void)
{
    dfu_SetGattServiceUpdateFlags();
    device_t upgrade_device = BtDevice_GetUpgradeDevice();
    /* SILENT_COMMIT : During DFU, if USER chooses option to install update 
       "LATER" in AG means SILENT commit is enabled.
       In other words, earbud may not need to remain active for handset to 
       complete the commit phase. 
       Therefore , if earbuds goes into case before COMMIT triggered by AG
       then earbuds independent of AG and applies the new DFU image and 
       won't need AG to complete the COMMIT. 
       INTERACTIVE_COMMIT : During DFU, if USER chooses option to install update
       "NOW" in AG means INTERACTIVE commit is enabled. 

       Setting the MRU flag for upgrade device only when SILENT_COMMIT is not 
       enabled so after DFU reboot,upgrade handset is tried for re-connection first. */
    if ((upgrade_device != NULL) && !Dfu_IsSilentCommitEnabled())
    {
        bdaddr handset_addr = DeviceProperties_GetBdAddr(upgrade_device);
        DEBUG_LOG("dfu_SwapImage upgrade_device 0x%p [%04x,%02x,%06lx]",
                  upgrade_device, 
                  handset_addr.nap,
                  handset_addr.uap,
                  handset_addr.lap);
        appDeviceUpdateMruDevice(&handset_addr);

        /* Store device data in ps */
        DeviceDbSerialiser_Serialise();
    }
    UpgradeImageSwap();
}

static void dfu_HandleUpgradeShutAudio(void)
{
    DEBUG_LOG("dfu_HandleUpgradeShutAudio");
    dfu_SwapImage();
}


static void dfu_HandleUpgradeCopyAudioImageOrSwap(void)
{
    DEBUG_LOG("dfu_HandleUpgradeCopyAudioImageOrSwap");
    dfu_SwapImage();
}

#ifdef INCLUDE_DFU_PEER
static void dfu_PeerEraseCompletedTx(bool success)
{
    bool is_secondary = !BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerEraseCompletedTx is_secondary:%d, success:%d", is_secondary, success);

    if (is_secondary)
    {
        dfu_peer_erase_req_res_t *ind = PanicUnlessMalloc(sizeof(dfu_peer_erase_req_res_t));
        memset(ind, 0, sizeof(dfu_peer_erase_req_res_t));

        /* Erase response sent */
        ind->peer_erase_req_res = FALSE;
        ind->peer_erase_status = success;
        appPeerSigMarshalledMsgChannelTx(Dfu_GetTask(),
                                        PEER_SIG_MSG_CHANNEL_DFU,
                                        ind, MARSHAL_TYPE(dfu_peer_erase_req_res_t));
    }
}

static void dfu_PeerEraseCompletedRx(dfu_peer_erase_req_res_t *msg)
{
    bool is_primary = BtDevice_IsMyAddressPrimary();
    bool peer_erase_status = msg->peer_erase_status;

    DEBUG_LOG("dfu_PeerEraseCompletedRx is_primary:%d, peer_erase_status:%d", is_primary, peer_erase_status);

    if (is_primary)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();

        if (msg->peer_erase_status)
        {
            /* Erase response was successful */

            /*
             * Unblock the conditionally queued
             * DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE
             */
            the_dfu->peerEraseDone = FALSE;
        }
        else
        {
            /* Erase response was failure */

            /*
             * One of the erase failure on the peer (Secondary) is out of
             * memory. Though its possible that memory is reclaimed later but
             * still we abort. Because if we progress DFU then peer DFU erase
             * shall be not driven simulataneously to local. And if in such
             * scenarios, DFU is abrupted with local reset when erase was
             * triggered then post reset when the roles are dynamically set and
             * DFU is resumed (as applicable) and also the earbuds are out of
             * case, then profile establishment with the handset and peer can
             * occur concurrently. The profile establishment probably update
             * psstore and since erase is ongoing, it may probably cause the
             * psstore operations to blocked and invariably apps P1 too.
             * To protect against undefined behavior especially panic on
             * assert in concurrent profile establishment, its better to
             * gracefully abort the DFU.
             *
             * Note: Generic error code reported to the host.
             */
            UpgradeHandleAbortDuringUpgrade();

            /*
             * Dont unblock DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE
             * if queued rather cancel as DFU is anyhow abored.
             */
            MessageCancelAll(Dfu_GetTask(),
                                DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE);
        }
    }
}

static void dfu_PeerEraseStartTx(void)
{
    bool is_primary = BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerEraseStartTx is_primary:%d", is_primary);

    if (is_primary)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();
        dfu_peer_erase_req_res_t *ind = PanicUnlessMalloc(sizeof(dfu_peer_erase_req_res_t));
        memset(ind, 0, sizeof(dfu_peer_erase_req_res_t));

        /*
         * Block/Hold DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE until
         * peer (Secondary) erase is done.
         */
        the_dfu->peerEraseDone = TRUE;

        /* Erase request sent */
        ind->peer_erase_req_res = TRUE;
        appPeerSigMarshalledMsgChannelTx(Dfu_GetTask(),
                                        PEER_SIG_MSG_CHANNEL_DFU,
                                        ind, MARSHAL_TYPE(dfu_peer_erase_req_res_t));
    }
}

static void dfu_PeerEraseStartRx(void)
{
    bool is_secondary = !BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerEraseStartRx is_secondary:%d", is_secondary);

    if (is_secondary)
    {
        bool wait_for_erase_complete;
        if (UpgradePartitionDataInitWrapper(&wait_for_erase_complete))
        {
            DEBUG_LOG("dfu_PeerEraseStartRx wait_for_erase_complete:%d", wait_for_erase_complete);
            if (!wait_for_erase_complete)
            {
                /* Already erased, response sent as success */
                dfu_PeerEraseCompletedTx(TRUE);
            }
        }
        else
        {
            DEBUG_LOG("dfu_PeerEraseStartRx no_memory error");
            /* Erase response sent as failed */
            dfu_PeerEraseCompletedTx(FALSE);
        }
    }
}

static void dfu_PeerDeviceNotInUseTx(void)
{
    bool is_primary = BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerDeviceNotInUseTx is_primary:%d", is_primary);

    if (is_primary)
    {
        dfu_peer_device_not_in_use_t *ind = PanicUnlessMalloc(sizeof(dfu_peer_device_not_in_use_t));
        memset(ind, 0, sizeof(dfu_peer_erase_req_res_t));

        /* send device_not_in_use indication to secondary device */
        appPeerSigMarshalledMsgChannelTx(Dfu_GetTask(),
                                        PEER_SIG_MSG_CHANNEL_DFU,
                                        ind, MARSHAL_TYPE(dfu_peer_device_not_in_use_t));
    }
}

static void dfu_PeerDeviceNotInUseRx(void)
{
    DEBUG_LOG("dfu_PeerDeviceNotInUseRx");
    Dfu_HandleDeviceNotInUse();
}

static void dfu_PeerSetContextTx(upgrade_context_t context)
{
    DEBUG_LOG("dfu_PeerSetContextTx context %d", context);

    dfu_peer_set_context_t *ind = PanicUnlessMalloc(sizeof(dfu_peer_set_context_t));
    memset(ind, 0, sizeof(dfu_peer_set_context_t));
    ind->context = (uint16) context;

    /* send dfu_peer_set_context_t indication to secondary device */
    appPeerSigMarshalledMsgChannelTx(Dfu_GetTask(),
                                    PEER_SIG_MSG_CHANNEL_DFU,
                                    ind, MARSHAL_TYPE(dfu_peer_set_context_t));
}

static void dfu_PeerSetContextRx(dfu_peer_set_context_t *msg)
{
    DEBUG_LOG("dfu_PeerSetContextRx context %d", msg->context);
    Upgrade_SetContext((upgrade_context_t) msg->context);
}

static void dfu_HandlePeerSigMarshalledMsgChannelRxInd(const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind)
{
    DEBUG_LOG("dfu_HandlePeerSigMarshalledMsgChannelRxInd. Channel 0x%x, type %d", ind->channel, ind->type);

    switch (ind->type)
    {
        case MARSHAL_TYPE_dfu_peer_erase_req_res_t:
            {
                dfu_peer_erase_req_res_t *msg = (dfu_peer_erase_req_res_t *)ind->msg;
                if (msg->peer_erase_req_res)
                {
                    /* Erase request received */
                    dfu_PeerEraseStartRx();
                }
                else
                {
                    /* Erase response received */
                    dfu_PeerEraseCompletedRx(msg);
                }
            }
            break;

        case MARSHAL_TYPE_dfu_peer_device_not_in_use_t:
            {
                /* device_not_in_use indication received */
                dfu_PeerDeviceNotInUseRx();
            }
            break;

        case MARSHAL_TYPE_dfu_peer_set_context_t:
            {
                dfu_peer_set_context_t *msg = (dfu_peer_set_context_t *)ind->msg;
                /* dfu_peer_set_context indication received */
                dfu_PeerSetContextRx(msg);
            }
            break;

        default:
            break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

static void dfu_HandlePeerSigMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm)
{
    peerSigStatus status = cfm->status;

    if (peerSigStatusSuccess != status)
    {
        DEBUG_LOG("dfu_HandlePeerSigMarshalledMsgChannelTxCfm reports failure code 0x%x(%d)", status, status);
    }

}

static void dfu_HandlePeerSigConnectInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    DEBUG_LOG("dfu_HandlePeerSigConnectInd, status %u", ind->status);

    /*
     * Make DFU domain aware of the current device role (Primary/Secondary)
     */
    if (ind->status == peerSigStatusConnected)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();
        bool is_primary = BtDevice_IsMyAddressPrimary();

        if (is_primary)
        {
            the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_PEERSIG;
            DEBUG_LOG("dfu_HandlePeerSigConnectInd (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
        }
        else
        {
            /*Cancel pending UPGRADE_PEER_CONNECT_REQ, if any*/
            UpgradePeerCancelDFU();
        }

        DfuPeer_SetRole(is_primary);

        /* Unblock the peer DFU L2CAP connection (if any) */
        UpgradePeerUpdateBlockCond(UPGRADE_PEER_BLOCK_NONE);

        /* Reset the peer DFU L2CAP disconnection reason to 0 (connected).
         */
        UpgradePeerStoreDisconReason(upgrade_peer_l2cap_connected);

        /* If the reboot reason is a defined reset as part of DFU process, then
         * start the peer connection once again, and continue with commit phase
         */
        if(Dfu_GetRebootReason() == REBOOT_REASON_DFU_RESET)
        {
            DEBUG_LOG("dfu_HandlePeerSigConnectInd: UpgradePeerApplicationReconnect()");
            /* Device is  restarted in upgrade process, send connect request again */
            UpgradePeerApplicationReconnect();
        }

    }
    /* In Panic situation, the peer device gets disconneted and peerSigStatusDisconnected is sent by peer_signalling which needs to be handled */
    else if (ind->status == peerSigStatusLinkLoss || ind->status == peerSigStatusDisconnected)
    {
        /*
         * In the post reboot DFU commit phase, now main role (Primary/Secondary)
         * are no longer fixed rather dynamically selected by Topology using role
         * selection. This process may take time so its recommendable to reset
         * this reconnection timer in linkloss scenarios (if any) in the post
         * reboot DFU commit phase.
         */
        UpgradeRestartReconnectionTimer();

        /* Block the peer DFU L2CAP connection in cases of link-loss to peer */
        UpgradePeerUpdateBlockCond(UPGRADE_PEER_BLOCK_UNTIL_PEER_SIG_CONNECTED);
    }

}

static void dfu_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T* ind)
{
    bool is_upgrade_in_progress = Dfu_IsUpgradeInProgress();
    bool is_primary = BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_HandleConManagerConnectionInd Conn:%u BLE:%u %04x,%02x,%06lx", ind->connected,
                                                                                          ind->ble,
                                                                                          ind->bd_addr.nap,
                                                                                          ind->bd_addr.uap,
                                                                                          ind->bd_addr.lap);
    if(!ind->ble && appDeviceIsPeer(&ind->bd_addr) && ind->connected && is_upgrade_in_progress && is_primary)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();
        the_dfu->peerProfilesToConnect = appPhyStateGetState() == PHY_STATE_IN_CASE ?
                        DEVICE_PROFILE_PEERSIG : TwsTopologyConfig_PeerProfiles();
        DEBUG_LOG("dfu_HandleConManagerConnectionInd PEER BREDR Connected (profiles:x%x) to connect", the_dfu->peerProfilesToConnect);
    }
}
#endif

static void dfu_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("dfu_MessageHandler. MESSAGE:dfu_internal_messages_t:0x%X", id);

    switch (id)
    {
#ifdef INCLUDE_DFU_PEER
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            dfu_HandlePeerSigMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            dfu_HandlePeerSigMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            dfu_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

        case PEER_SIG_CONNECTION_IND:
            dfu_HandlePeerSigConnectInd((const PEER_SIG_CONNECTION_IND_T *)message);
            break;

        /* MIRROR PROFILE MESSAGES */
        case MIRROR_PROFILE_CONNECT_IND:
            {
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();
                bool is_primary = BtDevice_IsMyAddressPrimary();

                if (is_primary)
                {
                    the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_MIRROR;
                    DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
                }
            }
            break;

        case HANDOVER_PROFILE_CONNECTION_IND:
            {
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();
                bool is_primary = BtDevice_IsMyAddressPrimary();

                if (is_primary)
                {
                    the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_HANDOVER;
                    DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
                }
            }
            break;
#endif

            /* Message sent in response to UpgradeInit().
             * In this case we need to forward to the app to unblock initialisation.
             */
        case UPGRADE_INIT_CFM:
            {
                const UPGRADE_INIT_CFM_T *init_cfm = (const UPGRADE_INIT_CFM_T *)message;

                DEBUG_LOG("dfu_MessageHandler. UPGRADE_INIT_CFM %d (sts)",init_cfm->status);

                dfu_ForwardInitCfm(init_cfm);
            }
            break;

            /* Message sent during initialisation of the upgrade library
                to let the VM application know that a restart has occurred
                and reconnection to a host may be required. */
        case UPGRADE_RESTARTED_IND:
            dfu_HandleRestartedInd((UPGRADE_RESTARTED_IND_T*)message);
            break;

            /* Message sent to application to request applying a downloaded upgrade.
                Note this may include a warm reboot of the device.
                Application must respond with UpgradeApplyResponse() */
        case UPGRADE_APPLY_IND:
            {
#ifdef INCLUDE_DFU_PEER
                bool isPrimary = BtDevice_IsMyAddressPrimary();
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();

                DEBUG_LOG("dfu_MessageHandler UPGRADE_APPLY_IND, isPrimary:%d", isPrimary);
#ifndef HOSTED_TEST_ENVIRONMENT
                if (isPrimary)
                {
                    /*
                     * As per the legacy scheme, Primary reboots post Secondary
                     * having rebooted. And as part of Secondary reboot, the
                     * peer links (including DFU L2CAP channel) are
                     * re-established. Wait for the connections of these other
                     * peer profiles to complete before Primary reboot, in order
                     * to avoid undefined behavior on the Secondary
                     * (such as panic on asserts) owing to invalid connection
                     * handles while handling disconnection sequence because
                     * of linkloss to Primary if the Primary didn't await for
                     * the peer profile connections to complete.
                     * Since no direct means to cancel the peer connection from
                     * DFU domain except Topology which can cancel through
                     * cancellable goals, for now its better to wait for the
                     * peer profile connections to be done before Primary
                     * reboot for a deterministic behavior and avoid the problem
                     * as described above.
                     *
                     * (Note: The invalid connection handle problem was seen
                     *        with Mirror Profile.)
                     *
                     */
                    MessageSendConditionally(Dfu_GetTask(),
                                            DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED, NULL,
                                            (uint16 *)&the_dfu->peerProfilesToConnect);
                }
                else
#endif
#endif
                {
                    DEBUG_LOG("dfu_MessageHandler. UPGRADE_APPLY_IND saying now !");
                    dfu_NotifyActivity();
                    if (Dfu_GetTaskData()->reboot_permission_required)
                    {
                        dfu_NotifyReadyToReboot();
                    }
                    else
                    {
                        UpgradeApplyResponse(0);
                    }
                }
            }
            break;

            /* Message sent to application to request blocking the system for an extended
                period of time to erase serial flash partitions.
                Application must respond with UpgradeBlockingResponse() */
        case UPGRADE_BLOCKING_IND:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_BLOCKING_IND");
            dfu_NotifyActivity();
            UpgradeBlockingResponse(0);
            break;

            /* Message sent to application to indicate that blocking operation is finished */
        case UPGRADE_BLOCKING_IS_DONE_IND:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_BLOCKING_IS_DONE_IND");
            dfu_NotifyActivity();
            break;

            /* Message sent to application to inform of the current status of an upgrade. */
        case UPGRADE_STATUS_IND:
            dfu_HandleUpgradeStatusInd((const UPGRADE_STATUS_IND_T *)message);
            break;

            /* Message recieved from upgrade library to handle upgrade operations */
        case UPGRADE_OPERATION_IND:
            dfu_HandleUpgradeOperationInd((const UPGRADE_OPERATION_IND_T *)message);
            break;

            /* Message received from upgrade library to get transport connection status */
        case UPGRADE_NOTIFY_TRANSPORT_STATUS:
            dfu_HandleUpgradeTransportNotification((const UPGRADE_NOTIFY_TRANSPORT_STATUS_T *)message);
            break;

            /* Message sent to application to request any audio to get shut */
        case UPGRADE_SHUT_AUDIO:
            dfu_HandleUpgradeShutAudio();
            break;
            
            /* Message sent to application to inform that upgrade is ready for the silent commit. */
        case UPGRADE_READY_FOR_SILENT_COMMIT:
            dfu_NotifyReadyforSilentCommit();
            break;

            /* Message sent to application set the audio busy flag and copy audio image */
        case UPRGADE_COPY_AUDIO_IMAGE_OR_SWAP:
            dfu_HandleUpgradeCopyAudioImageOrSwap();
            break;

            /* Message sent to application to reset the audio busy flag */
        case UPGRADE_AUDIO_COPY_FAILURE:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_AUDIO_COPY_FAILURE (not handled)");
            break;

            /* Message sent to application to inform that the actual upgrade has started */
        case UPGRADE_START_DATA_IND:
            {    
#ifdef INCLUDE_DFU_PEER
                bool is_primary = BtDevice_IsMyAddressPrimary();
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();

                DEBUG_LOG("dfu_MessageHandler UPGRADE_START_DATA_IND, is_primary:%d", is_primary);
#ifndef HOSTED_TEST_ENVIRONMENT
                if (is_primary)
                {
                    MessageSendConditionally(Dfu_GetTask(),
                                            DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE, NULL,
                                            (uint16 *)&the_dfu->peerEraseDone);
                }
                else
#endif
#endif
                {
                    dfu_NotifyStart();
                }
            }
            break;

            /* Message sent to application to inform that the actual upgrade has ended */
        case UPGRADE_END_DATA_IND:
            {
                UPGRADE_END_DATA_IND_T *end_data_ind = (UPGRADE_END_DATA_IND_T *)message;
                DEBUG_LOG("dfu_MessageHandler. UPGRADE_END_DATA_IND %d (handled for abort indication)", end_data_ind->state);

#ifdef INCLUDE_DFU_PEER
                /*
                 * If DFU is ended either as complete or aborted (device
                 * initiated: Handover or internal FatalError OR Host
                 * initiated), cancel the queued DFU start indication (if any)
                 * as its pointless to notify start indication after DFU has
                 * ended.
                 */
                MessageCancelAll(Dfu_GetTask(),
                                DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE);
#endif

                /* Notify application that upgrade has ended owing to abort. */
                if (end_data_ind->state == upgrade_end_state_abort)
                {
                    dfu_NotifyAborted();
                    /* To Do: remove when merging GAA resume changes because context gets cleared
                       as a part of upgrade pskey */
                    Upgrade_SetContext(UPGRADE_CONTEXT_UNUSED);
#ifdef INCLUDE_DFU_PEER
                    Dfu_SetPeerDataTransferStatus(DFU_PEER_DATA_TRANSFER_NOT_STARTED);
#endif
                }
            }
            break;

            /* Message sent to application to inform for cleaning up DFU state variables on Abort */
        case UPGRADE_CLEANUP_ON_ABORT:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_CLEANUP_ON_ABORT");
            dfu_NotifyAbort();
            break;

#ifdef INCLUDE_DFU_PEER
        case DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE:
            {
                bool isPrimary = BtDevice_IsMyAddressPrimary();
                DEBUG_LOG("dfu_MessageHandler. DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE");
                dfu_NotifyStart();

                /*
                 * Ideally the handled msg is triggered on the Primary.
                 * Even then its still safe to rely on bt_device to pass the
                 * appropriate main role (i.e. Primary/Secondary).
                 *
                 * This is required because concurrent DFU is always started by
                 * the Primary and if the role is not updated then peer DFU(
                 * (either conncurrent or serial) shall fail to start.
                 */
                DfuPeer_SetRole(isPrimary);

                /* Start peer dfu if supported */
                dfu_StartPeerDfu();
            }
            break;

            case DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED:
            {
                DEBUG_LOG("dfu_MessageHandler. DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED, Respond to UPGRADE_APPLY_IND now!");
                dfu_NotifyActivity();
                UpgradeApplyResponse(0);
            }
            break;
#endif

            /* Set appropriate reboot reason if a commit is reverted or 
             * unexpected reset of device encountered during post reboot phase.
             */
        case UPGRADE_REVERT_RESET:
            DEBUG_LOG_DEBUG("dfu_MessageHandler. UPGRADE_REVERT_RESET");
            Dfu_SetRebootReason(REBOOT_REASON_REVERT_RESET);
            break;

        case DFU_INTERNAL_CONTINUE_HASH_CHECK_REQUEST:
            {
                DEBUG_LOG_INFO("dfu_MessageHandler. DFU_INTERNAL_CONTINUE Hash Checking");
                UpgradeSmStartHashChecking();
            }
            break;

        case MESSAGE_IMAGE_UPGRADE_ERASE_STATUS:
            DEBUG_LOG("dfu_MessageHandler. MESSAGE_IMAGE_UPGRADE_ERASE_STATUS");

            dfu_NotifyActivity();
#ifdef INCLUDE_DFU_PEER
            MessageImageUpgradeEraseStatus *msg =
                                    (MessageImageUpgradeEraseStatus *)message;
            dfu_PeerEraseCompletedTx(msg->erase_status);
#endif
            UpgradeEraseStatus(message);
            break;

        case MESSAGE_IMAGE_UPGRADE_COPY_STATUS:
            DEBUG_LOG("dfu_MessageHandler. MESSAGE_IMAGE_UPGRADE_COPY_STATUS");

            dfu_NotifyActivity();
            UpgradeCopyStatus(message);
            break;

        case MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS:
            DEBUG_LOG("dfu_MessageHandler. MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS");
            UpgradeHashAllSectionsUpdateStatus(message);
            break;
            /* Catch-all panic for unexpected messages */
        default:
            if (UPGRADE_UPSTREAM_MESSAGE_BASE <= id && id <  UPGRADE_UPSTREAM_MESSAGE_TOP)
            {
                DEBUG_LOG_ERROR("dfu_MessageHandler. Unexpected upgrade library message MESSAGE:0x%x", id);
            }
#ifdef INCLUDE_DFU_PEER
            else if (PEER_SIG_INIT_CFM <= id && id <= PEER_SIG_LINK_LOSS_IND)
            {
                DEBUG_LOG("dfu_MessageHandler. Unhandled peer sig message MESSAGE:0x%x", id);
            }
#endif
            else
            {
                DEBUG_LOG_ERROR("dfu_MessageHandler. Unexpected message MESSAGE:dfu_internal_messages_t:0x%X", id);
            }
            break;
    }

}

static void dfu_SetGattServiceUpdateFlagForHandset(device_t device, void *data)
{
    UNUSED(data);
    if(BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET)
    {
        if(!BtDevice_IsFirstConnectAfterDFU(device))
        {
            appDeviceSetFirstConnectAfterDFU(device, TRUE);
            DeviceDbSerialiser_SerialiseDevice(device);
        }
    }
}

static void dfu_SetGattServiceUpdateFlags(void)
{
    DeviceList_Iterate(dfu_SetGattServiceUpdateFlagForHandset, NULL);
}

static void dfu_GattConnect(gatt_cid_t cid)
{
    DEBUG_LOG("dfu_GattConnect. cid:0x%X", cid);
    device_t device = NULL;

    device = GattConnect_GetBtDevice(cid);
    if(device)
    {
        DEBUG_LOG("dfu_GattConnect retrieving property device=0x%p", device);
        if(BtDevice_IsFirstConnectAfterDFU(device))
        {
            GattServerGatt_SetServerServicesChanged(cid);
            appDeviceSetFirstConnectAfterDFU(device, FALSE);
        }
    }
}

static void dfu_GattDisconnect(gatt_cid_t cid)
{
    DEBUG_LOG("dfu_GattDisconnect. cid:0x%X", cid);

    /* We choose not to do anything when GATT is disconnect */
}

static void dfu_GetVersionInfo(dfu_VersionInfo *ver_info)
{
    *ver_info = Dfu_GetTaskData()->verInfo;
}

bool Dfu_AllowUpgrades(bool allow)
{
    upgrade_status_t sts = (upgrade_status_t)-1;
    bool successful = FALSE;

    /* The Upgrade library API can panic very easily if UpgradeInit had
       not been called previously */
    if (SystemState_GetState() > system_state_initialised)
    {
        upgrade_permission_t permission = upgrade_perm_no;

        if (allow && Dfu_GetTaskData()->reboot_permission_required)
        {
            permission = upgrade_perm_always_ask;
        }
        else if (allow)
        {
            permission = upgrade_perm_assume_yes;
        }

         sts = UpgradePermit(permission);
         successful = (sts == upgrade_status_success);
    }

    DEBUG_LOG("Dfu_AllowUpgrades(%d) - success:%d (sts:%d)", allow, successful, sts);

    return successful;
}

void Dfu_RequireRebootPermission(bool permission_required)
{
    DEBUG_LOG("Dfu_RequireRebootPermission %u", permission_required);
    Dfu_GetTaskData()->reboot_permission_required = permission_required;
}

void Dfu_RebootConfirm(void)
{
    DEBUG_LOG("Dfu_RebootConfirm rebooting now");
    UpgradeApplyResponse(0);
}

void Dfu_ClientRegister(Task tsk)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), tsk);
}

/* Since primary device is the one connected to Host, Dfu_SetContext will be 
 * called only on primary device and not for secondary device.
 */
void Dfu_SetContext(upgrade_context_t context)
{
    /* Upgrade_SetContext sets the context of the upgrade module in a PS Key
     * whereas Upgrade_SetHostType is used to set a variable in upgrade 
     * context which is used to decide the resume methodology for Primary 
     * EB to Secondary EB resume. (As of now PEB to SEB transfer is via GAIA).
     */
    Upgrade_SetContext(context);
    Upgrade_SetHostType(context);

#ifdef INCLUDE_DFU_PEER
    /* Synchronize the upgrade context with peer.*/
    dfu_PeerSetContextTx(context);
#endif /* INCLUDE_DFU_PEER */
}

upgrade_context_t Dfu_GetContext(void)
{
    return Upgrade_GetContext();
}

/*! \brief Return REBOOT_REASON_DFU_RESET for defined reboot phase of upgrade
           else REBOOT_REASON_ABRUPT_RESET for abrupt reset.
 */
dfu_reboot_reason_t Dfu_GetRebootReason(void)
{
    return Dfu_GetTaskData()->dfu_reboot_reason;
}

/*! \brief Set to REBOOT_REASON_DFU_RESET for defined reboot phase of upgrade
           else REBOOT_REASON_ABRUPT_RESET for abrupt reset.
 */
void Dfu_SetRebootReason(dfu_reboot_reason_t val)
{
    Dfu_GetTaskData()->dfu_reboot_reason = val;
}

extern bool UpgradePSClearStore(void);
/*! \brief Clear upgrade related PSKeys.
 */
bool Dfu_ClearPsStore(void)
{
    /* Clear out any in progress DFU status */
    return UpgradePSClearStore();
}

uint32 Dfu_GetDFUHandsetProfiles(void)
{
    return (DEVICE_PROFILE_GAIA | DEVICE_PROFILE_GAA | DEVICE_PROFILE_ACCESSORY);
}

void Dfu_HandleDeviceNotInUse(void)
{
    DEBUG_LOG_INFO("Dfu_HandleDeviceNotInUse: Initiate DFU reboot");

#ifdef INCLUDE_DFU_PEER
    /* Inform the peer about device not in use. */
    dfu_PeerDeviceNotInUseTx();
#endif /* INCLUDE_DFU_PEER */

    UpgradeRebootForSilentCommit();
}

bool Dfu_IsSilentCommitEnabled(void)
{
    return UpgradeIsSilentCommitEnabled();
}

bool Dfu_IsUpgradeInProgress(void)
{
    return (UpgradeInProgressId() != 0);
}

void Dfu_SetVersionInfo(uint16 uv_major, uint16 uv_minor, uint16 cfg_ver)
{
    Dfu_GetTaskData()->verInfo.upgrade_ver.major =  uv_major;
    Dfu_GetTaskData()->verInfo.upgrade_ver.minor = uv_minor;
    Dfu_GetTaskData()->verInfo.config_ver = cfg_ver;
}

void Dfu_SetSilentCommitSupported(uint8 is_silent_commit_supported)
{
    DEBUG_LOG_INFO("Dfu_SetSilentCommitSupported: is_silent_commit_supported %d",
                    is_silent_commit_supported);
    UpgradeSetSilentCommitSupported(is_silent_commit_supported);
}

void Dfu_RequestQOS(void)
{
    bdaddr bd_addr;
    tp_bdaddr tpaddr ;

    cm_qos_t current_qos;

    /* Get Handset BT Address */
    if(appDeviceGetHandsetBdAddr(&bd_addr))
    {
        memset(&tpaddr, 0, sizeof(tp_bdaddr));
        tpaddr.transport  = TRANSPORT_BLE_ACL;
        tpaddr.taddr.type = TYPED_BDADDR_PUBLIC;
        tpaddr.taddr.addr = bd_addr;

        current_qos = conManagerGetConnectionDeviceQos(&tpaddr);
        /* If it is already set as low_latency (sometime in case of Earbuds),
         * no need to set it again */
        if(current_qos == cm_qos_low_latency)
        {
            DEBUG_LOG_INFO("Dfu_RequestQos: for BLE transport low latency QoS is already set");
        }
        else
        {
            DEBUG_LOG_INFO("Dfu_RequestQos: for BLE transport set QOS to low latency");
            ConManagerRequestDeviceQos(&tpaddr, cm_qos_low_latency);
            Dfu_GetTaskData()->is_qos_release_needed_post_dfu = TRUE;
        }
    }
}

void Dfu_ReleaseQOS(void)
{
    bdaddr bd_addr;
    tp_bdaddr tpaddr ;
    cm_qos_t current_qos;

    /* Check if Handset BT address is set and fetch it accordingly */
    if(appDeviceGetHandsetBdAddr(&bd_addr))
    {
        memset(&tpaddr, 0, sizeof(tp_bdaddr));
        tpaddr.transport  = TRANSPORT_BLE_ACL;
        tpaddr.taddr.type = TYPED_BDADDR_PUBLIC;
        tpaddr.taddr.addr = bd_addr;

        current_qos = conManagerGetConnectionDeviceQos(&tpaddr);
        /* QOS released from cm_qos_low_latency to low_power if the current QOS is
         * set as cm_qos_low_latency due to Dfu_RequestQOS() call
         */
        if (current_qos == cm_qos_low_latency && Dfu_GetTaskData()->is_qos_release_needed_post_dfu)
        {
            DEBUG_LOG_INFO("Dfu_ReleaseQos: for BLE transport release QOS from low latency");
            ConManagerReleaseDeviceQos(&tpaddr, cm_qos_low_latency);
        }
    }
    /* Set this to FALSE post DFU, as older values might exist due to role
     * switch/handover sometimes for Earbuds. Also, there is no harm in setting
     * it to FALSE irrespective of whether release QoS took place or not, once
     * the DFU is over.
     */
    Dfu_GetTaskData()->is_qos_release_needed_post_dfu = FALSE;

}

#ifdef INCLUDE_DFU_PEER
void Dfu_SetPeerDataTransferStatus(peer_data_transfer_status status)
{
    Dfu_GetTaskData()->peerDataTransferStatus = status;
}

void Dfu_UpgradeHostRspSwap(bool is_primary)
{
    if(!is_primary)
    {
        UpgradeSetFPtr(UpgradePeerGetFPtr());
    }
    else
    {
        UpgradeSetFPtr(UpgradeGetFPtr());
    }
}
#endif /* INCLUDE_DFU_PEER */

#endif /* INCLUDE_DFU */
