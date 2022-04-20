/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       swift_pair.h
\brief      Header file for the Swift Pair
*/

#ifndef SWIFT_PAIR_H_
#define SWIFT_PAIR_H_

#ifdef INCLUDE_SWIFT_PAIR
#include <task_list.h>
#include <logging.h>
#include <panic.h>

#include "le_advertising_manager.h"
#include "pairing.h"

typedef struct
{
    /*! The swift pair module task */
    TaskData task;
}swiftPairTaskData;

/*! Private API to initialise swiftpair advertising module

    Function to initialise the swiftpair advertising module

 */
void swiftPair_SetUpAdvertising(void);

/*! \brief Check if the device is in handset discoverable mode so that swift pair payload can be advertised
*/
void swiftPair_SetIdentifiable(const le_adv_data_set_t data_set);

/*! \brief Handle the Pairing activity messages from pairing module */
void swiftPair_PairingActivity(PAIRING_ACTIVITY_T *message);

/*! Handler for all messages to swift pair Module

    This function is called to handle any messages sent to the swift pairing module.

    \param  task            Task to which this message was sent
    \param  id              Identifier of the message
    \param  message         The message content (if any)

    \returns None
 */
void swiftPair_HandleMessage(Task task, MessageId id, Message message);

/*! Get pointer to Swift Pair data structure

    This function is called fetch the swift pair context.

    \param  None

    \returns swift pair data context
 */
swiftPairTaskData* swiftPair_GetTaskData(void);

/*! Initialise the swit pair application module.

    This function is called to initilaze swift pairing module.
    If a message is processed then the function returns TRUE.

    \param  init_task       Initialization Task

    \returns TRUE if successfully processed
 */
bool SwiftPair_Init(Task init_task);
#endif /* INCLUDE_SWIFT_PAIR */

#endif /* SWIFT_PAIR_H_ */
