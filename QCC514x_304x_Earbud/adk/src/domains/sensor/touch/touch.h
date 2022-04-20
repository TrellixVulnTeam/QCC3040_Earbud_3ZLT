/*!
\copyright  Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Header file for touch / capacitive sense support
*/

#ifndef TOUCH_H
#define TOUCH_H

#include <task_list.h>
#include <touch_config.h>

#include "domain_message.h"
#include "dormant.h"


typedef enum
{
    TOUCH_SENSOR_ACTION = TOUCH_MESSAGE_BASE,

    /*! This must be the final message */
    TOUCH_SENSOR_MESSAGE_END
} touch_sensor_messages;

typedef struct
{
    touch_action_t  action;
} TOUCH_SENSOR_ACTION_T;


/*! @brief Touchpad module state. */
typedef struct
{
    /*! Touch State module message task. */
    TaskData task;
    /*! List of registered client tasks */
    TASK_LIST_WITH_INITIAL_CAPACITY(TOUCH_CLIENTS_INITIAL_CAPACITY) ui_clients;
    /*! List of client tasks registered for raw action events (no filtering) */
    TASK_LIST_WITH_INITIAL_CAPACITY(TOUCH_CLIENTS_INITIAL_CAPACITY) action_clients;
    /*! The config */
    const touchConfig *config;
    /*! Input action table */
    const touch_event_config_t *action_table;
    /*! Input action table size */
    uint32 action_table_size;
    /*! number of seconds touch was pressed */
    uint8 number_of_seconds_held;
    /*! number of quick touch was pressed */
    uint8 number_of_press;

} touchTaskData;

/*!< Task information for proximity sensor */
extern touchTaskData app_touch;
/*! Get pointer to the proximity sensor data structure */
#define TouchGetTaskData()   (&app_touch)

/*! Get pointer to the Touch sensor UI client tasks */
#define TouchSensor_GetUiClientTasks() (task_list_flexible_t *)(&app_touch.ui_clients)

/*! Get pointer to the Touch sensor action client tasks */
#define TouchSensor_GetActionClientTasks() (task_list_flexible_t *)(&app_touch.action_clients)

#ifdef INCLUDE_CAPSENSE
/*! \brief Obtain the key/value to pass to DormantConfigure to allow the
    touch sensor interrupt to wake the device from dormant mode. The caller may
    just pass the returned values into the DormantConfigure trap, or modify them
    to enable other wake sources on the same key.

    \param[out] key     The key to be passed to DormantConfigure
    \param[OUT] value   The value to be passed to DormantConfigure

    \return TRUE if the touch sensor is able to wake the chip from dormant and
            the key/value are valid, otherwise FALSE.
*/

bool AppTouchSensorGetDormantConfigureKeyValue(dormant_config_key *key, uint32* value);
#endif

#ifdef INCLUDE_CAPSENSE
/*! \brief Initialise the touch task data

    Called at start up to initialise the data for touch sensor.

    \param init_task Unused
*/
bool TouchSensor_Init(Task init_task);
#endif

#ifdef INCLUDE_CAPSENSE
/*! \brief Reset the touch sensor

    Called to reset the touch sensor; used at boot time to put the sensor
    into a known state; also when the device is taken in/out of the case
    or the ear, in case the user has accidentally touched the surface.

    \param hold If TRUE will try to hold the sensor in reset
*/
bool TouchSensor_Reset(bool hold);
#else
#define TouchSensor_Reset(bool) ((void)bool)
#endif

#ifdef INCLUDE_CAPSENSE
/*! \brief Register a UI action table and client task

    This registers a task to receive UI events. If an action_table is supplied
    it replaces an earlier table.

    It is possible to not supply an action_table. If no call has been made to
    register an action table then no messages will be sent.

    \param      task                Task to register
    \param      size_action_table   Size of action table (if any)
    \param[in]  action_table        Table converting touch events to UI events (if any)
 */
bool TouchSensorClientRegister(Task task, uint32 size_action_table, const touch_event_config_t *action_table);

/*! \brief Register a task to receive raw touch events

    The task will be sent a #TOUCH_SENSOR_ACTION message when a touch
    occurs. This message will be sent regardless of a UI translation 
    table (supplied in TouchSensorClientRegister())
    
    \param task Tsk to register for messages 
 */
extern bool TouchSensorActionClientRegister(Task task);
#else
#define TouchSensorClientRegister(task, size_action_table, action_table) FALSE
#define TouchSensorActionClientRegister(task) ((void)task)
#endif

#ifdef INCLUDE_CAPSENSE
/*! \brief Unregister a task for UI events from the touch sensor

    \param task The task to unregister.

    The sensor will be disabled when the final client of any kind unregisters. 
 */
extern void TouchSensorClientUnRegister(Task task);

/*! \brief Unregister a task for touch actions

    \param task The task to unregister.

    The sensor will be disabled when the final client of any kind unregisters. 
 */
extern void TouchSensorActionClientUnRegister(Task task);
#else
#define TouchSensorClientUnRegister(task) ((void)task)
#define TouchSensorActionClientUnRegister(task) ((void)task)
#endif


#endif /* TOUCH_H */
