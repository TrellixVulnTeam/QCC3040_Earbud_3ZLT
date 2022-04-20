/****************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_peer.h
    
DESCRIPTION
    Header file for the Upgrade Peer library.
*/
/*!
@file   upgrade_peer.h

*/
#ifndef UPGRADE_PEER_H_
#define UPGRADE_PEER_H_

#include <library.h>
#include <message.h>
#include <upgrade_protocol.h>
#include <upgrade.h>

#define UPGRADE_PEER_IS_SUPPORTED           UpgradePeerIsSupported()
#define UPGRADE_PEER_IS_PRIMARY             UpgradePeerIsPrimary()
#define UPGRADE_PEER_IS_COMMITED            UpgradePeerIsCommited()
#define UPGRADE_PEER_IS_RESTARTED           UpgradePeerIsRestarted()
#define UPGRADE_PEER_IS_STARTED             UpgradePeerIsStarted()
#define UPGRADE_PEER_IS_CONNECTED           UpgradePeerIsConnected()
#define UPGRADE_PEER_IS_COMMIT_CONTINUE     UpgradePeerIsCommitContinue()
#define UPGRADE_PEER_IS_ENDED               !UpgradePeerIsStarted()
#define UPGRADE_PEER_IS_BLOCKED             UpgradePeerIsBlocked()
#define UPGRADE_PEER_IS_LINK_LOSS           UpgradePeerIsLinkLoss()

/* Partition data is limited with UPGRADE_MAX_PARTITION_DATA_BLOCK_SIZE.
 * This gives low throughput. when peer upgrade is happening via ACL
 * connection we can increase this with 240 bytes.
 */
#define UPGRADE_PEER_MAX_DATA_BLOCK_SIZE 240

/*!
    @brief Timed wait before polling again for
           - Peer upgrade completion OR
           - Peer is connected for the post reboot DFU commit phase.
 */
#define UPGRADE_PEER_POLL_INTERVAL 300

/* The primary upgrade key size is 18 words. For peer upgrade 6 words are needed.
   So reusing UPGRADE_CONTEXT_KEY by giving offset of 26 to avoid overlap.
 */
#define UPGRADE_PEER_CONTEXT_OFFSET             26  /*!< Offset with that key to area for upgrade peer library */

/* Enum to indicate whether image copy check is required or not before starting
 * the peer dfu
 */
typedef enum
{
    UPGRADE_IMAGE_COPY_CHECK_IGNORE,

    UPGRADE_IMAGE_COPY_CHECK_REQUIRED
}upgrade_image_copy_status_check_t;
typedef enum
{
    UPGRADE_PEER_START_REQ                                        = 0x01,
    UPGRADE_PEER_START_CFM                                        = 0x02,
    UPGRADE_PEER_DATA_BYTES_REQ                                   = 0x03,
    UPGRADE_PEER_DATA                                             = 0x04,
    UPGRADE_PEER_SUSPEND_IND                                      = 0x05, /* Not Supported */
    UPGRADE_PEER_RESUME_IND                                       = 0x06, /* Not Supported */
    UPGRADE_PEER_ABORT_REQ                                        = 0x07,
    UPGRADE_PEER_ABORT_CFM                                        = 0x08,
    UPGRADE_PEER_PROGRESS_REQ                                     = 0x09, /* Not Supported */
    UPGRADE_PEER_PROGRESS_CFM                                     = 0x0A, /* Not Supported */
    UPGRADE_PEER_TRANSFER_COMPLETE_IND                            = 0x0B,
    UPGRADE_PEER_TRANSFER_COMPLETE_RES                            = 0x0C,
    UPGRADE_PEER_IN_PROGRESS_IND                                  = 0x0D, /* Not Supported */
    UPGRADE_PEER_IN_PROGRESS_RES                                  = 0x0E, /* Not Used */
    UPGRADE_PEER_COMMIT_REQ                                       = 0x0F,
    UPGRADE_PEER_COMMIT_CFM                                       = 0x10,
    UPGRADE_PEER_ERROR_WARN_IND                                   = 0x11,
    UPGRADE_PEER_COMPLETE_IND                                     = 0x12,
    UPGRADE_PEER_SYNC_REQ                                         = 0x13,
    UPGRADE_PEER_SYNC_CFM                                         = 0x14,
    UPGRADE_PEER_START_DATA_REQ                                   = 0x15,
    UPGRADE_PEER_IS_VALIDATION_DONE_REQ                           = 0x16,
    UPGRADE_PEER_IS_VALIDATION_DONE_CFM                           = 0x17,
    UPGRADE_PEER_SYNC_AFTER_REBOOT_REQ                            = 0x18, /* Not Supported */
    UPGRADE_PEER_VERSION_REQ                                      = 0x19,
    UPGRADE_PEER_VERSION_CFM                                      = 0x1A,
    UPGRADE_PEER_VARIANT_REQ                                      = 0x1B,
    UPGRADE_PEER_VARIANT_CFM                                      = 0x1C,
    UPGRADE_PEER_ERASE_SQIF_REQ                                   = 0x1D,
    UPGRADE_PEER_ERASE_SQIF_CFM                                   = 0x1E,
    UPGRADE_PEER_ERROR_WARN_RES                                   = 0x1F,
    UPGRADE_PEER_SILENT_COMMIT_SUPPORTED_REQ                      = 0x20,
    UPGRADE_PEER_SILENT_COMMIT_SUPPORTED_CFM                      = 0x21,
    UPGRADE_PEER_SILENT_COMMIT_CFM                                = 0x22
} upgrade_peer_msg_t;

