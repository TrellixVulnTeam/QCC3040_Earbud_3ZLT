/****************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_ctx.c
    
DESCRIPTION
    Global state of Upgrade library.
*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <panic.h>

#include "upgrade_ctx.h"
#include "upgrade_psstore.h"
#include "logging.h"

UpgradeCtx *upgradeCtx;

/****************************************************************************
NAME
    UpgradeCtxGet

DESCRIPTION
    Sets a new library context
*/

void UpgradeCtxSet(UpgradeCtx *ctx)
{
    DEBUG_LOG_VERBOSE("UpgradeCtx: size of UpgradeCtx is %d", sizeof(*ctx));
    DEBUG_LOG_VERBOSE("UpgradeCtx: size of UpgradePartitionDataCtx is %d", sizeof(UpgradePartitionDataCtx));
    if (ctx)
    {
        DEBUG_LOG_VERBOSE("- of which data buffer is %d", UPGRADE_PARTITION_DATA_BLOCK_SIZE(ctx));
    }
    else
    {
        DEBUG_LOG_VERBOSE("- with NULL ctx");
    }
    DEBUG_LOG_VERBOSE("size of UPGRADE_LIB_PSKEY is %d", sizeof(UPGRADE_LIB_PSKEY));
    upgradeCtx = ctx;
}

/****************************************************************************
NAME
    UpgradeCtxGet

RETURNS
    Context of upgrade library
*/
UpgradeCtx *UpgradeCtxGet(void)
{
    if(!upgradeCtx)
    {
        DEBUG_LOG_ERROR("UpgradeGetCtx: you shouldn't have done that");
        Panic();
    }

    return upgradeCtx;
}

/****************************************************************************
NAME
    UpgradeCtxSetPartitionData

DESCRIPTION
    Sets the partition data context in the upgrade context.
*/
void UpgradeCtxSetPartitionData(UpgradePartitionDataCtx *ctx)
{
    DEBUG_LOG_VERBOSE("size of UpgradePartitionDataCtx is %d of which data buffer is %d",
            sizeof(UpgradePartitionDataCtx), UPGRADE_PARTITION_DATA_BLOCK_SIZE(upgradeCtx));

    upgradeCtx->partitionData = ctx;
}

/****************************************************************************
NAME
    UpgradeCtxGetPartitionData

RETURNS
    Context of upgrade partition data.
    Note that it may be NULL if it has not been set yet.
*/
UpgradePartitionDataCtx *UpgradeCtxGetPartitionData(void)
{
    return upgradeCtx->partitionData;
}

/****************************************************************************
NAME
    UpgradeCtxSetWaitForPeerAbort

DESCRIPTION
    Set waitForPeerAbort variable in upgradeCtx
*/
void UpgradeCtxSetWaitForPeerAbort(bool set)
{
    UpgradeCtxGet()->waitForPeerAbort = set;
}

/****************************************************************************
NAME
    UpgradeCtxGetPartitionDataOffset

RETURNS
    Return the partitionData offset
*/
uint32 UpgradeCtxGetPartitionDataOffset(void)
{
    return UpgradeCtxGetPartitionData()->offset;
}

/****************************************************************************
NAME
    UpgradeCtxGetPartitionDatatotalReceivedSize

RETURNS
    Return the total received partition data
*/
uint32 UpgradeCtxGetPartitionDataTotalReceivedSize(void)
{
    return UpgradeCtxGetPartitionData()->totalReceivedSize;
}

/****************************************************************************
NAME
    UpgradeCtxGetFW

RETURNS
    Return the fwCtx of upgradeCtx
*/
UpgradeFWIFCtx *UpgradeCtxGetFW(void)
{
    return &upgradeCtx->fwCtx;
}

/****************************************************************************
NAME
    UpgradeCtxGetFWPartitionNum

RETURNS
    Return the partition number stored in fwCtx
*/
uint32 UpgradeCtxGetFWPartitionNum(void)
{
    return UpgradeCtxGetFW()->partitionNum;
}

/****************************************************************************
NAME
    UpgradeCtxIsPartitionDataStateFooter

RETURNS
    Return TRUE if the Partition Data State is footer
*/
bool UpgradeCtxIsPartitionDataStateFooter(void)
{
    return (UpgradeCtxGetPartitionData()->state == UPGRADE_PARTITION_DATA_STATE_FOOTER);
}

/****************************************************************************
NAME
    UpgradeCtxGetPartitionDataPartitionLength

RETURNS
    Return the length of partition data
*/
uint32 UpgradeCtxGetPartitionDataPartitionLength(void)
{
    return UpgradeCtxGetPartitionData()->partitionLength;
}

