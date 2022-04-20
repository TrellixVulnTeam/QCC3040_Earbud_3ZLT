/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_compact.c
\brief      Implement functions for compacting the file system.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_message.h"
#include "rafs_compact.h"
#include "rafs_directory.h"
#include "rafs_fat.h"
#include "rafs_utils.h"

/* A structure to associate a FAT index position with a sequence number */
/* At the moment, sequence numbers are always in FAT table order, but */
/* advanced compaction will re-write FAT entries and break this implied assumption */
/* So this is more for the future, rather than now. */
struct resequence {
    uint16      fat_index;
    uint16      sequence_number;
};

/* Compact worker helper functions */
/* ------------------------------- */
static uint32 Rafs_GetNumberOfDirEntries(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 limit = rafs_self->files->free_dir_slot;
    if( limit == 0 )
    {
        /* In case of a restarted compact from boot */
        /* we have to scan all the directory entries */
        /* and rely on \see Rafs_IsErasedData as the */
        /* alternative way out of loops. */
        limit = DIRECTORY_SIZE/sizeof(dir_entry_t);
    }
    return limit;
}


static uint16 Rafs_CountValidFiles(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint16 result = 0;
    uint32 limit = Rafs_GetNumberOfDirEntries();
    for(uint32 i = 1 ; i < limit ; i++)
    {
        if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, i) )
        {
            if( Rafs_IsErasedData(rafs_self->files->scan_dir_entry, sizeof(*rafs_self->files->scan_dir_entry)) )
            {
                break;
            }
            if( !Rafs_IsDeletedEntry(rafs_self->files->scan_dir_entry) )
            {
                result++;
            }
        }
    }
    return result;
}

static int Rafs_SortSequenceNumberAscending(const void *pa, const void *pb)
{
    const struct resequence *a = pa;
    const struct resequence *b = pb;
    if( a->sequence_number < b->sequence_number ) return -1;
    if( a->sequence_number > b->sequence_number ) return +1;
    return 0;
}

static int Rafs_SortFatIndexAscending(const void *pa, const void *pb)
{
    const struct resequence *a = pa;
    const struct resequence *b = pb;
    if( a->fat_index < b->fat_index ) return -1;
    if( a->fat_index > b->fat_index ) return +1;
    return 0;
}

static void Rafs_GetNormalisedSequenceNumbers(struct resequence *resequence, uint16 n_files)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 n = 0;
    uint32 limit = Rafs_GetNumberOfDirEntries();
    for(uint32 i = 1 ; n < n_files && i < limit ; i++)
    {
        if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, i) )
        {
            if( Rafs_IsErasedData(rafs_self->files->scan_dir_entry, sizeof(*rafs_self->files->scan_dir_entry)) )
            {
                break;
            }
            if( !Rafs_IsDeletedEntry(rafs_self->files->scan_dir_entry) )
            {
                resequence[n].fat_index = i;
                resequence[n].sequence_number = rafs_self->files->scan_dir_entry->stat_counters.sequence_count;
                n++;
            }
        }
    }
    PanicFalse(n == n_files);

    /* Example */
    /* Three directory entries[n] have sequence numbers [0]4 [1]11 [2]7 */

    /* Order by sequence number, so they can be modified whilst */
    /* preserving the relative ordering */
    /* [0]4 [2]7 [1]11 */
    qsort(resequence, n, sizeof(*resequence), Rafs_SortSequenceNumberAscending);

    /* Normalise the sequence numbers */
    /* [0]1 [2]2 [1]3 */
    for(uint16 i = 0 ; i < n_files ; i++)
    {
        /* Sequence IDs start from 1 */
        resequence[i].sequence_number = i + 1;
    }

    /* When the directory is read again to copy it, this array can */
    /* just be read in sequential order */
    /* [0]1 [1]3 [2]2 */
    qsort(resequence, n, sizeof(*resequence), Rafs_SortFatIndexAscending);
}

/* Compaction state transitions and actions */
/* ======================================== */

/* STATE COMPACT_START */
static bool Rafs_CompactWorkerStartAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerStartAction");
    char ch = FCS_COMPACT_START;
    return Rafs_DirEntryWriteMember(PRI_FAT, &ch, ROOT_DIRENT_NUMBER, offsetof(dir_entry_t,filename), sizeof(ch));
}
static void Rafs_SendNextState(compact_work_t *compact_msg, bool ok, rafs_compact_states_t next_state)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    MESSAGE_MAKE(msg,compact_work_t);
    *msg = *compact_msg;
    if( ok )
    {
        msg->state = next_state;
    }
    else
    {
        /* On error, jump to the last state, and flag an error */
        msg->state = COMPACT_SEND_COMPLETION_MESSAGE;
        msg->status = RAFS_FAT_WRITE_FAILED;
    }
    MessageSend(&rafs_self->task, MESSAGE_RAFS_COMPACT_WORK, msg);
}
static void Rafs_CompactWorkerStart(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerStartAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_ERASE_SECONDARY_FAT);
}

