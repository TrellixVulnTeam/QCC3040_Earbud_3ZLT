/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_domain
\brief      Internal interface for the case domain.
*/
/*! \addtogroup case_domain
@{
*/

#ifndef CASE_PRIVATE_H
#define CASE_PRIVATE_H

#include "case.h"

#include <task_list.h>

#include <stdlib.h>

#ifdef INCLUDE_CASE

/*! Defines the roles changed task list initalc capacity */
#define STATE_CLIENTS_TASK_LIST_INIT_CAPACITY 2

#define BATTERY_STATE_CHARGING_BIT      (0x80)
#define BATTERY_STATE_SET_CHARGING(x)   ((x) |= BATTERY_STATE_CHARGING_BIT)
#define BATTERY_STATE_CLEAR_CHARGING(x) ((x) &= ~BATTERY_STATE_CHARGING_BIT)
#define BATTERY_STATE_IS_CHARGING(x)    (((x) & BATTERY_STATE_CHARGING_BIT) == BATTERY_STATE_CHARGING_BIT)

#define BATTERY_STATE_PERCENTAGE(x)     ((x) & 0x7F)

/*! Structure holding information for the Case domain. */
typedef struct
{
    /*! Task for handling messages. */
    TaskData task;

    /*! Current known state of the case lid. */
    case_lid_state_t lid_state;

    /*! Current known state of the case battery level. */
    uint8 case_battery_state;

    /*! Current known state of the peer battery level (learnt via the case). */
    uint8 peer_battery_state;

    /*! Current known state of the case charger connectivity. */
    unsigned case_charger_connected:1;
    
    /*! List of clients registered to receive case state notification messages. */
    TASK_LIST_WITH_INITIAL_CAPACITY(STATE_CLIENTS_TASK_LIST_INIT_CAPACITY)   state_client_tasks;
} case_state_t;

/* Make the main Case data structure visible throughput the component. */
extern case_state_t case_state;

/*! Get pointer to the case task data. */
#define Case_GetTaskData()   (&case_state)

/*! Get pointer to the case task. */
#define Case_GetTask()       (&case_state.task)

/*! Get task list with clients requiring state messages. */
#define Case_GetStateClientTasks() (task_list_flexible_t *)(&case_state.state_client_tasks)

/*! Create a case message. */
#define MAKE_CASE_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))

#endif /* INCLUDE_CASE */

#endif /* CASE_PRIVATE_H */
/*! @} End of group documentation */
