/****************************************************************************
Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_peer.c

DESCRIPTION
    This file handles Upgrade Peer connection state machine and DFU file
    transfers.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade_peer
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <ps.h>

#include "upgrade_peer_private.h"

#include "upgrade_peer_if_data.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(upgrade_peer_internal_msg_t)

/*
 * Optimize back to back pmalloc pool request while handling
 * UPGRADE_PEER_START_DATA_BYTES_REQ from the peer and framing the response DFU
 * data pdu UPGRADE_HOST_DATA in the same context.
 *
 * This optimization is much required especially when the DFU data pdus are
 * large sized and when concurrent DFU is ongoing (i.e both the earbuds are in
 * the data transfer phase).
 *
 * NOTE: Comment the following define, to strip off this optimization.
 */
#define UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE

#define SHORT_MSG_SIZE (sizeof(uint8) + sizeof(uint16))

/* PSKEYS are intentionally limited to 32 words to save stack. */
#define PSKEY_MAX_STORAGE_LENGTH    32

#define MAX_START_REQ_RETRAIL_NUM       (5)

#define HOST_MSG(x)     (x + UPGRADE_HOST_MSG_BASE)

#define INTERNAL_PEER_MSG_DELAY 5000

#define INTERNAL_PEER_MSG_VALIDATION_SEND_DELAY D_SEC(2)

/* A fixed gap is introduced between read and write offset
 * for parallel read from the same partition during DFU
 * (due to P0 constraint, refer B-307688)*/
#define READ_WRITE_OFFSET_GAP 1024

#define INTERNAL_PEER_MSG_SHORT_DELAY 200

#define MAX_PACKET_SIZE (240)
UPGRADE_PEER_INFO_T *upgradePeerInfo = NULL;

/* This is used to store upgrade context when reboot happens */
typedef struct {
    uint16 length;
    uint16 ram_copy[PSKEY_MAX_STORAGE_LENGTH];
} FSTAB_PEER_COPY;

const upgrade_response_functions_t UpgradePeer_fptr = {
    .SendSyncCfm = UpgradePeerIFDataSendSyncCfm,
    .SendShortMsg = UpgradePeerIFDataSendShortMsg,
    .SendStartCfm = UpgradePeerIFDataSendStartCfm,
    .SendBytesReq = UpgradePeerIFDataSendBytesReq,
    .SendErrorInd = UpgradePeerIFDataSendErrorInd,
    .SendIsCsrValidDoneCfm = UpgradePeerIFDataSendIsCsrValidDoneCfm,
    .SendSilentCommitSupportedCfm = UpgradePeerIFDataSendSilentCommitSupportedCfm
};

static void upgradePeer_SendAbortReq(void);
static void upgradePeer_SendConfirmationToPeer(upgrade_confirmation_type_t type,
                                   upgrade_action_status_t status);
static void upgradePeer_SendTransferCompleteReq(upgrade_action_status_t status);
static void upgradePeer_SendCommitCFM(upgrade_action_status_t status);
static void upgradePeer_SendInProgressRes(upgrade_action_status_t status);
#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
static void upgradePeer_SendPeerData(uint8 *data, uint16 dataSize);
#else
static void upgradePeer_SendPeerData(uint8 *data, uint16 dataSize, bool isPreAllocated);
#endif
static void upgradePeer_SendStartReq (void);
static void upgradePeer_SendErrorConfirmation(uint16 error);
static void upgradePeer_SendValidationDoneReq (void);
static void upgradePeer_SendSyncReq (uint32 md5_checksum);
static void upgradePeer_SendStartDataReq (void);
static UPGRADE_PEER_CTX_T *upgradePeer_CtxGet(void);
static void upgradePeer_StopUpgrade(void);

/**
 * Set resume point as provided by Secondary device.
 */
static void upgradePeer_SetResumePoint(upgrade_peer_resume_point_t point)
{
    upgradePeerInfo->SmCtx->mResumePoint = point;
}

/**
 * Return the abort status of Peer DFU.
 */
bool UpgradePeerIsPeerDFUAborted(void)
{
    if(upgradePeerInfo != NULL)
    {
        return upgradePeerInfo ->is_dfu_aborted;
    }
    else
    {
        DEBUG_LOG("UpgradePeerIsPeerDFUAborted: Invalid access of upgradePeerInfo ptr. Panic the app");
        Panic();
        /* To satisfy the compiler */
        return FALSE;
    }
}

/**
 * Abort Peer upgrade is case error occurs.
 */
static void upgradePeer_AbortPeerDfu(void)
{
    DEBUG_LOG("upgradePeer_AbortPeerDfu");

    /* Once connection is establish, first send abort to peer device */
    if (upgradePeerInfo->SmCtx->isUpgrading)
    {
        UpgradeCtxSetWaitForPeerAbort(TRUE);
        upgradePeer_SendAbortReq();
        upgradePeerInfo->SmCtx->isUpgrading = FALSE;
    }
    else
    {
        /* We are here because user has aborted upgrade and peer device
         * upgrade is not yet started. Send Link disconnetc request.
         */
        if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_SYNC)
        {
            upgradePeer_StopUpgrade();
        }
    }
    /* Since the Peer DFU is aborted now, set this to TRUE*/
    upgradePeerInfo->is_dfu_aborted = TRUE;
}

/**
 * Primary device got confirmation from Host. Send this to secondary device
 * now.
 */
static void upgradePeer_SendConfirmationToPeer(upgrade_confirmation_type_t type,
                                   upgrade_action_status_t status)
{
    DEBUG_LOG("upgradePeer_SendConfirmationToPeer: type enum:upgrade_confirmation_type_t:%d, status enum:upgrade_action_status_t:%d", type, status);

    switch (type)
    {
        case UPGRADE_TRANSFER_COMPLETE:
            if(status == UPGRADE_CONTINUE || status == UPGRADE_SILENT_COMMIT)
            {
                upgradePeer_SendTransferCompleteReq(status);
            }
            else
            {
                upgradePeer_AbortPeerDfu();
            }
            break;

        case UPGRADE_COMMIT:
            upgradePeer_SendCommitCFM(status);
            break;

        case UPGRADE_IN_PROGRESS:
            if(status == UPGRADE_CONTINUE)
            {
                upgradePeer_SendInProgressRes(status);
            }
            else
            {
                upgradePeer_AbortPeerDfu();
            }
            break;

        default:
            DEBUG_LOG("upgradePeer_SendConfirmationToPeer: unhandled");
            break;
    }

    if (status == UPGRADE_ABORT)
    {
        upgradePeer_AbortPeerDfu();
    }
}

/**
 * To continue the process this manager needs the listener to confirm it.
 *
 */
static void upgradePeer_AskForConfirmation(upgrade_confirmation_type_t type)
{
    upgradePeerInfo->SmCtx->confirm_type = type;
    DEBUG_LOG("upgradePeer_AskForConfirmation: type enum:upgrade_confirmation_type_t:%d", type);

    switch (type)
    {
        case UPGRADE_TRANSFER_COMPLETE:
            /* Send message to UpgradeSm indicating TRANSFER_COMPLETE_IND */
            UpgradeHandleMsg(NULL, HOST_MSG(UPGRADE_PEER_TRANSFER_COMPLETE_IND), NULL);
            break;
        case UPGRADE_COMMIT:
            /* Send message to UpgradeSm */
            UpgradeCommitMsgFromUpgradePeer();
            break;
        case UPGRADE_IN_PROGRESS:
            /* Device is rebooted let inform Host to continue further.
             * Send message to UpgradeSm.
             */
            if(upgradePeerInfo->SmCtx->peerState ==
                               UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT)
            {
                UpgradePeerSetState(UPGRADE_PEER_STATE_COMMIT_HOST_CONTINUE);
                /* We can resume DFU only when primary device is rebooted */
                UpgradeHandleMsg(NULL, HOST_MSG(UPGRADE_PEER_SYNC_AFTER_REBOOT_REQ), NULL);
            }
            break;
        default:
            DEBUG_LOG("upgradePeer_AskForConfirmation: unhandled");
            break;
    }
}

/**
 * Destroy UpgradePeer context when upgrade process is aborted or completed.
 */
