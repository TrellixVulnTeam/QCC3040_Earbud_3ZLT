/****************************************************************************
Copyright (c) 2014 - 2018 Qualcomm Technologies International, Ltd.


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
#include <imageupgrade.h>
#include <ps.h>
#include <system_clock.h>

#include "upgrade_partition_data.h"
#include "upgrade_partition_data_priv.h"
#include "upgrade_ctx.h"
#include "upgrade_fw_if.h"
#include "upgrade_psstore.h"
#include "upgrade_partitions.h"
#include "rsa_decrypt.h"


#if defined (UPGRADE_RSA_2048)
/*
 * EXPECTED_SIGNATURE_SIZE is the size of an RSA-2048 signature.
 * 2048 bits / 8 bits per byte is 256 bytes.
 */
#define EXPECTED_SIGNATURE_SIZE 256
#elif defined (UPGRADE_RSA_1024)
/*
 * EXPECTED_SIGNATURE_SIZE is the size of an RSA-1024 signature.
 * 1024 bits / 8 bits per byte is 128 bytes.
 */
#define EXPECTED_SIGNATURE_SIZE 128
#else
#error "Neither UPGRADE_RSA_2048 nor UPGRADE_RSA_1024 defined."
#endif

#define FIRST_WORD_SIZE 4

/*
* A macro to identify if we need to store DFU file headers in the PS Key.
* Information stored in header is required for reconstructing DFU file and 
* transferring to peer (if supported) and also in resuming of DFU. 
* The OR part of the condition will hit for headset build, when the host is GAA - in that
* case too we want to save header PS Keys and further use it during GAA OTA resume scenario.
*/
#define UPGRADE_IS_HEADER_STORING_REQ (UPGRADE_IS_PEER_SUPPORTED || Upgrade_GetContext() == UPGRADE_CONTEXT_GAA_OTA)

#define BYTES_TO_WORDS(_b_)       ((_b_+1) >> 1)
#define UPGRADE_PARTITION_ZERO 0

/*
 * A module variable set by the UpgradePartitionDataHandleHeaderState function
 * to the value of the last byte in the data array. Retrieved by the
 * UpgradeFWIFValidateFinalize function using the
 * UpgradePartitionDataGetSigningMode function.
 */
static uint8 SigningMode = 1;
uint8 first_word_size = FIRST_WORD_SIZE;

uint8 UpgradePartitionDataGetSigningMode(void);

/****************************************************************************
NAME
    IsValidPartitionNum

DESCRIPTION
    Validates the partition number

RETURNS
    bool TRUE if the partition number is valid, else FALSE
*/
static bool IsValidPartitionNum(uint16 partNum)
{
    switch (partNum)
    {
    case IMAGE_SECTION_NONCE:
    case IMAGE_SECTION_APPS_P0_HEADER:
    case IMAGE_SECTION_APPS_P1_HEADER:
    case IMAGE_SECTION_AUDIO_HEADER:
    case IMAGE_SECTION_CURATOR_FILESYSTEM:
    case IMAGE_SECTION_APPS_P0_IMAGE:
    case IMAGE_SECTION_APPS_RO_CONFIG_FILESYSTEM:
    case IMAGE_SECTION_APPS_RO_FILESYSTEM:
    case IMAGE_SECTION_APPS_P1_IMAGE:
    case IMAGE_SECTION_APPS_DEVICE_RO_FILESYSTEM:
    case IMAGE_SECTION_AUDIO_IMAGE:
    case IMAGE_SECTION_APPS_RW_CONFIG:
        return TRUE;

    case IMAGE_SECTION_APPS_RW_FILESYSTEM:
    default:
        return FALSE;
    }
}

/****************************************************************************
NAME
    IsValidSqifNum

DESCRIPTION
    Validates the SQIF number for the partition

RETURNS
    bool TRUE if the SQIF and partition numbers are valid, else FALSE
*/
static bool IsValidSqifNum(uint16 sqifNum, uint16 partNum)
{
    UNUSED(partNum);
    /* TODO
     * Until audio is supported, only SQIF zero is valid.
     */
    return (sqifNum == 0);
}

