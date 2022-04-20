/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_integrity.c
\brief      Performs integrity checks on the filesystem.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <stddef.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_compact.h"
#include "rafs_directory.h"
#include "rafs_fat.h"
#include "rafs_integrity.h"
#include "rafs_utils.h"

/*!
  * Each dirent goes through the progression of unused, used, deleted.
  */
typedef enum {
    DT_UNKNOWN, /*!< It's unknown what this dirent contains (fatal) */
    DT_ERASED,  /*!< This contains an unused entry */
    DT_FILE,    /*!< This contains a valid file */
    DT_DELETED, /*!< This contains a deleted file */
} dirent_type;

static dirent_type Rafs_ClassifyDirent(char ch)
{
    if( (uint8)ch == 0xffu ) return DT_ERASED;
    else if( ch == 0x00 ) return DT_DELETED;
    else if( Rafs_IsValidFilenameCharacter(ch) ) return DT_FILE;
    return DT_UNKNOWN;
}

/*
 * This function checks for interrupted file(write) closing and file removal.
 * Basically, it looks for inconsistencies between the primary and secondary
 * FAT entries for each file.
 */
static rafs_errors_t Rafs_CheckDirectoryEntries(void)
{
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    bool ok = TRUE;

    for(uint32 d = 1 ; ok && d < DIRECTORY_SIZE/sizeof(dir_entry_t) ; d++)
    {
        char pri_ch = '\0', sec_ch = '\0';
        bool pri_ok = Rafs_DirEntryReadMember(PRI_FAT, &pri_ch, d, offsetof(dir_entry_t,filename), sizeof(pri_ch));
        bool sec_ok = Rafs_DirEntryReadMember(SEC_FAT, &sec_ch, d, offsetof(dir_entry_t,filename), sizeof(sec_ch));
        if( pri_ok && sec_ok )
        {
            dirent_type pri_type = Rafs_ClassifyDirent(pri_ch);
            dirent_type sec_type = Rafs_ClassifyDirent(sec_ch);
            if( pri_type == DT_ERASED && sec_type == DT_ERASED )
            {
                break;
            }
            else if( pri_type == DT_UNKNOWN || sec_type == DT_UNKNOWN )
            {
                DEBUG_LOG_ALWAYS("Rafs_CheckDirectoryEntries: Entry:%lu, unknown chars %x,%x", d, pri_ch, sec_ch);
                result = RAFS_INVALID_FAT;
                ok = FALSE;
            }
            else
            {
                if( pri_type == DT_ERASED )
                {
                    if( sec_type != DT_ERASED )
                    {
                        /* Secondary FAT is unusable at this point */
                        DEBUG_LOG_ALWAYS("Rafs_CheckDirectoryEntries: Entry:%lu, secondary FAT improperly erased", d);
                        result = RAFS_INVALID_FAT;
                        ok = FALSE;
                    }
                }
                else if( pri_type == DT_FILE )
                {
                    if( sec_type == DT_ERASED )
                    {
                        /* Expect this from interrupted file close (write) */
                        /* Recovery is to copy the primary to the secondary */
                        DEBUG_LOG_DEBUG("Rafs_CheckDirectoryEntries: Entry:%lu, secondary FAT caught up to primary(write) - OK", d);
                        ok = Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, d);
                        ok = ok && Rafs_DirEntryWrite(SEC_FAT, rafs_self->files->scan_dir_entry, d);
                    }
                    else if( sec_type == DT_DELETED )
                    {
                        DEBUG_LOG_ALWAYS("Rafs_CheckDirectoryEntries: Entry:%lu, secondary FAT improperly deleted", d);
                        result = RAFS_INVALID_FAT;
                        ok = FALSE;
                    }
                }
                else if( pri_type == DT_DELETED )
                {
                    if( sec_type != DT_DELETED )
                    {
                        /* Repair backup FAT by marking the file as also deleted */
                        /* Expect this from interrupted file removal */
                        DEBUG_LOG_DEBUG("Rafs_CheckDirectoryEntries: Entry:%lu, secondary FAT caught up to primary(remove) - OK", d);
                        ok = Rafs_DirEntryWriteMember(SEC_FAT, &pri_ch, d, offsetof(dir_entry_t,filename), sizeof(pri_ch));
                    }
                }
            }
        }
        else
        {
            result = RAFS_INVALID_FAT;
            ok = FALSE;
        }
    }

    return result;
}

