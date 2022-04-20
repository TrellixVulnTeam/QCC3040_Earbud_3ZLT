/****************************************************************************
Copyright (c) 2004 - 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_partition_data.h

DESCRIPTION
    Upgrade file processing module.
    It parses headers, write partition data to the SQIF.
    As well as verifies OEM signature.
*/
#ifndef UPGRADE_PARTITION_DATA_H_
#define UPGRADE_PARTITION_DATA_H_

#include <csrtypes.h>

#include <upgrade_protocol.h>
#include "upgrade_fw_if.h"

#define HEADER_FIRST_PART_SIZE 12
#define ID_FIELD_SIZE 8
#define PARTITION_SECOND_HEADER_SIZE 4
#define UPGRADE_HEADER_MIN_SECOND_PART_SIZE 14
#define UPGRADE_VERSION_SIZE 4
#define UPGRADE_NO_OF_COMPATIBLE_UPGRADES_SIZE 2
#define UPGRADE_NO_OF_COMPATIBLE_PS_VERSION_SIZE 4
#define UPGRADE_HEADER_EXTRA_INFO 2

#define PARTITION_SECOND_HEADER_SIZE 4
#define PARTITION_LEN_SIZE 4
#define PARTITION_TYPE_SIZE 2
#define PARTITION_NUM_SIZE 2
#define PARTITION_FIRST_WORD_SIZE 4

/* DFU headers will be stored on PSKEYs.
   DFU_HEADER_PSKEY_START: First PSKEY number where DFU headers will be stored.
   DFU_HEADER_NUM_PSKEY: Number of PSKEYs which will be used to store the headers.
   Upgrade Header Size: 12 + 36 = 48 bytes
   Partition Header (plus first word) Size: N * 20; where max N is 13 (num partition), so 13 * 20 = 260 bytes
   Footer Size: 12 + 256 = 268
   Total 576 bytes. So using 10 PSKEYs (10 * 64 = 640 bytes)
*/
#define DFU_HEADER_PSKEY_START  510 /* Reserved PSKEY for upgrade DFU_HEADER_PSKEY_START */
#define DFU_HEADER_PSKEY_LEN    10
#define DFU_HEADER_PSKEY_END    (DFU_HEADER_PSKEY_START + DFU_HEADER_PSKEY_LEN - 1)

extern uint8 first_word_size;

/*!
    @brief State of data transfer after last segment received
*/
typedef enum {
    UPGRADE_PARTITION_DATA_XFER_ERROR,
    UPGRADE_PARTITION_DATA_XFER_COMPLETE,
    UPGRADE_PARTITION_DATA_XFER_IN_PROGRESS
    } UpgradePartitionDataPartialTransferState;

/*!
    @brief Store the DFU headers in PSKEYs.
    @param data Pointer to data to be strored.
    @param len Length data to be strored.
    @return Upgrade library error code.
*/
UpgradeHostErrorCode UpgradeSaveHeaderInPSKeys(const uint8 *data, uint16 data_size);

/*!
    @brief Initialisation of PartitionData module.
    @param waitForEraseComplete Set TURE if need to wait for UpgradeSMEraseStatus.
    @return TRUE if ok, FALSE if out of memory.
*/
bool UpgradePartitionDataInit(bool *waitForEraseComplete);

/*!
    @brief Free memory allocated for partition data.
*/
void UpgradePartitionDataDestroy(void);

/*!
    @brief UpgradePartitionDataGetNextReqSize

    @return Size of data request for a host.
            It will never be bigger than blockSize.
*/
uint32 UpgradePartitionDataGetNextReqSize(void);

/*!
    @brief UpgradePartitionDataGetNextOffset

    @return Offset, from current position in a file, from which data should be retrieved.
*/
uint32 UpgradePartitionDataGetNextOffset(void);