/* Error codes based on Upgrade library */
typedef enum
{
    UPGRADE_PEER_SUCCESS = UPGRADE_HOST_SUCCESS,
    UPGRADE_PEER_OEM_VALIDATION_SUCCESS,

    UPGRADE_PEER_ERROR_INTERNAL_ERROR_DEPRECATED = UPGRADE_HOST_ERROR_INTERNAL_ERROR_DEPRECATED,
    UPGRADE_PEER_ERROR_UNKNOWN_ID,
    UPGRADE_PEER_ERROR_BAD_LENGTH_DEPRECATED,
    UPGRADE_PEER_ERROR_WRONG_VARIANT,
    UPGRADE_PEER_ERROR_WRONG_PARTITION_NUMBER,

    UPGRADE_PEER_ERROR_PARTITION_SIZE_MISMATCH,
    UPGRADE_PEER_ERROR_PARTITION_TYPE_NOT_FOUND_DEPRECATED,
    UPGRADE_PEER_ERROR_PARTITION_OPEN_FAILED,
    UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DEPRECATED,
    UPGRADE_PEER_ERROR_PARTITION_CLOSE_FAILED_DEPRECATED,

    UPGRADE_PEER_ERROR_SFS_VALIDATION_FAILED,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_DEPRECATED,
    UPGRADE_PEER_ERROR_UPDATE_FAILED,
    UPGRADE_PEER_ERROR_APP_NOT_READY,

    UPGRADE_PEER_ERROR_LOADER_ERROR,
    UPGRADE_PEER_ERROR_UNEXPECTED_LOADER_MSG,
    UPGRADE_PEER_ERROR_MISSING_LOADER_MSG,

    UPGRADE_PEER_ERROR_BATTERY_LOW,
    UPGRADE_PEER_ERROR_INVALID_SYNC_ID,
    UPGRADE_PEER_ERROR_IN_ERROR_STATE,
    UPGRADE_PEER_ERROR_NO_MEMORY,
    UPGRADE_PEER_ERROR_SQIF_ERASE,
    UPGRADE_PEER_ERROR_SQIF_COPY,
    UPGRADE_PEER_ERROR_AUDIO_SQIF_COPY,
    UPGRADE_PEER_ERROR_HANDOVER_DFU_ABORT,

    /* The remaining errors are grouped, each section starting at a fixed
     * offset */
    UPGRADE_PEER_ERROR_BAD_LENGTH_PARTITION_PARSE = UPGRADE_HOST_ERROR_BAD_LENGTH_PARTITION_PARSE,
    UPGRADE_PEER_ERROR_BAD_LENGTH_TOO_SHORT,
    UPGRADE_PEER_ERROR_BAD_LENGTH_UPGRADE_HEADER,
    UPGRADE_PEER_ERROR_BAD_LENGTH_PARTITION_HEADER,
    UPGRADE_PEER_ERROR_BAD_LENGTH_SIGNATURE,
    UPGRADE_PEER_ERROR_BAD_LENGTH_DATAHDR_RESUME,

    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_HEADERS = UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_HEADERS,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_UPGRADE_HEADER,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_PARTITION_HEADER1,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_PARTITION_HEADER2,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_PARTITION_DATA,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_FOOTER,
    UPGRADE_PEER_ERROR_OEM_VALIDATION_FAILED_MEMORY,

    UPGRADE_PEER_ERROR_PARTITION_CLOSE_FAILED = UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED,
    UPGRADE_PEER_ERROR_PARTITION_CLOSE_FAILED_HEADER,
    /*! When sent, the error indicates that an upgrade could not be completed 
     * due to concerns about space in Persistent Store.  No other upgrade
     * activities will be possible until the device restarts.
     * This error requires a UPGRADE_PEER_ERRORWARN_RES response (following
     * which the library will cause a restart, if the VM application permits)
     */
    UPGRADE_PEER_ERROR_PARTITION_CLOSE_FAILED_PS_SPACE,

    UPGRADE_PEER_ERROR_PARTITION_TYPE_NOT_MATCHING = UPGRADE_HOST_ERROR_PARTITION_TYPE_NOT_MATCHING,
    UPGRADE_PEER_ERROR_PARTITION_TYPE_TWO_DFU,

    UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_HEADER = UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_HEADER,
    UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA,

    UPGRADE_PEER_ERROR_FILE_TOO_SMALL = UPGRADE_HOST_ERROR_FILE_TOO_SMALL,
    UPGRADE_PEER_ERROR_FILE_TOO_BIG,

    UPGRADE_PEER_ERROR_INTERNAL_ERROR_1 = UPGRADE_HOST_ERROR_INTERNAL_ERROR_1,
    UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY,
    UPGRADE_PEER_ERROR_INTERNAL_ERROR_3,
    UPGRADE_PEER_ERROR_INTERNAL_ERROR_4,
    UPGRADE_PEER_ERROR_INTERNAL_ERROR_5,
    UPGRADE_PEER_ERROR_INTERNAL_ERROR_6,
    UPGRADE_PEER_ERROR_INTERNAL_ERROR_7,

    UPGRADE_PEER_ERROR_SILENT_COMMIT_NOT_SUPPORTED = UPGRADE_HOST_ERROR_SILENT_COMMIT_NOT_SUPPORTED,

    UPGRADE_PEER_WARN_APP_CONFIG_VERSION_INCOMPATIBLE = UPGRADE_HOST_WARN_APP_CONFIG_VERSION_INCOMPATIBLE,
    UPGRADE_PEER_WARN_SYNC_ID_IS_DIFFERENT
} upgrade_peer_status_t;

