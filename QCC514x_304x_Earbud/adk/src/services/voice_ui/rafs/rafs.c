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
#include <stdlib.h>
#include <pmalloc.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_fat.h"
#include "rafs_format.h"
#include "rafs_directory.h"
#include "rafs_message.h"
#include "rafs_file.h"
#include "rafs_init.h"
#include "rafs_utils.h"
#include "rafs_power.h"
#include "rafs_volume.h"
#include "rafs_iocontrol.h"

LOGGING_PRESERVE_MESSAGE_TYPE(rafs_message_id_t)
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(MESSAGE_RAFS_LAST)

static rafs_instance_t *rafs_instance = NULL;

/* Global functions that are private to this module */
rafs_instance_t *Rafs_GetTaskData(void)
{
    return rafs_instance;
}

void Rafs_SetTaskData(rafs_instance_t *self)
{
    rafs_instance = self;
}

/* =================== */
/* The RAFS public API */
/* =================== */
rafs_errors_t Rafs_Open(Task task, const char *pathname, rafs_mode_t flags, rafs_file_t *file_id)
{
    PanicFalse(pathname != NULL);   /* Because PanicNull can't deal with const pointers */
    PanicNull(file_id);
    rafs_errors_t   result = Rafs_GetMountStatus(pathname);
    if( result == RAFS_OK )
    {
        char filename[RAFS_MAX_FILELEN];
        result = Rafs_GetFilename(pathname, filename, sizeof(filename));
        if( result == RAFS_OK )
        {
            result = Rafs_ValidateOpen(filename, flags, file_id);
            if( result == RAFS_OK)
            {
                Rafs_AllocateScanDirent();
                result = Rafs_DoOpen(task, filename, flags, file_id);
                Rafs_FreeScanDirent();
            }
        }
    }
    return result;
}

rafs_errors_t Rafs_Close(rafs_file_t file_id)
{
    rafs_errors_t   result = Rafs_ValidateFileIdentifier(file_id);
    if( result == RAFS_OK )
    {
        result = Rafs_DoClose(file_id);
    }
    return result;
}

rafs_errors_t Rafs_Read(rafs_file_t file_id, void *buf,
                        rafs_size_t num_bytes_to_read, rafs_size_t *num_bytes_read)
{
    PanicNull(buf);
    PanicNull(num_bytes_read);
    rafs_errors_t   result = Rafs_ValidateFileIdentifier(file_id);
    if( result == RAFS_OK )
    {
        result = Rafs_DoRead(file_id, buf, num_bytes_to_read, num_bytes_read);
    }
    return result;
}


rafs_errors_t Rafs_Write(rafs_file_t file_id, const void *buf,
                         rafs_size_t num_bytes_to_write, rafs_size_t *num_bytes_written)
{
    PanicFalse(buf != NULL);
    PanicNull(num_bytes_written);
    rafs_errors_t   result = Rafs_ValidateFileIdentifier(file_id);
    if( result == RAFS_OK )
    {
        result = Rafs_DoWrite(file_id, buf, num_bytes_to_write, num_bytes_written);
    }
    return result;
}

rafs_errors_t Rafs_WriteBackground(rafs_file_t file_id, const void *buf, rafs_size_t num_bytes_to_write)
{
    PanicFalse(buf != NULL);
    rafs_errors_t result = Rafs_ValidateFileIdentifier(file_id);
    if( result == RAFS_OK )
    {
        Rafs_CreateWriteWorker(file_id, buf, num_bytes_to_write);
    }
    return result;
}

rafs_errors_t Rafs_SetPosition(rafs_file_t file_id, rafs_size_t position)
{
    rafs_errors_t   result = Rafs_ValidateFileIdentifier(file_id);
    if( result == RAFS_OK )
    {
        result = Rafs_DoSetPosition(file_id, position);
    }
    return result;
}

rafs_errors_t Rafs_GetPosition(rafs_file_t file_id, rafs_size_t *position)
{
    PanicFalse(position != NULL);
    rafs_errors_t   result = Rafs_ValidateFileIdentifier(file_id);
    if( result == RAFS_OK )
    {
        result = Rafs_DoGetPosition(file_id, position);
    }
    return result;
}