/*!
    @brief UpgradePartitionDataParseIncomplete

    @param data Part of file conforming to upgrade file format.
    @param len  Size of data, less than requested due to transport packet size
                limits.

    @return The upgrade library error code
*/
UpgradeHostErrorCode UpgradePartitionDataParseIncomplete(const uint8 *data, uint16 data_len);

/*!
    @brief UpgradePartitionDataParse

    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.

    @return The upgrade library error code
*/
UpgradeHostErrorCode UpgradePartitionDataParse(const uint8 *data, uint16 len);

/*!
    @brief Handle an incoming data packet and move to internal buffer

    @note NOT NORMALLY CALLED DIRECTLY. Use UpgradePartitionDataParse()
    which handles the data and processes if complete.

    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.

    @return Status of the transfer
*/
UpgradePartitionDataPartialTransferState UpgradePartitionDataParseDataCopy(const uint8 *data, uint16 len);

/*!
    @brief Notify the data parser to stop future data handling.

    Causes @ref UpgradePartitionDataParseDataCopy to stop handling incoming 
    data and report errors. The error condition should be reset on a new 
    transfer/data request initiated by the library.
*/
void UpgradePartitionDataStopData(void);

/*!
    @brief UpgradePartitionDataIsDfuUpdate

    @return TRUE if one of a partitions contains a DFU file.
*/
bool UpgradePartitionDataIsDfuUpdate(void);

/*!
    @brief UpgradePartitionDataGetDfuPartition

    @return Partition number containing DFU file.
*/
uint16 UpgradePartitionDataGetDfuPartition(void);

/*!
    @brief UpgradePartitionDataHandleDataHeaderState
    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.
    @param reqComplete  Indication of whether the request is complete.


    @return The upgrade library error code
*/
UpgradeHostErrorCode UpgradePartitionDataHandleDataHeaderState(const uint8 *data, uint16 len, bool reqComplete);

/*!
    @brief UpgradePartitionDataRequestData
    @param size Possible size of the next data request
*/
void UpgradePartitionDataRequestData(uint32 size, uint32 offset);

/*!
    @brief UpgradePartitionDataHandleGeneric1stPartState
    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.
    @param reqComplete  Indication of whether the request is complete.

    @return The upgrade library error code
*/
UpgradeHostErrorCode UpgradePartitionDataHandleGeneric1stPartState(const uint8 *data, uint16 len, bool reqComplete);

/*!
    @brief UpgradePartitionDataHandleDataState
    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.
    @param reqComplete  Indication of whether the request is complete.

    @return The upgrade library error code
*/
UpgradeHostErrorCode UpgradePartitionDataHandleDataState(const uint8 *data, uint16 len, bool reqComplete);

/*!
    @brief UpgradePartitionDataCopyFromStream
    @param signature The signature array being copied to.
    @param offset The offset into the signature being copied to.
    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.

    @return The upgrade library error code
*/
void UpgradePartitionDataCopyFromStream(uint8 *signature, uint16 offset, const uint8 *data, uint16 len);

/*!
    @brief UpgradePartitionDataHandleHeaderState
    @param data Part of file conforming to upgrade file format.
    @param len  Size of data. It may be less than requested but
                it can't be more.
    @param reqComplete  Indication of whether the request is complete.

    @return The upgrade library error code
*/
UpgradeHostErrorCode UpgradePartitionDataHandleHeaderState(const uint8 *data, uint16 len, bool reqComplete);

/*!
    @brief Clear all the PSKEYs which are used to store the DFU headers
    @param none
    
    @return none
*/
void UpgradeClearHeaderPSKeys(void);

/*!
    @brief Open a write only handle to a physical partition on the external flash.
            For initial testing, the CRC check on the partition is also disabled.

    @param physPartition Physical partition number in external flash.
    @param firstWord First word of partition data.

    @return UpgradeFWIFPartitionHdl A valid handle or NULL if the open failed.
*/
UpgradeFWIFPartitionHdl UpgradePartitionDataPartitionOpen(uint16 physPartition, uint32 firstWord);

#endif /* UPGRADE_PARTITION_DATA_H_ */
