/****************************************************************************
Copyright (c) 2014 - 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_partition_data.c

DESCRIPTION
    Upgrade file processing state machine.
    It is parsing and validating headers.
    All received data are passed to MD5 validation.
    Partition data are written to SQIF.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <string.h>
#include <stdlib.h>
#include <byte_utils.h>
#include <panic.h>
#include <print.h>

#include "upgrade_partition_data.h"
#include "upgrade_partition_data_priv.h"
#include "upgrade_ctx.h"
#include "upgrade_fw_if.h"
#include "upgrade_psstore.h"
#include "upgrade_partitions.h"

/*
*  For the time being, UPGRADE_HEADER_DATA_LOG is being defined to 36 [fixed] and can be updated/changed later
*  when when exact size be found out what makes DFU FIRST HEADER as 48 (12 from HEADER_FIRST_PART_SIZE)  while
* assuming remaining 36 comes from UPGRADE_HEADER_DATA_LOG
*
*  It should be ensured that UPGRADE_HEADER_DATA_LOG is NOT USED in the code.
*/
#define UPGRADE_HEADER_DATA_LOG 36

/*
* A macro to identify if we need to store DFU file headers in the PS Key.
* Information stored in header is required for reconstructing DFU file and 
* transferring to peer (if supported) and also in resuming of DFU. 
* The OR part of the condition will hit for headset build, when the host is GAA - in that
* case too we want to save header PS Keys and further use it during GAA OTA resume scenario.
*/
#define UPGRADE_IS_HEADER_STORING_REQ (UPGRADE_IS_PEER_SUPPORTED || Upgrade_GetContext() == UPGRADE_CONTEXT_GAA_OTA)

/* This compile time assert deals with total number of sectoins and NOT the actual
*  numbe of upgradable sections as:-
*  1. Exact number of upgradable sections are not available in the public domain.
*  2. Upgradable section and Non-Upgradable sections does not occupy seperate spaces and
*     are intermingled. If U is Upgradable section and N is Non-upgradable, sections may
*     be arranged like UNNUUUUUNNUNUNUN and not UUUUUUUUU followed by NNNNNNN.
*  Hence this assert is an early WARNING as it takes into account 'MAX' sections available.
*  Actual/Real warning is only possible iff exact number of Upgradable Sections are known.
*  
*  In calculation below-
*  Upgrade Header Size: 12 + 36 = 48 bytes which is-
*		HEADER_FIRST_PART_SIZE+ UPGRADE_HEADER_DATA_LOG = 12 + 36 =48
*
*  Partition Header (plus first word) Size: IMAGE_SECTION_ID_MAX * (HEADER_FIRST_PART_SIZE+FIRST_WORD_SIZE*PARTITION_SECOND_HEADER_SIZE); 
*		where IMAGE_SECTION_ID_MAX is 0xe i.e, 15 partitions (num partition), so 15 * (12+4+4) = 15*20= 300 bytes
*
*  Footer Size: 12 + 256(OEM Signature length) = 268 = HEADER_FIRST_PART_SIZE+EXPECTED_SIGNATURE_SIZE
*
*  Total 616 bytes. So using 10 PSKEYs (10 * 64 = 640 bytes) where
*	PSKEY_MAX_STORAGE_LENGTH = 32 words of uint16 size, but sizeof is not allowed in a preprocessor hence replaced with 2.
*	DFU_HEADER_PSKEY_LEN = 10 for 10 PSKEYs
*
*
*   Negation of condition is used, i.e, while LHS is < RHS, overflow will not happen and hence no warning.
*/
#if (((HEADER_FIRST_PART_SIZE+UPGRADE_HEADER_DATA_LOG)+\
	(IMAGE_SECTION_ID_MAX*(HEADER_FIRST_PART_SIZE+PARTITION_SECOND_HEADER_SIZE+FIRST_WORD_SIZE)+\
        (HEADER_FIRST_PART_SIZE+EXPECTED_SIGNATURE_SIZE)))>= (2*PSKEY_MAX_STORAGE_LENGTH*DFU_HEADER_PSKEY_LEN))
        #error *** Early WARNING! May exhaust PSKEYS ***