/*
 * This function checks for interrupted compaction.
 * Essentially, this means checking the two ident bytes at the start
 * of both FATs, which are the values in \see fat_compact_state
 *
 * Writing a single byte to flash is as atomic as it's going to get, so
 * we can be reasonably sure that these single byte values mark specific
 * known points in the compaction state machine.
 */
static bool Rafs_CheckAndRepairFileAllocationTables(void)
{
    /* The first pass should fix all the issues */
    /* The second pass should trivally exit because both FATs are valid */
    /* If the first pass repair turns out to be unsuccessful, then return failure */
    for(int pass = 0 ; pass < 2 ; pass++ )
    {
        DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: pass %d", pass);
        uint8 pri_status = 0;
        uint8 sec_status = 0;
        bool pri_ok = Rafs_DirEntryReadMember(PRI_FAT, &pri_status, ROOT_DIRENT_NUMBER,
                                offsetof(dir_entry_t,filename), sizeof(pri_status));
        bool sec_ok = Rafs_DirEntryReadMember(SEC_FAT, &sec_status, ROOT_DIRENT_NUMBER,
                                offsetof(dir_entry_t,filename), sizeof(sec_status));
        if( !(pri_ok && sec_ok) )
        {
            DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: read failed(%d,%d)",
                            pri_ok, sec_ok);
            /* Not formatted at all */
            return FALSE;
        }
        /* cases */
        if( pri_status == FCS_VALID && sec_status == FCS_VALID )
        {
            DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: All OK");
            /* Compact was not in progress,
             * or completed,
             * or was interrupted before any changes were made to flash */
            return TRUE;
        }
        else if( pri_status == FCS_ERASED && sec_status == FCS_ERASED )
        {
            DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: Not formatted");
            /* Not formatted at all */
            return FALSE;
        }
        else if( pri_status == FCS_COMPACT_START && (sec_status == FCS_VALID || sec_status == FCS_ERASED) )
        {
            /* Compact just started, primary is still master, and secondary is in some unknown state */
            DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: RepairFromPrimary");
            Rafs_CompactRepairFromPrimary();
        }
        else if( sec_status == FCS_SECONDARY_MASTER )
        {
            /* Compact is about half-way through, secondary is the master and the primary is in an unknown state */
            DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: RepairFromSecondary");
            Rafs_CompactRepairFromSecondary();
        }
        else if( pri_status == FCS_ERASED && sec_status == FCS_VALID )
        {
            /* Just missing the final step of marking primary as valid */
            uint8 value = FCS_VALID;
            DEBUG_LOG_DEBUG("Rafs_CheckAndRepairFileAllocationTables: Fix ident");
            Rafs_DirEntryWriteMember(PRI_FAT, &value, ROOT_DIRENT_NUMBER,
                                     offsetof(dir_entry_t,filename), sizeof(value) );
        }
    }
    DEBUG_LOG_ALWAYS("Rafs_CheckAndRepairFileAllocationTables");
    return FALSE;
}

static bool Rafs_CheckFileAllocationTables(void)
{
    dir_entry_t temp = { 0 };
    const dir_entry_t *pri_root = Rafs_GetInitialRoot(PRI_FAT);
    const dir_entry_t *sec_root = Rafs_GetInitialRoot(SEC_FAT);
    bool primaryOK   = Rafs_DirEntryRead(PRI_FAT, &temp, ROOT_DIRENT_NUMBER) &&
                       memcmp(&temp, pri_root, sizeof(temp)) == 0;
    bool secondaryOK = Rafs_DirEntryRead(SEC_FAT, &temp, ROOT_DIRENT_NUMBER) &&
                       memcmp(&temp, sec_root, sizeof(temp)) == 0;

    if( primaryOK && secondaryOK )
    {
        return TRUE;
    }
    else
    {
        /* One of the two FATs is in an undefined state due to compaction */
        /* or the partition is just unformatted */
        bool result = Rafs_CheckAndRepairFileAllocationTables();
        return result;
    }
}

rafs_errors_t Rafs_CheckIntegrity(void)
{
    rafs_errors_t result = RAFS_OK;
    if( Rafs_CheckFileAllocationTables() )
    {
        result = Rafs_CheckDirectoryEntries();
    }
    else
    {
        result = RAFS_INVALID_FAT;
    }
    return result;
}
