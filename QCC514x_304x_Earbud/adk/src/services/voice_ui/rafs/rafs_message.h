/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_message.h
\brief      Implements the internal message queue of RAFS.

*/

#ifndef RAFS_MESSAGE_H
#define RAFS_MESSAGE_H

#include "rafs_dirent.h"
#include "rafs_compact.h"

typedef enum {
    MESSAGE_RAFS_MOUNT_WORK = MESSAGE_RAFS_LAST,
    MESSAGE_RAFS_COMPACT_WORK,
    MESSAGE_RAFS_WRITE_WORK,
    MESSAGE_RAFS_REMOVE_WORK,
    MESSAGE_RAFS_IDLE_TIMEOUT,

    /*! This must be the final message */
    MESSAGE_RAFS_MESSAGE_END
} rafs_work_message_id_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(MESSAGE_RAFS_MESSAGE_END)

typedef struct {
    Task        cfm_task;
    uint32      block_to_scan;
} mount_work_t;

typedef struct {
    rafs_file_t file_id;
    const void  *buf;
    rafs_size_t count;
} write_work_t;

typedef struct {
    Task        cfm_task;
    uint32      num;
    inode_t     inodes[MAX_INODES];
} remove_work_t;

/* Worker creation */
/* =============== */
/* Create the initial message to queue to the input queue */
void Rafs_CreateMountWorker(Task cfm_task);
void Rafs_CreateCompactWorker(Task cfm_task);
void Rafs_CreateWriteWorker(rafs_file_t file_id, const void *buf, rafs_size_t num_bytes_to_write);
void Rafs_CreateRemoveWorker(Task cfm_task, const inode_t inodes[], uint32 num);

/* Worker progress */
/* =============== */
/* Perform some incremental update, or send appropriate completion message */
void Rafs_ProgressMountWorker(mount_work_t *mount_msg);
void Rafs_ProgressWriteWorker(write_work_t *write_msg);
void Rafs_ProgressRemoveWorker(remove_work_t *remove_msg);

/*!
 * \brief Rafs_MessageInit
 * Perform the initial setup of the RAFS message handler, and
 * subcriptions to other services.
 */
void Rafs_MessageInit(void);

/*!
 * \brief Rafs_MessageShutdown
 * Cancel subscriptions to other services.
 */
void Rafs_MessageShutdown(void);

#endif /* RAFS_MESSAGE_H */