static void upgradePeer_CtxDestroy(void)
{
    /* Only free UpgradePeer SM context. This can be allocated again once
     * DFU process starts during same power cycle.
     * UpgradePeer Info is allocated only once during boot and never destroyed
     */
    if(upgradePeerInfo != NULL)
    {
        if(upgradePeerInfo->SmCtx != NULL)
        {
            upgradePeerInfo->SmCtx->confirm_type = UPGRADE_TRANSFER_COMPLETE;
            upgradePeerInfo->SmCtx->peerState = UPGRADE_PEER_STATE_SYNC;
            upgradePeerInfo->SmCtx->mResumePoint = UPGRADE_PEER_RESUME_POINT_START;
            if(upgradePeerInfo->SmCtx != NULL)
            {
                free(upgradePeerInfo->SmCtx);
                upgradePeerInfo->SmCtx = NULL;
            }
            /* If peer data transfer is going on then "INTERNAL_PEER_DATA_CFM_MSG" could be queued
             * at this point which can lead to NULL pointer dereference in SelfKickNextDataBlock()
             * function because, we are clearing upgradePeerInfo->SmCtx */
            MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_DATA_CFM_MSG);
        }
    }
}

/**
 * To stop the upgrade process.
 */
static void upgradePeer_StopUpgrade(void)
{
    upgradePeerInfo->SmCtx->isUpgrading = FALSE;
    MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_DISCONNECT_REQ, NULL);
    /* Clear Pskey to start next upgrade from fresh */
    memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    UpgradePeerSavePSKeys();
    upgradePeer_CtxDestroy();
}

/**
 * To clean and set the upgradePeerInfo for next DFU
 */
static void upgradePeer_CleanUpgradePeerCtx(void)
{
    upgradePeerInfo->SmCtx->isUpgrading = FALSE;
    /* Clear Pskey to start next upgrade from fresh */
    memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS * sizeof(uint16));
    UpgradePeerSavePSKeys();
    upgradePeer_CtxDestroy();
}

/**
 * To reset the file transfer.
 */
static void upgradePeer_CtxSet(void)
{
    upgradePeerInfo->SmCtx->mStartAttempts = 0;
}

/**
 * Immediate answer to secondury device, data is the same as the received one.
 */
static void upgradePeer_HandleErrorWarnRes(uint16 error)
{
    DEBUG_LOG("HandleErrorWarnRes: UpgradePeer: Handle Error Ind");
    upgradePeer_SendErrorConfirmation(error);
}

/**
 * To send an UPGRADE_START_REQ message.
 */
