/****************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_partitions_config.c

DESCRIPTION

    Implementation of partition management, config dependant parts
    of for the upgrade_partitions.c module for CONFIG_HYDRACORE.


IMPLEMENTATION NOTES

See the the upgrade_partitions.c module.

*/
    
#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <ps.h>
#include <partition.h>
#include <sink.h>
#include <stream.h>
#include <imageupgrade.h>

#include "upgrade_private.h"
#include "upgrade_ctx.h"
#include "upgrade_partitions.h"
#include "upgrade_sm.h"

/****************************************************************************
NAME
    UpgradePartitionsEraseAllManaged  -  Erase partitions

DESCRIPTION
    For the CONFIG_HYDRACORE version this just calls ImageUpgradeErase
*/
UpgradePartitionsState UpgradePartitionsEraseAllManaged(void)
{
    DEBUG_LOG_DEBUG("UpgradePartitionsEraseAllManaged");
    if (UPGRADE_RESUME_POINT_ERASE != UpgradeCtxGetPSKeys()->upgrade_in_progress_key)
    {
        DEBUG_LOG_VERBOSE("UpgradePartitionsEraseAllManaged: ImageUpgradeErase()");
        ImageUpgradeErase();
    }
    return UPGRADE_PARTITIONS_ERASED;
}