/***************************************************************************
NAME
    UpgradePartitionDataPartitionOpen

DESCRIPTION
    Open a write only handle to a physical partition on the external flash.
    For initial testing, the CRC check on the partition is also disabled.

PARAMS
    physPartition Physical partition number in external flash.
    firstWord First word of partition data.

RETURNS
    UpgradeFWIFPartitionHdl A valid handle or NULL if the open failed.
*/
UpgradeFWIFPartitionHdl UpgradePartitionDataPartitionOpen(uint16 physPartition, uint32 firstWord)
{
    Sink sink;
    /** TODO
     * When audio is supported, we can determine the QSPI to use from the partition.
     * Until then only QSPI zero is used.
     */
    uint16 QSPINum = 0;

    DEBUG_LOG_DEBUG("UpgradeFWIFPartitionHdl: opening partition %u for resume", physPartition);
    sink = ImageUpgradeStreamGetSink(QSPINum, physPartition, firstWord);
    DEBUG_LOG_DEBUG("UpgradeFWIFPartitionHdl: ImageUpgradeStreamGetSink(%d, %d, 0x%08lx) returns %p\n", QSPINum, physPartition, firstWord, sink);
    if (!sink)
    {
        DEBUG_LOG_ERROR("UpgradeFWIFPartitionHdl: failed to open raw partition %u for resume", physPartition);
        return (UpgradeFWIFPartitionHdl)NULL;
    }
    SinkConfigure(sink, VM_SINK_MESSAGES, VM_MESSAGES_NONE);

    UpgradeCtxGetFW()->partitionNum = physPartition;

    return (UpgradeFWIFPartitionHdl)sink;
}


/***************************************************************************
NAME
    UpgradeClearHeaderPSKeys

DESCRIPTION
    Clear all the PSKEYs which are used to store the DFU headers
*/
void UpgradeClearHeaderPSKeys(void)
{
    uint16 upgrade_pskey;

    DEBUG_LOG_INFO("UpgradeClearHeaderPSKeys: Clear Header PSKeys");
    
    for(upgrade_pskey = DFU_HEADER_PSKEY_START;
        upgrade_pskey <= DFU_HEADER_PSKEY_END; upgrade_pskey++)
    {
        PsStore(upgrade_pskey, NULL, 0);
    }
}

/***************************************************************************
NAME
    UpgradeSaveHeaderInPSKeys

DESCRIPTION
    Store the DFU headers in PSKEYs

RETURNS
    Upgrade library error code.
*/
UpgradeHostErrorCode UpgradeSaveHeaderInPSKeys(const uint8 *data, uint16 data_size)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    uint8 keyCache[PSKEY_MAX_STORAGE_LENGTH_IN_BYTES];
    
    do
    {
        uint16 data_write_size;

        /* Find out how many words are written into PSKEY already and read contents
         * into local cache */
        uint16 pskey_length = PsRetrieve(ctx->dfuHeaderPskey, NULL, 0);
        DEBUG_LOG_VERBOSE("UpgradeSaveHeaderInPSKeys, Current Header PS KEY = %d, len = %d words, offset = %d", ctx->dfuHeaderPskey, pskey_length, ctx->dfuHeaderPskeyOffset);

        if (pskey_length)
            PsRetrieve(ctx->dfuHeaderPskey, keyCache, pskey_length);

        /* Check and limit how much data can be written into this PSKEY */
        if (ctx->dfuHeaderPskeyOffset + data_size > PSKEY_MAX_STORAGE_LENGTH_IN_BYTES)
            data_write_size = PSKEY_MAX_STORAGE_LENGTH_IN_BYTES - ctx->dfuHeaderPskeyOffset;
        else
            data_write_size = data_size;

        /* To prevent re-writing of Header PS Key,
         * if header data is already stored in the PSKey, do not re-write it.
         * This situation can occur during DFU resume when some of the header
         * data was already stored before interruption.
         * NOTE : pskey_length is in words so converted it to bytes for the conditional check*/
        if(ctx->dfuHeaderPskeyOffset + data_write_size > (pskey_length<<1))
        {
            DEBUG_LOG_VERBOSE("UpgradeSaveHeaderInPSKeys, Writing header PS Key : %d from offset : %d of len : %d",ctx->dfuHeaderPskey, ctx->dfuHeaderPskeyOffset, data_write_size);
            /* Copy data into cache */
            memcpy(&keyCache[ctx->dfuHeaderPskeyOffset], data, data_write_size);

            /* Update PSKEY,  */
            pskey_length = BYTES_TO_WORDS(ctx->dfuHeaderPskeyOffset + data_write_size);
            if (pskey_length != PsStore(ctx->dfuHeaderPskey, keyCache, pskey_length))
            {
                DEBUG_LOG_ERROR("UpgradeSaveHeaderInPSKeys, PsStore failed, key_num %u, offset %u, length %u",
                                ctx->dfuHeaderPskey, ctx->dfuHeaderPskeyOffset, pskey_length);
                return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
            }
        }

        ctx->dfuHeaderPskeyOffset += data_write_size;
        data += data_write_size;
        data_size -= data_write_size;

        /* If this PSKEY is now full, reset offset and advance PSKEY number
         * so that remainder is written into the next PSKEY */
        if (ctx->dfuHeaderPskeyOffset == PSKEY_MAX_STORAGE_LENGTH_IN_BYTES)
        {
            /* Move to next PSKEY */
            ctx->dfuHeaderPskey += 1;

            /* Reset offset */
            ctx->dfuHeaderPskeyOffset = 0;

            /* Return error if we've run out of PSKEYs */
            if (ctx->dfuHeaderPskey > DFU_HEADER_PSKEY_END)
            {
                DEBUG_LOG_ERROR("UpgradeSaveHeaderInPSKeys, no more PSKEYs available");
                return UPGRADE_HOST_ERROR_NO_MEMORY;
            }

        }
    } while (data_size != 0);

    DEBUG_LOG_INFO("UpgradeSaveHeaderInPSKeys, ctx->dfuHeaderPskey %d", ctx->dfuHeaderPskey);
    DEBUG_LOG_INFO("UpgradeSaveHeaderInPSKeys, ctx->dfuHeaderPskeyOffset %d", ctx->dfuHeaderPskeyOffset);
    
    return UPGRADE_HOST_SUCCESS;
}