static void upgradePeer_SendStartReq (void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendStartReq: UpgradePeer: Start REQ");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_START_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

/****************************************************************************
NAME
    UpgradeHostIFClientSendData

DESCRIPTION
    Send a data packet to a connected upgrade client.

*/
static void upgradePeer_SendErrorMsg(upgrade_peer_status_t error)
{
    UpgradeErrorMsgFromUpgradePeer(error);
}

/**
 * This method is called when we received an UPGRADE_START_CFM message. This
 * method reads the message and starts the next step which is sending an
 * UPGRADE_START_DATA_REQ message, or aborts the upgrade depending on the
 * received message.
 */
static void upgradePeer_ReceiveStartCFM(UPGRADE_PEER_START_CFM_T *data)
{
    DEBUG_LOG("upgradePeer_ReceiveStartCFM");

    // the packet has to have a content.
    if (data->common.length >= UPRAGE_HOST_START_CFM_DATA_LENGTH)
    {
        //noinspection IfCanBeSwitch
        if (data->status == UPGRADE_PEER_SUCCESS)
        {
            upgradePeerInfo->SmCtx->mStartAttempts = 0;
            UpgradePeerSetState(UPGRADE_PEER_STATE_DATA_READY);
            upgradePeer_SendStartDataReq();
        }
        else if (data->status == UPGRADE_PEER_ERROR_APP_NOT_READY)
        {
            if (upgradePeerInfo->SmCtx->mStartAttempts < MAX_START_REQ_RETRAIL_NUM)
            {
                // device not ready we will ask it again.
                upgradePeerInfo->SmCtx->mStartAttempts++;
                MessageSendLater((Task)&upgradePeerInfo->myTask,
                                 INTERNAL_START_REQ_MSG, NULL, 2000);
            }
            else
            {
                upgradePeerInfo->SmCtx->mStartAttempts = 0;
                upgradePeerInfo->SmCtx->upgrade_status =
                                   UPGRADE_PEER_ERROR_IN_ERROR_STATE;
                upgradePeer_SendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
            }
        }
        else
        {
            upgradePeerInfo->SmCtx->upgrade_status = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
            upgradePeer_SendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
        }
    }
    else {
        upgradePeerInfo->SmCtx->upgrade_status = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
        upgradePeer_SendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
    }
}

/**
 * To send a UPGRADE_SYNC_REQ message.
 */
static void upgradePeer_SendSyncReq (uint32 md5_checksum)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendSyncReq: md5_checksum 0x%08X", md5_checksum);

    payload = PanicUnlessMalloc(UPGRADE_SYNC_REQ_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_SYNC_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                    UPGRADE_SYNC_REQ_DATA_LENGTH);
    byteIndex += ByteUtilsSet4Bytes(payload, byteIndex, md5_checksum);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * This method is called when we received an UPGRADE_SYNC_CFM message.
 * This method starts the next step which is sending an UPGRADE_START_REQ
 * message.
 */
static void upgradePeer_ReceiveSyncCFM(UPGRADE_PEER_SYNC_CFM_T *update_cfm)
{
    DEBUG_LOG("upgradePeer_ReceiveSyncCFM");

    upgradePeer_SetResumePoint(update_cfm->resume_point);
    UpgradePeerSetState(UPGRADE_PEER_STATE_READY);
    upgradePeer_SendStartReq();
}

/**
 * To send an UPGRADE_DATA packet.
 */
static void upgradePeer_SendDataToPeer (uint32 data_length, uint8 *data, bool is_last_packet)
{
    uint16 byteIndex = 0;

    byteIndex += ByteUtilsSet1Byte(data, byteIndex, UPGRADE_PEER_DATA);
    byteIndex += ByteUtilsSet2Bytes(data, byteIndex, data_length +
                                    UPGRADE_DATA_MIN_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(data, byteIndex,
                                   is_last_packet);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(data, data_length + UPGRADE_PEER_PACKET_HEADER +
                                     UPGRADE_DATA_MIN_DATA_LENGTH);
#else
    upgradePeer_SendPeerData(data, data_length + UPGRADE_PEER_PACKET_HEADER +
                                     UPGRADE_DATA_MIN_DATA_LENGTH, TRUE);
#endif
}

static bool upgradePeer_StartPeerData(uint32 dataLength, uint8 *packet, bool is_last_packet)
{
    DEBUG_LOG_VERBOSE("upgradePeer_StartPeerData: data %p, len %u", packet, dataLength);

    /* Set up parameters */
    if (packet == NULL)
    {
        upgradePeerInfo->SmCtx->upgrade_status = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
        return FALSE;
    }

    upgradePeer_SendDataToPeer(dataLength, packet, is_last_packet);

    if (is_last_packet)
    {
        DEBUG_LOG("upgradePeer_StartPeerData: last packet");
        if (upgradePeerInfo->SmCtx->mResumePoint == UPGRADE_PEER_RESUME_POINT_START)
        {
            upgradePeer_SetResumePoint(UPGRADE_PEER_RESUME_POINT_PRE_VALIDATE);
            /* For concurrent DFU, Since peer dfu is completed now, set the
             * peer data transfer complete
             */
            MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_END_DATA_TRANSFER, NULL);

            upgradePeer_SendValidationDoneReq();
        }
    }
    return TRUE;
}



static upgrade_peer_status_t upgradePeer_SendData(void)
{
#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    UPGRADE_PEER_DATA_IND_T *dataInd = NULL;
#endif
    const uint32 remaining_size =  upgradePeerInfo->SmCtx->total_req_size -  upgradePeerInfo->SmCtx->total_sent_size;
    const uint16 req_data_bytes = MIN(remaining_size, MAX_PACKET_SIZE);

    /* This will be updated as how much data device is sending */
    uint32 data_length = 0;
    bool is_last_packet = FALSE;
    uint8 *payload = NULL;
    upgrade_peer_status_t status;

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    const uint16 pkt_len = req_data_bytes + UPGRADE_PEER_PACKET_HEADER + UPGRADE_DATA_MIN_DATA_LENGTH;

    /* Allocate memory for data read from partition */
    payload = PanicUnlessMalloc(pkt_len);
#else
    const uint16 pkt_len = (req_data_bytes + UPGRADE_PEER_PACKET_HEADER +
                            UPGRADE_DATA_MIN_DATA_LENGTH +
                            sizeof(UPGRADE_PEER_DATA_IND_T) - 1);

    /* Allocate memory for data read from partition */
    dataInd =(UPGRADE_PEER_DATA_IND_T *)PanicUnlessMalloc(pkt_len);

    payload = &dataInd->data[0];
    DEBUG_LOG_V_VERBOSE("upgradePeer_SendData: dataInd %p, pkt_len %u, payload %p", dataInd, pkt_len, payload);

#endif

    status = UpgradePeerPartitionMoreData(&payload[UPGRADE_PEER_PACKET_HEADER + UPGRADE_DATA_MIN_DATA_LENGTH],
                                            &is_last_packet, req_data_bytes,
                                            &data_length, 
                                            upgradePeerInfo->SmCtx->req_start_offset);

    upgradePeerInfo->SmCtx->total_sent_size += data_length;

    /* data has been read from partition, now send to peer device */
    if (status == UPGRADE_PEER_SUCCESS)
    {
        if (!upgradePeer_StartPeerData(data_length, payload, is_last_packet))
            status = UPGRADE_PEER_ERROR_PARTITION_OPEN_FAILED;
    }

    if (status != UPGRADE_PEER_SUCCESS)
    {
#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
        if (payload)
            free (payload);
#else
        if (dataInd)
            free (dataInd);
#endif
    }

    return status;
}


/**
 * This method is called when we received an UPGRADE_DATA_BYTES_REQ message.
 * We manage this packet and use it for the next step which is to upload the
 * file on the device using UPGRADE_DATA messages.
 */
static void upgradePeer_ReceiveDataBytesREQ(UPGRADE_PEER_START_DATA_BYTES_REQ_T *data)
{
    upgrade_peer_status_t error;

    DEBUG_LOG_VERBOSE("UpgradePeer_ReceiveDataBytesREQ: bytes %u, offset %u", data->data_bytes, data->start_offset);

    /* Checking the data has the good length */
    if (data->common.length == UPGRADE_DATA_BYTES_REQ_DATA_LENGTH)
    {
        UpgradePeerSetState(UPGRADE_PEER_STATE_DATA_TRANSFER);

        upgradePeerInfo->SmCtx->total_req_size = data->data_bytes;
        upgradePeerInfo->SmCtx->total_sent_size = 0;
        /*
         * Honour the peer requested start offset which is significant when
         * peer requests for partition data which was partially received
         * owing to an abrupt reset. This is must to support seamless DFU
         * even after interruption owing to either earbud having being abruptly
         * reset.
         */
        upgradePeerInfo->SmCtx->req_start_offset = data->start_offset;

        error = upgradePeer_SendData();
    }
    else
    {
        error = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
        DEBUG_LOG_ERROR("upgradePeer_ReceiveDataBytesREQ: invalid length %u", data->common.length);
    }

    if(error == UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY)
    {
        UPGRADE_PEER_DATA_BYTES_REQ_T *msg;
        
        DEBUG_LOG_WARN("upgradePeer_ReceiveDataBytesREQ: Concurrent DFU pskey not filled in");
        msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
        memmove(msg, data, sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
        MessageSendLater((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_MSG, msg,INTERNAL_PEER_MSG_DELAY);
        return;
    }
 
    if(error != UPGRADE_PEER_SUCCESS)
    {
        DEBUG_LOG_ERROR("upgradePeer_ReceiveDataBytesREQ: error enum:upgrade_peer_status_t:%d", error);

        upgradePeerInfo->SmCtx->upgrade_status = error;
        upgradePeer_SendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
    }
    else
    {
        DEBUG_LOG_VERBOSE("upgradePeer_ReceiveDataBytesREQ: total_size %u, total_sent %u, req_start_offset %u",
                          upgradePeerInfo->SmCtx->total_req_size,
                          upgradePeerInfo->SmCtx->total_sent_size,
                          upgradePeerInfo->SmCtx->req_start_offset);
    }
}

/**
 * This method is called when we receive UPGRADE_PEER_DATA_BYTES_REQ and also
 * from upgradePeer_SelfKickNextDataBlock. This contains a series of checks to make sure
 * that during DFU File transfer while the Host is writing to and Peer is reading
 * on same partition, the read does not go ahead of write.
 */
static bool upgradePeer_DelayParallelReadRequest(void)
{
    uint32 prim_offset    = UpgradeCtxGetPartitionDataOffset();
    uint32 sec_offset     = UpgradePeerPartitionDataCtxGet()->partition_offset;
    uint32 written_so_far = prim_offset + UpgradeCtxGetPartitionDataTotalReceivedSize();
    uint32 read_so_far    = sec_offset+upgradePeerInfo->SmCtx->total_sent_size; 
    uint16 prim_partn     = UpgradeCtxGetFWPartitionNum();
    uint16 sec_partn      = UpgradePeerPartitionDataCtxGet()->partNum;

    DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest : Primary at partition = %d and Secondary at partition = %d", prim_partn, sec_partn);
    DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest : Total size of primary Partition = %u", UpgradeCtxGetPartitionDataPartitionLength());

    DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest : Written so far (primary)........= %u / %u", written_so_far, UpgradeCtxGetPartitionDataPartitionLength());
    DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest : Read so far    (secondary)......= %u / %u", read_so_far, upgradePeerInfo->SmCtx->total_req_size);

    DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest : Primary   Offset................= %lu",prim_offset);
    DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest : Secondary Offset................= %lu",sec_offset);

    /*In scenarios like primary reset the device iterates through all partitions 
    once back while secondary is at last opened partition. Hence directly return true
    until primary is behind in terms of partition numbers*/
    if(prim_partn < sec_partn)
    {
        DEBUG_LOG("upgradePeer_DelayParallelReadRequest : as secondary partiton > primary partition");
        return TRUE;
    }
    /*If primary is writing to and secondary is reading from same partition,
    compare the write and read offsets and return true if secondary is requesting ahead.*/
    if(prim_partn == sec_partn )
    {
        /*
         * Since partition numbers remain same when partition data state changes to footer we use
         * UpgradeSmState : UPGRADE_STATE_DATA_HASH_CHECKING as an indicator to identify that
         * Footer partition data state is completed and footer can thus be requested by secondary device.
         */
        if(UpgradeCtxIsPartitionDataStateFooter())
        {
            DEBUG_LOG_INFO("upgradePeer_DelayParallelReadRequest: Footer state, hash checking = %d",UpgradeSmStateIsDataHashChecking());
            return UpgradeSmStateIsDataHashChecking() ? FALSE: TRUE;
        }
        else
        {
            /* Since concurrent flash section data write and read support for ImageUpgrade Source stream is not there
             * as of now, therefore maintaining a fixed gap between read and write offset when accessing the same partition.*/
            if(written_so_far <= (READ_WRITE_OFFSET_GAP + read_so_far))
            {
                DEBUG_LOG_VERBOSE("upgradePeer_DelayParallelReadRequest: Secondary trying to read within %d bytes of write offset", READ_WRITE_OFFSET_GAP);
                return TRUE;
            }
        }
        
    }
    return FALSE;
}

static void upgradePeer_SelfKickNextDataBlock(void)
{
    /* If upgrade or upgrade peer context is not available then sending next 
       block of data could lead to accessing null poiner. */
    if(!UpgradeCtxIsPartitionDataCtxValid() || !upgradePeerInfo->SmCtx)
    {
        /* At this point either handover or DFU abort has started anyway so,
            no need to do anything. */
        DEBUG_LOG_ERROR("upgradePeer_SelfKickNextDataBlock upgrade or upgrade peer context not available");
        return;
    }
    DEBUG_LOG_VERBOSE("upgradePeer_SelfKickNextDataBlock : total_size %u, total_sent %u", upgradePeerInfo->SmCtx->total_req_size, upgradePeerInfo->SmCtx->total_sent_size);
    if (upgradePeerInfo->SmCtx->total_sent_size < upgradePeerInfo->SmCtx->total_req_size)
    {
        /* Due to parallel read, check whether secondary is trying to read ahead
        * of primary for the same partition, if so delay the read*/
        if(upgradePeer_DelayParallelReadRequest())
        {
            DEBUG_LOG_VERBOSE("upgradePeer_SelfKickNextDataBlock needs to be delayed");
            MessageSendLater((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_DATA_CFM_MSG, NULL, INTERNAL_PEER_MSG_SHORT_DELAY);
        }
        else
        {
            DEBUG_LOG_VERBOSE("UpgradePeer_SelfKickNextDataBlock: req_start_offset %u",
                          upgradePeerInfo->SmCtx->req_start_offset);

            /*
             * As per the current optimized scheme, one UPGRADE_PEER_DATA_BYTES_REQ
             * is received per partition field. For the data field of the partition,
             * the whole size of the partition is sent in
             * UPGRADE_PEER_DATA_BYTES_REQ and sender (Primary) self kicks on each
             * MessageMoreSpace event on the Sink stream, to continue sending
             * UPGRADE_PEER_DATA_IND until the whole requested partition size is sent.
             * Hence its insignificant on internal kicks and hence reset to zero.
             */
            upgradePeerInfo->SmCtx->req_start_offset = 0;

            upgrade_peer_status_t error = upgradePeer_SendData();

            if (error != UPGRADE_PEER_SUCCESS)
            {
                DEBUG_LOG_ERROR("upgradePeer_SelfKickNextDataBlock: error enum:upgrade_peer_status_t:%d", error);

                upgradePeerInfo->SmCtx->upgrade_status = error;
                upgradePeer_SendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
                /* Cancel queued "INTERNAL_PEER_DATA_CFM_MSG" in case of error */
                MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_DATA_CFM_MSG);
            }
            else
            {
                DEBUG_LOG_VERBOSE("upgradePeer_SelfKickNextDataBlock: total_size %u, total_sent %u",
                                  upgradePeerInfo->SmCtx->total_req_size,
                                  upgradePeerInfo->SmCtx->total_sent_size);
            }
        }
    }
}



/**
 * To send an UPGRADE_IS_VALIDATION_DONE_REQ message.
 */
static void upgradePeer_SendValidationDoneReq (void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendValidationDoneReq");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_IS_VALIDATION_DONE_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}



/**
 * This method is called when we received an UPGRADE_IS_VALIDATION_DONE_CFM
 * message. We manage this packet and use it for the next step which is to send
 * an UPGRADE_IS_VALIDATION_DONE_REQ.
 */
static void upgradePeer_ReceiveValidationDoneCFM(UPGRADE_PEER_VERIFICATION_DONE_CFM_T *data)
{
    if ((data->common.length == UPGRADE_VAIDATION_DONE_CFM_DATA_LENGTH) &&
         data->delay_time > 0)
    {
        MessageSendLater((Task)&upgradePeerInfo->myTask,
                         INTERNAL_VALIDATION_DONE_MSG,
                         NULL, data->delay_time);
    }
    else
    {
        upgradePeer_SendValidationDoneReq();
    }
}

/**
 * This method is called when we received an UPGRADE_TRANSFER_COMPLETE_IND
 * message. We manage this packet and use it for the next step which is to send
 * a validation to continue the process or to abort it temporally.
 * It will be done later.
 */
static void upgradePeer_ReceiveTransferCompleteIND(void)
{
    DEBUG_LOG("UpgradePeer_ReceiveTransferCompleteIND");
    upgradePeer_SetResumePoint(UPGRADE_PEER_RESUME_POINT_PRE_REBOOT);
    /* Send TRANSFER_COMPLETE_IND to host to get confirmation */
    upgradePeer_AskForConfirmation(UPGRADE_TRANSFER_COMPLETE);
}

/**
 * This method is called when we received an UPGRADE_COMMIT_RES message.
 * We manage this packet and use it for the next step which is to send a
 * validation to continue the process or to abort it temporally.
 * It will be done later.
 */
static void upgradePeer_ReceiveCommitREQ(void)
{
    DEBUG_LOG("upgradePeer_ReceiveCommitREQ");
    upgradePeer_SetResumePoint(UPGRADE_PEER_RESUME_POINT_COMMIT);
    upgradePeer_AskForConfirmation(UPGRADE_COMMIT);
}

/**
 * This method is called when we received an UPGRADE_IN PROGRESS_IND message.
 */
static void upgradePeer_ReceiveProgressIND(void)
{
    DEBUG_LOG("upgradePeer_ReceiveProgressIND");
    upgradePeer_AskForConfirmation(UPGRADE_IN_PROGRESS);
}

/**
 * This method is called when we received an UPGRADE_ABORT_CFM message after
 * we asked for an abort to the upgrade process.
 */
static void upgradePeer_ReceiveAbortCFM(void)
{
    DEBUG_LOG("upgradePeer_ReceiveAbortCFM");
    UpgradeCtxSetWaitForPeerAbort(FALSE);
    upgradePeer_StopUpgrade();
}

/****************************************************************************
NAME
    SendPeerData

DESCRIPTION
    Send a data packet to a connected upgrade client.

*/
#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
static void upgradePeer_SendPeerData(uint8 *data, uint16 dataSize)
#else
static void upgradePeer_SendPeerData(uint8 *data, uint16 dataSize, bool isPreAllocated)
#endif
{
    UPGRADE_PEER_DATA_IND_T *dataInd = NULL;
#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    uint32 offsetof_idx;
#endif

    if(upgradePeerInfo->appTask != NULL)
    {

#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
        if (isPreAllocated)
        {
            offsetof_idx = offsetof(UPGRADE_PEER_DATA_IND_T, data);
            DEBUG_LOG_VERBOSE("upgradePeer_SendPeerData: data:%p offsetof_idx:%d", data, offsetof_idx);
            /*
             * When pre-allocated, data is embedded within dataInd in
             * sequential memory and hence recompute the base pointer for
             * dataInd, as returned by pmalloc heap when it was allocated.
             */
            dataInd = (UPGRADE_PEER_DATA_IND_T *)(data - offsetof_idx);
            DEBUG_LOG("SendPeerData: dataInd:%p size:%u", dataInd, dataSize);

            /*
             * If data is pre-allocated, then dataInd embeds data and shall be freed
             * as part of the schedule handling dataInd. So data too shouldn't
             * be freed.
             */
        }
        else
#endif
        {
            dataInd =(UPGRADE_PEER_DATA_IND_T *)PanicUnlessMalloc(
                                                sizeof(*dataInd) + dataSize - 1);

            DEBUG_LOG_VERBOSE("upgradePeer_SendPeerData: size %u", dataSize);

            ByteUtilsMemCpyToStream(dataInd->data, data, dataSize);

            free(data);
        }


        dataInd->size_data = dataSize;
        dataInd->is_data_state = TRUE;
        MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_DATA_IND, dataInd);
    }
    else
    {
#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
        if (isPreAllocated)
        {
            if (dataInd)
                free(dataInd);
        }
        else
#endif
        {
            if (data)
                free(data);
        }
    }
}

/**
 * To send an UPGRADE_START_DATA_REQ message.
 */
static void upgradePeer_SendStartDataReq(void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendStartDataReq");

    upgradePeer_SetResumePoint(UPGRADE_PEER_RESUME_POINT_START);

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_START_DATA_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

    /* Save the current state to pskeys. This will be used for resuming
     * DFU when upgrade peer is reset during primary to secondary data
     * transfer
     */
    upgradePeerInfo->UpgradePSKeys.currentState =
                                   upgradePeerInfo->SmCtx->peerState;
    UpgradePeerSavePSKeys();

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send an UPGRADE_TRANSFER_COMPLETE_RES packet.
 */
static void upgradePeer_SendTransferCompleteReq(upgrade_action_status_t status)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendTransferCompleteReq");

    payload = PanicUnlessMalloc(UPGRADE_TRANSFER_COMPLETE_RES_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_TRANSFER_COMPLETE_RES);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                   UPGRADE_TRANSFER_COMPLETE_RES_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, status);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send an UPGRADE_IN_PROGRESS_RES packet.
 */
static void upgradePeer_SendInProgressRes(upgrade_action_status_t status)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendInProgressRes");

    payload = PanicUnlessMalloc(UPGRADE_IN_PROGRESS_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_IN_PROGRESS_RES);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                   UPGRADE_IN_PROGRESS_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, status);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send an UPGRADE_COMMIT_CFM packet.
 */
static void upgradePeer_SendCommitCFM(upgrade_action_status_t status)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendCommitCFM");

    payload = PanicUnlessMalloc(UPGRADE_COMMIT_CFM_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_COMMIT_CFM);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                    UPGRADE_COMMIT_CFM_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, status);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send a message to abort the upgrade.
 */
static void upgradePeer_SendAbortReq(void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendAbortReq");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_ABORT_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif

    /*Cancel internal peer data_req message - needed in concurrent DFU cases when data request is on same partition
        and internal message is sent.Race condition can hapen with message in the queue and meanwhile abort happens.
        Also can happen during sco active scenarios as well*/
    MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_MSG);
}