typedef enum
{
    /**
     * Used by the application to confirm that the upgrade should continue.
     */
    UPGRADE_CONTINUE,

    /**
     * Used by the application to confirm that the upgrade should abort.
     */
    UPGRADE_ABORT,

    /**
     * Used by the application to confirm that the upgrade should do silent commit.
     */
    UPGRADE_SILENT_COMMIT,

    /**
     * Used to error indicate to peer of a Handover so that peer can pause
     * upgrade to support for seamless DFU..
     */
    UPGRADE_HANDOVER_ERROR_IND,
}upgrade_action_status_t;

/*!
    @brief States which the upgrade peer library may be in.
*/
typedef enum
{
    /*! If L2CAP connection is success */
    UPGRADE_PEER_CONNECT_SUCCESS,

    /*! If L2CAP connection is failed */
    UPGRADE_PEER_CONNECT_FAILED
} upgrade_peer_connect_state_t;

/*!
    @brief Enumeration of message IDs sent by application to Upgrade Peer
           library.
*/
typedef enum
{
    /*! Message sent in reponse to
        UPGRADE_PEER_CONNECT_REQ/UPGRADE_PEER_RESTARTED_IND. */
    UPGRADE_PEER_CONNECT_CFM = UPGRADE_PEER_DOWNSTREAM_MESSAGE_BASE,
    /*! Message sent by APP when it noticed link conenction is lost. */
    UPGRADE_PEER_DISCONNECT_IND,
    /*! Message sent by APP when msg received from peer device.*/
    UPGRADE_PEER_GENERIC_MSG,
    /*! Message sent by APP when data is already sent to Bluetooth.*/
    UPGRADE_PEER_DATA_SEND_CFM
} upgrade_peer_app_msg_t;

