/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_utils.c
\brief      Various internal support functions for rafs

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <ra_partition_api.h>
#include <stdlib.h>
#include <string.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_utils.h"

raPartition_result Rafs_PartWrite(raPartition_handle *handle, uint32 offset, uint32 length, const void * buffer)
{
    return RaPartitionWrite(handle, offset, length, buffer);
}

raPartition_result Rafs_PartRead(raPartition_handle *handle, uint32 offset, uint32 length, void * buffer)
{
    return RaPartitionRead(handle, offset, length, buffer);
}

/* TODO: Think about making these safe functions conform to the C11 strnxxx_s functions */
bool safe_strncpy(char *dest, const char *src, rafs_size_t num)
{
    if( num == 0)
    {
        return FALSE;
    }
    rafs_size_t  i;
    for(i = 0 ; i < num-1 && src[i] != '\0' ; i++ )
        dest[i] = src[i];
    bool result = src[i] == '\0';
    for( ; i < num ; i++ )
        dest[i] = '\0';
    return result;
}

int safe_strncmp(const char *s1, const char *s2, rafs_size_t num)
{
    return strncmp(s1, s2, num);
}

int safe_strnlen(const char *src, rafs_size_t num)
{
    if( num == 0)
    {
        return -1;
    }
    int result = 0;
    rafs_size_t i;
    for(i = 0 ; i < num-1 && src[i] != '\0' ; i++ )
        ;
    result = src[i] == '\0' ? (int)i : -1;
    return result;
}

bool Rafs_IsErasedData(const void *ptr, rafs_size_t len)
{
    bool result = TRUE;
    const uint8 *mem = ptr;
    for( rafs_size_t i = 0 ; result && i < len ; i++ )
    {
        result = mem[i] == 0xFF;
    }
    return result;
}

bool Rafs_IsValidFilenameCharacter(char ch)
{
    static const char *validFilenameChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "_.";
    return strchr(validFilenameChars, ch) != NULL;
}

rafs_errors_t Rafs_IsValidFilename(const char *filename)
{
    PanicFalse(filename != NULL);

    rafs_errors_t result = RAFS_OK;

    if( safe_strnlen(filename, RAFS_MAX_FILELEN) <= 0 )
    {
        result = RAFS_INVALID_LENGTH;
    }
    else
    {
        for( const char *p = filename ; *p ; p++ )
        {
            if( !Rafs_IsValidFilenameCharacter(*p) )
            {
                result = RAFS_INVALID_CHARACTER;
                break;
            }
        }
    }

    return result;
}

/*!
 * \brief Rafs_GetPathnameComponent
 * This function breaks out individual path components from an absolute path.
 *
 * \param path  The absolute pathname to be examined.
 * \param buff  The buffer where to store the n'th component.
 * \param len   The length of that buffer.
 * \param field The n'th field within the path to extract.
 * \return RAFS_OK if a field was extracted OK, or an error indicating a problem with the path.
 */
static rafs_errors_t Rafs_GetPathnameComponent(const char *path, char *buff, rafs_size_t len, uint16 field)
{
    const char sep = RAFS_PATH_SEPARATOR[0];

    /* Find the n'th path separator */
    while( *path != '\0' )
    {
        if( *path == sep && field-- == 0 )
            break;
        path++;
    }
    if( *path == '\0' )
        return RAFS_INVALID_PATH;

    path++;
    rafs_size_t i;
    for( i = 0 ; i < len ; i++ )
    {
        if( *path == '\0' || *path == sep )
        {
            break;
        }
        buff[i] = *path++;
    }

    if( i == len )
        return RAFS_INVALID_LENGTH;

    buff[i] = '\0';

    return RAFS_OK;
}

rafs_errors_t Rafs_GetPartitionName(const char *path, char *buff, rafs_size_t len)
{
    return Rafs_GetPathnameComponent(path, buff, len, 0);
}

rafs_errors_t Rafs_GetFilename(const char *path, char *buff, rafs_size_t len)
{
    return Rafs_GetPathnameComponent(path, buff, len, 1);
}

rafs_errors_t Rafs_IsReady(void)
{
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    PanicNull(rafs_self);
    if( rafs_self->busy )
    {
        result = RAFS_BUSY;
    }
    return result;
}

rafs_errors_t Rafs_PartitionOpen(const char *partition_name)
{
    rafs_errors_t result = RAFS_OK;
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    PanicFalse(rafs_self->partition == NULL);

    rafs_self->partition = PanicUnlessMalloc(sizeof(*rafs_self->partition));
    memset(rafs_self->partition, 0, sizeof(*rafs_self->partition));

    result = Rafs_GetPartitionName(partition_name,
                rafs_self->partition->partition_name, sizeof(rafs_self->partition->partition_name));

    if( result == RAFS_OK )
    {
        raPartition_result ra_result = RaPartitionNamedOpen(rafs_self->partition->partition_name,
                    &rafs_self->partition->part_handle, &rafs_self->partition->part_info);
        if( ra_result != RA_PARTITION_RESULT_SUCCESS)
        {
            DEBUG_LOG_WARN("Rafs_PartitionOpen: failed to open partition, result=%d",ra_result);
            result = RAFS_NO_PARTITION;
            Panic();
        }
    }

    if( result != RAFS_OK )
    {
        free(rafs_self->partition);
        rafs_self->partition = NULL;
    }
    return result;
}

void Rafs_PartitionClose(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    PanicFalse(rafs_self->partition != NULL);
    raPartition_result ra_result = RaPartitionClose(&rafs_self->partition->part_handle);
    if( ra_result != RA_PARTITION_RESULT_SUCCESS )
    {
        DEBUG_LOG_WARN("Rafs_PartitionClose: Unclean RaPartitionClose, result=%d", ra_result);
    }
    free(rafs_self->partition);
    rafs_self->partition = NULL;
}

void Rafs_AllocateScanDirent(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    PanicFalse(rafs_self->files->scan_dir_entry == NULL);
    rafs_self->files->scan_dir_entry = PanicUnlessMalloc(sizeof(*rafs_self->files->scan_dir_entry));
}

void Rafs_FreeScanDirent(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    free(rafs_self->files->scan_dir_entry);
    rafs_self->files->scan_dir_entry = NULL;
}