/**
 * To send an UPGRADE_ERROR_WARN_RES packet.
 */
static void upgradePeer_SendErrorConfirmation(uint16 error)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("upgradePeer_SendErrorConfirmation");

    payload = PanicUnlessMalloc(UPGRADE_ERROR_IND_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                UPGRADE_PEER_ERROR_WARN_RES);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                   UPGRADE_ERROR_IND_DATA_LENGTH);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, error);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    upgradePeer_SendPeerData(payload, byteIndex);
#else
    upgradePeer_SendPeerData(payload, byteIndex, FALSE);
#endif
}

static void upgradePeer_HandlePeerAppMsg(uint8 *data)
{
    uint16 msgId;
    upgrade_peer_status_t error;
    UPGRADE_PEER_DATA_BYTES_REQ_T *msg;
    uint16 *scoFlag;
    msgId = ByteUtilsGet1ByteFromStream(data);
    UPGRADE_PEER_DATA_BYTES_REQ_T *local_msg;

    DEBUG_LOG_DEBUG("upgradePeer_HandlePeerAppMsg: MESSAGE:upgrade_peer_msg_t:0x%04X", msgId);

    switch(msgId)
    {
        case UPGRADE_PEER_SYNC_CFM:
            upgradePeer_ReceiveSyncCFM((UPGRADE_PEER_SYNC_CFM_T *)data);
            break;

        case UPGRADE_PEER_START_CFM:
            upgradePeer_ReceiveStartCFM((UPGRADE_PEER_START_CFM_T *)data);
            break;

        case UPGRADE_PEER_IS_VALIDATION_DONE_CFM:
            upgradePeer_ReceiveValidationDoneCFM(
                            (UPGRADE_PEER_VERIFICATION_DONE_CFM_T *)data);
            break;

        case UPGRADE_PEER_ABORT_CFM:
            upgradePeer_ReceiveAbortCFM();
            break;

        case UPGRADE_PEER_START_REQ:
            upgradePeer_SendStartReq();
            break;

        case UPGRADE_PEER_DATA_BYTES_REQ:
            /* If upgrade or upgrade peer context is not available then sending this 
               requested data could lead to accessing null poiner. */
            if(!UpgradeCtxIsPartitionDataCtxValid() || !upgradePeerInfo->SmCtx)
            {
                /* At this point either handover or DFU abort has started anyway so,
                    no need to do anything. */
                DEBUG_LOG_ERROR("upgradePeer_HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ, upgrade or upgrade peer context not available");
                return;
            }
#ifndef HOSTED_TEST_ENVIRONMENT
            DEBUG_LOG("upgradePeer_HandlePeerAppMsg: last closed partition %d, peer read partition %d",
                      UpgradeCtxGetPSKeysLastClosedPartition() - 1,
                      UpgradePeerPartitionDataCtxGet()->partNum);
            DEBUG_LOG("UpgradePeer_HandlePeerAppMsg: peer read partition %d",UpgradePeerPartitionDataCtxGet()->partNum);

            local_msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)data;
            DEBUG_LOG("UpgradePeer_HandlePeerAppMsg : UPGRADE_PEER_DATA_BYTES_REQ_T start_offset = %lu",(local_msg->start_offset));
            UpgradePeerPartitionDataCtxGet()->partition_offset = local_msg->start_offset;

            if(UpgradeIsDataTransferMode())
            {
                DEBUG_LOG("upgradePeer_HandlePeerAppMsg: concurrent DFU");
                if(upgradePeer_DelayParallelReadRequest())
                {
                    DEBUG_LOG_INFO("upgradePeer_HandlePeerAppMsg: delay UPGRADE_PEER_DATA_BYTES_REQ");
                    msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                    memmove(msg, data, sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                    MessageSendLater((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_MSG, msg,INTERNAL_PEER_MSG_DELAY);
                    return;
                }
            }
            scoFlag = UpgradeIsScoActive();
            if(*scoFlag)
            {
                DEBUG_LOG("upgradePeer_HandlePeerAppMsg: defer sending data as SCO is active");
                msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                memcpy(msg, data, sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                MessageSendConditionally((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_MSG, msg,UpgradeIsScoActive());
            }
            else
#endif
            {
                DEBUG_LOG("upgradePeer_HandlePeerAppMsg: process it");
                upgradePeer_ReceiveDataBytesREQ((UPGRADE_PEER_START_DATA_BYTES_REQ_T *)data);
            }
            break;

        case UPGRADE_PEER_COMMIT_REQ:
            UpgradePeerSetState(UPGRADE_PEER_STATE_COMMIT_CONFIRM);
            upgradePeer_ReceiveCommitREQ();
            break;

        case UPGRADE_PEER_TRANSFER_COMPLETE_IND:
            /* Check if the UpgradeSm state is UPGRADE_STATE_VALIDATED. If not,
             * then wait for 2 sec and send the TRANSFER_COMPLETE_IND to UpgradeSm
             * after 2 sec, until the state is UPGRADE_STATE_VALIDATED.
             * This will ensure that the TRANSFER_COMPLETE_IND message is handled
             * correctly in UpgradeSMHandleValidated().
             */
            if(!UpgradeSmStateIsValidated())
            {
                DEBUG_LOG("upgradePeer_HandlePeerAppMsg: not UPGRADE_STATE_VALIDATED");
                UPGRADE_PEER_TRANSFER_COMPLETE_IND_T* peer_msg = 
                    (UPGRADE_PEER_TRANSFER_COMPLETE_IND_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_TRANSFER_COMPLETE_IND_T));
                memmove(peer_msg, data, sizeof(UPGRADE_PEER_TRANSFER_COMPLETE_IND_T));
                MessageSendLater((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_MSG, peer_msg, INTERNAL_PEER_MSG_VALIDATION_SEND_DELAY);
            }
            else
            {
                UpgradePeerSetState(UPGRADE_PEER_STATE_VALIDATED);
                upgradePeer_ReceiveTransferCompleteIND();
            }
            break;

        case UPGRADE_PEER_COMPLETE_IND:
            /* Inform the upgrade library that peer upgrade is successful so, it can also do the commit. */
            UpgradeCompleteMsgFromUpgradePeer();
            /* Peer upgrade is finished let disconnect the peer connection */
            upgradePeer_StopUpgrade();
            break;

        case UPGRADE_PEER_ERROR_WARN_IND:
            /* send Error message to Host */
            error = ByteUtilsGet2BytesFromStream(data +
                                                UPGRADE_PEER_PACKET_HEADER);
            DEBUG_LOG("upgradePeer_HandlePeerAppMsg: error enum:upgrade_peer_status_t:%d", error);
            upgradePeer_SendErrorMsg(error);
            break;

        case UPGRADE_PEER_IN_PROGRESS_IND:
            upgradePeer_ReceiveProgressIND();
            break;

        case UPGRADE_PEER_SILENT_COMMIT_CFM:
            /* Silent commit request rx'ed */
            UpgradeSendReadyForSilentCommitInd();
            break;

        default:
            DEBUG_LOG("upgradePeer_HandlePeerAppMsg: unhandled");
            break;
    }
}

static void upgradePeer_HandleLocalMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_DEBUG("upgradePeer_HandleLocalMessage: MESSAGE:upgrade_peer_internal_msg_t:0x%04X", id);
    switch(id)
    {
        case INTERNAL_START_REQ_MSG:
            upgradePeer_SendStartReq();
            break;

        case INTERNAL_VALIDATION_DONE_MSG:
            upgradePeer_SendValidationDoneReq();
            break;

        case INTERNAL_PEER_MSG:
            upgradePeer_HandlePeerAppMsg((uint8 *)message);
            break;

        case INTERNAL_PEER_DATA_CFM_MSG:
            upgradePeer_SelfKickNextDataBlock();
            break;

        default:
            DEBUG_LOG("HandleLocalMessage: UpgradePeer: unhandled MESSAGE:upgrade_peer_internal_msg_t:0x%x", id);
            break;
    }
}

/****************************************************************************
NAME
    UpgradePeerApplicationReconnect

DESCRIPTION
    Check the upgrade status and decide if the application needs to consider
    restarting communication / UI so that it can connect to a host and start
    the commit process after defined reboot.

    If needed, builds and send an UPGRADE_PEER_RESTARTED_IND message and
    sends to the application task.

NOTE
    UpgradePeerApplicationReconnect() is called after the peer signalling
    is established, which ensures that the peers are connected  and the
    roles are set.
*/
void UpgradePeerApplicationReconnect(void)
{
    DEBUG_LOG_INFO("UpgradePeerApplicationReconnect: Resume point after reboot 0x%x",
               upgradePeerInfo->UpgradePSKeys.upgradeResumePoint);

    switch(upgradePeerInfo->UpgradePSKeys.upgradeResumePoint)
    {
        case UPGRADE_PEER_RESUME_POINT_POST_REBOOT:

            /* We are here because reboot happens during upgrade process,
             * first reinit UpgradePeer Sm Context.
             */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                UpgradePeerCtxInit();
                upgradePeerInfo->SmCtx->mResumePoint =
                    upgradePeerInfo->UpgradePSKeys.upgradeResumePoint;
                upgradePeerInfo->SmCtx->isUpgrading = TRUE;

                /* Primary device is rebooted, lets ask App to establish peer
                 * connection as well.
                 */
                UpgradePeerSetState(UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT);

                MessageSend(upgradePeerInfo->appTask,
                            UPGRADE_PEER_CONNECT_REQ, NULL);

            }
        break;

        default:
            DEBUG_LOG("UpgradePeerApplicationReconnect: unhandled msg");
    }
}

/**
 * This method starts the upgrade process. It checks that there isn't already a
 * upgrade going on. It resets the manager to start the upgrade and sends the
 * UPGRADE_SYNC_REQ to start the process.
 * This method can dispatch an error object if the manager has not been able to
 * start the upgrade process. The possible errors are the following:
 * UPGRADE_IS_ALREADY_PROCESSING
 */
static bool upgradePeer_StartUpgradePeerProcess(uint32 md5_checksum)
{
    UPGRADE_PEER_CTX_T *upgrade_peer = upgradePeer_CtxGet();
    DEBUG_LOG("upgradePeer_StartUpgradePeerProcess: md5_checksum 0x%08X", md5_checksum);

    if (!upgrade_peer->isUpgrading)
    {
        upgrade_peer->isUpgrading = TRUE;
        upgradePeer_CtxSet();
        upgradePeer_SendSyncReq(md5_checksum);
        upgrade_peer->upgrade_status = UPGRADE_PEER_SUCCESS;
    }
    else
    {
        upgrade_peer->upgrade_status = UPGRADE_PEER_ERROR_UPDATE_FAILED;
    }

    return (upgrade_peer->upgrade_status == UPGRADE_PEER_SUCCESS);
}

/****************************************************************************
NAME
    upgradePeer_CtxGet

RETURNS
    Context of upgrade library
*/
static UPGRADE_PEER_CTX_T *upgradePeer_CtxGet(void)
{
    if(upgradePeerInfo->SmCtx == NULL)
    {
        DEBUG_LOG("upgradePeer_CtxGet: you shouldn't have done that");
        Panic();
    }

    return upgradePeerInfo->SmCtx;
}

/*!
    @brief Clear upgrade related peer Pskey info.
    @param none
    
    Returns none
*/
void UpgradePeerClearPSKeys(void)
{
    if (upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerClearPSKeys: Can't be NULL\n");
        return;
    }

    /* Clear Pskey to prepare for subsequent upgrade from fresh */
    memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    UpgradePeerSavePSKeys();

}


/****************************************************************************
NAME
    upgradePeer_LoadPSStore  -  Load PSKEY on boot

DESCRIPTION
    Save the details of the PSKEY and offset that we were passed on
    initialisation, and retrieve the current values of the key.

    In the unlikely event of the storage not being found, we initialise
    our storage to 0x00 rather than panicking.
*/
static void upgradePeer_LoadPSStore(uint16 dataPskey,uint16 dataPskeyStart)
{
    union {
        uint16      keyCache[PSKEY_MAX_STORAGE_LENGTH];
        FSTAB_PEER_COPY  fstab;
        } stack_storage;
    uint16 lengthRead;

    upgradePeerInfo->upgrade_peer_pskey = dataPskey;
    upgradePeerInfo->upgrade_peer_pskeyoffset = dataPskeyStart;

    /* Worst case buffer is used, so confident we can read complete key
     * if it exists. If we limited to what it should be, then a longer key
     * would not be read due to insufficient buffer
     * Need to zero buffer used as the cache is on the stack.
     */
    memset(stack_storage.keyCache,0,sizeof(stack_storage.keyCache));
    lengthRead = PsRetrieve(dataPskey,stack_storage.keyCache,
                            PSKEY_MAX_STORAGE_LENGTH);
    if (lengthRead)
    {
        memcpy(&upgradePeerInfo->UpgradePSKeys,
               &stack_storage.keyCache[dataPskeyStart],
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    }
    else
    {
        memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    }
}

void UpgradePeerSavePSKeys(void)
{
    uint16 keyCache[PSKEY_MAX_STORAGE_LENGTH];
    uint16 min_key_length = upgradePeerInfo->upgrade_peer_pskeyoffset +
                            UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS;

    /* Find out how long the PSKEY is */
    uint16 actualLength = PsRetrieve(upgradePeerInfo->upgrade_peer_pskey,NULL,0);

    /* Clear the keyCache memory before it is used for reading and writing to
     * UPGRADE LIB PSKEY
     */
    memset(keyCache,0x0000,sizeof(keyCache));

    if (actualLength)
    {
        PsRetrieve(upgradePeerInfo->upgrade_peer_pskey,keyCache,actualLength);
    }
    else
    {
        if (upgradePeerInfo->upgrade_peer_pskeyoffset)
        {
            /* Initialise the portion of key before us */
            memset(keyCache,0x0000,sizeof(keyCache));
        }
        actualLength = min_key_length;
    }

    /* Correct for too short a key */
    if (actualLength < min_key_length)
    {
        actualLength = min_key_length;
    }

    memcpy(&keyCache[upgradePeerInfo->upgrade_peer_pskeyoffset],
           &upgradePeerInfo->UpgradePSKeys,
           UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    PsStore(upgradePeerInfo->upgrade_peer_pskey,keyCache,actualLength);
}

/****************************************************************************
NAME
    UpgradePeerProcessHostMsg

DESCRIPTION
    Process message received from Host/UpgradeSm.

*/
void UpgradePeerProcessHostMsg(upgrade_peer_msg_t msgid,
                                 upgrade_action_status_t status)
{
    UPGRADE_PEER_CTX_T *upgrade_peer = upgradePeerInfo->SmCtx;
    DEBUG_LOG("UpgradePeerProcessHostMsg:  MESSAGE:upgrade_peer_msg_t:0x%04X", msgid);

    if(upgrade_peer == NULL)
    {
        /* This may happen if abort is triggered and peers are not connected.
         * If this happens then initiate peer connection to send abort to peer.
         */
        DEBUG_LOG("UpgradePeerProcessHostMsg: Context is NULL");

        /* Wait for peer to connect before sending upgrade connect request. */
        MessageSendConditionally(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL, (uint16 *)&upgradePeerInfo->block_cond);
        upgradePeerInfo->is_dfu_abort_trigerred = TRUE;
        /* Needed for initiating the abort after the l2cap connection is established */
        UpgradePeerCtxInit();
        return;
    }

    switch(msgid)
    {
        case UPGRADE_PEER_SYNC_REQ:
            upgradePeer_SendSyncReq(upgradePeerInfo->md5_checksum);
            break;
        case UPGRADE_PEER_ERROR_WARN_RES:
            /*
             * For Handover, error indicate to the peer else the error code
             * should ideally be same as that received from the peer via
             * UPGRADE_HOST_ERRORWARN_RES. Here since its not known here, so
             * use error_in_error.
             */
            upgradePeer_HandleErrorWarnRes((uint16)(status == UPGRADE_HANDOVER_ERROR_IND ?
                                UPGRADE_PEER_ERROR_HANDOVER_DFU_ABORT :
                                UPGRADE_PEER_ERROR_IN_ERROR_STATE));
            break;

        case UPGRADE_PEER_TRANSFER_COMPLETE_RES:
            {
                /* We are going to reboot save current state */
                /* Save the current state and resume point in both the devices,
                 * which will be need for the commit phase, where any of the
                 * earbuds can become primary post reboot.
                 */
                upgradePeerInfo->UpgradePSKeys.upgradeResumePoint =
                                        UPGRADE_PEER_RESUME_POINT_POST_REBOOT;
                upgradePeerInfo->UpgradePSKeys.currentState =
                                               UPGRADE_PEER_STATE_VALIDATED;
                UpgradePeerSavePSKeys();

                /* Send the confirmation only if you are primary device */
                if(UPGRADE_PEER_IS_PRIMARY)
                {
                    upgradePeer_SendConfirmationToPeer(upgrade_peer->confirm_type, status);
                }
            }
            break;

        case UPGRADE_PEER_IN_PROGRESS_RES:
            upgrade_peer->confirm_type = UPGRADE_IN_PROGRESS;
            UpgradePeerSetState(UPGRADE_PEER_STATE_COMMIT_VERIFICATION);
            upgradePeer_SendConfirmationToPeer(upgrade_peer->confirm_type, status);
            break;

        case UPGRADE_PEER_COMMIT_CFM:
            upgrade_peer->confirm_type = UPGRADE_COMMIT;
            upgradePeer_SendConfirmationToPeer(upgrade_peer->confirm_type, status);
            break;
        case UPGRADE_PEER_ABORT_REQ:
            if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_ABORTING)
            {
                /* Peer Disconnection already occured. It's time to stop upgrade
                 */
                upgradePeer_StopUpgrade();
            }
            else
            {
                /* Abort the DFU */
                upgradePeer_AbortPeerDfu();
            }
            break;
        case UPGRADE_PEER_DATA_SEND_CFM:
            MessageSend((Task)&upgradePeerInfo->myTask,
                         INTERNAL_PEER_DATA_CFM_MSG, NULL);
            break;

        default:
            DEBUG_LOG("UpgradePeerProcessHostMsg: unhandled");
            break;
    }
}

/****************************************************************************
NAME
    UpgradePeerResumeUpgrade

DESCRIPTION
    resume the upgrade peer procedure.
    If an upgrade is processing, to resume it after a disconnection of the
    process, this method should be called to restart the existing running
    process.

*/
bool UpgradePeerResumeUpgrade(void)
{
    UPGRADE_PEER_CTX_T *upgrade_peer = upgradePeer_CtxGet();
    if (upgrade_peer->isUpgrading)
    {
        upgradePeer_CtxSet();
        upgradePeer_SendSyncReq(upgradePeerInfo->md5_checksum);
    }

    return upgrade_peer->isUpgrading;
}

bool UpgradePeerIsSupported(void)
{
    return (upgradePeerInfo != NULL);
}

bool UpgradePeerIsPrimary(void)
{
    return ((upgradePeerInfo != NULL) && upgradePeerInfo->is_primary_device);
}

bool UpgradePeerIsSecondary(void)
{
    return upgradePeerInfo && !upgradePeerInfo->is_primary_device;
}

void UpgradePeerCtxInit(void)
{
    if(upgradePeerInfo != NULL && !UPGRADE_PEER_IS_STARTED)
    {
        UPGRADE_PEER_CTX_T *peerCtx;

        DEBUG_LOG("UpgradePeerCtxInit: UpgradePeer");

        peerCtx = (UPGRADE_PEER_CTX_T *) PanicUnlessMalloc(sizeof(*peerCtx));
        memset(peerCtx, 0, sizeof(*peerCtx));

        upgradePeerInfo->SmCtx = peerCtx;

        upgradePeer_CtxSet();
        UpgradePeerParititonInit();
    }
}

/****************************************************************************
 NAME
    UpgradePeerInit

 DESCRIPTION
    Perform initialisation for the upgrade library. This consists of fixed
    initialisation as well as taking account of the information provided
    by the application.
*/

void UpgradePeerInit(Task appTask, uint16 dataPskey,uint16 dataPskeyStart)
{
    UPGRADE_PEER_INFO_T *peerInfo;

    DEBUG_LOG("UpgradePeerInit");

    peerInfo = (UPGRADE_PEER_INFO_T *) PanicUnlessMalloc(sizeof(*peerInfo));
    memset(peerInfo, 0, sizeof(*peerInfo));

    upgradePeerInfo = peerInfo;

    upgradePeerInfo->appTask = appTask;
    upgradePeerInfo->myTask.handler = upgradePeer_HandleLocalMessage;

    upgradePeerInfo->is_primary_device = TRUE;

    /* Peers are not connected at starting so, initialize block_cond accordingly. */
    upgradePeerInfo->block_cond = UPGRADE_PEER_BLOCK_UNTIL_PEER_SIG_CONNECTED;

    upgradePeer_LoadPSStore(dataPskey,dataPskeyStart);

    MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_INIT_CFM, NULL);
}

/****************************************************************************
NAME
    UpgradePeerSetState

DESCRIPTION
    Set current state.
*/
void UpgradePeerSetState(upgrade_peer_state_t nextState)
{
    upgradePeerInfo->SmCtx->peerState = nextState;
}

/****************************************************************************
NAME
    UpgradePeerIsRestarted

DESCRIPTION
    After upgrade device is rebooted, check that is done or not.
*/
bool UpgradePeerIsRestarted(void)
{
    return ((upgradePeerInfo->SmCtx != NULL) &&
             (upgradePeerInfo->SmCtx->mResumePoint ==
                UPGRADE_PEER_RESUME_POINT_POST_REBOOT));
}

/****************************************************************************
NAME
    UpgradePeerIsCommited

DESCRIPTION
    Upgrade peer device has sent commit request or not.
*/
bool UpgradePeerIsCommited(void)
{
    return ((upgradePeerInfo->SmCtx != NULL) &&
             (upgradePeerInfo->SmCtx->peerState ==
                UPGRADE_PEER_STATE_COMMIT_CONFIRM));
}

/****************************************************************************
NAME
    UpgradePeerIsCommitContinue

DESCRIPTION
    Upgrade peer should be ready for commit after reboot.
*/
bool UpgradePeerIsCommitContinue(void)
{
    return (upgradePeerInfo->SmCtx->peerState ==
            UPGRADE_PEER_STATE_COMMIT_HOST_CONTINUE);
}

/****************************************************************************
NAME
    UpgradePeerIsStarted

DESCRIPTION
    Peer device upgrade procedure is started or not.
*/
bool UpgradePeerIsStarted(void)
{
    return (upgradePeerInfo->SmCtx != NULL);
}

/****************************************************************************
NAME
    UpgradePeerDeInit

DESCRIPTION
    Perform uninitialisation for the upgrade library once upgrade is done
*/
void UpgradePeerDeInit(void)
{
    DEBUG_LOG("UpgradePeerDeInit");

    free(upgradePeerInfo->SmCtx);
    upgradePeerInfo->SmCtx = NULL;
}

void UpgradePeerStoreMd5(uint32 md5)
{
    if(upgradePeerInfo == NULL)
    {
        return;
    }

    upgradePeerInfo->md5_checksum = md5;
}

/****************************************************************************
NAME
    UpgradePeerStartDfu

DESCRIPTION
    Start peer device DFU procedure.

    If Upgrade Peer library has not been initialised do nothing.
*/
bool UpgradePeerStartDfu(upgrade_image_copy_status_check_t status)
{
    if(upgradePeerInfo == NULL || upgradePeerInfo->appTask == NULL)
    {
        return FALSE;
    }

    DEBUG_LOG("UpgradePeerStartDfu: DFU Started");

    /* Allocate UpgradePeer SM Context */
    UpgradePeerCtxInit();

    /* Peer DFU is starting,lets be in SYNC state */
    UpgradePeerSetState(UPGRADE_PEER_STATE_SYNC);

    /* Peer DFU is going to start, so it's not aborted yet*/
    upgradePeerInfo->is_dfu_aborted = FALSE;

    if(status == UPGRADE_IMAGE_COPY_CHECK_REQUIRED)
    {
        /* Peer DFU will start, once the image upgrade copy is completed */
        MessageSendConditionally(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL, (uint16 *)UpgradeCtxGetImageCopyStatus());
    }
    else if(status == UPGRADE_IMAGE_COPY_CHECK_IGNORE)
    {
        DEBUG_LOG("UpgradePeerStartDfu block_cond:%d", upgradePeerInfo->block_cond);
        MessageSendConditionally(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL, (uint16 *)&upgradePeerInfo->block_cond);
    }

    return TRUE;
}

void UpgradePeerCancelDFU(void)
{
    MessageCancelAll(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ);
}

void UpgradePeerResetStateInfo(void)
{
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerResetStateInfo upgradePeerInfo context is not yet created, return");
        return;
    }

    upgradePeerInfo->is_dfu_aborted = FALSE;
    upgradePeerInfo->is_primary_device = FALSE;
}

