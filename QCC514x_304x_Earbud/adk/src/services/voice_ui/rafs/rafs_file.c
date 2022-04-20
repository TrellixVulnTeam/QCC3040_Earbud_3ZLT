/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_file.c
\brief      Implements operations acting on files in rafs

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <ra_partition_api.h>
#include <string.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_directory.h"
#include "rafs_extent.h"
#include "rafs_fat.h"
#include "rafs_power.h"
#include "rafs_utils.h"
#include "rafs_file.h"

rafs_errors_t Rafs_ValidateOpen(const char *filename, rafs_mode_t flags, rafs_file_t *file_id)
{
    PanicNull(file_id);
    PanicFalse((flags == RAFS_RDONLY) || (flags == RAFS_WRONLY));

    rafs_errors_t result = RAFS_OK;
    result = Rafs_IsValidFilename(filename);
    return result;
}

static open_file_t *Rafs_NewOpenFile(rafs_mode_t flags, uint32 dir_slot, Task task)
{
    open_file_t *result = PanicUnlessMalloc(sizeof(*result));
    memset(result, 0, sizeof(*result) );

    result->flags = flags;
    result->file_position = 0;
    result->extent_position = 0;
    result->directory_index = dir_slot;
    result->task = task;
    result->current_inode = 0;

    return result;
}

static rafs_errors_t rafs_doOpenRead(Task task, const char *filename, rafs_mode_t flags, rafs_file_t *file_id)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;
    uint32  dir_slot = 0;
    if( Rafs_FindDirEntryByFilename(filename, &dir_slot) )
    {
        *file_id = Rafs_FindFreeFileSlot();
        if( *file_id != RAFS_INVALID_FD )
        {
            open_file_t *of = Rafs_NewOpenFile(flags, dir_slot, task);
            rafs_self->files->open_files[*file_id] = of;
            of->file_dir_entry = *rafs_self->files->scan_dir_entry;
            rafs_self->files->num_open_files++;

            /* Update the access count on reading */
            of->file_dir_entry.stat_counters.access_count <<= 1;
            Rafs_DirEntryWriteMember(PRI_FAT, &of->file_dir_entry.stat_counters.access_count, dir_slot,
                                            offsetof(dir_entry_t,stat_counters.access_count),
                                            sizeof(of->file_dir_entry.stat_counters.access_count));
            Rafs_DirEntryWriteMember(SEC_FAT, &of->file_dir_entry.stat_counters.access_count, dir_slot,
                                            offsetof(dir_entry_t,stat_counters.access_count),
                                            sizeof(of->file_dir_entry.stat_counters.access_count));
        }
        else
        {
            result = RAFS_MAX_FILES_OPENED;
        }
    }
    else
    {
        result = RAFS_FILE_NOT_FOUND;
    }
    return result;
}

static rafs_errors_t rafs_doOpenWrite(Task task, const char *filename, rafs_mode_t flags, rafs_file_t *file_id)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;
    uint32  dir_slot = 0;
    if( Rafs_FindDirEntryByFilename(filename, &dir_slot) )
    {
        result = RAFS_FILE_EXISTS;
    }
    else if( rafs_self->files->file_is_open_for_writing )
    {
        /* Because we have a very simple sector allocator, */
        /* we only support one open file for writing at once. */
        /* Concurrent reading isn't a problem. */
        result = RAFS_MAX_FILES_OPENED;
    }
    else if( rafs_self->files->free_dir_slot == MAX_DIR_ENTRIES )
    {
        /* We won't be able to create a directory entry */
        result = RAFS_FILE_SYSTEM_FULL;
    }
    else
    {
        *file_id = Rafs_FindFreeFileSlot();
        if( *file_id != RAFS_INVALID_FD )
        {
            inode_t free_space = Rafs_FindLongestFreeExtent();
            if( free_space.offset == 0 && free_space.length == 0 )
            {
                /* There is no usable space to store the file */
                result = RAFS_FILE_SYSTEM_FULL;
                *file_id = RAFS_INVALID_FD;
            }
            else
            {
                dir_slot = rafs_self->files->free_dir_slot++;

                rafs_self->files->open_files[*file_id] = Rafs_NewOpenFile(flags, dir_slot, task);
                Rafs_MakeNewDirEntryForFilename(
                            &rafs_self->files->open_files[*file_id]->file_dir_entry, filename, &free_space);

                rafs_self->files->file_is_open_for_writing = TRUE;

                rafs_self->files->num_open_files++;

                /* Update the create and access counters on writing */
                /* This will be written when the file is closed */
                rafs_self->files->open_files[*file_id]->file_dir_entry.stat_counters.access_count = INITIAL_ACCESS;
                rafs_self->files->open_files[*file_id]->file_dir_entry.stat_counters.sequence_count =
                        ++rafs_self->files->sequence_number;
            }
        }
        else
        {
            result = RAFS_MAX_FILES_OPENED;
        }
    }
    return result;
}