/*!
    @brief Enumeration of message IDs used by Upgrade Peer library when sending
           messages to the application.
*/
typedef enum
{
    /*! Message sent in response to UpgradePeerInit(). */
    UPGRADE_PEER_INIT_CFM = UPGRADE_PEER_UPSTREAM_MESSAGE_BASE,
    /*! Message sent to application to inform of the current status
        of an upgrade. */
    UPGRADE_PEER_STATUS_IND,
    /*! Message sent to application to inform that the actual upgrade has
        started */
    UPGRADE_PEER_START_DATA_IND,
    /*! Message sent to application to inform that the actual upgrade has
        ended */
    UPGRADE_PEER_END_DATA_IND,
    /*! Message sent to app when upgrade peer link connection is required. */
    UPGRADE_PEER_CONNECT_REQ,
    /*! Message sent when DFU is aborted or done. */
    UPGRADE_PEER_DISCONNECT_REQ,
    /*! Message sent to a transport to send a data packet to a client. */
    UPGRADE_PEER_DATA_IND,
    /*! Message sent in response to UpgradeProcessDataRequest(). */
    UPGRADE_PEER_DATA_CFM,
    /*! Message sent to dfu_peer domain to inform that the peer data transfer
        has completed */
    UPGRADE_PEER_END_DATA_TRANSFER

} upgrade_peer_signal_msg_t;

/*!
 * Enum to indicate the blocking condition (if any) for peer DFU L2CAP
 * connection.
 */
typedef enum
{
    UPGRADE_PEER_BLOCK_NONE,
    UPGRADE_PEER_BLOCK_UNTIL_PEER_SIG_CONNECTED,
}upgrade_peer_block_cond_for_conn_t;

/*!
 * Enum to indicate the connection status of peer l2cap link.
 */
typedef enum
{
    /*! The L2CAP link is connected. */
    upgrade_peer_l2cap_connected,

    /*! The L2CAP link was disconnected due to link loss. */
    upgrade_peer_l2cap_link_loss
} upgrade_peer_l2cap_status;

/*!
    @brief Definition of the UPGRADE_PEER_CONNECT_CFM message sent for connect
    status.
*/
typedef struct
{
    /*! Status of the library initialisation request. */
    upgrade_peer_connect_state_t     connect_state;
} UPGRADE_PEER_CONNECT_STATUS_T;

/*!
    @brief Definition of the message sent to a application to send a data to a
    client.
*/
typedef struct
{
    /*! Checks if upgrade in data phase or not. */
    bool                 is_data_state;
    /*! Size of data packet to send. */
    uint16               size_data;
    /*! Pointer to data packet to send. */
    uint8                data[1];
} UPGRADE_PEER_DATA_IND_T;


/*----------------------------------------------------------------------------*
 * PURPOSE
 *      Command Packet Common Fields and Return packet
 *
 *---------------------------------------------------------------------------*/
typedef struct
{
    uint8    op_code;             /* op code of command */
    uint16   length;              /* parameter total length */
} UPGRADE_PEER_COMMON_CMD_T;


typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint32                      upgrade_file_id;
} UPGRADE_PEER_SYNC_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint8                       resume_point;
    uint32                      upgrade_file_id;
    uint8                       protocol_id;
} UPGRADE_PEER_SYNC_CFM_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_START_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint8                       status;
    uint16                      battery_level;
} UPGRADE_PEER_START_CFM_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_START_DATA_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint32                      data_bytes;
    uint32                      start_offset;
} UPGRADE_PEER_START_DATA_BYTES_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint8                       more_data;
    uint8                       image_data;
} UPGRADE_PEER_DATA_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint32                      data_bytes;
    uint32                      start_offset;
} UPGRADE_PEER_DATA_BYTES_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_VERIFICATION_DONE_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint16                      delay_time;
} UPGRADE_PEER_VERIFICATION_DONE_CFM_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_TRANSFER_COMPLETE_IND_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint8                       action;
} UPGRADE_PEER_TRANSFER_COMPLETE_RES_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint8                       action;
} UPGRADE_PEER_PROCEED_TO_COMMIT_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_COMMIT_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint8                       action;
} UPGRADE_PEER_COMMIT_CFM_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_UPGRADE_COMPLETE_IND_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint16                       error;
} UPGRADE_PEER_UPGRADE_ERROR_IND_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
    uint16                       error;
} UPGRADE_PEER_UPGRADE_ERROR_RES_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_ABORT_REQ_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T    common;
} UPGRADE_PEER_ABORT_CFM_T;

