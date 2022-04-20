/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_peer_if_data.h
    
DESCRIPTION
    Interface to the module which handles protocol message communications
    between the peer devices.
    
    A set of functions for building and sending protocol message from peer 
    device to primary device.
    
    A generic function for handling incoming protocol messages from the peer,
    building an internal message, and passing it to the primary device.
    Currently that client is the upgrade state machine.
*/
#ifndef UPGRADE_PEER_IF_DATA_H_
#define UPGRADE_PEER_IF_DATA_H_

#include <message.h>
#include <upgrade_protocol.h>

/*!
    @brief Prepare UPGRADE_PEER_COMMON_CMD protocol message and send it to the peer.

    @param message enum variable used for finding command opcode using a base value.

    @return void
    */
void UpgradePeerIFDataSendShortMsg(uint16 message);

/*!
    @brief Prepare UPGRADE_PEER_SYNC_CFM protocol message and send it to the peer.

    @param status 2 byte value having current status information.
    @param id 4 byte value having upgrade file id.

    @return void
    */
void UpgradePeerIFDataSendSyncCfm(uint16 status, uint32 id);

/*!
    @brief Prepare UPGRADE_PEER_START_CFM protocol message and send it to the peer.

    @param status 2 byte value having current status information.
    @param batteryLevel current battery level information.

    @return void
    */
void UpgradePeerIFDataSendStartCfm(uint16 status, uint16 batteryLevel);

/*!
    @brief Prepare UPGRADE_PEER_START_DATA_BYTES_REQ protocol message and send it to the peer.

    @param numBytes number of data bytes requested.
    @param startOffset offset for figuring out starting point.

    @return void
    */
void UpgradePeerIFDataSendBytesReq(uint32 numBytes, uint32 startOffset);

/*!
    @brief Prepare UPGRADE_PEER_UPGRADE_ERROR_IND protocol message and send it to the peer.

    @param errorCode 2 byte Error code.

    @return void
    */
void UpgradePeerIFDataSendErrorInd(uint16 errorCode);

 /*!
    @brief Prepare UPGRADE_PEER_VERIFICATION_DONE_CFM protocol message and send it to the peer.

    @param backOffTime time delay.

    @return void
    */
void UpgradePeerIFDataSendIsCsrValidDoneCfm(uint16 backOffTime);

/*!
    @brief TODO
    @return void
 */
void UpgradePeerIFDataSendSilentCommitSupportedCfm(uint8 is_silent_commit_supported);

#endif /* UPGRADE_PEER_IF_DATA_H_ */
