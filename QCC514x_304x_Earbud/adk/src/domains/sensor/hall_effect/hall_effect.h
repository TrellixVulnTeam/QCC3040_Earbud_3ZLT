/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       hall_effect.h
\brief      Header file for hall effect sensor support
*/

#ifndef HALL_EFFECT_H
#define HALL_EFFECT_H

#include <task_list.h>
#include "domain_message.h"

/*! Enumeration of messages the hall effect sensor can send to its clients */
typedef enum 
{
    /*! Detect magnet present. */
    HALL_EFFECT_MESSAGE_MAGNET_PRESENT = HALL_EFFECT_MESSAGE_BASE,
    /*! Magnet not present. */
    HALL_EFFECT_MESSAGE_MAGNET_NOT_PRESENT,

    /*! This must be the final message */
    HALL_EFFECT_MESSAGE_END

} hall_effect_messages_t;

#define HALL_EFFECT_CLIENT_TASK_LIST_INIT_CAPACITY 1

/*! @brief Hall Effect module state. */
typedef struct
{
    /*! Hall Effect module message task. */
    TaskData task;
    /*! List of registered client tasks */
    TASK_LIST_WITH_INITIAL_CAPACITY(HALL_EFFECT_CLIENT_TASK_LIST_INIT_CAPACITY) client_tasks;
    /*! PIO used to connect to hall effect sensor */
    uint8 hall_effect_sensor_pio;
} hallEffectTaskData;

/*! \brief Register with hall effect sensor to receive notifications.
    \param task The task to register.
    \return TRUE if the client was successfully registered.
            FALSE if registration was unsuccessful or if the platform does not
            have a hall effect sensor.
    The sensor will be enabled the first time a client registers.
*/
#if defined(INCLUDE_HALL_EFFECT_SENSOR)
extern bool HallEffectSensor_ClientRegister(Task task);
#else
#define HallEffectSensor_ClientRegister(task) FALSE
#endif

/*! \brief Unregister with hall effect sensor.
    \param task The task to unregister.
    The sensor will be disabled when the final client unregisters. */
#if defined(INCLUDE_HALL_EFFECT_SENSOR)
extern void HallEffectSensor_ClientUnRegister(Task task);
#else
#define HallEffectSensor_ClientUnRegister(task) ((void)task)
#endif

#endif /* HALL_EFFECT_H */