/****************************************************************************
NAME
    UpgradePartitionDataInit  -  Initialise.

DESCRIPTION
    Initialises the partition data header handling.

RETURNS
    bool TRUE if OK, FALSE if not.
*/
bool UpgradePartitionDataInit(bool *waitForEraseComplete)
{
    UpgradePartitionDataCtx *ctx;
    *waitForEraseComplete = FALSE;
    uint32 size = HEADER_FIRST_PART_SIZE;
    uint32 offset = 0;

    ctx = UpgradeCtxGetPartitionData();
    if (!ctx)
    {
        ctx = malloc(sizeof(*ctx));
        if (!ctx)
        {
            return FALSE;
        }
        memset(ctx, 0, sizeof(*ctx));
        UpgradeCtxSetPartitionData(ctx);
    }

    size = ctx->newReqSize;
    if(size == 0)
    {
        DEBUG_LOG_INFO("UpgradePartitionDataInit : No size available");
        size = HEADER_FIRST_PART_SIZE;
        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    }
    offset = ctx->offset;
    DEBUG_LOG_INFO("UpgradePartitionDataInit : next req size : %d, from offset : %d\n", size, offset);
    
    UpgradePartitionDataRequestData(size, offset);

    /*these were initialized as part of UpgradePartitionDataInitHelper called from dfu domain*/
    DEBUG_LOG_INFO("UpgradePartitionDataInit, Post UpgradePartitionDataRequestData: UpgradePartitionDataCtx->partitionLength = %d, totalReqSize = %d, totalReceivedSize = %d, newReqSize = %d, offset = %d, state enum:UpgradePartitionDataState:%d",
                    ctx->partitionLength, ctx->totalReqSize, ctx->totalReceivedSize, 
                    ctx->newReqSize, ctx->offset, ctx->state);

    UpgradeFWIFInit();
    UpgradeFWIFValidateInit();

    /* \todo May need to take the status of peer into account, but not directly available */
    if ((UpgradeCtxGetPSKeys()->upgrade_in_progress_key == UPGRADE_RESUME_POINT_START) &&
        (UpgradeCtxGetPSKeys()->state_of_partitions == UPGRADE_PARTITIONS_UPGRADING) &&
        (UpgradeCtxGetPSKeys()->last_closed_partition > 0) &&
        !UpgradeCtxGet()->force_erase)
    {
        /* A partial update has been interrupted. Don't erase. */
        DEBUG_LOG_INFO("UpgradePartitionDataInit: partial update interrupted. Not erasing");
        return TRUE;
    }
    /* Ensure the other bank is erased before we start. */
    if (UPGRADE_PARTITIONS_ERASED == UpgradePartitionsEraseAllManaged())
    {
        *waitForEraseComplete = TRUE;
        return TRUE;
    }

    return FALSE;
}