typedef struct {
    UPGRADE_PEER_COMMON_CMD_T   common;
    uint8                       is_silent_commit_supported;
} UPGRADE_PEER_SILENT_COMMIT_SUPPORTED_CFM_T;

/*!
    @brief Check if Upgrade peer is in commit confirm state.
    Returns TRUE if peer device has requested COMMIT_REQ.
*/
bool UpgradePeerIsCommited(void);

/*!
    @brief Check is UpgradePeer module is restarted after upgrade is done.
    Returns TRUE if UpgradePeer is in Post Reboot state.
*/
bool UpgradePeerIsRestarted(void);

/*!
    @brief Handle message received from UpgradeSm (i.e received from Host)

    @param msgid This indictaes message id.
    @param status Continue or Abort
    Returns None
*/
void UpgradePeerProcessHostMsg(upgrade_peer_msg_t msgid,
                               upgrade_action_status_t status);

/*!
    @brief Start peer device upgrade procedure.

    @param status To check whether to wait for image copy
            completion or start the DFU immediately
    Returns TRUE if peer device upgrade procedure is started.
*/
bool UpgradePeerStartDfu(upgrade_image_copy_status_check_t status);

/*!
    @brief Request to resume the peer device upgrade.
        
    Returns TRUE if peer device upgrade is resumed.
*/
bool UpgradePeerResumeUpgrade(void);

/*!
    @brief Check if reboot is done and peer connection is up for commit
           continue.
        
    Returns TRUE if peer device is ready for commit continue.
*/
bool UpgradePeerIsCommitContinue(void);

/****************************************************************************
NAME
    UpgradePeerResetCurState

BRIEF
    Reset the current state element of PeerPskeys and SmCtx to default value
    as a result of role switch during DFU.
*/

void UpgradePeerResetCurState(void);

/*!
    @brief Clear upgrade related peer Pskey info.

    @param none
    @return none
*/
void UpgradePeerClearPSKeys(void);

/****************************************************************************
NAME
    UpgradePeerCtxInit

DESCRIPTION
    Initialize the upgrade peer context

*/

void UpgradePeerCtxInit(void);

/****************************************************************************
NAME
    UpgradePeerGetFPtr

DESCRIPTION
    Returns the pointer to UpgradePeer_fptr structure variable
*/
const upgrade_response_functions_t *UpgradePeerGetFPtr(void);

/*!
    @brief Handle data request from APP.

    @param iD upgrade_peer_app_msg_t message id
    @param data Pointer to data memory which has been received from peer device.
    @param dataSize The size of the data.
*/
void UpgradePeerProcessDataRequest(upgrade_peer_app_msg_t id, uint8 *data,
                                   uint16 dataSize);

/*!
    @brief Initialise the Upgrade Peer library.

    Must be called before any other Upgrade Peer library API is used.
    Upgrade Peer library will respond with #UPGRADE_INIT_CFM_T message. with
    SUCESS .

    @param appTask The application task to receive Upgrade library messages.
    @param dataPskey  Pskey the Upgrade peer library may use to store data.
    @param dataPskeyStart The word offset within the PSKey from which the
           upgrade may store data.
    Returns None
*/
void UpgradePeerInit(Task appTask, uint16 dataPskey, uint16 dataPskeyStart);

/*!
    @brief UnInitialise the Upgrade Peer library.
    Returns None
*/
void UpgradePeerDeInit(void);

/*!
    @brief Check in Upgrade Peer is initialized or not.

    @return TRUE if Upgrade Peer is initialised
*/

bool UpgradePeerIsSupported(void);

