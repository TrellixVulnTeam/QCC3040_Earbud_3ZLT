/****************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_ctx.h

DESCRIPTION
    Header file for the Upgrade context.
*/

#ifndef UPGRADE_CTX_H_
#define UPGRADE_CTX_H_

#include <message.h>

#include "imageupgrade.h"

#include "upgrade_partition_data_priv.h"
#include "upgrade_fw_if_priv.h"
#include "upgrade_sm.h"
#include "upgrade_private.h"
#include "upgrade.h"
#include "upgrade_psstore_priv.h"

/* Enum to determine whether to start certain operation based on image upgrade
 * copy status
 */

typedef enum
{
    IMAGE_UPGRADE_COPY_COMPLETED,

    IMAGE_UPGRADE_COPY_IN_PROGRESS,

    IMAGE_UPGRADE_COPY_NOT_STARTED
}image_upgrade_copy_status;

/* Enum to determine whether to start certain operation based on image upgrade
 * hash status
 */
typedef enum
{
    IMAGE_UPGRADE_HASH_SUCCESS,

    IMAGE_UPGRADE_HASH_FAILED,

    IMAGE_UPGRADE_HASH_NOT_STARTED
}image_upgrade_hash_status;


/* Enum to determine if host has requested silent commit or interactive commit
 */
typedef enum
{
    UPGRADE_COMMIT_INTERACTIVE,
    UPGRADE_COMMIT_SILENT = 2
}upgrade_commit_mode;