/****************************************************************************
NAME
    UpgradePartitionDataHandleHeaderState  -  Parser for the main header.

DESCRIPTION
    Validates content of the main header.

RETURNS
    Upgrade library error code.

NOTES
    Currently when main header size will grow beyond block size it won't work.
*/
UpgradeHostErrorCode UpgradePartitionDataHandleHeaderState(const uint8 *data, uint16 len, bool reqComplete)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    UpgradeHostErrorCode rc = UPGRADE_HOST_SUCCESS;
    upgrade_version newVersion;
    upgrade_version currVersion;
    uint16 newPSVersion;
    uint16 currPSVersion;
    uint16 compatibleVersions;
    uint16 psconfigVersions;
    const uint8 *ptr; /* Pointer into variable length portion of header */
    unsigned i;
    uint16 pktSize;
    if (!reqComplete)
    {
        /* Handle the case where packet size is smaller than header size */
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, header not complete");
        return UpgradePartitionDataParseIncomplete(data, len);
    }

    /* Length must contain at least ID FIELD, major, minor and compatibleVersions */
    if (len < (ID_FIELD_SIZE + UPGRADE_VERSION_SIZE + UPGRADE_NO_OF_COMPATIBLE_UPGRADES_SIZE))
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleHeaderState, packet size incorrect");
        return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
    }

    /* TODO: Check length */

    if((strlen(UpgradeFWIFGetDeviceVariant()) > 0) &&
       (strncmp((char *)data, UpgradeFWIFGetDeviceVariant(), ID_FIELD_SIZE)))
    {
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, wrong variant");
        return UPGRADE_HOST_ERROR_WRONG_VARIANT;
    }

    newVersion.major = ByteUtilsGet2BytesFromStream(&data[ID_FIELD_SIZE]);
    newVersion.minor = ByteUtilsGet2BytesFromStream(&data[ID_FIELD_SIZE + 2]);
    compatibleVersions = ByteUtilsGet2BytesFromStream(&data[ID_FIELD_SIZE + 4]);
    currVersion.major = UpgradeCtxGetPSKeys()->version.major;
    currVersion.minor = UpgradeCtxGetPSKeys()->version.minor;

    DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, current version %u.%u, new version %u.%u, compatible versions %u",
                      currVersion.major, currVersion.minor, newVersion.major, newVersion.minor, compatibleVersions);

    /* Check packet is big enough so we know how many compatibleVersion are in the packet. */
    /* Packet size is calculated as:  ID_FIELD_SIZE + UPGRADE_VERSION_SIZE + UPGRADE_NO_OF_COMPATIBLE_UPGRADES_SIZE 
     * + (4 * compatibleVersions) + UPGRADE_NO_OF_COMPATIBLE_PS_VERSION_SIZE + (2 * psconfigVersions ) + UPGRADE_HEADER_EXTRA_INFO;
     * Which is : ID_FIELD_SIZE + 6 + (4 * compatibleVersions) + 2 + 2
     * A check has been introduced if actual length is smaller than the above value, then the packet is malformed and error is
     * reported to the user. Otherwise the control goes ahead to calculate packet length.
     * The pktSize varies based on the number of compatible versions and ps config versions.
     */
    /* Currently, the first for loop starts reading at offset ID_FIELD + 6 and 
     * and it reads 4 bytes of compatible upgrades (compatibleVersions) data each time round the loop.
     * Then the second loop starts reading at offset psconfigVersions, i.e,  ID_FIELD_SIZE + 6 + (4 * compatibleVersions)
     * size calculation stops here if actual length is smaller than the length that is being calculated by adding psconfigVersions.
     * It reads 2 bytes of compatible upgrades (psconfigVersions) data each time round the loop.
     * maintained while calculating the pktsize.
     */


    /* Check to ensure the array is not read beyond the end of truncated packet */
    if (len < ID_FIELD_SIZE + 6 + (4 * compatibleVersions) + 2+2)
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleHeaderState, Malformed packet. Invalid length: %u", len);
        return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
    }
    else /* len >= ID_FIELD_SIZE + 6 + (4 * compatibleVersions) + 2+2 */
    {
        psconfigVersions = ByteUtilsGet2BytesFromStream(&data[ID_FIELD_SIZE + 6 + (4 * compatibleVersions) + 2]);
        pktSize = ID_FIELD_SIZE + UPGRADE_VERSION_SIZE + UPGRADE_NO_OF_COMPATIBLE_UPGRADES_SIZE + (4 * compatibleVersions)
              + UPGRADE_NO_OF_COMPATIBLE_PS_VERSION_SIZE + (2 * psconfigVersions ) + UPGRADE_HEADER_EXTRA_INFO;
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, packet size:  %u, len: %u",
                        pktSize, len);
    }
    DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, pktSize: %u, len: %u",pktSize,len);
    if (len != pktSize)
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleHeaderState, packet size incorrect, required %u, actual %u",
                        pktSize, len);
        return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
    }

    ptr = &data[ID_FIELD_SIZE + 6];
    for (i = 0; i < compatibleVersions; i++)
    {
        upgrade_version version;
        version.major = ByteUtilsGet2BytesFromStream(ptr);
        version.minor = ByteUtilsGet2BytesFromStream(ptr + 2);
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, compatible version %u.%u",
                           version.major, version.minor);

        if (version.major == currVersion.major)
            if ((version.minor == currVersion.minor) || (version.minor == 0xFFFFu))
                break;

        ptr += 4;

    }

    /* We failed to find a compatibility match */
    if (i == compatibleVersions)
    {
        DEBUG_LOG_WARN("UpgradePartitionDataHandleHeaderState, no compatible versions");
        return UPGRADE_HOST_WARN_APP_CONFIG_VERSION_INCOMPATIBLE;
    }

    ptr = &data[ID_FIELD_SIZE + 6 + (4 * compatibleVersions)];
    currPSVersion = UpgradeCtxGetPSKeys()->config_version;
    newPSVersion = ByteUtilsGet2BytesFromStream(ptr);
    DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, current PS version %u, new PS version %u",
                      currPSVersion, newPSVersion);

    if (currPSVersion != newPSVersion)
    {
        ptr += 4;
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, number of compatible PS versions %u",
                          psconfigVersions);
        for (i = 0; i < psconfigVersions; i++)
        {
            uint16 version;
            version = ByteUtilsGet2BytesFromStream(ptr);
            DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, compatible PS version %u",
                              version);
            /* Break out of loop if compatible */
            if (version == currPSVersion)
                break;
            ptr += 2;
        }
        if (i == psconfigVersions)
        {
            DEBUG_LOG_WARN("UpgradePartitionDataHandleHeaderState, no compatible PS versions");
            return UPGRADE_HOST_WARN_APP_CONFIG_VERSION_INCOMPATIBLE;
        }
    }

    /* Store the in-progress upgrade version */
    UpgradeCtxGetPSKeys()->version_in_progress.major = newVersion.major;
    UpgradeCtxGetPSKeys()->version_in_progress.minor = newVersion.minor;
    UpgradeCtxGetPSKeys()->config_version_in_progress = newPSVersion;

    DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, saving versions %u.%u, PS version %u",
                       newVersion.major, newVersion.minor, newPSVersion);

    /* At this point, partitions aren't actually dirty - but want to minimise PSKEYS
     * @todo: Need to check this variable before starting an upgrade
     */
    UpgradeCtxGetPSKeys()->state_of_partitions = UPGRADE_PARTITIONS_UPGRADING;

    /*!
        @todo Need to minimise the number of times that we write to the PS
              so this may not be the optimal place. It will do for now.
    */
    UpgradeSavePSKeys();

    if(UPGRADE_IS_HEADER_STORING_REQ)
    {
        rc = UpgradeSaveHeaderInPSKeys(data, len);
        if (UPGRADE_HOST_SUCCESS != rc)
            return rc;
    }

    UpgradePartitionDataRequestData(HEADER_FIRST_PART_SIZE, 0);
    ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;

    /* Set the signing mode to the value of the last byte in the data array */
    SigningMode = data[len - 1];
    DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleHeaderState, signing mode %u", SigningMode);

    return rc;
}

