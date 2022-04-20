/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_format.c
\brief      Implement functions to format the RAFS file system.

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <limits.h>
#include <ra_partition_api.h>

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_fat.h"
#include "rafs_format.h"
#include "rafs_utils.h"

rafs_errors_t Rafs_DoFormat(const char *partition_name, rafs_format_type_t type)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_errors_t result = RAFS_OK;
    PanicFalse(type == format_normal);

    result = Rafs_PartitionOpen(partition_name);
    if( result == RAFS_OK )
    {
        raPartition_result r1 = RaPartitionErase(&rafs_self->partition->part_handle, PRI_FAT*rafs_self->partition->part_info.block_size);
        raPartition_result r2 = RaPartitionErase(&rafs_self->partition->part_handle, SEC_FAT*rafs_self->partition->part_info.block_size);
        if( r1 == RA_PARTITION_RESULT_SUCCESS &&
            r2 == RA_PARTITION_RESULT_SUCCESS )
        {
            bool primaryOK   = Rafs_WriteRootDirent(PRI_FAT);
            bool secondaryOK = Rafs_WriteRootDirent(SEC_FAT);
            if( primaryOK && secondaryOK )
            {
                /* Erasing the rest of the flash is performed as part of the */
                /* normal operation of mount. */
                /* It would normally be expected to be clean after a factory reset, */
                /* and mount always has to be able to cope with finding dirty */
                /* unused blocks anyway. */
                result = RAFS_OK;
            }
            else
            {
                result = RAFS_INVALID_FAT;
            }
        }
        Rafs_PartitionClose();
    }
    else
    {
        DEBUG_LOG_WARN("Rafs_DoFormat: failed to open partition, result=%d", result);
        result = RAFS_NO_PARTITION;
        Panic();
    }

    return result;
}
