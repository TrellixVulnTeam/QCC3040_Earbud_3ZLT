/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      
*/
/*! \addtogroup case_comms
@{
*/

#ifndef CC_WITH_EARBUDS_PRIVATE_H
#define CC_WITH_EARBUDS_PRIVATE_H

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_MODE_CASE

#include "cc_with_earbuds.h"

#include <task_list.h>

#define CLIENTS_TASK_LIST_INIT_CAPACITY 2

/* Internal messages used by cc_with_earbuds. */
typedef enum ccwe_internal_message_ids
{
    CCWE_INTERNAL_TIMEOUT_GET_EB_STATUS
} ccwe_internal_message_ids_t;

/*! Current state known about each Earbud via Case Comms. */
typedef struct
{
    /*! Battery and charger state. */
    unsigned battery_state:8;

    /*! The Earbud is known to be present in the case. */
    bool present:1;

    /*! Peer pairing status. */
    pp_state_t peer_paired:2;

    /* Add new state here to the existing bitfield */

    /*! Programmed BT address of the Earbud. */
    bdaddr addr;
} eb_state;

/*! Task data for case comms with Earbuds. */
typedef struct
{
    /*! Task for handling messages. */
    TaskData task;

    /*! List of clients registered to receive case state notification messages. */
    TASK_LIST_WITH_INITIAL_CAPACITY(CLIENTS_TASK_LIST_INIT_CAPACITY)  client_tasks;

    /*! Current state of both Earbuds. */
    eb_state earbuds_state[2];

    /*! Counter tracking loopback messages sent. */
    uint8 loopback_sent;

    /*! Counter tracking loopback messages received. */
    uint8 loopback_recv;
} cc_with_earbuds_t;

/* Make the main Case data structure visible throughput the component. */
extern cc_with_earbuds_t cc_with_earbuds;

/*! Get pointer to the case task data. */
#define CcWithEarbuds_GetTaskData()   (&cc_with_earbuds)

/*! Get pointer to the case task. */
#define CcWithEarbuds_GetTask()       (&cc_with_earbuds.task)

/*! Get task list with clients requiring state messages. */
#define CcWithEarbuds_GetClientTasks() (task_list_flexible_t *)(&cc_with_earbuds.client_tasks)

#endif /* HAVE_CC_MODE_CASE */
#endif /* INCLUDE_CASE_COMMS */
#endif /* CC_WITH_EARBUDS_PRIVATE_H */
/*! @} End of group documentation */
