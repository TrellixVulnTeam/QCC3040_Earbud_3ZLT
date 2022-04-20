/****************************************************************************
Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_host_if_data.c

DESCRIPTION
    Implementation of the module which handles protocol message communications
    between host and device.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <panic.h>
#include <byte_utils.h>

#include "upgrade_ctx.h"
#include "upgrade_host_if.h"
#include "upgrade_host_if_data.h"
#include "upgrade_sm.h"

#define UPGRADE_MSG_HEADER_SIZE (sizeof(uint8) + sizeof(uint16))


void UpgradeHostIFDataSendShortMsg(uint16 message)
{
    UpgradeMsgHost msgId = (UpgradeMsgHost)message;
    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, msgId - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

void UpgradeHostIFDataSendSyncCfm(uint16 status, uint32 id)
{
    DEBUG_LOG_INFO("UpgradeHostIFDataSendSyncCfm, status %d, id 0x%lx, version %d", status, id, PROTOCOL_CURRENT_VERSION);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_SYNC_CFM_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_SYNC_CFM - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_SYNC_CFM_BYTE_SIZE);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, (uint8)status);
    byteIndex += ByteUtilsSet4Bytes(payload, byteIndex, id);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, PROTOCOL_CURRENT_VERSION);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

void UpgradeHostIFDataSendStartCfm(uint16 status, uint16 batteryLevel)
{
    DEBUG_LOG_INFO("UpgradeHostIFDataSendStartCfm, status %d, battery_level %u", status, batteryLevel);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_START_CFM_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_START_CFM - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_START_CFM_BYTE_SIZE);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, (uint8)status);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, batteryLevel);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

void UpgradeHostIFDataSendBytesReq(uint32 numBytes, uint32 startOffset)
{
    DEBUG_LOG_INFO("UpgradeHostIFDataSendBytesReq, num_bytes %ld, start_offset 0x%lx", numBytes, startOffset);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_DATA_BYTES_REQ_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_DATA_BYTES_REQ - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_DATA_BYTES_REQ_BYTE_SIZE);
    byteIndex += ByteUtilsSet4Bytes(payload, byteIndex, numBytes);
    byteIndex += ByteUtilsSet4Bytes(payload, byteIndex, startOffset);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

void UpgradeHostIFDataSendErrorInd(uint16 errorCode)
{
    DEBUG_LOG_ERROR("UpgradeHostIFDataSendErrorInd, error_code 0x%x", errorCode);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_ERRORWARN_IND_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_ERRORWARN_IND - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_ERRORWARN_IND_BYTE_SIZE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, errorCode);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

void UpgradeHostIFDataSendIsCsrValidDoneCfm(uint16 backOffTime)
{
    DEBUG_LOG_INFO("UpgradeHostIFDataSendIsCsrValidDoneCfm, backoff_time %u", backOffTime);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_IS_CSR_VALID_DONE_CFM_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_IS_CSR_VALID_DONE_CFM - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_IS_CSR_VALID_DONE_CFM_BYTE_SIZE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, backOffTime);

    UpgradeHostIFClientSendData(payload, byteIndex);
}


/*
NAME
    UpgradeHostIFDataSendVersionCfm - Send an UPGRADE_VERSION_CFM message to the host.

DESCRIPTION
    Build an UPGRADE_VERSION_CFM protocol message, containing the current major and
    minor upgrade version numbers, and the current PS config version, and send it to
    the host.
 */
void UpgradeHostIFDataSendVersionCfm(const uint16 major_ver, const uint16 minor_ver, const uint16 ps_config_ver)
{
    DEBUG_LOG_INFO("UpgradeHostIFDataSendVersionCfm, major %u, minor %u, ps %u", major_ver, minor_ver, ps_config_ver);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_VERSION_CFM_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_VERSION_CFM - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_VERSION_CFM_BYTE_SIZE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, major_ver);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, minor_ver);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, ps_config_ver);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

/*
NAME
    UpgradeHostIFDataSendVariantCfm - Sned an UPGRADE_VARIANT_CFM message to the host

DESCRIPTION
    Build an UPGRADE_VARIANT_CFM protocol message, containing the 8 byte device variant
    identifier, and send it to the host.
 */
void UpgradeHostIFDataSendVariantCfm(const char *variant)
{
    DEBUG_LOG_INFO("UpgradeHostIFDataSendVariantCfm, variant %s", variant);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_VARIANT_CFM_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_VARIANT_CFM - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_VARIANT_CFM_BYTE_SIZE);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[0]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[1]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[2]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[3]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[4]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[5]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[6]);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, variant[7]);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

void UpgradeHostIFDataSendSilentCommitSupportedCfm(uint8 is_silent_commit_supported)
{
    DEBUG_LOG_ERROR("UpgradeHostIFDataSendSilentCommitSupportedCfm, is_silent_commit_supported 0x%x", is_silent_commit_supported);

    uint8 *payload = PanicUnlessMalloc(UPGRADE_MSG_HEADER_SIZE + UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_CFM_BYTE_SIZE);
    uint16 byteIndex = 0;
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_CFM - UPGRADE_HOST_MSG_BASE);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_CFM_BYTE_SIZE);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, is_silent_commit_supported);

    UpgradeHostIFClientSendData(payload, byteIndex);
}