/*!
    @brief Check if we are a primary device in an upgrade

    @return TRUE if Upgrade Peer is initialised and this is the primary
            device, FALSE otherwise
*/
bool UpgradePeerIsPrimary(void);

/*! @brief Check if we are a secondary device in an upgrade

    @return TRUE if Upgrade Peer is initialised and this is the secondary
            device, FALSE otherwise.
 */
bool UpgradePeerIsSecondary(void);

/*!
    @brief Store MD5 for DFU file for peer device.

    @param md5 md5 checksum of DFU file.
    Returns None
*/
void UpgradePeerStoreMd5(uint32 md5);

/*!
    @brief Return the abort status of Peer DFU

    Returns TRUE if peer DFU is aborted, else FALSE
*/
bool UpgradePeerIsPeerDFUAborted(void);

/*!
    @brief Reset the state information maintained by peer library.

    @note: Reset the state variables of peer maintained by library especially
           the role, abort state as these can be inappropriate if a handover
           has kicked in.
           This is needed because currently DFU data is not marshalled during
           an handover.
    @return None.
*/
void UpgradePeerResetStateInfo(void);

/*!
    @brief Cancel the Peer DFU Start if image upgrade copy is not successful

    Returns None
*/
void UpgradePeerCancelDFU(void);

/*!
    @brief UpgradePeerSetRole

    Update the current main role (Primary/Secondary) in upgrade library.

    @param role TRUE if Primary and FALSE if Secondary.
    @return none
*/
void UpgradePeerSetRole(bool role);

/*!
    @brief UpgradePeerUpdateBlockCond

    Update the blocking condition (if any) to setup peer DFU L2CAP channel.
    Based on this blocking condtion, the setup of the peer DFU L2CAP channel
    shall be blocked or unblocked.

    @param cond Enum indication blocking condition.
    @return none
*/
void UpgradePeerUpdateBlockCond(upgrade_peer_block_cond_for_conn_t cond);

/*!
    @brief UpgradePeerStoreDisconReason

    Update the disconnect reason (if any) for peer DFU L2CAP channel.
    Based on this disconnect reason, the setup of the peer DFU L2CAP channel
    shall be blocked.

    @param reason Enum indication disconnect reason.
    @return none
*/
void UpgradePeerStoreDisconReason(l2cap_disconnect_status reason);

/*!
    @brief Check is peer device upgrade is started.
        
    Returns TRUE if peer device upgrade is started.
*/
bool UpgradePeerIsStarted(void);

/*!
    @brief UpgradePeerIsConnected

    Peer DFU L2CAP channel is connected or not.

    @return TRUE if peer is connected else FALSE.
*/
bool UpgradePeerIsConnected(void);

/*!
    @brief UpgradePeerSetConnectedStatus

    Set Peer connection status for the DFU L2CAP channel.

    @param val Set TRUE when connected else FALSE.
    @return none.
*/
void UpgradePeerSetConnectedStatus(bool val);

/*!
    @brief UpgradePeerCtxInit

    Create Peer context to be used for peer DFU L2CAP channel establishment.

    @param None
    @return none
*/
void UpgradePeerCtxInit(void);

/*!
    @brief UpgradePeerSavePSKeys

    Writes the peer copy of PSKEY information to the PS Store. 
    Note that this API will not panic, even if no PS storage has been
    made available.

    @param None
    @return none
*/

void UpgradePeerSavePSKeys(void);

/*!
    @brief UpgradePeerApplicationReconnect

    Check the upgrade status and decide if the application needs to consider
    restarting communication / UI so that it can connect to a host and start
    the commit process after defined reboot.

    @param None
    @return none
*/

void UpgradePeerApplicationReconnect(void);

/*!
    @brief check the link loss between peers
    returns TRUE if link loss else FALSE
*/
bool UpgradePeerIsBlocked(void);

/*!
    @brief UpgradePeerGetPeersConnectionStatus
    return 1 for link loss between peers else 0
*/
uint16 * UpgradePeerGetPeersConnectionStatus(void);

/*!
    @brief check the reason for l2cap disconnection
    returns TRUE if reason is link loss else FALSE
*/
bool UpgradePeerIsLinkLoss(void);

#endif /* UPGRADE_PEER_H_ */
