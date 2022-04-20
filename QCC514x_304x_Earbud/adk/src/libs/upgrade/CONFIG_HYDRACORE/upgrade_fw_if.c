/****************************************************************************
Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_fw_if.c

DESCRIPTION
    Implementation of functions which (largely) interact with the firmware.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <string.h>
#include <byte_utils.h>
#include <csrtypes.h>
#include <panic.h>
#include <partition.h>
#include <sink.h>
#include <stream.h>
#include <imageupgrade.h>

#include "upgrade_partitions.h"
#include "upgrade_ctx.h"

#include "upgrade_psstore.h"
#include "upgrade_partitions.h"
#include "upgrade_private.h"
#include <upgrade_protocol.h>

#include "upgrade_fw_if.h"
#include "upgrade_fw_if_priv.h"

/******************************************************************************
NAME
    UpgradeFWIFInit

DESCRIPTION
    Initialise the context for the Upgrade FW IF.

*/
void UpgradeFWIFInit(void)
{
    UpgradeFWIFCtx *fwctx = UpgradeCtxGetFW();

    fwctx->vctx = NULL;
    /* We need to maintain the value of partitionNum because it won't get set again
     * during GAA resume. */
    if(Upgrade_GetHostType() != UPGRADE_CONTEXT_GAA_OTA)
    {
        DEBUG_LOG("UpgradeFWIFInit reset fwctx->partitionNum");
        fwctx->partitionNum = 0;
    }
}

/******************************************************************************
NAME
    UpgradeFWIFGetPartitionID

DESCRIPTION
    Get the identifier for a partition header within an upgrade file.

RETURNS
    const char * Pointer to the partition header string.
*/
const char *UpgradeFWIFGetPartitionID(void)
{
    return "PARTDATA";
}

/******************************************************************************
NAME
    UpgradeFWIFGetFooterID

DESCRIPTION
    Get the identifier for the footer of an upgrade file.

RETURNS
    const char * Pointer to the footer string.
*/
const char *UpgradeFWIFGetFooterID(void)
{
    return "APPUPFTR";
}

/******************************************************************************
NAME
    UpgradeFWIFGetDeviceVariant

DESCRIPTION
    Get the identifier for the current device variant.

RETURNS
    const char * Pointer to the device variant string.
*/
const char *UpgradeFWIFGetDeviceVariant(void)
{
    return ( const char * )UpgradeCtxGet()->dev_variant;
}

/******************************************************************************
NAME
    UpgradeFWIFGetAppVersion

DESCRIPTION
    Get the current (running) app version.

RETURNS
    uint16 The running app version.
*/
uint16 UpgradeFWIFGetAppVersion(void)
{
    return 2;
}

#ifndef UPGRADE_USE_FW_STUBS

/******************************************************************************
NAME
    UpgradeFWIFGetPhysPartition

DESCRIPTION
    Given a logical partition number return the physical partition number into 
    which we can write data. This includes the SQUIF selector, use 
    UPGRADE_PARTITION_PHYSICAL_PARTITION to extract partition ID.

RETURNS
    uint16 Number of the physical partition available for write.
*/
uint16 UpgradeFWIFGetPhysPartition(uint16 logicPartition)
{
    return UpgradePartitionsPhysicalPartition(logicPartition,UpgradePartitionUpgradable);
}

/******************************************************************************
NAME
    UpgradeFWIFGetPhysPartitionNum

DESCRIPTION
    Work out how many partitions there are in the serial flash.

RETURNS
    uint16 Number of partitions in the serial flash.
*/
uint16 UpgradeFWIFGetPhysPartitionNum(void)
{
    return IMAGE_SECTION_APPS_RW_CONFIG;
}


