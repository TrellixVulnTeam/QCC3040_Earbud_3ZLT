/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Mass Storage file support utilities
*/

#include "usb_msc.h"
#include "usb_msc_common.h"

typedef struct
{
    FILE_INDEX  index;
    uint32      size;
} usb_file_info;

static void usbMsc_FileInfo(const char* name, usb_file_info* info)
{
    info->index = FileFind(FILE_ROOT, name, strlen(name));
    info->size = 0;

    if (info->index != FILE_NONE)
    {
        Source source = StreamFileSource(info->index);
        if (source)
        {
            uint16 data_size = SourceSize(source);
            while (data_size)
            {
                info->size += data_size;
                SourceDrop(source, data_size);
                data_size = SourceSize(source);
            }
            SourceClose(source);
        }
    }
}

bool UsbMsc_PrepareConfig(usb_msc_config_params_t *config,
        const char *root_file_name,
        const char *data_file_name,
        const char *fat_file_name)
{
    usb_file_info root_file;
    usb_file_info data_file;
    usb_file_info fat_file;

    /* check root directory FILE */
    usbMsc_FileInfo(root_file_name, &root_file);
    /* check data file FILE */
    usbMsc_FileInfo(data_file_name, &data_file);

    /* check optional FAT FILE */
    if (fat_file_name)
    {
        usbMsc_FileInfo(fat_file_name, &fat_file);
    }
    else
    {
        fat_file.index = FILE_NONE;
        fat_file.size = 0;
    }

    DEBUG_LOG_INFO("UsbMsc: config - root %u size %u, data file %u size %u",
                   root_file.index, root_file.size,
                   data_file.index, data_file.size);

    if(root_file.index != FILE_NONE &&
            data_file.index != FILE_NONE)
    {
        config->root_dir.file = root_file.index;
        config->root_dir.size = root_file.size;

        config->data_area.file = data_file.index;
        config->data_area.size = data_file.size;

        config->table.file = fat_file.index;
        config->table.size = fat_file.size;

        return TRUE;
    }
    DEBUG_LOG_WARN("UsbMsc: config - not found, disable class");
    return FALSE;
}