typedef struct
{
    TaskData smTaskData;
    Task mainTask;
    Task clientTask;
    Task transportTask;
    /* An indication of whether the transport needs a UPGRADE_TRANSPORT_DATA_CFM
     * or not.
     */
    upgrade_data_cfm_type_t data_cfm_type;
    /* The maximum number of bytes (possibly in multiple packets) that the
     * transport can handle as a response to a single UPGRADE_DATA_BYTES_REQ.
     * This might be due to buffer size constraints, for example. 0 = no limit.
     */
    uint32 max_request_size;
    UpgradeState smState;
    UpgradePartitionDataCtx *partitionData;
    UpgradeFWIFCtx fwCtx;
    uint16 partitionValidationNextPartition;
    const UPGRADE_UPGRADABLE_PARTITION_T *upgradeLogicalPartitions;
    uint16 upgradeNumLogicalPartitions;
    /* Storage for PSKEY management.
     *
     * The library uses a (portion of) PSKEY supplied by the application
     * so needs to remember the details of the PSKEY.
     */
    uint16 upgrade_library_pskey;
    uint16 upgrade_library_pskeyoffset;
    uint32 partitionDataBlockSize;
    /*! Storage for upgrade library information maintained in PSKEYS.
     *
     * The values are only @b read from PSKEY on boot.
     * The local copy can be read/written at any time, but should be written
     * @b to storage as little as possible.
     *
     * Since this PSKEY storage structure has multiple uses be aware that
     * any value you write to the local copy may be committed at any time.
     *
     * The values will always be initialised, to 0x00 at worst.
     */
    UPGRADE_LIB_PSKEY UpgradePSKeys;

    /* Current level of permission granted the upgrade library by the VM
     * application */
    upgrade_permission_t perms;

    /*! The current power management mode (disabled/battery powered) */
    upgrade_power_management_t power_mode;

    /*! The current power management state informed by the VM app */
    upgrade_power_state_t power_state;

    /*! device variant */
    char dev_variant[UPGRADE_HOST_VARIANT_CFM_BYTE_SIZE+1];

    const upgrade_response_functions_t *funcs;

    /* P0 Hash context */
    hash_context_t vctx;

    /* CSR Valid message received flag */
    bool isCsrValidDoneReqReceived;

    /*! Force erase when a START_REQ is received */
    bool force_erase;

    /*! The flow control scheme using dfu_rx_flow_off and pendingDataReq
     *  is especially needed for DFU over LE where the upgrade data is
     *  packet oriented and the max MTU is 64.
     *  Being packet oriented and smaller MTU as compared to BR/EDR, leads to
     *  more short (complete or partial) messages queued for processing as the
     *  Source Buffer is drained.
     *  Since these messages are allocated from heap (pmalloc pool), its highly
     *  probable to run out of pmalloc pools. So a flow control scheme is
     *  required to keep the queued messages for processing within acceptable
     *  limits. In case of LE, the GATT Source stream size is 512 which can hold
     *  at max 8 upgrade data packets as max MTU is 64. So the acceptable
     *  queued message limit is 8.
     *  TODO: Upgrade library is commonly used both for Host<->Primary upgrade
     *        and also Primary<->Secondary upgrade.
     *        Primary<->Secondary upgrade is always using BR/EDR link and
     *        as such this flow control scheme is not required owing to larger
     *        MTU and upgrade data being non-packet oriented.
     *        Also Host<->Primary upgrade can be over BR/EDR link, in which case
     *        this flow control scheme is not required as reasoned above.
     *        But even if this flow control scheme is commonly applied for
     *        DFU over LE or BR/EDR, this won't radically impact the KPIs for
     *        DFU over BR/EDR.
     *        In future, if need arises to localize this flow control scheme to
     *        DFU over LE then logic needs to be added to distinguish
     *        Host<->Primary transport and whether Host<->Primary upgrade or
     *        Primary<->Secondary upgrade.
     */

    /*! Track whether processing of upgrade data from Source Buffer is flowed
     *  off or not.
     *  Used for DFU over LE flow control scheme.
     */
    bool dfu_rx_flow_off;

    /*! Outstanding upgrade data messages extracted from Source Buffer
     *  and queued to be processed (i.e. written to flash).
     *  Used for DFU over LE flow control scheme.
     */
    uint32 pendingDataReq;

     /*! Flag included to defer sending of data request
     *  during DFU if SCO is active.
     *  Used to pause DFU during incoming/outgoing active calls.
     *  Type: isScoActive is uint16 as used in MessageSendConditionally
     */
    uint16 isScoActive;

   /*! Status to indicate the progress of image upgrade copy
    */
   uint16 imageUpgradeCopyProgress;

    /*! ImageUpgradeCopy() is completed
     *  i.e. received MESSAGE_IMAGE_UPGRADE_COPY_STATUS.
     */
    bool isImgUpgradeCopyDone:1;
    /*! Copy status is significant only when isImgUpgradeCopyDone is set.
     *  If set then copy is successful else failed.
     */
    bool ImgUpgradeCopyStatus:1;

    /*! If user has sent the commit_cfm message with action 1(no) then this flag will be set
     *  If set then app should do the reboot after handling abort to revert the image
     */
    bool isImageRevertNeededOnAbort:1;

    /* Flag to indicate whether peer dfu is supported or not */
    bool isUpgradePeerDfuSupported:1;

    /*! ImageUpgradeErase() is completed
     *  i.e. received MESSAGE_IMAGE_UPGRADE_ERASE_STATUS.
     */
    uint16 isImgUpgradeEraseDone;

    upgrade_reconnect_recommendation_t reconnect_reason;

    /*! this flag will be set if we send abort req to the peer and reset when
     *  we get abort cfm from peer. primary device should wait for the peer
     *  abort cfm before sending it to host. */
    uint16 waitForPeerAbort;

     /*! Flag to indicate if silent commit is supported */
    uint8 isSilentCommitSupported;

    
    /*! Status to track the progress of image upgrade hashing
     */
    image_upgrade_hash_status imageUpgradeHashStatus;
    
    /*! Offset from the start of the DFU file calculated at the resume time
     */
    uint32 dfu_file_offset;

    /*! Variable to identify type of upgrade host
    */
    upgrade_context_t upg_host_type;
} UpgradeCtx;

void UpgradeCtxSet(UpgradeCtx *ctx);
UpgradeCtx *UpgradeCtxGet(void);

/*!
    @brief Set the partition data context in the upgrade context.
*/
void UpgradeCtxSetPartitionData(UpgradePartitionDataCtx *ctx);

/*! @brief Get the partition data context.

    @return Pointer to the partition data context. It may be
            NULL if it has not been set.
*/
UpgradePartitionDataCtx *UpgradeCtxGetPartitionData(void);

UpgradeFWIFCtx *UpgradeCtxGetFW(void);
UPGRADE_LIB_PSKEY *UpgradeCtxGetPSKeys(void);


void UpgradeCtxSetImageCopyStatus(image_upgrade_copy_status status);

/*!
    @brief Clear upgrade related local Pskey info maintained in context.
    @param none
    
    Returns none
*/
void UpgradeCtxClearPSKeys(void);
#endif /* UPGRADE_CTX_H_ */