/****************************************************************************
NAME
    UpgradePeerResetCurState

BRIEF
    Reset the current state element of PeerPskeys and SmCtx to default value
    as a result of role switch during DFU.
*/

void UpgradePeerResetCurState(void)
{
    if(upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerResetCurState: Reset the current state");
        upgradePeerInfo->UpgradePSKeys.currentState = UPGRADE_PEER_STATE_CHECK_STATUS;
        UpgradePeerSavePSKeys();
        if(upgradePeerInfo->SmCtx != NULL)
        {
            upgradePeerInfo->SmCtx->peerState = UPGRADE_PEER_STATE_CHECK_STATUS;
        }
    }
}

/****************************************************************************
NAME
    UpgradePeerProcessDataRequest

DESCRIPTION
    Process a data packet from an Upgrade Peer client.

    If Upgrade Peer library has not been initialised do nothing.
*/
void UpgradePeerProcessDataRequest(upgrade_peer_app_msg_t id, uint8 *data,
                                   uint16 dataSize)
{
    uint8 *peer_data = NULL;
    upgrade_peer_connect_state_t state;

    if (upgradePeerInfo->SmCtx == NULL)
        return;

    DEBUG_LOG_VERBOSE("UpgradePeerProcessDataRequest, MESSAGE:upgrade_peer_app_msg_t:0x%x, size %u", 
                       id, dataSize);

    switch(id)
    {
        case UPGRADE_PEER_CONNECT_CFM:
            state = ByteUtilsGet1ByteFromStream(data);
            DEBUG_LOG("UpgradePeerProcessDataRequest: Connect CFM state %d", state);
            if(state == UPGRADE_PEER_CONNECT_SUCCESS)
            {
                switch(upgradePeerInfo->SmCtx->peerState)
                {
                    case UPGRADE_PEER_STATE_SYNC:
                        upgradePeer_StartUpgradePeerProcess(upgradePeerInfo->md5_checksum);
                    break;
                    case UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT:
                        UpgradePeerSetState(
                                   UPGRADE_PEER_STATE_COMMIT_HOST_CONTINUE);
                        UpgradeHandleMsg(NULL,
                                         HOST_MSG(UPGRADE_PEER_SYNC_AFTER_REBOOT_REQ),
                                         NULL);
                    break;
                    default:
                    {
                        /* We are here because an l2cap connection is created
                         * for sending abort to peer device.
                         */
                        if(upgradePeerInfo->is_dfu_abort_trigerred)
                        {
                             DEBUG_LOG("UpgradePeerProcessDataRequest: SendErrorConfirmation");
                             /* Send Error confirmation to Peer */
                             upgradePeer_SendErrorConfirmation(UPGRADE_PEER_ERROR_UPDATE_FAILED);
                             /* Since DFU abort is trigeered now, clear the flag */
                             upgradePeerInfo->is_dfu_abort_trigerred = FALSE;
                             /* L2cap connection is no longer needed, so disconnect */
                             MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_DISCONNECT_REQ, NULL);
                        }
                        else
                            DEBUG_LOG("UpgradePeerProcessDataRequest: unhandled msg");
                    }
                }
            }
            else
            {
                upgradePeerInfo->SmCtx->upgrade_status =
                                      UPGRADE_PEER_ERROR_APP_NOT_READY;
                upgradePeer_SendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
            }
            break;
        case UPGRADE_PEER_DISCONNECT_IND:
            if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_VALIDATED)
            {
                /* For now, do nothing, as we are supporting concurrent reboot */
                 DEBUG_LOG("UpgradePeerProcessDataRequest: UPGRADE_PEER_DISCONNECT_IND - VALIDATED state. Do nothing");
            }
            else if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_DATA_TRANSFER)
            {
                upgrade_peer_l2cap_status l2cap_disconnect_reason = 
                                upgradePeerInfo->SmCtx->l2cap_disconnect_reason;

                /* Peer DFU got interrupt due to Secondary reset. Start the DFU
                 * process again
                 */
                /* Cancel all the existing upgradePeerInfo messages */
                MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_MSG);

                /*
                 * Free the existing upgradePeerInfo->SmCtx and peerPartitionCtx
                 * Additionally also close, open partition (if any, probably
                 * possible when concurrent DFU is interrupted.
                 */
                UpgradePeerParititonDeInit();
                UpgradePeerDeInit();

                /*
                 * For disconnection due to linkloss, block peer DFU L2CAP
                 * channel setup until peer signaling channel is
                 * established which is a pre-condition to allow
                 * or DFU on a fresh start or resume post abrupt reset.
                 * Trigger the UpgradePeerStartDfu.
                 */
                if(l2cap_disconnect_reason == upgrade_peer_l2cap_link_loss)
                {
                    DEBUG_LOG("UpgradePeerProcessDataRequest: Peer disconnection due to linkloss");
                    UpgradePeerUpdateBlockCond(UPGRADE_PEER_BLOCK_UNTIL_PEER_SIG_CONNECTED);
                    /* Since we are resuming the peer DFU, image upgrade copy of 
                     * primary device is already completed, so ignore the check
                     */
                    UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_IGNORE);
                }
            }
            else if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_ABORTING)
            {
                /* Once the l2cap connection is disconnected, clear the 
                 * upgradePeerInfo flag as part of clearance process
                 */
                upgradePeer_CleanUpgradePeerCtx();
            }
            else 
            {
#if 1   /* ToDo: Once config controlled, replace with corresponding CC define */
                /*
                 * On abrupt reset of Secondary during Host->Primary
                 * data transfer results in linkloss to Secondary.
                 * Currently, stop the concurrent DFU and make provision for
                 * Primary to DFU in a serialized fashion.
                 * ToDo:
                 * 1) For DFU continue, this needs to be revisited.
                 * 2) Also reassess if fix for VMCSA-4340 can now be undone or
                 * still required for serialised DFU.
                 */
                MessageCancelAll((Task)&upgradePeerInfo->myTask,
                                    INTERNAL_PEER_MSG);
                UpgradePeerParititonDeInit();
                UpgradePeerDeInit();
                /*
                 * md5_checksum is retained so that same DFU file is
                 * relayed to the Secondary.
                 */
#else
                /* Peer device connection is lost and can't be recovered. Hence
                 * fail the upgrade process. Create the l2cap connection first,
                 * to send the DFU variables clearance to peer device.
                 */
                DEBUG_LOG("UpgradePeerProcessDataRequest: Create the l2cap connection to initiate DFU abort");
                /* Establish l2cap connection with peer to abort */
                MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL);
                upgradePeerInfo->is_dfu_abort_trigerred = TRUE;
