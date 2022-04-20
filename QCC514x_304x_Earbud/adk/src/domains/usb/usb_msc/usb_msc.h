/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB Mass Storage class driver

USB Mass Storage class implements a read-only storage presenting arbitrary
content.

The class emulates a read-only FAT16 filesystem. Long names are not supported
so all file names must be 11 capital characters (8 + 3).
Reserved sectors are hard-coded in the code and can not be changed.

Application provides 3 files with content for the following regions:
1. Root Directory Region: contains names and attributes of the data files.
2. FAT Region: FAT16 data.
3. Data Region: content of the data files.
These files shall be placed onto a read-only filesystem on the flash.

UsbMsc_PrepareConfig() can be called to initialize configuration data
with the file indices and sizes.
*/

#ifndef USB_MSC_H_
#define USB_MSC_H_

#include <usb_device.h>
#include <usb_device_utils.h>
#include <file.h>

/*! File descriptor data */
typedef struct
{
    FILE_INDEX file;
    uint32 size;
} usb_msc_file_t;

/*! Mass Storage class configuration parameters */
typedef struct
{
    /** File with the DATA area data */
    usb_msc_file_t data_area;
    /** File with the FAT area data */
    usb_msc_file_t table;
    /** File with the ROOT directory area data.*/
    usb_msc_file_t root_dir;
} usb_msc_config_params_t;

/*! Check files are present on the flash and populate USB mass storage config data.
 *
 * \param root_file_name name of the file with the Root Directory Region content.
 * \param data_file_name name of the file with the Data Region content.
 * \param fat_file_name name of the file with the FAT Region content. This
 *                      file is optional, if NULL then hard-coded FAT data is used
 *                      (clusters #0, #1 reserved, #2 the last).
 * \return TRUE if files were found and parsed. */
bool UsbMsc_PrepareConfig(usb_msc_config_params_t *config,
        const char *root_file_name,
        const char *data_file_name,
        const char *fat_file_name);

/*! USB Mass Storage interface
 *
 * Configuration shall be supplied in a structure of type "usb_msc_config_params_t". */
extern const usb_class_interface_cb_t UsbMsc_Callbacks;

#endif /* USB_MSC_H_ */

