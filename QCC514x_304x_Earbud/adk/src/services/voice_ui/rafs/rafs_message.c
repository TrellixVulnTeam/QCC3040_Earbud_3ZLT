/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_message.c
\brief      Implements the internal message queue of RAFS.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <ra_partition_api.h>
#include <string.h>

#include "charger_monitor.h"
#include "battery_region.h"

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_message.h"
#include "rafs_directory.h"
#include "rafs_fat.h"
#include "rafs_power.h"
#include "rafs_utils.h"

LOGGING_PRESERVE_MESSAGE_TYPE(rafs_work_message_id_t)

#define RAFS_IDLE_TIMEOUT()   D_MIN(10)

void Rafs_CreateMountWorker(Task cfm_task)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_CreateMountWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    MESSAGE_MAKE(mount_msg,mount_work_t);
    mount_msg->cfm_task = cfm_task;
    mount_msg->block_to_scan = PRI_FAT;
    MessageSend(&rafs_self->task, MESSAGE_RAFS_MOUNT_WORK, mount_msg);
}

void Rafs_CreateCompactWorker(Task cfm_task)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_CreateCompactWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    MESSAGE_MAKE(compact_msg,compact_work_t);
    compact_msg->cfm_task = cfm_task;
    compact_msg->status = RAFS_OK;
    if( rafs_self->files->free_dir_slot > 1 )
    {
        compact_msg->state = COMPACT_START;
    }
    else
    {
        /* It's an empty file system, there is nothing to do. */
        compact_msg->state = COMPACT_SEND_COMPLETION_MESSAGE;
    }
    MessageSend(&rafs_self->task, MESSAGE_RAFS_COMPACT_WORK, compact_msg);
}

void Rafs_CreateWriteWorker(rafs_file_t file_id, const void *buf, rafs_size_t num_bytes_to_write)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_CreateWriteWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    MESSAGE_MAKE(write_msg,write_work_t);
    write_msg->file_id = file_id;
    write_msg->buf = buf;
    write_msg->count = num_bytes_to_write;
    MessageSend(&rafs_self->task, MESSAGE_RAFS_WRITE_WORK, write_msg);
}

void Rafs_CreateRemoveWorker(Task cfm_task, const inode_t inodes[], uint32 num)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_CreateRemoveWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    MESSAGE_MAKE(remove_msg,remove_work_t);
    remove_msg->cfm_task = cfm_task;
    remove_msg->num = num;
    if( remove_msg->num > MAX_INODES )
        remove_msg->num = MAX_INODES;
    memcpy(remove_msg->inodes, inodes, remove_msg->num*sizeof(inode_t));
    MessageSend(&rafs_self->task, MESSAGE_RAFS_REMOVE_WORK, remove_msg);
}

/*!
 * \brief Clean a given flash block
 * \param block  The block to clean
 * \return TRUE if block was already clean, FALSE if erase is needed
 *
 * \note From reading the data sheet for the Winbond W25Q64FW, the read performance
 *       is around 3 orders of magnitude quicker than the erase performance.
 *       Whether that performance is maintained across the P0-P1 bridge in
 *       relatively small transfer sizes remains to be seen.
 *       But it would seem better to test a block to see if it needs erasing
 *       rather than blindly assuming it always does.
 *       Also, erasure draws more power over a longer period of time, so it's
 *       not something that should be done unless necessary.
 */
static bool Rafs_IsCleanBlock(uint32 block)
{
    char buff[64];      /* Limited stack space, pmalloc isn't much better either */
    bool isClean = TRUE;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32 num_partition_blocks = rafs_self->partition->part_info.partition_size / rafs_self->partition->part_info.block_size;
    uint32 offset = block * rafs_self->partition->part_info.block_size;

    PanicFalse( block < num_partition_blocks );

    /* Check a block, and exit early if erase is known to be necessary */
    for( uint32 i = 0 ; isClean && i < rafs_self->partition->part_info.block_size/RAFS_SIZEOF(buff); i++ )
    {
        if( Rafs_PartRead(&rafs_self->partition->part_handle, offset + i * RAFS_SIZEOF(buff),
                          RAFS_SIZEOF(buff), buff) == RA_PARTITION_RESULT_SUCCESS )
        {
            isClean = Rafs_IsErasedData(buff, RAFS_SIZEOF(buff));
        }
    }
    return isClean;
}

