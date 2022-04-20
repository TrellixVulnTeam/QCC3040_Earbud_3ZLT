/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_directory.c
\brief      Implement directory level accesses to rafs.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <ra_partition_api.h>
#include <string.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_utils.h"
#include "rafs_directory.h"

bool Rafs_DirEntryReadMember(block_id_t id, void *data, uint32 entryNumber, uint32 member_offset, uint32 member_size)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 offset = (DIRECTORY_SIZE * id) + (entryNumber * RAFS_SIZEOF(dir_entry_t)) + member_offset;
    raPartition_result ra_result = Rafs_PartRead(&rafs_self->partition->part_handle, offset, member_size, data);
    if( ra_result != RA_PARTITION_RESULT_SUCCESS )
        DEBUG_LOG_ALWAYS("Rafs_DirentReadMember(%d,%d)=%d", id, entryNumber, ra_result);
    return ra_result == RA_PARTITION_RESULT_SUCCESS;
}

bool Rafs_DirEntryWriteMember(block_id_t id, const void *data, uint32 entryNumber, uint32 member_offset, uint32 member_size)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 offset = (DIRECTORY_SIZE * id) + (entryNumber * RAFS_SIZEOF(dir_entry_t)) + member_offset;
    raPartition_result ra_result = Rafs_PartWrite(&rafs_self->partition->part_handle, offset, member_size, data);
    if( ra_result != RA_PARTITION_RESULT_SUCCESS )
        DEBUG_LOG_ALWAYS("Rafs_DirentWriteMember(%d,%d)=%d", id, entryNumber, ra_result);
    return ra_result == RA_PARTITION_RESULT_SUCCESS;
}

bool Rafs_DirEntryRead(block_id_t id, dir_entry_t *dir_entry, uint32 entryNumber)
{
    return Rafs_DirEntryReadMember(id, dir_entry, entryNumber, 0, RAFS_SIZEOF(*dir_entry));
}

bool Rafs_DirEntryWrite(block_id_t id, const dir_entry_t *dir_entry, uint32 entryNumber)
{
    /* Write everything except the first char, then the first char by itself */
    bool r1 = Rafs_DirEntryPartWrite(id, dir_entry, entryNumber, DIRENT_WRITE_TYPE_DATA);
    bool r2 = Rafs_DirEntryPartWrite(id, dir_entry, entryNumber, DIRENT_WRITE_TYPE_FIRST);
    return r1 && r2;
}

bool Rafs_DirEntryPartWrite(block_id_t id, const dir_entry_t *dir_entry, uint32 entryNumber, dirent_write_type_t type)
{
    uint32 offset = 0, length = 0;
    const uint8 *ptr = (const uint8 *)dir_entry;
    switch( type )
    {
        case DIRENT_WRITE_TYPE_ALL:
            offset = 0;
            length = RAFS_SIZEOF(dir_entry_t);
        break;
        case DIRENT_WRITE_TYPE_FIRST:
            offset = 0;
            length = 1;
        break;
        case DIRENT_WRITE_TYPE_DATA:
            offset = 1;
            length = RAFS_SIZEOF(dir_entry_t)-1;
        break;
        default:
            Panic();
        break;
    }
    return Rafs_DirEntryWriteMember(id, ptr+offset, entryNumber, offset, length);
}

open_file_t *Rafs_FindOpenFileInstanceById(rafs_file_t file_id)
{
    open_file_t *result = NULL;
    if( file_id >= 0 && file_id < RAFS_MAX_OPEN_FILES )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( rafs_self->files->open_files[file_id] )
        {
            result = rafs_self->files->open_files[file_id];
        }
    }
    return result;
}

open_file_t *Rafs_FindOpenFileInstanceByName(const char *filename)
{
    open_file_t *result = NULL;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    for( int i = 0 ; result == NULL && i < RAFS_MAX_OPEN_FILES ; i++ )
    {
        if( rafs_self->files->open_files[i] )
        {
            if( safe_strncmp(rafs_self->files->open_files[i]->file_dir_entry.filename,
                             filename,
                             RAFS_MAX_FILELEN) == 0 )
            {
                result = rafs_self->files->open_files[i];
            }
        }
    }
    return result;
}

rafs_file_t Rafs_FindFreeFileSlot(void)
{
    rafs_file_t result = RAFS_INVALID_FD;
    for(rafs_file_t i = 0 ; i < RAFS_MAX_OPEN_FILES ; i++)
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( !rafs_self->files->open_files[i] )
        {
            result = i;
            break;
        }
    }
    return result;
}

bool Rafs_FindDirEntryByFilename(const char *filename, uint32 *dir_slot)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    bool found = FALSE;
    for(uint32 i = 1 ; !found && i < rafs_self->files->free_dir_slot ; i++)
    {
        if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, i) )
        {
            found = safe_strncmp(rafs_self->files->scan_dir_entry->filename, filename, RAFS_MAX_FILELEN) == 0;
            if( found )
                *dir_slot = i;
        }
    }
    return found;
}

void Rafs_MakeNewDirEntryForFilename(dir_entry_t *result, const char *filename, inode_t *first_inode)
{
    memset(result->filename, 0, sizeof(result->filename));
    safe_strncpy(result->filename, filename, RAFS_MAX_FILELEN);
    result->file_size = 0;
    result->inodes[0] = *first_inode;

    /* Any unused inodes will still be usable after being written to flash */
    for( uint16 i = 1 ; i < MAX_INODES ; i++ )
    {
        result->inodes[i].offset = INODE_UNUSED;
        result->inodes[i].length = INODE_UNUSED;
    }
}