#endif
            }
            break;
        case UPGRADE_PEER_GENERIC_MSG:
            peer_data = (uint8 *) PanicUnlessMalloc(dataSize);
            ByteUtilsMemCpyToStream(peer_data, data, dataSize);
            MessageSend((Task)&upgradePeerInfo->myTask,
                         INTERNAL_PEER_MSG, peer_data);
            break;

        case UPGRADE_PEER_DATA_SEND_CFM:
            MessageSend((Task)&upgradePeerInfo->myTask,
                         INTERNAL_PEER_DATA_CFM_MSG, NULL);
            break;

        default:
            DEBUG_LOG_ERROR("UpgradePeerProcessDataRequest, unhandled message MESSAGE:upgrade_peer_app_msg_t:0x%x", id);
    }
}

void UpgradePeerSetRole(bool role)
{
    if (upgradePeerInfo != NULL)
    {
        upgradePeerInfo->is_primary_device = role;
        DEBUG_LOG("UpgradePeerSetRole is_primary_device:%d", upgradePeerInfo->is_primary_device);
    }
}

void UpgradePeerUpdateBlockCond(upgrade_peer_block_cond_for_conn_t cond)
{
    if (upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerUpdateBlockCond block_cond:%d", cond);
        upgradePeerInfo->block_cond = cond;
    }
}