/**
 * \brief Work through the list of unused blocks, and erase
 * the first unclean block found, and return.
 * At most one block will be cleaned per call to this function.
 * \return the block number of the block that was cleaned, or last if
 * there are no more blocks to examine.
 */
static uint32 Rafs_CleanNextBlock(uint32 first, uint32 last)
{
    do {
        if( !Rafs_SectorMapIsBlockInUse(first) )
        {
            if( !Rafs_IsCleanBlock(first) )
            {
                rafs_instance_t *rafs_self = Rafs_GetTaskData();
                uint32 offset = first * rafs_self->partition->part_info.block_size;
                RaPartitionBgErase(&rafs_self->partition->part_handle, offset);
                break;
            }
        }
        first++;
    } while ( first < last );
    return first;
}

void Rafs_ProgressMountWorker(mount_work_t *mount_msg)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_ProgressMountWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    uint32  num_blocks = rafs_self->partition->part_info.partition_size / rafs_self->partition->part_info.block_size;
    bool completed = TRUE;
    if( mount_msg->block_to_scan < num_blocks )
    {
        uint32  last_cleaned = Rafs_CleanNextBlock(mount_msg->block_to_scan, num_blocks);
        if( last_cleaned != num_blocks )
        {
            /* Resume later with cleaning the next block */
            MESSAGE_MAKE(msg,mount_work_t);
            *msg = *mount_msg;
            msg->block_to_scan = last_cleaned + 1;
            MessageSend(&rafs_self->task, MESSAGE_RAFS_MOUNT_WORK, msg);
            completed = FALSE;
        }
    }

    if( completed )
    {
        if( mount_msg->cfm_task )
        {
            MESSAGE_MAKE(cfm,rafs_mount_complete_cfm);
            cfm->status = RAFS_OK;
            MessageSend(mount_msg->cfm_task, MESSAGE_RAFS_MOUNT_COMPLETE, cfm);
        }
        Rafs_FreeScanDirent();
        rafs_self->busy = FALSE;
    }
}


void Rafs_ProgressWriteWorker(write_work_t *write_msg)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_ProgressWriteWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_size_t write_count;
    rafs_errors_t result = Rafs_Write(write_msg->file_id, write_msg->buf, write_msg->count, &write_count);

    if( rafs_self->files->open_files[write_msg->file_id]->task )
    {
        MESSAGE_MAKE(cfm,rafs_write_complete_cfm);
        cfm->result = result;
        cfm->file_id = write_msg->file_id;
        cfm->buf = write_msg->buf;
        cfm->request_count = write_msg->count;
        cfm->write_count = write_count;
        MessageSend(rafs_self->files->open_files[write_msg->file_id]->task, MESSAGE_RAFS_WRITE_COMPLETE, cfm);
    }
}

void Rafs_ProgressRemoveWorker(remove_work_t *remove_msg)
{
    DEBUG_LOG_FN_ENTRY("Do Rafs_ProgressRemoveWorker");
    rafs_instance_t *rafs_self = Rafs_GetTaskData();

    if( remove_msg->num > 0 )
    {
        uint32  n = remove_msg->num-1;
        uint32 offset = remove_msg->inodes[n].offset * rafs_self->partition->part_info.block_size;
        RaPartitionBgErase(&rafs_self->partition->part_handle, offset);
        remove_msg->inodes[n].offset++;
        remove_msg->inodes[n].length--;
        if( remove_msg->inodes[n].length == 0 )
            remove_msg->num--;
        if( remove_msg->num )
        {
            /* This will only happen when multiple inodes per file are supported */
            MESSAGE_MAKE(msg,remove_work_t);
            *msg = *remove_msg;
            MessageSend(&rafs_self->task, MESSAGE_RAFS_REMOVE_WORK, msg);
        }
    }

    if( remove_msg->num == 0 )
    {
        if( remove_msg->cfm_task )
        {
            MESSAGE_MAKE(cfm,rafs_remove_complete_cfm);
            cfm->status = RAFS_OK;
            MessageSend(remove_msg->cfm_task, MESSAGE_RAFS_REMOVE_COMPLETE, cfm);
        }
        Rafs_FreeScanDirent();
        rafs_self->busy = FALSE;
    }
}