rafs_errors_t Rafs_DoOpen(Task task, const char *filename, rafs_mode_t flags, rafs_file_t *file_id)
{
    rafs_errors_t result = RAFS_OK;

    if( flags == RAFS_RDONLY )
    {
        /* Although opening for reading does involve writing the access counter */
        /* the amount of data being modified is tiny compared to other longer duration */
        /* write operations. */
        result = rafs_doOpenRead(task, filename, flags, file_id);
    }
    else if( flags == RAFS_WRONLY )
    {
        result = Rafs_IsWriteSafe();
        if( result == RAFS_OK )
        {
            result = rafs_doOpenWrite(task, filename, flags, file_id);
        }
    }

    return result;
}

rafs_errors_t Rafs_ValidateFileIdentifier(rafs_file_t file_id)
{
    rafs_errors_t   result = Rafs_IsReady();
    if ( result == RAFS_OK )
    {
        if( Rafs_FindOpenFileInstanceById(file_id) == NULL )
        {
            result = RAFS_BAD_ID;
        }
    }
    return result;
}

rafs_errors_t Rafs_DoClose(rafs_file_t file_id)
{
    rafs_errors_t result = RAFS_OK;
    open_file_t *of = Rafs_FindOpenFileInstanceById(file_id);
    if( of != NULL )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( of->flags == RAFS_WRONLY )
        {
            /* Finalise the length of the last used extent */
            of->file_dir_entry.inodes[of->current_inode].length =
                    (uint16)(of->extent_position / rafs_self->partition->part_info.block_size +
                            (of->extent_position % rafs_self->partition->part_info.block_size != 0) );
            Rafs_AddExtentsToSectorMap(&of->file_dir_entry.inodes[of->current_inode], 1);
            of->file_dir_entry.file_size = of->file_position;
            bool pri_ok = Rafs_DirEntryWrite(PRI_FAT, &of->file_dir_entry, of->directory_index);
            bool sec_ok = Rafs_DirEntryWrite(SEC_FAT, &of->file_dir_entry, of->directory_index);
            if( !(pri_ok && sec_ok) )
            {
                result = RAFS_FAT_WRITE_FAILED;
            }
            rafs_self->files->file_is_open_for_writing = FALSE;
        }
        free(rafs_self->files->open_files[file_id]);
        rafs_self->files->open_files[file_id] = NULL;
        rafs_self->files->num_open_files--;
    }
    return result;
}

rafs_errors_t Rafs_DoRead(rafs_file_t file_id, void *buf,
                          rafs_size_t num_bytes_to_read, rafs_size_t *num_bytes_read)
{
    open_file_t *of = Rafs_FindOpenFileInstanceById(file_id);
    rafs_errors_t result = RAFS_OK;
    if( of != NULL )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        char *buff_ptr = buf;
        *num_bytes_read = 0;
        uint32 remaining_to_read = num_bytes_to_read;
        do {
            inode_t inode = of->file_dir_entry.inodes[of->current_inode];
            uint32 extent_offset = inode.offset * rafs_self->partition->part_info.block_size;
            uint32 extent_length = inode.length * rafs_self->partition->part_info.block_size;
            uint32 extent_start  = extent_offset + of->extent_position;
            uint32 extent_free   = extent_length - of->extent_position;
            uint32 file_remaining= of->file_dir_entry.file_size - of->file_position;
            uint32 this_read_len = MIN(MIN(file_remaining,remaining_to_read),extent_free);

            if( this_read_len > 0 )
            {
                raPartition_result ra_result = Rafs_PartRead(&rafs_self->partition->part_handle,
                                                     extent_start, this_read_len, buff_ptr);
                if( ra_result != RA_PARTITION_RESULT_SUCCESS )
                    DEBUG_LOG_ALWAYS("Rafs_DoRead(%d,%lu)=%d", file_id, num_bytes_to_read, ra_result);
            }

            buff_ptr += this_read_len;
            of->file_position += this_read_len;
            of->extent_position += this_read_len;
            *num_bytes_read += this_read_len;
            remaining_to_read -= this_read_len;

            if( remaining_to_read > 0 )
            {
                /* The current extent has been fully read, try the next one */
                if( of->current_inode < (MAX_INODES-1))
                {
                    /* If it's a valid inode, then start reading from it */
                    if( rafs_isValidInode(&of->file_dir_entry.inodes[of->current_inode+1]) )
                    {
                        of->current_inode++;
                        of->extent_position = 0;
                    }
                    else
                    {
                        result = RAFS_NO_MORE_DATA;
                    }
                }
                else
                {
                    result = RAFS_NO_MORE_DATA;
                }
            }
        } while ( result == RAFS_OK &&
                  remaining_to_read != 0 &&
                  of->file_position < of->file_dir_entry.file_size
                  );

        /* Reading some bytes at least counts as success */
        if( *num_bytes_read > 0 )
        {
            result = RAFS_OK;
        }
    }
    return result;
}

