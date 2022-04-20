/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_volume.c
\brief      Implement functions which operate on a partition volume.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_dirent.h"
#include "rafs_extent.h"
#include "rafs_fat.h"
#include "rafs_integrity.h"
#include "rafs_volume.h"
#include "rafs_utils.h"

rafs_errors_t Rafs_DoMount(const char *partition_name)
{
    rafs_errors_t result = RAFS_OK;

    result = Rafs_PartitionOpen(partition_name);
    if( result == RAFS_OK )
    {
        result = Rafs_CheckIntegrity();

        if( result == RAFS_OK )
        {
            Rafs_InitialiseSectorMap();
            Rafs_ScanDirectory();
        }
        else
        {
            Rafs_PartitionClose();
            result = RAFS_INVALID_FAT;
        }
    }

    return result;
}

rafs_errors_t Rafs_DoUnmount(const char *partition_name)
{
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    char temp[sizeof(rafs_self->partition->partition_name)];

    result = Rafs_GetPartitionName(partition_name, temp, sizeof(temp));
    if( result == RAFS_OK )
    {
        if( safe_strncmp(rafs_self->partition->partition_name, temp, sizeof(temp)) == 0 )
        {
            Rafs_PartitionClose();
        }
        else
        {
            result = RAFS_NOT_MOUNTED;
        }
    }

    return result;
}

rafs_errors_t Rafs_DoStatfs(const char *partition_name, rafs_statfs_t *buf)
{
    UNUSED(partition_name);
    rafs_instance_t *rafs_self = Rafs_GetTaskData();

    /* Copied from the partition manager data */
    buf->sector_size    = rafs_self->partition->part_info.page_size;
    buf->partition_size = rafs_self->partition->part_info.partition_size;
    buf->block_size     = rafs_self->partition->part_info.block_size;

    /* The partition size, minus two pages for the two FATs*/
    buf->available_space= rafs_self->partition->part_info.partition_size - rafs_self->partition->part_info.block_size * 2;

    /* The sum of all the free sectors */
    buf->free_space     = Rafs_CountFreeSectors() * rafs_self->partition->part_info.block_size;

    /* For as many inodes that a file can have, sum the sizes of the n largest free extents */
    /* This represents the size of the largest file that can currently be created. */
    uint16 num = Rafs_CountFreeExtents();
    inode_t *inodes = PanicUnlessMalloc(num * sizeof(*inodes));
    uint16 actual = Rafs_GetFreeExtents(inodes, num);
    PanicFalse(actual == num);
    buf->file_space = 0;
    for( uint16 i = 0 ; i < MAX_INODES && i < num ; i++ )
    {
        buf->file_space += inodes[i].length * rafs_self->partition->part_info.block_size;
    }
    /* The single biggest of those, for the largest contiguous file */
    buf->contiguous_space = inodes[0].length * rafs_self->partition->part_info.block_size;
    free(inodes);

    /* Directory information */
    buf->max_dir_entries= MAX_DIR_ENTRIES - 1;
    buf->num_dir_entries= Rafs_CountActiveFiles();

    return RAFS_OK;
}

rafs_errors_t Rafs_GetMountStatus(const char *partition_name)
{
    rafs_errors_t result = Rafs_IsReady();
    if( result == RAFS_OK )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( !rafs_self->partition )
        {
            result = RAFS_NOT_MOUNTED;
        }
        else
        {
            char temp[sizeof(rafs_self->partition->partition_name)];

            result = Rafs_GetPartitionName(partition_name, temp, sizeof(temp));
            if( result == RAFS_OK )
            {
                if( safe_strncmp(rafs_self->partition->partition_name, temp, sizeof(temp)) != 0 )
                {
                    /* Something is mounted, but not with the given partition name */
                    result = RAFS_NOT_MOUNTED;
                }
            }
        }
    }
    return result;
}