/* Internal task queue and processing */
/* ---------------------------------- */
static void Rafs_IdleTimeout(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    bool defer_timer_restart = FALSE;

    if( rafs_self->partition )
    {
        if( rafs_self->idler.appIsIdle != 0 &&
            rafs_self->idler.appIsIdle(rafs_self->idler.context) )
        {
            char mount_point[RAFS_MOUNTPOINT_LEN+1];
            snprintf(mount_point, sizeof(mount_point), "%s%s",
                     RAFS_PATH_SEPARATOR, rafs_self->partition->partition_name);

            /* Do compaction - maybe */
            /* There are several conditions which mean there is nothing to do at this time. */
            rafs_errors_t result = Rafs_Compact(&rafs_self->task, mount_point);
            DEBUG_LOG_DEBUG("Rafs_IdleTimeout - app is idle, compact=%d", result);
            if( result == RAFS_OK)
            {
                /* Let the compact completion message restart the idle timer */
                defer_timer_restart = TRUE;
            }
        }
    }

    if( !defer_timer_restart )
    {
        MessageSendLater(&rafs_self->task, MESSAGE_RAFS_IDLE_TIMEOUT, NULL, RAFS_IDLE_TIMEOUT());
    }
}

static void Rafs_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch ( id )
    {
        case MESSAGE_BATTERY_REGION_UPDATE:
            Rafs_UpdateBatteryState((MESSAGE_BATTERY_REGION_UPDATE_STATE_T*)message);
            break;
        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_DETACHED:
        case CHARGER_MESSAGE_COMPLETED:
        case CHARGER_MESSAGE_CHARGING_OK:
        case CHARGER_MESSAGE_CHARGING_LOW:
            Rafs_UpdateChargerState(id);
            break;

        case MESSAGE_RAFS_MOUNT_WORK:
            Rafs_ProgressMountWorker((mount_work_t*)message);
            break;
        case MESSAGE_RAFS_COMPACT_WORK:
            Rafs_ProgressCompactWorker((compact_work_t*)message);
            break;
        case MESSAGE_RAFS_WRITE_WORK:
            Rafs_ProgressWriteWorker((write_work_t*)message);
            break;
        case MESSAGE_RAFS_REMOVE_WORK:
            Rafs_ProgressRemoveWorker((remove_work_t*)message);
            break;

        case MESSAGE_RAFS_COMPACT_COMPLETE:
            /* An internally sourced compaction has completed */
            MessageSendLater(task, MESSAGE_RAFS_IDLE_TIMEOUT, NULL, RAFS_IDLE_TIMEOUT());
            break;
        case MESSAGE_RAFS_IDLE_TIMEOUT:
            Rafs_IdleTimeout();
            break;

        default :
            DEBUG_LOG("Rafs_MessageHandler, unknown id MESSAGE:rafs_work_message_id_t:0x%x",id);
            break;
    }
}

void Rafs_MessageInit(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_self->task.handler = Rafs_MessageHandler;

    if( !Charger_ClientRegister(&rafs_self->task) )
    {
        DEBUG_LOG_ALWAYS("Rafs_MessageInit: Failed to register with charger monitor");
    }
    
    if( !BatteryRegion_Register(&rafs_self->task) )
    {
        DEBUG_LOG_ALWAYS("Rafs_MessageInit: Failed to register with battery monitor");
    }
    rafs_self->power.battery_state = battery_region_ok;

#if !defined(HOSTED_TEST_ENVIRONMENT)
    MessageSendLater(&rafs_self->task, MESSAGE_RAFS_IDLE_TIMEOUT, NULL, RAFS_IDLE_TIMEOUT());
#endif
}

void Rafs_MessageShutdown(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    MessageCancelAll(&rafs_self->task, MESSAGE_RAFS_IDLE_TIMEOUT);
    BatteryRegion_Unregister(&rafs_self->task);
    Charger_ClientUnregister(&rafs_self->task);
}