void UpgradePeerStoreDisconReason(upgrade_peer_l2cap_status reason)
{
    if (upgradePeerInfo != NULL && upgradePeerInfo->SmCtx != NULL)
    {
        DEBUG_LOG("UpgradePeerStoreDisconReason reason:%d", reason);
        upgradePeerInfo->SmCtx->l2cap_disconnect_reason = reason;
    }
}

bool UpgradePeerIsConnected(void)
{
    return (upgradePeerInfo && upgradePeerInfo->is_peer_connected);
}

void UpgradePeerSetConnectedStatus(bool val)
{
    if (upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerSetConnectedStatus is_peer_connected:%d", val);
        upgradePeerInfo->is_peer_connected = val;
    }
}

bool UpgradePeerIsBlocked(void)
{
    return (upgradePeerInfo && upgradePeerInfo->block_cond);
}

uint16 * UpgradePeerGetPeersConnectionStatus(void)
{
    return &(upgradePeerInfo->block_cond);
}

bool UpgradePeerIsLinkLoss(void)
{
    return (upgradePeerInfo && upgradePeerInfo->SmCtx
        && (upgradePeerInfo->SmCtx->l2cap_disconnect_reason  == upgrade_peer_l2cap_link_loss));
}

/****************************************************************************
NAME
    UpgradePeerGetFPtr

DESCRIPTION
    To get the Upgrade Peer librabry fptr to be set in UpgradeCtxGet()->funcs.
*/
const upgrade_response_functions_t *UpgradePeerGetFPtr(void)
{
    return &UpgradePeer_fptr;
}