/****************************************************************************
NAME
    UpgradePartitionDataHandleDataHeaderState  -  Parser for the partition data header.

DESCRIPTION
    Validates content of the partition data header.

RETURNS
    Upgrade library error code.
*/
UpgradeHostErrorCode UpgradePartitionDataHandleDataHeaderState(const uint8 *data, uint16 len, bool reqComplete)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    uint16 partNum, sqifNum;
    uint32 firstWord;
    UNUSED(reqComplete);

    if (!reqComplete)
    {
        /* Handle the case where packet size is smaller than header size */
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleDataHeaderState, header not complete");
        return UpgradePartitionDataParseIncomplete(data, len);
    }

    if (len < PARTITION_SECOND_HEADER_SIZE + FIRST_WORD_SIZE)
        return UPGRADE_HOST_ERROR_BAD_LENGTH_DATAHDR_RESUME;

    sqifNum = ByteUtilsGet2BytesFromStream(data);
    DEBUG_LOG("UpgradePartitionDataHandleDataHeaderState PART_DATA: SQIF number %u", sqifNum);

    partNum = ByteUtilsGet2BytesFromStream(&data[2]);
    DEBUG_LOG("UpgradePartitionDataHandleDataHeaderState PART_DATA: partition number %u", partNum);

    if (!IsValidPartitionNum(partNum))
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataHeaderState, partition %u, is not valid", partNum);
        return UPGRADE_HOST_ERROR_WRONG_PARTITION_NUMBER;
    }

    if (!IsValidSqifNum(sqifNum, partNum))
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataHeaderState, sqif %u, is not valid", sqifNum);
        return UPGRADE_HOST_ERROR_PARTITION_TYPE_NOT_MATCHING;
    }

    if (UPGRADE_IS_HEADER_STORING_REQ)
    {
        UpgradeHostErrorCode rc = UpgradeSaveHeaderInPSKeys(data, PARTITION_SECOND_HEADER_SIZE + FIRST_WORD_SIZE);
        if (UPGRADE_HOST_SUCCESS != rc)
        {
            DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataHeaderState, failed to store PSKEYSs, error %u", rc);
            return rc;
        }
    }

    /* reset the firstword before using */
    firstWord  = (uint32)data[PARTITION_SECOND_HEADER_SIZE + 3] << 24;
    firstWord |= (uint32)data[PARTITION_SECOND_HEADER_SIZE + 2] << 16;
    firstWord |= (uint32)data[PARTITION_SECOND_HEADER_SIZE + 1] << 8;
    firstWord |= (uint32)data[PARTITION_SECOND_HEADER_SIZE];
    DEBUG_LOG_INFO("UpgradePartitionDataHandleDataHeaderState, first word is 0x%08lx", firstWord);

    if (!UpgradeFWIFValidateUpdate(NULL, partNum))
    {
        return UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_PARTITION_HEADER1;
    }

    if (UpgradeCtxGetPSKeys()->last_closed_partition > partNum)
    {
        DEBUG_LOG_INFO("UpgradePartitionDataHandleDataHeaderState, already handled partition %u, skipping it", partNum);
        UpgradePartitionDataRequestData(HEADER_FIRST_PART_SIZE, ctx->partitionLength - FIRST_WORD_SIZE);
        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
        UpgradeCtxGetFW()->partitionNum = partNum;
        return UPGRADE_HOST_SUCCESS;
    }

    if (ctx->partitionLength > UpgradeFWIFGetPhysPartitionSize(partNum))
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataHeaderState, partition size mismatch, upgrade %lu, actual %lu", ctx->partitionLength, UpgradeFWIFGetPhysPartitionSize(partNum));
        return UPGRADE_HOST_ERROR_PARTITION_SIZE_MISMATCH;
    }

    /* Partition could have been already open if we are resuming (without rebooting the device) 
       the upgrade so, check and use the partitionHdl if its non-zero. */
    if(!ctx->partitionHdl)
    {
        DEBUG_LOG_INFO("UpgradePartitionDataHandleDataHeaderState, open partion %d to write", partNum);
        ctx->partitionHdl = UpgradePartitionDataPartitionOpen(partNum, firstWord);
    }
    else
    {
        /* fwCtx gets reinitialized on resume so, set the partitionNum even if we are using old partitionHdl. */
        UpgradeCtxGetFW()->partitionNum = partNum;
    }

    if (!ctx->partitionHdl)
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataHeaderState, failed to open partition %u", partNum);
        return UPGRADE_HOST_ERROR_PARTITION_OPEN_FAILED;
    }

    uint32 offset = UpgradeFWIFPartitionGetOffset(ctx->partitionHdl);
    DEBUG_LOG_INFO("UpgradePartitionDataHandleDataHeaderState, partition length %lu, offset %lu", ctx->partitionLength, offset);
    if ((offset + FIRST_WORD_SIZE) < ctx->partitionLength)
    {        
        ctx->time_start = SystemClockGetTimerTime();

        /* Get partition data from the offset, but skipping the first word. */
        UpgradePartitionDataRequestData(ctx->partitionLength - offset - FIRST_WORD_SIZE, offset);
        ctx->state = UPGRADE_PARTITION_DATA_STATE_DATA;
    }
    else if ((offset + FIRST_WORD_SIZE) == ctx->partitionLength)
    {
        /* A case when all data are in but partition is not yet closed */
        UpgradeHostErrorCode closeStatus = UpgradeFWIFPartitionClose(ctx->partitionHdl);
        ctx->partitionHdl = NULL;
        if (UPGRADE_HOST_SUCCESS != closeStatus)
        {
            DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataHeaderState, failed to close partition %u, status %u", partNum, closeStatus);
            return closeStatus;
        }

        ctx->openNextPartition = TRUE;

        offset -= FIRST_WORD_SIZE;
        UpgradePartitionDataRequestData(HEADER_FIRST_PART_SIZE, offset);
        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    }
    else
    {
        /* It is considered bad when offset is bigger than partition size */
        return UPGRADE_HOST_ERROR_INTERNAL_ERROR_3;
    }

    return UPGRADE_HOST_SUCCESS;
}