/******************************************************************************
NAME
    UpgradeFWIFGetPhysPartitionSize

DESCRIPTION
    Find out the size of a specified partition in bytes.

RETURNS
    uint32 Size of physPartition in bytes.
*/
uint32 UpgradeFWIFGetPhysPartitionSize(uint16 physPartition)
{   /** 
     * When audio is supported, we can determine the QSPI to use from the partition.
     * Until then only QSPI zero is used.
     */
    #define QSPI_NUM 0
    uint32 size;
    if(ImageUpgradeGetInfo(QSPI_NUM, physPartition, IMAGE_SIZE, &size))
    {
        return (2 * size);
    }
    return 0;
}

/******************************************************************************
NAME
    UpgradeFWIFValidPartitionType

DESCRIPTION
    Determine if the partition type (in the flash) is a valid type that the
    upgrade library is expecting (and knows how to handle).

RETURNS
    bool TRUE if type is valid, FALSE if type is invalid
*/
bool UpgradeFWIFValidPartitionType(UpgradeFWIFPartitionType type, uint16 physPartition)
{
    static const partition_type valid_types[UPGRADE_FW_IF_PARTITION_TYPE_NUM] =
    {
        PARTITION_TYPE_FILESYSTEM, /* UPGRADE_FW_IF_PARTITION_TYPE_EXE */
        PARTITION_TYPE_RAW_SERIAL, /* UPGRADE_FW_IF_PARTITION_TYPE_DFU */
        PARTITION_TYPE_FILESYSTEM, /* UPGRADE_FW_IF_PARTITION_TYPE_CONFIG */
        PARTITION_TYPE_FILESYSTEM, /* UPGRADE_FW_IF_PARTITION_TYPE_DATA */
        PARTITION_TYPE_RAW_SERIAL  /* UPGRADE_FW_IF_PARTITION_TYPE_DATA_RAW_SERIAL */
    };

    if ((type < UPGRADE_FW_IF_PARTITION_TYPE_EXE) ||
        (type >= UPGRADE_FW_IF_PARTITION_TYPE_NUM))
    {
        DEBUG_LOG_ERROR("UpgradeFWIFValidPartitionType: unknown header type %u", type);
        return FALSE;
    }

    /* Add check that header partition type is compatible with type in partition info */
    if (physPartition > IMAGE_SECTION_APPS_RW_CONFIG)
    {
        DEBUG_LOG_ERROR("UpgradeFWIFValidPartitionType: no info for partition %u", physPartition);
        return FALSE;
    }

    if (valid_types[type] != (partition_type)physPartition)
    {
        DEBUG_LOG_ERROR("UpgradeFWIFValidPartitionType: partition type [%u] %u does not match actual type %u",
            type, valid_types[type], physPartition);

        return FALSE;
    }

    return TRUE;
}

/***************************************************************************
NAME
    UpgradeFWIFPartitionOpen

DESCRIPTION
    Open a write only handle to a physical partition on the external flash.
    For initial testing, the CRC check on the partition is also disabled.

PARAMS
    logic Logic
    physPartition Physical partition number in external flash.
    firstWord First word of partition data.

RETURNS
    UpgradeFWIFPartitionHdl A valid handle or NULL if the open failed.
*/
UpgradeFWIFPartitionHdl UpgradeFWIFPartitionOpen(uint16 logic,uint16 physPartition, uint16 firstWord)
{   /** 
     * When audio is supported, we can determine the QSPI to use from the partition.
     * Until then only QSPI zero is used.
     */
    #define QSPI_NUM 0

    Sink sink;

    DEBUG_LOG_DEBUG("UpgradeFWIFPartitionOpen: opening partition %u for resume", physPartition);

    sink = ImageUpgradeStreamGetSink(QSPI_NUM, physPartition, firstWord);
    if (!sink)
    {
        DEBUG_LOG_ERROR("UpgradeFWIFPartitionOpen: failed to open raw partition %u for resume", physPartition);
        return (UpgradeFWIFPartitionHdl)(int)NULL;
    }
    SinkConfigure(sink, VM_SINK_MESSAGES, VM_MESSAGES_NONE);

    UpgradePartitionsMarkUpgrading(logic);

    UpgradeCtxGetFW()->partitionNum = physPartition;

    return (UpgradeFWIFPartitionHdl)(int)sink;
}

