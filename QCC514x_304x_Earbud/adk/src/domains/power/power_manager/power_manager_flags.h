/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    power_manager
\brief      Utility functions to manage client flags.
 
*/

#ifndef POWER_MANAGER_FLAGS_H_
#define POWER_MANAGER_FLAGS_H_


/*@{*/

#define appPowerGetClients() TaskList_GetBaseTaskList(&PowerGetTaskData()->clients)

/*! \brief The client task allows sleep */
#define APP_POWER_ALLOW_SLEEP                       0x00000001
/*! \brief Power is waiting for a response to #POWER_SHUTDOWN_PREPARE_IND from the client task. */
#define APP_POWER_SHUTDOWN_PREPARE_RESPONSE_PENDING 0x00000002
/*! \brief Power is waiting for a response to #POWER_SLEEP_PREPARE_IND from the clienttask. */
#define APP_POWER_SLEEP_PREPARE_RESPONSE_PENDING    0x00000004

/* \brief Inspect task list data to determine if all clients have flag bits
    cleared in their tasklist data.

    \param flag Bit mask to check against

    \return TRUE if all clients have specified flags cleared
*/
bool appPowerAllClientsHaveFlagCleared(uint32 flag);

/* \brief Inspect task list data to determine if all clients have flag bits
    set in their tasklist data.

    \param flag Bit mask to check against

    \return TRUE if all clients have specified flags set
*/
bool appPowerAllClientsHaveFlagSet(uint32 flag);

/*! \brief Set a flag in one client's tasklist data

    \param task Client task
    \param flag Bit mask to set

    \return FALSE if given task doesn't have data which can be set
*/
bool appPowerSetFlagInClient(Task task, uint32 flag);

/*! \brief Clear a flag in one client's tasklist data
 *
    \param task Client task
    \param flag Bit mask to clear

    \return FALSE if given task doesn't have data which can be clear
*/
bool appPowerClearFlagInClient(Task task, uint32 flag);

/*! \brief Set a flag in all client's tasklist data

    \param flag Bit mask to set
*/
void appPowerSetFlagInAllClients(uint32 flag);

/*@}*/

#endif /* POWER_MANAGER_FLAGS_H_ */
