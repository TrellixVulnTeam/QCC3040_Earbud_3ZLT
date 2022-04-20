/****************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_partition_data_priv.h
    
DESCRIPTION
    Definition of partition data processing state datatypes.
*/

#ifndef UPGRADE_PARTITION_DATA_PRIV_H_
#define UPGRADE_PARTITION_DATA_PRIV_H_

#include <ps.h>
#include "upgrade_fw_if.h"
#include "upgrade.h"
#include <rtime.h>

#define UPGRADE_PARTITION_DATA_BLOCK_SIZE(CTX) (((CTX)->partitionDataBlockSize) ? ((CTX)->partitionDataBlockSize) : UPGRADE_MAX_PARTITION_DATA_BLOCK_SIZE )

#define PREFETCH_UPGRADE_BLOCKS 3

typedef enum
{
    UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART,
    UPGRADE_PARTITION_DATA_STATE_HEADER,
    UPGRADE_PARTITION_DATA_STATE_DATA_HEADER,
    UPGRADE_PARTITION_DATA_STATE_DATA,
    UPGRADE_PARTITION_DATA_STATE_FOOTER
} UpgradePartitionDataState;

typedef struct {
    uint8 size;
    uint8 data[UPGRADE_MAX_PARTITION_DATA_BLOCK_SIZE];
} UpgradePartitionDataIncompleteData;

typedef struct {

    rtime_t time_start;

    uint32 newReqSize;          /* size of new request */
    uint32 totalReqSize;        /* total size of all requests */
    uint32 totalReceivedSize;   /* total size of data received */
    uint32 chunkReceivedSize;   /* size of data received for current chunk */
    uint32 offset;              /* current offset */

    UpgradePartitionDataIncompleteData incompleteData;

    UpgradePartitionDataState state;
    UpgradeFWIFPartitionHdl partitionHdl;
    uint32 partitionLength;

    uint8 *signature;
    uint16 signatureReceived;


    bool openNextPartition:1;
    bool isUpgradeHdrAvailable:1;

    /* PSKEY information to store the DFU headers */
    uint16 dfuHeaderPskey;
    uint16 dfuHeaderPskeyOffset;

} UpgradePartitionDataCtx;

#endif /* UPGRADE_PARTITION_DATA_PRIV_H_ */