/***************************************************************************
NAME
    UpgradeFWIFPartitionClose

DESCRIPTION
    Close a handle to an external flash partition.

PARAMS
    handle Handle to a writeable partition.

RETURNS
    bool TRUE if a valid handle is given, FALSE otherwise.
*/
UpgradeHostErrorCode UpgradeFWIFPartitionClose(UpgradeFWIFPartitionHdl handle)
{

    Sink sink = (Sink)(int)handle;

    DEBUG_LOG_DEBUG("UpgradeFWIFPartitionClose");

    if (!sink)
    {
        return UPGRADE_HOST_SUCCESS;
    }

    if (!UpgradePSSpaceForCriticalOperations())
    {
        return UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED_PS_SPACE;
    }

    if (!SinkClose(sink))
    {
        DEBUG_LOG_ERROR("UpgradeFWIFPartitionClose: unable to close sink");
        return UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED;
    }
    /* last_closed_partition == partition_num + 1
     * so value 0 means no partitions were closed
     */
    UpgradeCtxGetPSKeys()->last_closed_partition = UpgradeCtxGetFW()->partitionNum + 1;
    UpgradeSavePSKeys();
    DEBUG_LOG("UpgradeFWIFPartitionClose: last_closed_partition is %d", UpgradeCtxGetPSKeys()->last_closed_partition);

    return UPGRADE_HOST_SUCCESS;
}

#else /* UPGRADE_USE_FW_STUBS */

uint16 UpgradeFWIFGetPhysPartition(uint16 logicPartition)
{
    return logicPartition;
}

uint16 UpgradeFWIFGetPhysPartitionNum(void)
{
    return 8;
}

uint32 UpgradeFWIFGetPhysPartitionSize(uint16 physPartition)
{
    UNUSED(physPartition);
    return 4000000;
}

bool UpgradeFWIFValidPartitionType(UpgradeFWIFPartitionType type, uint16 physPartition)
{
    UNUSED(type);
    UNUSED(physPartition);
    return TRUE;
}

UpgradeFWIFPartitionHdl UpgradeFWIFPartitionOpen(uint16 logic,uint16 physPartition, uint16 firstWord)
{
    UNUSED(logic);
    UNUSED(firstWord);
    return (UpgradeFWIFPartitionHdl)(unsigned)physPartition;
}

uint16 UpgradeFWIFPartitionWrite(UpgradeFWIFPartitionHdl handle, const uint8 *data, uint16 len)
{
    UNUSED(handle);
    UpgradeCtxGetFW()->lastPartitionData = data;
    UpgradeCtxGetFW()->lastPartitionDataLen = len;
    return len;
}

UpgradeHostErrorCode UpgradeFWIFPartitionClose(UpgradeFWIFPartitionHdl handle)
{
    UNUSED(handle);
    return UPGRADE_HOST_SUCCESS;
}

#endif /* UPGRADE_USE_FW_STUBS */


/******************************************************************************
NAME
    UpgradeFWIFPartitionGetOffset
*/
uint32 UpgradeFWIFPartitionGetOffset(UpgradeFWIFPartitionHdl handle)
{
    return UpgradeFWIFGetSinkPosition((Sink)handle);
}

/******************************************************************************
NAME
    UpgradeFWIFGetSinkPosition
*/
uint32 UpgradeFWIFGetSinkPosition(Sink sink)
{
    uint32 offset = 0;
    bool result = ImageUpgradeSinkGetPosition(sink, &offset);
    DEBUG_LOG_DEBUG("ImageUpgradeSinkGetPosition(%p, @%p -> %ld) returns %d", sink, &offset, offset, result);
    return offset;
}