rafs_errors_t Rafs_DoWrite(rafs_file_t file_id, const void *buf,
                           rafs_size_t num_bytes_to_write, rafs_size_t *num_bytes_written)
{
    open_file_t *of = Rafs_FindOpenFileInstanceById(file_id);
    rafs_errors_t result = RAFS_OK;
    if( of != NULL )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        const char *buff_ptr = buf;
        *num_bytes_written = 0;
        uint32 remaining_to_write = num_bytes_to_write;
        do {
            inode_t inode = of->file_dir_entry.inodes[of->current_inode];
            uint32 extent_offset = inode.offset * rafs_self->partition->part_info.block_size;
            uint32 extent_length = inode.length * rafs_self->partition->part_info.block_size;
            uint32 extent_start  = extent_offset + of->extent_position;
            uint32 extent_free   = extent_length - of->extent_position;
            uint32 this_write_len= MIN(remaining_to_write,extent_free);

            if( this_write_len > 0 )
            {
                raPartition_result ra_result = Rafs_PartWrite(&rafs_self->partition->part_handle,
                                                      extent_start, this_write_len, buff_ptr);
                if( ra_result != RA_PARTITION_RESULT_SUCCESS )
                    DEBUG_LOG_ALWAYS("Rafs_DoWrite(%d,%lu)=%d", file_id, this_write_len, ra_result);
            }

            buff_ptr += this_write_len;
            of->file_position += this_write_len;
            of->extent_position += this_write_len;
            *num_bytes_written += this_write_len;
            remaining_to_write -= this_write_len;

            if( remaining_to_write > 0 )
            {
                /* The current extent has been fully written, try the next one */
                if( of->current_inode < (MAX_INODES-1))
                {
                    /* Mark the extent just filled as now in use, so the find will */
                    /* the next largest available extent */
                    Rafs_AddExtentsToSectorMap(&of->file_dir_entry.inodes[of->current_inode], 1);
                    inode_t free_space = Rafs_FindLongestFreeExtent();

                    if( free_space.offset == 0 && free_space.length == 0 )
                    {
                        /* Leave the last used extent to be handled by Rafs_DoClose */
                        Rafs_RemoveExtentsFromSectorMap(&of->file_dir_entry.inodes[of->current_inode], 1);

                        /* There is no usable space left to store the file */
                        result = RAFS_FILE_SYSTEM_FULL;
                    }
                    else
                    {
                        of->current_inode++;
                        of->file_dir_entry.inodes[of->current_inode] = free_space;
                        of->extent_position = 0;
                    }
                }
                else
                {
                    result = RAFS_FILE_FULL;
                }
            }
        } while ( result == RAFS_OK && remaining_to_write != 0 );
    }
    return result;
}

rafs_errors_t Rafs_DoSetPosition(rafs_file_t file_id, rafs_size_t position)
{
    open_file_t *of = Rafs_FindOpenFileInstanceById(file_id);
    rafs_errors_t result = RAFS_OK;
    if( of != NULL )
    {
        if( of->flags != RAFS_RDONLY )
        {
            result = RAFS_NOT_SEEKABLE;
        }
        else if( position < of->file_dir_entry.file_size )
        {
            rafs_instance_t *rafs_self = Rafs_GetTaskData();
            of->file_position = position;
            of->current_inode = 0;
            of->extent_position = 0;
            /* Locate the correct inode for the new position */
            uint32 scan = position;
            do {
                inode_t inode = of->file_dir_entry.inodes[of->current_inode];
                uint32 extent_length = inode.length * rafs_self->partition->part_info.block_size;
                if( scan < extent_length )
                {
                    of->extent_position = scan;
                    scan = 0;
                }
                else
                {
                    of->current_inode++;
                    scan -= extent_length;
                }
            } while( scan != 0 );
        }
        else
        {
            result = RAFS_INVALID_SEEK;
        }
    }
    return result;
}

rafs_errors_t Rafs_DoGetPosition(rafs_file_t file_id, rafs_size_t *position)
{
    open_file_t *of = Rafs_FindOpenFileInstanceById(file_id);
    rafs_errors_t result = RAFS_OK;
    if( of != NULL )
    {
        *position = of->file_position;
    }
    return result;
}
