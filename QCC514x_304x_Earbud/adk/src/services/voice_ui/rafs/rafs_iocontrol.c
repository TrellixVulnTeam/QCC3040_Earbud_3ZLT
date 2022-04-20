/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_iocontrol.c
\brief      Implementation of IO control

*/
#include <csrtypes.h>
#include <vmtypes.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_directory.h"
#include "rafs_utils.h"
#include "rafs_volume.h"
#include "rafs_iocontrol.h"

rafs_errors_t Rafs_DoIocGetPartitionHandle(const char *partition_name, raPartition_handle *handle, rafs_size_t *out_length)
{
    rafs_errors_t result;
    result = Rafs_GetMountStatus(partition_name);
    if( result == RAFS_OK )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        *handle = rafs_self->partition->part_handle;
        *out_length = sizeof(*handle);
    }
    return result;
}

rafs_errors_t Rafs_DoIocGetFileOffset(const char *pathname, uint32 *value, rafs_size_t *out_length)
{
    rafs_errors_t result;
    result = Rafs_GetMountStatus(pathname);
    if( result == RAFS_OK )
    {
        char filename[RAFS_MAX_FILELEN];
        result = Rafs_GetFilename(pathname, filename, sizeof(filename));
        if( result == RAFS_OK )
        {
            uint32  dir_slot = 0;
            if( Rafs_FindDirEntryByFilename(filename, &dir_slot) )
            {
                rafs_instance_t *rafs_self = Rafs_GetTaskData();
                *value = rafs_self->files->scan_dir_entry->inodes[0].offset *
                         rafs_self->partition->part_info.block_size;
                *out_length = sizeof(*value);
            }
            else
            {
                result = RAFS_FILE_NOT_FOUND;
            }
        }
    }
    return result;
}

rafs_errors_t Rafs_DoIocSetAppStatusIdleCb(const rafs_ioc_set_app_status_idle_cb_t *data)
{
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_self->idler = *data;
    return result;
}