/* STATE COMPACT_ERASE_SECONDARY_FAT */
static bool Rafs_CompactWorkerEraseBackupFatAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerEraseBackupFatAction");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    raPartition_result ra_result = RaPartitionErase(&rafs_self->partition->part_handle,
                                                    SEC_FAT*rafs_self->partition->part_info.block_size);
    if( ra_result != RA_PARTITION_RESULT_SUCCESS )
        DEBUG_LOG_ALWAYS("Rafs_CompactWorkerEraseBackupFatAction = %d", ra_result);
    return ra_result == RA_PARTITION_RESULT_SUCCESS;
}
static void Rafs_CompactWorkerEraseBackupFat(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerEraseBackupFatAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_COPY_DIRENTS_TO_BACKUP);
}

/* STATE COMPACT_COPY_DIRENTS_TO_BACKUP */
static bool Rafs_CompactWorkerCopyToBackupAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerCopyToBackupAction");
    bool ok = TRUE;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();

    uint16 n_files = Rafs_CountValidFiles();
    struct resequence *resequence = PanicUnlessMalloc(n_files * sizeof(*resequence));
    Rafs_GetNormalisedSequenceNumbers(resequence, n_files);

    /* Copy the valid directory entries to the backup FAT */
    /* This does all the hard work of filtering out deleted files */
    /* and rebasing all the sequence counts of the files */
    uint32 j = 1;
    uint32 limit = Rafs_GetNumberOfDirEntries();
    for(uint32 i = 1 ; ok && i < limit ; i++)
    {
        if( Rafs_DirEntryRead(PRI_FAT, rafs_self->files->scan_dir_entry, i) )
        {
            if( Rafs_IsErasedData(rafs_self->files->scan_dir_entry, sizeof(*rafs_self->files->scan_dir_entry)) )
            {
                break;
            }
            if( !Rafs_IsDeletedEntry(rafs_self->files->scan_dir_entry) )
            {
                rafs_self->files->scan_dir_entry->stat_counters.sequence_count = resequence[j-1].sequence_number;
                ok = Rafs_DirEntryWrite(SEC_FAT, rafs_self->files->scan_dir_entry, j);
                j++;
            }
        }
    }
    if( ok )
    {
        PanicFalse(j-1 == n_files);

        if( n_files > 0 )
        {
            rafs_self->files->sequence_number = resequence[n_files-1].sequence_number;
        }
        rafs_self->files->free_dir_slot = j;
    }
    free(resequence);
    return ok;
}
static void Rafs_CompactWorkerCopyToBackup(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerCopyToBackupAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_SET_SECONDARY_AS_MASTER);
}

/* STATE COMPACT_SET_SECONDARY_AS_MASTER */
static bool Rafs_CompactWorkerSecondaryIsMasterAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerSecondaryIsMasterAction");
    char ch = FCS_SECONDARY_MASTER;
    return Rafs_DirEntryWriteMember(SEC_FAT, &ch, ROOT_DIRENT_NUMBER, offsetof(dir_entry_t,filename), sizeof(ch));
}
static void Rafs_CompactWorkerSecondaryIsMaster(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerSecondaryIsMasterAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_ERASE_PRIMARY_FAT);
}

/* STATE COMPACT_ERASE_PRIMARY_FAT */
static bool Rafs_CompactWorkerErasePrimaryFatAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerErasePrimaryFatAction");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    raPartition_result ra_result = RaPartitionErase(&rafs_self->partition->part_handle,
                                                    PRI_FAT*rafs_self->partition->part_info.block_size);
    if( ra_result != RA_PARTITION_RESULT_SUCCESS )
        DEBUG_LOG_ALWAYS("Rafs_CompactWorkerErasePrimaryFatAction = %d", ra_result);
    return ra_result == RA_PARTITION_RESULT_SUCCESS;
}
static void Rafs_CompactWorkerErasePrimaryFat(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerErasePrimaryFatAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_COPY_DIRENTS_TO_PRIMARY);
}

/* STATE COMPACT_COPY_DIRENTS_TO_PRIMARY */
static bool Rafs_CompactWorkerCopyToPrimaryAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerCopyToPrimaryAction");
    bool ok = TRUE;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 limit = Rafs_GetNumberOfDirEntries();
    for(uint32 i = 1 ; ok && i < limit ; i++)
    {
        if( Rafs_DirEntryRead(SEC_FAT, rafs_self->files->scan_dir_entry, i) )
        {
            if( Rafs_IsErasedData(rafs_self->files->scan_dir_entry, sizeof(*rafs_self->files->scan_dir_entry)) )
            {
                break;
            }
            ok = Rafs_DirEntryWrite(PRI_FAT, rafs_self->files->scan_dir_entry, i);
        }
    }
    return ok;
}
static void Rafs_CompactWorkerCopyToPrimary(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerCopyToPrimaryAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_WRITE_FAT_HEADER_DATA);
}