#endif


#ifndef MIN
#define MIN(a,b)    (((a)<(b))?(a):(b))
#endif

static UpgradeHostErrorCode upgradeParseCompleteData(const uint8 *data, uint16 len, bool reqComplete);
static UpgradeHostErrorCode upgradeHandleFooterState(const uint8 *data, uint16 len, bool reqComplete);


void UpgradePartitionDataDestroy(void)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    if (ctx)
    {
        if(ctx->signature)
        {
            free(ctx->signature);
            ctx->signature = NULL;
        }
        free(ctx);

        UpgradeCtxSetPartitionData(NULL);
    }
}


uint32 UpgradePartitionDataGetNextReqSize(void)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    uint32 size = ctx->totalReqSize - ctx->totalReceivedSize;

    if (UpgradeCtxGet()->max_request_size)
    {
        /* Transport has a max request size, so check against it. */
        if (size > UpgradeCtxGet()->max_request_size)
        {
            /* Remaining data larger than transport's max request size.
               Request the biggest chunk we can instead. */
            size = UpgradeCtxGet()->max_request_size;
        }

        /* Wait for each chunk to finish before requesting the next. */
        if (ctx->chunkReceivedSize > 0)
        {
            /* Still receiving current chunk, check if it's finished. */
            if (ctx->chunkReceivedSize < UpgradeCtxGet()->max_request_size)
            {
                /* Chunk not finished, don't request more data until it has. */
                size = 0;
            }
            else
            {
                /* Chunk finished, reset chunk data count. */
                ctx->chunkReceivedSize = 0;
            }
        }
    }
    else
    {
        /* Transport does not have a max request size. Request the entire
           partition first time around (unless resuming a previous upgrade - the
           value stored in newReqSize takes account of this), then always return
           0 for subsequent calls whilst the transfer is in progress. */
        size = ctx->newReqSize;
        ctx->newReqSize = 0;
    }
    return size;
}


uint32 UpgradePartitionDataGetNextOffset(void)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    return ctx->offset;
}


UpgradeHostErrorCode UpgradePartitionDataParseIncomplete(const uint8 *data, uint16 data_len)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    UpgradeHostErrorCode status = UPGRADE_HOST_SUCCESS;

    DEBUG_LOG_VERBOSE("UpgradePartitionDataParseIncomplete, data_len %d", data_len);

    /* TODO: Allocate dynamic buffer, instead of static buffer */

    /* Calculate space left in buffer */
    uint8 buf_space = sizeof(ctx->incompleteData.data) - ctx->incompleteData.size;

    if (data_len > buf_space)
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataParseIncomplete, header too big for buffer");
        return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
    }

    /* Copy into buffer */
    memmove(&ctx->incompleteData.data[ctx->incompleteData.size], data, data_len);
    ctx->incompleteData.size += data_len;

    /* Parse data if request complete or buffer is full */
    const bool req_complete = (ctx->totalReceivedSize == ctx->totalReqSize);
    DEBUG_LOG_VERBOSE("UpgradePartitionDataParseIncomplete, received %u, requested %u, complete %u",
                   ctx->totalReceivedSize, ctx->totalReqSize, req_complete);

    /* If request is now complete attempt to parse again */
    if (req_complete)
    {
        DEBUG_LOG_DATA_V_VERBOSE(ctx->incompleteData.data, ctx->incompleteData.size);
        status = upgradeParseCompleteData(ctx->incompleteData.data, ctx->incompleteData.size,
                                          req_complete);
        ctx->incompleteData.size = 0;
    }

    DEBUG_LOG_VERBOSE("UpgradePartitionDataParseIncomplete, status %u", status);
    return status;
}