rafs_errors_t Rafs_OpenDirectory(const char *pathname, rafs_dir_t *dir_id, uint16 *num)
{
    PanicFalse(pathname != NULL);
    PanicNull(dir_id);
    PanicNull(num);
    rafs_errors_t   result = Rafs_GetMountStatus(pathname);
    if( result == RAFS_OK )
    {
        char dirpath[RAFS_MAX_FILELEN];
        result = Rafs_GetFilename(pathname, dirpath, sizeof(dirpath));
        if( result == RAFS_OK )
        {
            Rafs_AllocateScanDirent();
            result = Rafs_DoOpendir(dirpath, dir_id, num);
            Rafs_FreeScanDirent();
        }
    }
    return result;
}

rafs_errors_t Rafs_ReadDirectory(rafs_dir_t dir_id, rafs_stat_t *stat)
{
    PanicNull(stat);
    rafs_errors_t   result = Rafs_IsReady();
    if( result == RAFS_OK )
    {
        Rafs_AllocateScanDirent();
        result = Rafs_DoReaddir(dir_id, stat);
        Rafs_FreeScanDirent();
    }
    return result;
}

rafs_errors_t Rafs_CloseDirectory(rafs_dir_t dir_id)
{
    rafs_errors_t   result = Rafs_IsReady();
    if( result == RAFS_OK )
    {
        result = Rafs_DoClosedir(dir_id);
    }
    return result;
}

rafs_errors_t Rafs_Remove(Task task, const char *pathname)
{
    PanicFalse(pathname != NULL);
    rafs_errors_t   result = Rafs_GetMountStatus(pathname);
    if( result == RAFS_OK )
    {
        char filename[RAFS_MAX_FILELEN];
        result = Rafs_GetFilename(pathname, filename, sizeof(filename));
        if( result == RAFS_OK )
        {
            if( Rafs_FindOpenFileInstanceByName(filename) != NULL )
            {
                result = RAFS_FILE_STILL_OPEN;
            }
            else
            {
                result = Rafs_IsWriteSafe();
                if( result == RAFS_OK )
                {
                    Rafs_AllocateScanDirent();
                    result = Rafs_DoRemove(task, filename);
                    if( result != RAFS_OK )
                    {
                        Rafs_FreeScanDirent();
                    }
                }
            }
        }
    }
    return result;
}

rafs_errors_t Rafs_Format(const char *partition_name, rafs_format_type_t type)
{
    PanicFalse(partition_name != NULL);
    rafs_errors_t result = Rafs_DoFormat(partition_name, type);
    return result;
}

rafs_errors_t Rafs_Compact(Task task, const char *partition_name)
{
    PanicFalse(partition_name != NULL);
    rafs_errors_t   result = Rafs_GetMountStatus(partition_name);
    if( result == RAFS_OK )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( rafs_self->files->num_open_files != 0 )
        {
            DEBUG_LOG_WARN("Rafs_Compact: File(s) still open");
            result = RAFS_FILE_STILL_OPEN;
        }
        else if( rafs_self->directory )
        {
            DEBUG_LOG_WARN("Rafs_Compact: Directory still open");
            result = RAFS_DIRECTORY_OPEN;
        }
        else if( !rafs_self->files->compact_is_needed )
        {
            result = RAFS_FILE_SYSTEM_CLEAN;
        }
        else
        {
            result = Rafs_IsWriteSafe();
            if( result == RAFS_OK )
            {
                rafs_self->files->compact_is_needed = FALSE;
                rafs_self->busy = TRUE;
                Rafs_AllocateScanDirent();
                Rafs_CreateCompactWorker(task);
            }
        }
    }
    return result;
}

/* Resource management for Mount/Unmount */
static void rafs_mount_allocate(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_self->files = PanicUnlessMalloc(sizeof(*rafs_self->files));
    memset(rafs_self->files, 0, sizeof(*rafs_self->files));
}

static void rafs_mount_free(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();

    /* cleanup all the memory */
    /* Files opened for reading can simply be dropped without RAFS damage. */
    for(int i = 0 ; i < RAFS_MAX_OPEN_FILES ; i++)
    {
        free(rafs_self->files->open_files[i]);
    }
    free(rafs_self->files);
    rafs_self->files = NULL;

    free(rafs_self->sector_in_use_map);
    rafs_self->sector_in_use_map = NULL;
}

