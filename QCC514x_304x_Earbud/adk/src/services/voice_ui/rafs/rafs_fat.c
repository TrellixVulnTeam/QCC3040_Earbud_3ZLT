/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_fat.c
\brief      Implement functions which operate on the FAT table of rafs.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_fat.h"
#include "rafs_extent.h"
#include "rafs_directory.h"
#include "rafs_message.h"
#include "rafs_utils.h"

/**
 * \brief The initial state of the root directory entries for the
 *        primary and backup FATs.
 *
 * \note The first character is deliberately chosen as ~, as it has the
 *       most bits set (0x7E = 0111 1110) of any printable character.
 *       This provides the opportunity to record any long duration state
 *       information by clearing bits in the first character of the ident string.
 */
static const dir_entry_t pri_root_dirent = {
    "~QTIL-RAFS-V" RAFS_VERSION,
    DIRECTORY_SIZE,
    {
        /* Age information */
        0, 0
    },
    {
        /* Inode information */
        { 0, 1 },      /* Occupies page 0 */
        { INODE_UNUSED, INODE_UNUSED },
        { INODE_UNUSED, INODE_UNUSED },
    }
};

static const dir_entry_t sec_root_dirent = {
    "~QTIL-RAFS-V" RAFS_VERSION,
    DIRECTORY_SIZE,
    {
        /* Age information */
        0, 0
    },
    {
        /* Inode information */
        { 1, 1 },      /* Occupies page 1 */
        { INODE_UNUSED, INODE_UNUSED },
        { INODE_UNUSED, INODE_UNUSED },
    }
};


static bool rafs_isUnusedInode(const inode_t *inode)
{
    bool is_unused = inode->offset == INODE_UNUSED &&
                     inode->length == INODE_UNUSED;
    bool has_data  = inode->length > 0;
    return is_unused || !has_data;
}

static bool rafs_isDeletedInode(const inode_t *inode)
{
    return inode->offset == INODE_DELETED &&
           inode->length == INODE_DELETED;
}

bool rafs_isValidInode(const inode_t *inode)
{
    return !( rafs_isUnusedInode(inode) ||
              rafs_isDeletedInode(inode) );
}

bool Rafs_IsDeletedEntry(const dir_entry_t *dir_entry)
{
    return dir_entry->filename[0] == '\x00';
}

static bool Rafs_IsUnusedEntry(const dir_entry_t *dir_entry)
{
    return (uint8)(dir_entry->filename[0]) == 0xffu;
}

static bool Rafs_IsValidEntry(const dir_entry_t *dir_entry)
{
    return !( Rafs_IsUnusedEntry(dir_entry) ||
              Rafs_IsDeletedEntry(dir_entry) );
}

static void Rafs_GetIndexAndMask(uint32 block_number, uint32 *idx, uint32 *mask)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32  bits_per_word = sizeof(rafs_self->sector_in_use_map[0]) * CHAR_BIT;
    *idx = block_number / bits_per_word;
    PanicFalse(*idx < rafs_self->sector_in_use_map_length);
    *mask= 1u << (block_number % bits_per_word);
}

void Rafs_SectorMapMarkFreeBlock(uint32 block_number)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 idx, mask;
    Rafs_GetIndexAndMask(block_number, &idx, &mask);
    rafs_self->sector_in_use_map[idx] |= mask;
}

void Rafs_SectorMapMarkUsedBlock(uint32 block_number)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 idx, mask;
    Rafs_GetIndexAndMask(block_number, &idx, &mask);
    rafs_self->sector_in_use_map[idx] &= ~mask;
}

bool Rafs_SectorMapIsBlockInUse(uint32 block_number)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 idx, mask;
    Rafs_GetIndexAndMask(block_number, &idx, &mask);
    return (rafs_self->sector_in_use_map[idx] & mask) == 0;
}

uint16 Rafs_CountFreeSectors(void)
{
    uint16 result = 0;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32  num_blocks = rafs_self->partition->part_info.partition_size / rafs_self->partition->part_info.block_size;
    for(uint32 block = 0 ; block < num_blocks ; block++)
    {
        if( !Rafs_SectorMapIsBlockInUse(block) )
        {
            result++;
        }
    }
    return result;
}


