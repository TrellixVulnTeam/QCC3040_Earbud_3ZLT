/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      FAT16 support for USB mass storage class
*/

#ifndef USB_MSC_FAT16_H_
#define USB_MSC_FAT16_H_

#include "usb_msc_common.h"

/* General FAT defines */
#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 4
#define NO_FATS 2
#define SECTORS_PER_FAT 134
#define SECTORS_PER_TRACK 63
#define RESERVED_SECTORS 4
#define ROOT_DIR_ENTRIES 512
#define ROOT_DIR_SIZE 32
#define NUMBER_HEADS 255
#define TOTAL_SECTORS 31556

/* Starting sectors for each area of data */
#define MBR_SECTOR 0 
#define BOOT_SECTOR 63 
#define FAT1_SECTOR (BOOT_SECTOR + RESERVED_SECTORS)
#define FAT2_SECTOR (FAT1_SECTOR + SECTORS_PER_FAT)
#define ROOT_SECTOR (FAT2_SECTOR + SECTORS_PER_FAT)
#define DATA_SECTOR (ROOT_SECTOR + (ROOT_DIR_ENTRIES * ROOT_DIR_SIZE / BYTES_PER_SECTOR))

/* cluster defines for File Allocation Table */
#define FAT_UNUSED_CLUSTER 0x0000
#define FAT_BAD_CLUSTER 0xfff7
#define FAT_LAST_CLUSTER 0xffff

/* directory attribute defines */
#define DIR_ATTRIBUTE_READ_ONLY 0x01
#define DIR_ATTRIBUTE_HIDDEN 0x02
#define DIR_ATTRIBUTE_SYSTEM 0x04
#define DIR_ATTRIBUTE_VOLUME_LABEL 0x08
#define DIR_ATTRIBUTE_SUB_DIR 0x10
#define DIR_ATTRIBUTE_ARCHIVE 0x20


/* First 446 bytes of Master Boot Record is executable code */     
typedef struct
{
    uint8 exe[446];
} MasterBootRecordExeType;

/* Partition details in Master Boot Record */
typedef struct
{
    uint8 bootIndicator[1];
    uint8 startingHead[1];
    uint8 startingSectorCyl[2];
    uint8 partitionType[1];
    uint8 endingHead[1];
    uint8 endingSectorCyl[2];
    uint8 startingSector[4];
    uint8 sectorsInPartition[4];
} MasterBootRecordPartitionType;

/* Executable signature */
typedef struct
{
    uint8 executableSignature[2];
} ExeSignatureType;

/* Boot record header information */
typedef struct
{
    uint8 jumpInstruction[3];
    uint8 oemName[8];
    uint8 bytesPerSector[2];
    uint8 sectorsPerCluster[1];
    uint8 noReservedSectors[2];
    uint8 noFats[1];
    uint8 maxRootDirEntries[2];
    uint8 totalSectorCountSmall[2];
    uint8 mediaDescriptor[1];
    uint8 sectorsPerFat[2];
    uint8 sectorsPerTrack[2];
    uint8 noHeads[2];
    uint8 hiddenSectors[4];
    uint8 totalSectorCountLarge[4];
    uint8 phyiscalDriveNumber[1];
    uint8 reserved1[1];
    uint8 extendedBootSignature[1];
    uint8 serialNumber[4];
    uint8 volumeLabel[11];
    uint8 fileSystemType[8];
} BootSectorType;

typedef struct
{
    uint8 exe[448];
} BootSectorExeType;

typedef struct
{
    uint8 filename[8];
    uint8 extension[3];
    uint8 attributes[1];
    uint8 ignore1[1];
    uint8 creationTime[3];
    uint8 creationDate[2];
    uint8 lastAccessDate[2];
    uint8 lastAccessTime[2];
    uint8 lastWriteTime[2];
    uint8 lastWriteDate[2];
    uint8 firstLogicalCluster[2];
    uint8 fileSizeBytes[4];    
} DirectoryType;



#endif /* USB_MSC_FAT16_H_ */
