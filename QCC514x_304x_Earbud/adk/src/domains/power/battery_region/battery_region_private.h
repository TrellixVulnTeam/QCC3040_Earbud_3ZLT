/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file        battery_region_private.h
\brief      Battery region internal data
*/

#ifndef BATTERY_REGION_PRIVATE_H_
#define BATTERY_REGION_PRIVATE_H_

#include "battery_region.h"
#include <task_list.h>

/*! The interval at which the battery voltage is read. */
#define batteryRegion_GetReadingPeriodMs() D_SEC(1)
/*! Defines the battery region client tasks list initial capacity */
#define BATTERY_REGION_CLIENT_TASKS_LIST_INIT_CAPACITY 5
#define BATTERY_REGION_UNDEFINED 0xFF

typedef struct
{
    /*! Battery Region task */
    TaskData         task;
    /*! List of client tasks */
    TASK_LIST_WITH_INITIAL_CAPACITY(BATTERY_REGION_CLIENT_TASKS_LIST_INIT_CAPACITY) client_tasks_list;
    /*! The measurement period. */
    uint16 period;
    /* region table to use */
    const charge_region_t *region_table;
    /* region table length */
    uint8 region_table_len;
    uint8 region;
    battery_region_state_t state;
} battery_region_data_t;

/*! \brief Battery component task data. */
extern battery_region_data_t app_battery_region;
/*! \brief Access the battery region data. */
#define GetBatteryRegionData()    (&app_battery_region)

#define BatteryRegion_GetClientTasks() (task_list_flexible_t *)(&app_battery_region.client_tasks_list)

#endif /* BATTERY_REGION_PRIVATE_H_ */