void Rafs_InitialiseSectorMap(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    const uint32 bits_per_word = sizeof(rafs_self->sector_in_use_map[0]) * CHAR_BIT;
    uint32  num_blocks = rafs_self->partition->part_info.partition_size / rafs_self->partition->part_info.block_size;
    rafs_self->sector_in_use_map_length = ( num_blocks + bits_per_word - 1 ) / bits_per_word;

    if( rafs_self->sector_in_use_map )
        free(rafs_self->sector_in_use_map);

    rafs_self->sector_in_use_map = PanicUnlessMalloc(sizeof(*rafs_self->sector_in_use_map) *
                                                     rafs_self->sector_in_use_map_length);

    for(uint32 i = 0 ; i < rafs_self->sector_in_use_map_length ; i++)
        rafs_self->sector_in_use_map[i] = 0;

    for(uint32 i = 0 ; i < num_blocks ; i++)
        Rafs_SectorMapMarkFreeBlock(i);
}



void Rafs_ScanDirectory(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    bool ok = TRUE;

    /* Mark the two FATs as being in use */
    Rafs_AddExtentsToSectorMap(pri_root_dirent.inodes, MAX_INODES);
    Rafs_AddExtentsToSectorMap(sec_root_dirent.inodes, MAX_INODES);

    /* Assume for the moment the directory is completely full. */
    rafs_self->files->free_dir_slot = DIRECTORY_SIZE/sizeof(dir_entry_t);

    for(uint32 d = 1 ; ok && d < DIRECTORY_SIZE/sizeof(dir_entry_t) ; d++)
    {
        if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, d) )
        {
            if( Rafs_IsErasedData(rafs_self->files->scan_dir_entry, sizeof(*rafs_self->files->scan_dir_entry)) )
            {
                /* All FF's means flash erased data, therefore we can use it for a new file. */
                rafs_self->files->free_dir_slot = d;
                break;
            }
            else if( !Rafs_IsDeletedEntry(rafs_self->files->scan_dir_entry) )
            {
                /* not all FF's and not deleted. */
                ok = Rafs_AddExtentsToSectorMap(rafs_self->files->scan_dir_entry->inodes, MAX_INODES);
                if( rafs_self->files->scan_dir_entry->stat_counters.sequence_count > rafs_self->files->sequence_number )
                {
                    rafs_self->files->sequence_number = rafs_self->files->scan_dir_entry->stat_counters.sequence_count;
                }
            }
        }
    }
}

uint16 Rafs_CountActiveFiles(void)
{
    uint16  result = 0;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    for(uint32 d = 1 ; d < rafs_self->files->free_dir_slot ; d++)
    {
        if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, d) )
        {
            if( Rafs_IsValidEntry(rafs_self->files->scan_dir_entry) )
            {
                result++;
            }
        }
    }
    return result;
}

const dir_entry_t *Rafs_GetInitialRoot(block_id_t id)
{
    if( id == PRI_FAT ) return &pri_root_dirent;
    if( id == SEC_FAT ) return &sec_root_dirent;
    Panic();
    return NULL;
}

bool Rafs_WriteRootDirent(block_id_t id)
{
    bool result = FALSE;
    if(id == PRI_FAT)
    {
        dir_entry_t temp = *Rafs_GetInitialRoot(PRI_FAT);
        result = Rafs_DirEntryWrite(PRI_FAT, &temp, ROOT_DIRENT_NUMBER);
    }
    else if( id == SEC_FAT)
    {
        dir_entry_t temp = *Rafs_GetInitialRoot(SEC_FAT);
        result = Rafs_DirEntryWrite(SEC_FAT, &temp, ROOT_DIRENT_NUMBER);
    }
    return result;
}

rafs_errors_t Rafs_DoOpendir(const char *path, rafs_dir_t *dir_id, uint16 *num)
{
    UNUSED(path);
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;

    if( rafs_self->directory )
    {
        *dir_id = INVALID_DIR;
        result = RAFS_DIRECTORY_OPEN;
    }
    else
    {
        *dir_id = VALID_DIR;
        *num = Rafs_CountActiveFiles();

        rafs_self->directory = PanicUnlessMalloc(sizeof(*rafs_self->directory));
        memset(rafs_self->directory, 0, sizeof(*rafs_self->directory));

        rafs_self->directory->readdir_index = 1;
        rafs_self->directory->num_files_present = *num;
        rafs_self->directory->num_files_read = 0;
    }

    return result;
}