bool UpgradeHostIFDataBuildIncomingMsg(Task clientTask, const uint8 *data, uint16 dataSize)
{
    if ((dataSize < UPGRADE_MSG_HEADER_SIZE) || (data == NULL))
    {
        DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, size %d too small for header", dataSize);
        return FALSE;
    }

    uint16 msgId = ByteUtilsGet1ByteFromStream(data) + UPGRADE_HOST_MSG_BASE;
    uint16 msgLen = ByteUtilsGet2BytesFromStream(data + 1);

    /* Validate message has enough data as specified in message length field */
    if (dataSize >= (msgLen + UPGRADE_MSG_HEADER_SIZE))
        DEBUG_LOG_VERBOSE("UpgradeHostIFDataBuildIncomingMsg, size %d, msg_id 0x%02x", dataSize, msgId);
    else
    {
        DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, size %d too small, ", dataSize);
        return FALSE;
    }

    /* For convenience get pointer to message payload */
    const uint8 *msgPayload = data + UPGRADE_MSG_HEADER_SIZE;

    /* Extract message data and pass to state machine via message send or directly */
    switch (msgId)
    {
        case UPGRADE_HOST_SYNC_REQ:
        {
            if (msgLen == UPGRADE_HOST_SYNC_REQ_BYTE_SIZE)
            {
                MESSAGE_MAKE(msg, UPGRADE_HOST_SYNC_REQ_T);
                msg->inProgressId = ByteUtilsGet4BytesFromStream(msgPayload);
                MessageSend(clientTask, msgId, msg);
                DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_SYNC_REQ, in_progress_id %08lx", msg->inProgressId);
                DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_SYNC_REQ, Sending ops to DFU Domain");
#ifndef HOSTED_TEST_ENVIRONMENT
                UpgradeSendUpgradeOpsStatus(UpgradeGetAppTask(), upgrade_ops_handle_upgrade_partition_init, NO_ACTION);
#endif
            }
            else
                DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_SYNC_REQ, message size %d incorrect", msgLen);
        }
        break;

        case UPGRADE_HOST_START_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_START_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_START_DATA_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_START_DATA_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_ABORT_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_ABORT_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_PROGRESS_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_PROGRESS_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_IS_CSR_VALID_DONE_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_SYNC_AFTER_REBOOT_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_SYNC_AFTER_REBOOT_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_VERSION_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_VERSION_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_VARIANT_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_VARIANT_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_ERASE_SQIF_CFM:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_ERASE_SQIF_CFM");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_REQ:
            DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_REQ");
            MessageSend(clientTask, msgId, NULL);
            break;

        case UPGRADE_HOST_DATA:
        {
            if (msgLen >= UPGRADE_HOST_DATA_MIN_BYTE_SIZE)
            {
                MESSAGE_MAKE(msg, UPGRADE_HOST_DATA_T);
                msg->length = msgLen - 1;
                msg->lastPacket = ByteUtilsGet1ByteFromStream(msgPayload);
                msg->data = msgPayload + 1;
                DEBUG_LOG_VERBOSE("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_DATA, length %u, last_packet %u", msg->length, msg->lastPacket);
                MessageSend(clientTask, msgId, msg);
            }
            else
                DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_DATA, message size %d incorrect", msgLen);
        }
        break;

        case UPGRADE_HOST_TRANSFER_COMPLETE_RES:
        {
            if (msgLen == UPGRADE_HOST_TRANSFER_COMPLETE_RES_BYTE_SIZE)
            {
                MESSAGE_MAKE(msg, UPGRADE_HOST_TRANSFER_COMPLETE_RES_T);
                msg->action = ByteUtilsGet1ByteFromStream(msgPayload);
                DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_TRANSFER_COMPLETE_RES, action %u", msg->action);
                MessageSend(clientTask, msgId, msg);
            }
            else
                DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_TRANSFER_COMPLETE_RES, message size %d incorrect", msgLen);
        }
        break;

        case UPGRADE_HOST_IN_PROGRESS_RES:
        {
            if (msgLen == UPGRADE_HOST_IN_PROGRESS_RES_BYTE_SIZE)
            {
                MESSAGE_MAKE(msg, UPGRADE_HOST_IN_PROGRESS_RES_T);
                msg->action = ByteUtilsGet1ByteFromStream(msgPayload);
                DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_IN_PROGRESS_RES, action %u", msg->action);
                MessageSend(clientTask, msgId, msg);
            }
            else
                DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_IN_PROGRESS_RES, message size %d incorrect", msgLen);
        }
        break;

        case UPGRADE_HOST_COMMIT_CFM:
        {
            if (msgLen == UPGRADE_HOST_COMMIT_CFM_BYTE_SIZE)
            {
                MESSAGE_MAKE(msg, UPGRADE_HOST_COMMIT_CFM_T);
                msg->action = ByteUtilsGet1ByteFromStream(msgPayload);
                DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_COMMIT_CFM, action %u", msg->action);
                MessageSend(clientTask, msgId, msg);
            }
            else
                DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_COMMIT_CFM, message size %d incorrect", msgLen);
        }
        break;

        case UPGRADE_HOST_ERRORWARN_RES:
        {
            if (msgLen == UPGRADE_HOST_ERRORWARN_RES_BYTE_SIZE)
            {
                MESSAGE_MAKE(msg, UPGRADE_HOST_ERRORWARN_RES_T);
                msg->errorCode = ByteUtilsGet2BytesFromStream(msgPayload);
                DEBUG_LOG_INFO("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_ERRORWARN_RES, error %u", msg->errorCode);
                MessageSend(clientTask, msgId, msg);
            }
            else
                DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, UPGRADE_HOST_ERRORWARN_RES, message size %d incorrect", msgLen);
        }
        break;

        default:
            DEBUG_LOG_ERROR("UpgradeHostIFDataBuildIncomingMsg, unhandled message, id MESSAGE:0x%x", msgId);
            return FALSE;
    }

    return TRUE;
}
/*
NAME
    UpgradeHostIFProtocolCurrentVersion - Return the upgrade host protocol version

DESCRIPTION
    Return the upgrade host protocol current version to peer device
 */
uint8 UpgradeHostIFProtocolCurrentVersion(void)
{
    return PROTOCOL_CURRENT_VERSION;
}