/****************************************************************************
NAME
    UpgradePartitionDataParse

DESCRIPTION
    If the received upgrade data is larger than the expected size in the next 
    data request 'nextReqSize' then this loops through until the received data 
    is either completely written into the SQIF or copied into the upgrade 
    buffer,'incompleteData'

RETURNS
    Upgrade library error code.
*/

UpgradeHostErrorCode UpgradePartitionDataParse(const uint8 *data, uint16 data_len)
{
    uint32 pc;
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    UpgradeHostErrorCode status = UPGRADE_HOST_SUCCESS;

    /* Update total received size */
    ctx->totalReceivedSize += data_len;
    ctx->chunkReceivedSize += data_len;

    pc = 100 * ctx->totalReceivedSize / ctx->totalReqSize;
    DEBUG_LOG_INFO("UpgradePartitionDataParse, data_len %u, total_size %u, total_received %u (%u%%)",
                   data_len, ctx->totalReqSize, ctx->totalReceivedSize, pc);

    /* Error if we've received more than we requested */
    if (ctx->totalReceivedSize > ctx->totalReqSize)
        return UPGRADE_HOST_ERROR_BAD_LENGTH_PARTITION_PARSE;

    /* Handle case of incomplete data */
    if (ctx->incompleteData.size)
        status = UpgradePartitionDataParseIncomplete(data, data_len);
    else
    {
        /* Parse block */
        const bool req_complete = ctx->totalReceivedSize == ctx->totalReqSize;
        status = upgradeParseCompleteData(data, data_len, req_complete);
    }

    DEBUG_LOG_VERBOSE("UpgradePartitionDataParse, status %u", status);
    return status;
}


/****************************************************************************
NAME
    UpgradePartitionDataStopData  -  Stop processing incoming data

DESCRIPTION
    Function to stop the processing of incoming data.
*/
void UpgradePartitionDataStopData(void)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    ctx->totalReqSize = ctx->newReqSize = 0;
    ctx->totalReceivedSize = 0;
    ctx->chunkReceivedSize = 0;
}


/****************************************************************************
NAME
    UpgradePartitionDataRequestData

DESCRIPTION
    Determine size of the next data request
*/
void UpgradePartitionDataRequestData(uint32 size, uint32 offset)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    DEBUG_LOG_DEBUG("UpgradePartitionDataRequestData: size=%u offset=%u", size, offset);
    ctx->totalReqSize = ctx->newReqSize = size;
    ctx->totalReceivedSize = 0;
    ctx->chunkReceivedSize = 0;
    ctx->offset = offset;
}


/****************************************************************************
NAME
    upgradeParseCompleteData  -  Parser state machine

DESCRIPTION
    Calls state handlers depending of current state.
    All state handlers are setting size of next data request.

RETURNS
    Upgrade library error code.
*/
UpgradeHostErrorCode upgradeParseCompleteData(const uint8 *data, uint16 len, bool reqComplete)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    UpgradeHostErrorCode rc = UPGRADE_HOST_ERROR_INTERNAL_ERROR_1;

    DEBUG_LOG_VERBOSE("UpgradePartitionDataParse, state %u, length %d, complete %u, offset %u",
                      ctx->state, len, reqComplete, ctx->offset);

    UpgradePartitionDataState state = ctx->state;
    switch (ctx->state)
    {
    case UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART:
        DEBUG_LOG_VERBOSE("upgradeParseCompleteData, UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART");
        rc = UpgradePartitionDataHandleGeneric1stPartState(data, len, reqComplete);
        break;

    case UPGRADE_PARTITION_DATA_STATE_HEADER:
        DEBUG_LOG_VERBOSE("upgradeParseCompleteData, UPGRADE_PARTITION_DATA_STATE_HEADER");
        rc = UpgradePartitionDataHandleHeaderState(data, len, reqComplete);
        break;

    case UPGRADE_PARTITION_DATA_STATE_DATA_HEADER:
        DEBUG_LOG_VERBOSE("upgradeParseCompleteData, UPGRADE_PARTITION_DATA_STATE_DATA_HEADER");
        rc = UpgradePartitionDataHandleDataHeaderState(data, len, reqComplete);
        break;

    case UPGRADE_PARTITION_DATA_STATE_DATA:
        DEBUG_LOG_VERBOSE("upgradeParseCompleteData, UPGRADE_PARTITION_DATA_STATE_DATA");
        rc = UpgradePartitionDataHandleDataState(data, len, reqComplete);
        break;

    case UPGRADE_PARTITION_DATA_STATE_FOOTER:
        DEBUG_LOG_VERBOSE("upgradeParseCompleteData, UPGRADE_PARTITION_DATA_STATE_FOOTER");
        rc = upgradeHandleFooterState(data, len, reqComplete);
        break;
    }

    if (state != ctx->state)
        DEBUG_LOG_INFO("UpgradePartitionDataParse, new state %u", ctx->state);
    if (rc != UPGRADE_HOST_SUCCESS)
        DEBUG_LOG_INFO("UpgradePartitionDataParse, status %u", rc);

    return rc;
}