rafs_errors_t Rafs_DoReaddir(rafs_dir_t dir_id, rafs_stat_t *stat)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;
    if( dir_id != VALID_DIR || !rafs_self->directory )
    {
        result = RAFS_BAD_ID;
    }
    else if( rafs_self->directory->num_files_present == rafs_self->directory->num_files_read )
    {
        result = RAFS_NO_MORE_FILES;
    }
    else
    {
        bool found = FALSE;
        /* From where we left off last time, read more dirents until the next valid file is found */
        for( ; !found && rafs_self->directory->readdir_index < rafs_self->files->free_dir_slot ; rafs_self->directory->readdir_index++ )
        {
            if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, rafs_self->directory->readdir_index) )
            {
                if( Rafs_IsValidEntry(rafs_self->files->scan_dir_entry) )
                {
                    found = TRUE;
                }
            }
        }
        if( found )
        {
            stat->file_size = rafs_self->files->scan_dir_entry->file_size;
            stat->stat_counters.sequence_count = rafs_self->files->scan_dir_entry->stat_counters.sequence_count;
            /* flip the access count bits, so it counts up from 0, not down from 0xffff */
            stat->stat_counters.access_count = ~rafs_self->files->scan_dir_entry->stat_counters.access_count;
            safe_strncpy(stat->filename, rafs_self->files->scan_dir_entry->filename, RAFS_MAX_FILELEN);
            rafs_self->directory->num_files_read++;
        }
        else
        {
            /* If a file is deleted after opendir, and before readdir has seen it */
            /* then num_files_present is off and num_files_read will not reach the */
            /* anticipated total. */
            result = RAFS_NO_MORE_FILES;
        }
    }

    return result;
}

rafs_errors_t Rafs_DoClosedir(rafs_dir_t dir_id)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;
    if( dir_id != VALID_DIR || !rafs_self->directory )
    {
        result = RAFS_BAD_ID;
    }
    else
    {
        free(rafs_self->directory);
        rafs_self->directory = NULL;
    }

    return result;
}

rafs_errors_t Rafs_DoRemove(Task task, const char *filename)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;
    result = Rafs_IsValidFilename(filename);
    if( result == RAFS_OK )
    {
        uint32  dir_slot;
        if( Rafs_FindDirEntryByFilename(filename, &dir_slot) )
        {
            if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, dir_slot) )
            {
                rafs_self->files->compact_is_needed = TRUE;
                rafs_self->files->scan_dir_entry->filename[0] = '\x00';  /* Wipe the first character */
                bool primaryOK   = Rafs_DirEntryWriteMember(PRI_FAT, &rafs_self->files->scan_dir_entry->filename[0], dir_slot,
                                                    offsetof(dir_entry_t,filename), sizeof(char));
                bool secondaryOK = Rafs_DirEntryWriteMember(SEC_FAT, &rafs_self->files->scan_dir_entry->filename[0], dir_slot,
                                                    offsetof(dir_entry_t,filename), sizeof(char));
                if( primaryOK && secondaryOK )
                {
                    Rafs_RemoveExtentsFromSectorMap(rafs_self->files->scan_dir_entry->inodes, MAX_INODES);
                    rafs_self->busy = TRUE;
                    uint32 valid_inodes = 0;
                    for(uint32 i = 0 ; i < MAX_INODES; i++)
                    {
                        PanicFalse(!rafs_isDeletedInode(&rafs_self->files->scan_dir_entry->inodes[i]));
                        if( !rafs_isUnusedInode(&rafs_self->files->scan_dir_entry->inodes[i]))
                        {
                            valid_inodes++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    Rafs_CreateRemoveWorker(task, rafs_self->files->scan_dir_entry->inodes, valid_inodes);
                }
                else
                {
                    result = RAFS_INVALID_FAT;
                }
            }
        }
        else
        {
            result = RAFS_FILE_NOT_FOUND;
        }
    }
    return result;
}
