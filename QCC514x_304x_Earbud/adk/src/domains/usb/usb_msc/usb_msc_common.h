/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB Mass Storage common code
*/

#ifndef USB_MASS_STORAGE_COMMON_H_
#define USB_MASS_STORAGE_COMMON_H_

#include <usb_device.h>
#include <usb_device_utils.h>
#include <usb.h>

#include <csrtypes.h>

#include <sink.h>
#include <source.h>
#include <stream.h>
#include <string.h>
#include <panic.h>
#include <print.h>
#include <stdlib.h>

#include "logging.h"

#include "usb_msc_class.h"

/* Number of logical units supported on this device */
#define MAX_LUN 1

typedef struct
{
    uint16 index;
    uint32 size;
    uint8 *params;
    uint32 current_start_sector;
    Source src;
    uint32 end_sector;
} FileInfoType;

typedef enum
{
    FILE_INFO_FAT,
    FILE_INFO_ROOT_DIR,
    FILE_INFO_DATA,
    FILE_INFO_MAX
} file_info_value;

/** USB Mass Storage class data */
typedef struct usb_msc_class_data_t
{
    Sink class_sink;
    Source class_source;
    Sink ep_sink;
    Source ep_source;

    usb_msc_request_sense_response_t req_sense_rsp;
    uint8 info_read;
    FileInfoType file_info[FILE_INFO_MAX];
} usb_msc_class_data_t;

extern usb_msc_class_data_t *msc_class_data;

/** Busy wait until space available in the sink  */
void UsbMsc_BlockWaitReady(Sink sink, uint16 size);

/** Send block of data over the bulk endpoint  */
void UsbMsc_SendBulkData(uint8 *data, uint16 size_data);


/* SCSI command module */

/** Init SCSI command response data */
void UsbMsc_ScsiInit(void);

/** Process SCSI command */
uint16 UsbMsc_ScsiCommand(bool is_to_host, scsi_command_t cmd, uint8 *data,
                    uint32 xfer_length);


/* FAT16 support module */

/** Initialises the FAT16 implementation. */
void UsbMsc_Fat16_Initialise(usb_msc_class_data_t *msc);

/** Reads data from FAT file system.
 * The data is read from the area starting at logical_address and ending
 * at the address depending on transfer_length.
 * All read data is written to the bulk endpoint to transfer data
 * back to the host. */
void UsbMsc_Fat16_Read(usb_msc_class_data_t *msc, uint32 logical_address,
                       uint32 transfer_length);


/** Retrieves the block size from the FAT file system. This will be the bytes per sector. */
uint32 UsbMsc_Fat16_GetBlockSize(void);


/** Retrieves the total number of blocks from the FAT file system.
 * This will be the total number of sectors. */
uint32 UsbMsc_Fat16_GetTotalBlocks(void);


/** Configures what data exists in the FAT data area.
 * Can pass in a file index and size (value_16 and value_32),
 * or a pointer to some data and size (params and value_32). */
void UsbMsc_Fat16_ConfigureDataArea(usb_msc_class_data_t *msc, uint16 value_16,
        uint32 value_32, uint8 *params);


/** Configures what data exists in the FAT table.
 * Can pass in a file index and size (value_16 and value_32),
 * or a pointer to some data and size (params and value_32). */
void UsbMsc_Fat16_ConfigureFat(usb_msc_class_data_t *msc, uint16 value_16,
        uint32 value_32, uint8 *params);


/** Configures what data exists in the FAT root directory.
 * Can pass in a file index and size (value_16 and value_32),
 * or a pointer to some data and size (params and value_32). */
void UsbMsc_Fat16_ConfigureRootDir(usb_msc_class_data_t *msc, uint16 value_16,
        uint32 value_32, uint8 *params);

#endif /* USB_MASS_STORAGE_COMMON_H_ */