rafs_errors_t Rafs_Mount(Task task, const char *partition_name)
{
    PanicFalse(partition_name != NULL);
    rafs_errors_t   result = Rafs_IsReady();
    if( result == RAFS_OK )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( !rafs_self->files )
        {
            rafs_mount_allocate();
        }
        if( !rafs_self->partition )
        {
            Rafs_AllocateScanDirent();
            result = Rafs_DoMount(partition_name);
            if( result == RAFS_OK )
            {
                rafs_self->busy = TRUE;
                Rafs_CreateMountWorker(task);
            }
            else
            {
                Rafs_FreeScanDirent();
                rafs_mount_free();
            }
        }
        else
        {
            result = RAFS_ALREADY_MOUNTED;
        }
    }
    return result;
}

rafs_errors_t Rafs_Unmount(const char *partition_name)
{
    PanicFalse(partition_name != NULL);
    rafs_errors_t   result = Rafs_IsReady();
    if( result == RAFS_OK )
    {
        rafs_instance_t *rafs_self = Rafs_GetTaskData();
        if( rafs_self->partition )
        {
            if( rafs_self->files->num_open_files != 0 )
            {
                /* Even if files are only open for reading, there */
                /* may be meta data (like file 'age') which needs to update */
                /* when a file is closed. */
                DEBUG_LOG_WARN("Rafs_Unmount: File(s) still open");
                result = RAFS_FILE_STILL_OPEN;
            }
            if( rafs_self->directory )
            {
                DEBUG_LOG_WARN("Rafs_Unmount: Directory still open");
                result = RAFS_DIRECTORY_OPEN;
            }
            if( result == RAFS_OK )
            {
                result = Rafs_DoUnmount(partition_name);
                if( result == RAFS_OK )
                {
                    rafs_mount_free();
                }
            }
        }
        else
        {
            result = RAFS_NOT_MOUNTED;
        }
    }
    return result;
}

rafs_errors_t Rafs_Statfs(const char *partition_name, rafs_statfs_t *buf)
{
    PanicFalse(partition_name != NULL);
    PanicNull(buf);
    rafs_errors_t   result = Rafs_GetMountStatus(partition_name);
    if( result == RAFS_OK )
    {
        Rafs_AllocateScanDirent();
        result = Rafs_DoStatfs(partition_name, buf);
        Rafs_FreeScanDirent();
    }
    return result;
}

bool Rafs_Init(Task task)
{
    UNUSED(task);
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    if( !rafs_self )
    {
        result = Rafs_DoInit();
    }
    return result == RAFS_OK;
}

rafs_errors_t Rafs_Shutdown(void)
{
    rafs_errors_t   result = Rafs_IsReady();

    if( result == RAFS_OK )
    {
        result = Rafs_DoShutdown();
    }

    return result;
}

rafs_errors_t Rafs_IoControl(rafs_ioc_type_t type, const void *in, const rafs_size_t in_size,
                             void *out, const rafs_size_t out_size, rafs_size_t *out_length)
{
    rafs_errors_t result = RAFS_OK;

    if( type == RAFS_IOC_GET_PARTITION_HANDLE )
    {
        PanicFalse(in != NULL);
        PanicFalse(out != NULL);
        PanicFalse(out_length != NULL);
        const char *partition_name = in;
        raPartition_handle *handle = out;
        PanicFalse(sizeof(*handle)<=out_size);
        result = Rafs_DoIocGetPartitionHandle(partition_name, handle, out_length);
    }
    else if( type == RAFS_IOC_GET_FILE_OFFSET )
    {
        PanicFalse(in != NULL);
        PanicFalse(out != NULL);
        PanicFalse(out_length != NULL);
        const char *pathname = in;
        uint32 *value = out;
        PanicFalse(sizeof(*value)<=out_size);
        Rafs_AllocateScanDirent();
        result = Rafs_DoIocGetFileOffset(pathname, value, out_length);
        Rafs_FreeScanDirent();
    }
    else if( type == RAFS_IOC_SET_APP_STATUS_IDLE_CB )
    {
        PanicFalse(in != NULL);
        PanicFalse(in_size == sizeof(rafs_ioc_set_app_status_idle_cb_t));
        PanicFalse(out == NULL);
        PanicFalse(out_size == 0);
        PanicFalse(out_length == NULL);
        result = Rafs_DoIocSetAppStatusIdleCb(in);
    }
    else
    {
        result = RAFS_UNSUPPORTED_IOC;
    }

    return result;
}
