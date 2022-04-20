/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_compact.h
\brief      Implement functions for compacting the file system.
*/

#ifndef RAFS_COMPACT_H
#define RAFS_COMPACT_H

typedef enum {
    FCS_COMPACT_START   = 0x70,
    FCS_VALID           = 0x7E, /*!< ie the ~ character */
    FCS_SECONDARY_MASTER= 0xFE,
    FCS_ERASED          = 0xFF,
} fat_compact_state;

/**
 * \brief Compacting the FATs involves a number of discrete steps
 *        where each step can potentially take more time than
 *        is reasonable.
 *
 * The steps are
 *  - start the compaction
 *  - erase the secondary FAT
 *  - copy valid directory entries from the primary FAT to the secondary FAT
 *  - set secondary FAT as master
 *  - erase the primary FAT
 *  - copy valid directory entries from the secondary FAT to the primary FAT
 *  - write the bulk of the FAT table (excluding first byte)
 *  - set both FATs to be in a valid state.
 */
typedef enum {
    COMPACT_START,
    COMPACT_ERASE_SECONDARY_FAT,
    COMPACT_COPY_DIRENTS_TO_BACKUP,
    COMPACT_SET_SECONDARY_AS_MASTER,
    COMPACT_ERASE_PRIMARY_FAT,
    COMPACT_COPY_DIRENTS_TO_PRIMARY,
    COMPACT_WRITE_FAT_HEADER_DATA,
    COMPACT_SET_FATS_VALID,
    COMPACT_SEND_COMPLETION_MESSAGE,
    COMPACT_LAST
} rafs_compact_states_t;

typedef struct {
    Task                    cfm_task;
    rafs_compact_states_t   state;
    rafs_errors_t           status;
} compact_work_t;

void Rafs_ProgressCompactWorker(compact_work_t *compact_msg);

void Rafs_CompactRepairFromPrimary(void);

void Rafs_CompactRepairFromSecondary(void);

#endif // RAFS_COMPACT_H