/****************************************************************************
NAME
    UpgradePartitionDataHandleGeneric1stPartState  -  Parser for ID & length part of a header.

DESCRIPTION
    Parses common beginning of any header and determines which header it is.
    All headers have the same first two fields which are 'header id' and
    length.

RETURNS
    Upgrade library error code.
*/
UpgradeHostErrorCode UpgradePartitionDataHandleGeneric1stPartState(const uint8 *data, uint16 len, bool reqComplete)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    uint32 length;
    UpgradeHostErrorCode rc = UPGRADE_HOST_SUCCESS;

    if (!reqComplete)
    {
        /* Handle the case where packet size is smaller than header size */
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleGeneric1stPartState, header not complete");
        return UpgradePartitionDataParseIncomplete(data, len);
    }

    if (len < HEADER_FIRST_PART_SIZE)
        return UPGRADE_HOST_ERROR_BAD_LENGTH_TOO_SHORT;

    length = ByteUtilsGet4BytesFromStream(&data[ID_FIELD_SIZE]);

    DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleGeneric1stPartState, id '%c%c%c%c%c%c%c%c', length 0x%lx",
                      data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length);


    /* APPUHDR5 */
    if(0 == strncmp((char *)data, UpgradeFWIFGetHeaderID(), ID_FIELD_SIZE))
    {
        if (length < UPGRADE_HEADER_MIN_SECOND_PART_SIZE)
            return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;

        if(UPGRADE_IS_HEADER_STORING_REQ)
        {
            /* Store the header data of upgrade header on PSKEY */
            ctx->dfuHeaderPskey = DFU_HEADER_PSKEY_START;
            ctx->dfuHeaderPskeyOffset = 0;
            rc = UpgradeSaveHeaderInPSKeys(data, len);
            if (UPGRADE_HOST_SUCCESS != rc)
                return rc;
        }

        /* Clear the dfu_partition_num pskey value before the file transfer,
         *.so that no junk value is stored. This pskey value is used during
         * application reconnect after defined reboot.
         */
        UpgradeCtxGetPSKeys()->dfu_partition_num = 0;
        UpgradeSavePSKeys();

        UpgradePartitionDataRequestData(length, 0);
        ctx->state = UPGRADE_PARTITION_DATA_STATE_HEADER;
        ctx->isUpgradeHdrAvailable = TRUE;
    }
    else if (0 == strncmp((char *)data, UpgradeFWIFGetPartitionID(), ID_FIELD_SIZE))
    {
        if((UpgradeCtxGetFW()->partitionNum == UPGRADE_PARTITION_ZERO) && (!ctx->isUpgradeHdrAvailable))
        {
            DEBUG_LOG_INFO("UpgradePartitionDataHandleGeneric1stPartState,Error: Upgrade Header is not available");
            return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
        }
        if(length < PARTITION_SECOND_HEADER_SIZE + FIRST_WORD_SIZE)
        {
            return UPGRADE_HOST_ERROR_BAD_LENGTH_PARTITION_HEADER;
        }

        if(UPGRADE_IS_HEADER_STORING_REQ)
        {
            rc = UpgradeSaveHeaderInPSKeys(data, len);
            if(UPGRADE_HOST_SUCCESS != rc)
            {
                return rc;
            }
        }

        UpgradePartitionDataRequestData(PARTITION_SECOND_HEADER_SIZE + FIRST_WORD_SIZE, 0);
        ctx->state = UPGRADE_PARTITION_DATA_STATE_DATA_HEADER;
        ctx->partitionLength = length - PARTITION_SECOND_HEADER_SIZE;
    }
    else if (0 == strncmp((char *)data, UpgradeFWIFGetFooterID(), ID_FIELD_SIZE))
    {
        if (length != EXPECTED_SIGNATURE_SIZE)
        {
            /* The length of signature must match expected length.
             * Otherwise OEM signature checking could be omitted by just
             * setting length to 0 and not sending signature.
             */
            return UPGRADE_HOST_ERROR_BAD_LENGTH_SIGNATURE;
        }

        if(UPGRADE_IS_HEADER_STORING_REQ)
        {
            rc = UpgradeSaveHeaderInPSKeys(data, len);
            if(UPGRADE_HOST_SUCCESS != rc)
            {
                return rc;
            }
        }

        UpgradePartitionDataRequestData(length, 0);

        if(!ctx->signature)
        {
            /* if earlier malloc'd signature could not be free'd, don't malloc again */
            ctx->signature = malloc(length);
        }
        if (!ctx->signature)
        {
            return UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_MEMORY;
        }

        ctx->state = UPGRADE_PARTITION_DATA_STATE_FOOTER;
    }
    else
    {
        return UPGRADE_HOST_ERROR_UNKNOWN_ID;
    }

    return UPGRADE_HOST_SUCCESS;
}

