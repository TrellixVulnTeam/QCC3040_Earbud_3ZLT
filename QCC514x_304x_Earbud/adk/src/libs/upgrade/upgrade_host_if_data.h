/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_host_if_data.h
    
DESCRIPTION
    Interface to the module which handles protocol message communications
    with the host.
    
    A set of functions for building and sending protocol message from the
    device to the host.
    
    A generic function for handling incoming protocol messages from the host,
    building an internal message, and passing it to the host interface client.
    Currently that client is the upgrade state machine.
*/
#ifndef UPGRADE_HOST_IF_DATA_H_
#define UPGRADE_HOST_IF_DATA_H_

#include <message.h>
#include <upgrade_protocol.h>

/*!
    Definition of version number.

    Sent by the device using SYNC_CFM command.
 */
typedef enum
{
    PROTOCOL_VERSION_1 = 1,
    PROTOCOL_VERSION_2,
    PROTOCOL_VERSION_3,
    PROTOCOL_VERSION_4
    
} ProtocolVersion;


/*!
    Definition the current protocol version in use.
 */
#define PROTOCOL_CURRENT_VERSION    PROTOCOL_VERSION_4

/*!
    @brief Build an packet and send it to the host.

    @param message enum variable used for finding command opcode using a base value.

    @return void
    */
void UpgradeHostIFDataSendShortMsg(uint16 message);

/*!
    @brief Prepare UPGRADE_HOST_SYNC_CFM protocol message and send it to the host.

    @param status 2 byte value having current status information.
    @param id 4 byte value having upgrade file id.

    @return void
    */
void UpgradeHostIFDataSendSyncCfm(uint16 status, uint32 id);

/*!
    @brief Prepare UPGRADE_HOST_START_CFM protocol message and send it to the host.

    @param status2 byte value having current status information.
    @param batteryLevel current battery level information.

    @return void
    */
void UpgradeHostIFDataSendStartCfm(uint16 status, uint16 batteryLevel);

/*!
    @brief Prepare UPGRADE_HOST_DATA_BYTES_REQ protocol message and send it to the host.

    @param numBytes number of data bytes requested
    @param startOffset offset for fisguring out starting point.

    @return void
    */
void UpgradeHostIFDataSendBytesReq(uint32 numBytes, uint32 startOffset);

/*!
    @brief Prepare UPGRADE_HOST_ERRORWARN_IND protocol message and send it to the host.

    @param errorCode 2 byte error code.

    @return void
    */
void UpgradeHostIFDataSendErrorInd(uint16 errorCode);

/*!
    @brief Prepare UPGRADE_HOST_IS_CSR_VALID_DONE protocol message and send it to the host.

    @param backOffTime delay time.

    @return void
    */
void UpgradeHostIFDataSendIsCsrValidDoneCfm(uint16 backOffTime);

/*!
    @brief Build and send an UPGRADE_VERSION_CFM message to the host.

    @param major_ver Current Upgrade Major version number.
    @param minor_ver Current Upgrade Minor version number.
    @param ps_config_ver Current PS Configuration version number.

    @return void
 */
void UpgradeHostIFDataSendVersionCfm(const uint16 major_ver,
                                     const uint16 minor_ver,
                                     const uint16 ps_config_ver);

/*!
    @brief Build and send an UPGRADE_VARIANT_CFM message to the host.

    @param variant 8 character string with device variant details. 

    @return void
 */
void UpgradeHostIFDataSendVariantCfm(const char *variant);

/*!
    @brief Build and send an UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_CFM message to the host.

    @param is_silent_commit_supported Information if silent commit is supported. 

    @return void
 */
void UpgradeHostIFDataSendSilentCommitSupportedCfm(uint8 is_silent_commit_supported);

/*!
    @brief Build and send packet on reading data from byte stream.

    @param clientTask task type to be performed.
    @param data To point message payload from byte stream.
    @param dataSize 2 byte variable to store header size.
    
    @return TRUE if packet is prepared and sent otherwise FALSE.
 */
bool UpgradeHostIFDataBuildIncomingMsg(Task clientTask, const uint8 *data, uint16 dataSize);

#endif /* UPGRADE_HOST_IF_DATA_H_ */