/* STATE COMPACT_WRITE_FAT_HEADER_DATA */
static bool Rafs_CompactWorkerWriteFatHeaderdataAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerWriteFatHeaderdataAction");
    dir_entry_t temp = *Rafs_GetInitialRoot(PRI_FAT);
    bool r1 = Rafs_DirEntryPartWrite(PRI_FAT, &temp, ROOT_DIRENT_NUMBER, DIRENT_WRITE_TYPE_DATA);

    temp = *Rafs_GetInitialRoot(SEC_FAT);
    bool r2 = Rafs_DirEntryPartWrite(SEC_FAT, &temp, ROOT_DIRENT_NUMBER, DIRENT_WRITE_TYPE_DATA);
    if( !(r1 && r2) )
    {
        DEBUG_LOG_ALWAYS("Rafs_CompactWorkerWriteFatHeaderdataAction");
    }
    return r1 && r2;
}
static void Rafs_CompactWorkerWriteFatHeaderdata(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerWriteFatHeaderdataAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_SET_FATS_VALID);
}

/* STATE COMPACT_SET_FATS_VALID */
static bool Rafs_CompactWorkerSetFatsValidAction(void)
{
    DEBUG_LOG_FN_ENTRY("Rafs_CompactWorkerSetFatsValidAction");
    dir_entry_t temp = *Rafs_GetInitialRoot(SEC_FAT);
    bool r2 = Rafs_DirEntryPartWrite(SEC_FAT, &temp, ROOT_DIRENT_NUMBER, DIRENT_WRITE_TYPE_FIRST);

    /* Marking the primary FAT valid is the very last step */
    temp = *Rafs_GetInitialRoot(PRI_FAT);
    bool r1 = Rafs_DirEntryPartWrite(PRI_FAT, &temp, ROOT_DIRENT_NUMBER, DIRENT_WRITE_TYPE_FIRST);

    if( !(r1 && r2) )
    {
        DEBUG_LOG_ALWAYS("Rafs_CompactWorkerSetFatsValidAction");
    }
    return r1 && r2;
}
static void Rafs_CompactWorkerSetFatsValid(compact_work_t *compact_msg)
{
    bool ok = Rafs_CompactWorkerSetFatsValidAction();
    Rafs_SendNextState(compact_msg, ok, COMPACT_SEND_COMPLETION_MESSAGE);
}

/* STATE COMPACT_SEND_COMPLETION_MESSAGE */
static void Rafs_CompactSendCompletionMessage(compact_work_t *compact_msg)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    if( compact_msg->cfm_task )
    {
        MESSAGE_MAKE(cfm,rafs_compact_complete_cfm);
        cfm->status = compact_msg->status;
        if( cfm->status == RAFS_OK )
        {
            cfm->num_free_slots = MAX_DIR_ENTRIES - rafs_self->files->free_dir_slot;
        }
        else
        {
            cfm->num_free_slots = 0;
        }
        MessageSend(compact_msg->cfm_task, MESSAGE_RAFS_COMPACT_COMPLETE, cfm);
    }
    Rafs_FreeScanDirent();
    rafs_self->busy = FALSE;
}

void Rafs_ProgressCompactWorker(compact_work_t *compact_msg)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_ProgressCompactWorker - state=%d", compact_msg->state);
    switch( compact_msg->state )
    {
        case COMPACT_START:
            Rafs_CompactWorkerStart(compact_msg);
            break;
        case COMPACT_ERASE_SECONDARY_FAT:
            Rafs_CompactWorkerEraseBackupFat(compact_msg);
            break;
        case COMPACT_COPY_DIRENTS_TO_BACKUP:
            Rafs_CompactWorkerCopyToBackup(compact_msg);
            break;
        case COMPACT_SET_SECONDARY_AS_MASTER:
            Rafs_CompactWorkerSecondaryIsMaster(compact_msg);
            break;
        case COMPACT_ERASE_PRIMARY_FAT:
            Rafs_CompactWorkerErasePrimaryFat(compact_msg);
            break;
        case COMPACT_COPY_DIRENTS_TO_PRIMARY:
            Rafs_CompactWorkerCopyToPrimary(compact_msg);
            break;
        case COMPACT_WRITE_FAT_HEADER_DATA:
            Rafs_CompactWorkerWriteFatHeaderdata(compact_msg);
            break;
        case COMPACT_SET_FATS_VALID:
            Rafs_CompactWorkerSetFatsValid(compact_msg);
            break;
        case COMPACT_SEND_COMPLETION_MESSAGE:
            Rafs_CompactSendCompletionMessage(compact_msg);
            break;
        default:
            break;
    }
}

void Rafs_CompactRepairFromPrimary(void)
{
    Rafs_CompactWorkerEraseBackupFatAction();
    Rafs_CompactWorkerCopyToBackupAction();
    Rafs_CompactWorkerSecondaryIsMasterAction();
    Rafs_CompactRepairFromSecondary();
}

void Rafs_CompactRepairFromSecondary(void)
{
    Rafs_CompactWorkerErasePrimaryFatAction();
    Rafs_CompactWorkerCopyToPrimaryAction();
    Rafs_CompactWorkerWriteFatHeaderdataAction();
    Rafs_CompactWorkerSetFatsValidAction();
}