/****************************************************************************
NAME
    UpgradeCtxGetPSKeys

RETURNS
    Return the upgrade lib pskey
*/
UPGRADE_LIB_PSKEY *UpgradeCtxGetPSKeys(void)
{
    return &upgradeCtx->UpgradePSKeys;
}

/****************************************************************************
NAME
    UpgradeCtxGetPSKeysLastClosedPartition

RETURNS
    Return the last closed partition
*/
uint16 UpgradeCtxGetPSKeysLastClosedPartition(void)
{
    return UpgradeCtxGetPSKeys()->last_closed_partition;
}

/****************************************************************************
NAME
    UpgradeCtxGetPSKeysIdInProgress

RETURNS
    Return id_in_progress stored in upgarde lib pskey
*/
uint32 UpgradeCtxGetPSKeysIdInProgress(void)
{
    return UpgradeCtxGetPSKeys()->id_in_progress;
}

/****************************************************************************
NAME
    UpgradeIsInitialised

RETURNS
    Return TRUE if upgradeCtx is initialized, else FALSE
*/
bool UpgradeIsInitialised(void)
{
    return (upgradeCtx != NULL);
}

/****************************************************************************
NAME
    UpgradeCtxGetImageCopyStatus

RETURNS
    imageUpgradeCopyProgress
*/
uint16 *UpgradeCtxGetImageCopyStatus(void)
{
    return &(upgradeCtx->imageUpgradeCopyProgress);
}

/****************************************************************************
NAME
    UpgradeCtxSetImageCopyStatus

DESCRIPTION
    Sets the imageUpgradeCopyProgress in the upgrade context.
*/

void UpgradeCtxSetImageCopyStatus(image_upgrade_copy_status status)
{
    upgradeCtx->imageUpgradeCopyProgress = status;
}

/*!
    @brief Clear upgrade related local Pskey info maintained in context.
    @param none
    
    Returns none
*/
void UpgradeCtxClearPSKeys(void)
{
    if (upgradeCtx == NULL)
    {
        DEBUG_LOG("UpgradeCtxClearPSKeys: Can't be NULL\n");
        return;
    }

    /*
     * ToDo: Check if the memberwise reset be replaced by a memset except for
     * future_partition_state which is exclusively done wherever requried.
     */
    UpgradeCtxGetPSKeys()->upgrade_context = UPGRADE_CONTEXT_UNUSED;
    UpgradeCtxGetPSKeys()->state_of_partitions = UPGRADE_PARTITIONS_ERASED;
    UpgradeCtxGetPSKeys()->version_in_progress.major = 0;
    UpgradeCtxGetPSKeys()->version_in_progress.minor = 0;
    UpgradeCtxGetPSKeys()->config_version_in_progress = 0;
    UpgradeCtxGetPSKeys()->id_in_progress = 0;

    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_START;
    UpgradeCtxGetPSKeys()->last_closed_partition = 0;
    UpgradeCtxGetPSKeys()->dfu_partition_num = 0;
    UpgradeCtxGetPSKeys()->loader_msg = UPGRADE_LOADER_MSG_NONE;
    UpgradeCtxGetPSKeys()->is_silent_commit = 0;

    UpgradeSavePSKeys();
}

/****************************************************************************
NAME
    UpgradeGetMD5Checksum

RETURNS
    UpgradeCtxGetPSKeys()->id_in_progress
*/

uint32 UpgradeGetMD5Checksum(void)
{
    return UpgradeCtxGetPSKeys()->id_in_progress;
}

/****************************************************************************
NAME
    UpgradeCtxIsPartitionDataCtxValid

RETURNS
    Return TRUE if partition data ctx is valid, else FALSE
*/

bool UpgradeCtxIsPartitionDataCtxValid(void)
{
    return (UpgradeCtxGetPartitionData() != NULL);
}

/****************************************************************************
NAME
    UpgardeCtxDfuHeaderPskey

RETURNS
    UpgradeCtxGetPartitionData()->dfuHeaderPskey
*/

uint16 UpgardeCtxDfuHeaderPskey(void)
{
    return UpgradeCtxGetPartitionData()->dfuHeaderPskey;
}

/****************************************************************************
NAME
    UpgardeCtxDfuHeaderPskeyOffset

RETURNS
    UpgradeCtxGetPartitionData()->dfuHeaderPskeyOffset
*/

uint16 UpgardeCtxDfuHeaderPskeyOffset(void)
{
    return UpgradeCtxGetPartitionData()->dfuHeaderPskeyOffset;
}
