/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_power.c
\brief      Management of battery and charger events and states

*/
#include <logging.h>
#include <panic.h>
#include <csrtypes.h>
#include <vmtypes.h>
#include <stdlib.h>
#include <ra_partition_api.h>
#include <string.h>

#include "charger_monitor.h"
#include "battery_monitor.h"

#include "rafs.h"
#include "rafs_private.h"
#include "rafs_power.h"

void Rafs_UpdateChargerState(MessageId id)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_self->power.charger_event = id;
}

void Rafs_UpdateBatteryState(MESSAGE_BATTERY_REGION_UPDATE_STATE_T *msg)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    rafs_self->power.battery_state = msg->state;
}

rafs_errors_t Rafs_IsWriteSafe(void)
{
    return RAFS_OK;
}