/****************************************************************************
NAME
    HandleFooterState  -  Signature data handling.

DESCRIPTION
    Collects MD5 signature data and sends it for validation.
    Completion of this step means that data were download to a SQIF.

RETURNS
    Upgrade library error code.
*/
UpgradeHostErrorCode upgradeHandleFooterState(const uint8 *data, uint16 len, bool reqComplete)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();

    DEBUG_LOG_DEBUG("HandleFooterState");

    UpgradePartitionDataCopyFromStream(ctx->signature, ctx->signatureReceived, data, len);

    ctx->signatureReceived += len;

    if(UPGRADE_IS_HEADER_STORING_REQ)
    {
        UpgradeHostErrorCode rc = UpgradeSaveHeaderInPSKeys(data, len);

        if(UPGRADE_HOST_SUCCESS != rc)
        {
            return rc;
        }
    }

    if(reqComplete)
    {
        ctx->signatureReceived = 0;
        return UPGRADE_HOST_DATA_TRANSFER_COMPLETE;        
    }

    return UPGRADE_HOST_SUCCESS;
}

bool UpgradePartitionDataIsDfuUpdate(void)
{
    return (UpgradeCtxGetPSKeys()->dfu_partition_num != 0);
}

uint16 UpgradePartitionDataGetDfuPartition(void)
{
    return UpgradeCtxGetPSKeys()->dfu_partition_num - 1;
}

bool UpgradeIsPartitionDataState(void)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    bool rc = TRUE;

    if(ctx == NULL)
    {
        return FALSE;
    }
     if(ctx->state == UPGRADE_PARTITION_DATA_STATE_DATA)
    {
        rc = TRUE;
    }
     else
    {
        rc = FALSE;
    }

    return rc;
}

uint32 UpgradeGetPartitionSizeInPartitionDataState(void)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    uint32 size;

    if(ctx == NULL)
    {
        size = 0;
    }
    else
    {
        size = ctx->partitionLength - ctx->offset - first_word_size;
    }

    return size;
}

uint16 UpgradePartitionDataDfuHeaderPskeyStart(void)
{
    return DFU_HEADER_PSKEY_START;
}

uint16 UpgradePartitionDataDfuHeaderPskeyEnd(void)
{
    return DFU_HEADER_PSKEY_END;
}

uint32 UpgradePartitionDataHeaderFirstPartSize(void)
{
    return HEADER_FIRST_PART_SIZE;
}

uint8 UpgradePartitionDataIdFieldSize(void)
{
    return ID_FIELD_SIZE;
}

uint32 UpgradePartitionDataPartitionSecondHeaderSize(void)
{
    return PARTITION_SECOND_HEADER_SIZE;
}