/****************************************************************************
NAME
    UpgradePartitionDataHandleDataState  -  Partition data handling.

DESCRIPTION
    Writes data to a SQIF and sends it the MD5 validation.

RETURNS
    Upgrade library error code.
*/
UpgradeHostErrorCode UpgradePartitionDataHandleDataState(const uint8 *data, uint16 len, bool reqComplete)
{
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();

    if (len != UpgradeFWIFPartitionWrite(ctx->partitionHdl, data, len))
    {
        DEBUG_LOG_ERROR("UpgradePartitionDataHandleDataState, partition write failed, length %u",
                        len);
        return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    if (reqComplete)
    {
        DEBUG_LOG_INFO("UpgradePartitionDataHandleDataState, partition write complete");

        rtime_t duration_ms = rtime_sub(SystemClockGetTimerTime(), ctx->time_start) / 1000;
        uint32 bytes_per_sec = (ctx->totalReqSize * 1000) / duration_ms;
        DEBUG_LOG_INFO("UpgradePartitionDataHandleDataState, took %lu ms, %lu bytes/s", duration_ms, bytes_per_sec);

        UpgradeHostErrorCode closeStatus;
        UpgradePartitionDataRequestData(HEADER_FIRST_PART_SIZE, 0);
        ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
        closeStatus = UpgradeFWIFPartitionClose(ctx->partitionHdl);
        ctx->partitionHdl = NULL;
        
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleDataState partition close status is enum:UpgradeHostErrorCode:%d", closeStatus);
        if (UPGRADE_HOST_SUCCESS != closeStatus)
        {
            DEBUG_LOG_INFO("UpgradePartitionDataHandleDataState, failed to close partition");
            return closeStatus;
        }

        ctx->openNextPartition = TRUE;
    }
    else
        DEBUG_LOG_VERBOSE("UpgradePartitionDataHandleDataState, waiting for more data");

    return UPGRADE_HOST_SUCCESS;
}


/****************************************************************************
NAME
    UpgradePartitionDataCopyFromStream  -  Copy from stream.

DESCRIPTION
    Accounts for differences in offset value in CONFIG_HYDRACORE.

RETURNS
    Nothing.
*/
void UpgradePartitionDataCopyFromStream(uint8 *signature, uint16 offset, const uint8 *data, uint16 len)
{
    ByteUtilsMemCpyFromStream(&signature[offset], data, len);
}

/****************************************************************************
NAME
    UpgradePartitionDataGetSigningMode

DESCRIPTION
    Gets the signing mode value set by the header.

RETURNS
    uint8 signing mode.
*/
uint8 UpgradePartitionDataGetSigningMode(void)
{
    return SigningMode;
}

