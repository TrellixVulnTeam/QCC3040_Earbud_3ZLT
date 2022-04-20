/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   system_state System State
\brief      Executes transitions between power states.

On itself system state doesn't call any components including power related.
It is also not deciding when to change a state.

System state defines handful of states and allows for registration of functions to be executed
on transition between states.

System state is not deciding when to change a state it must be told to change a state.
It notifies clients when the state change occurs.

System state enforces which state can transition to which state with exception of Emergency Shutdown,
to which system can transition from any other state.

To address varying needs of different applications it is possible to remove Limbo state.
See SystemState_RemoveLimboState.

There are two types of sates stable and transitional.
E.g. system_state_initialisation is transitional state,
whereas system_state_initialised is stable state.
See functions starting transitions like SystemState_Initialise().

Initial state is system_state_powered_off.

*/

#ifndef SYSTEM_STATE_H_
#define SYSTEM_STATE_H_

#include <message.h>
#include <domain_message.h>

/*@{*/

/*! \brief Transition step function prototype.

    An step function can be synchronous or asynchronous.

    Synchronous step functions must return either TRUE or FALSE,
    depending on whether they were successful.

    Asynchronous step functions should return TRUE if the
    step is in progress.

    When the step completes this should be signalled to the system_state
    module by sending the expected message to the #step_task passed into the
    #step_function. The message may optionally contain data, e.g. a success or
    fail code, in which case a #step_handler for the message can be used to
    check if the initialisation was successful.

    Asynchronous step functions may return FALSE if the
    initialisation fails before it reaches the asynchronous part.

    \param step_task Task to send the expected completion message to when
                     an asynchronous step has finished. Synchronous step
                     functions do not use this.

    \return TRUE if step was successful or is in progress,
            FALSE otherwise.
*/
typedef bool (*step_function)(Task step_task);

/*! \brief Asynchronous step complete handler prototype.

    If an asynchronous step function returns a message with a payload,
    for example a success code, then the user module can use a function
    of this type to process the contents of the payload and tell the
    system_state module if the initialisation was successful or not.

    \param message [in] Message payload of the step complete message.

    \return TRUE if the step was successful, FALSE otherwise.

*/
typedef bool (*step_handler)(Message message);

/*! \brief Transition step table entry
 */
typedef struct
{
    step_function step; /*!< Step function to call. */
    uint16 async_message_id; /*!< Message ID to wait for, 0 if no message required */
    step_handler async_handler; /*!< Function to call when message with above ID is received. */
} system_state_step_t;

typedef enum
{
    system_state_powered_off = (1 << 1),
    system_state_initialisation = (1 << 2),
    system_state_initialised = (1 << 3),
    system_state_starting_up = (1 << 4),
    system_state_limbo = (1 << 5),
    system_state_powering_on = (1 << 6),
    system_state_active = (1 << 7),
    system_state_powering_off = (1 << 8),
    system_state_going_to_sleep = (1 << 9),
    system_state_sleeping = (1 << 10),
    system_state_shutting_down = (1 << 11),
    system_state_shutting_down_promptly = (1 << 12)
} system_state_state_t;

typedef struct
{
    system_state_state_t old_state;
    system_state_state_t new_state;
} SYSTEM_STATE_STATE_CHANGE_T;

typedef enum
{
    SYSTEM_STATE_STATE_CHANGE = SYSTEM_STATE_MESSAGE_BASE,
} system_state_messages_t;

/*! \brief Init function */
void SystemState_Init(void);

/*! \brief Remove limbo state

    Component behaviour is alerted in the following way:
     - SystemState_StartUp() leads directly to the Active state.
     - SystemState_PowerOn() and SystemState_PowerOff() functions return FALSE and do nothing.
     - Powering on and powering off transitions are not used.
*/
void SystemState_RemoveLimboState(void);

/*! \brief Register table of functions to be executed during Initialise transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForInitialise(const system_state_step_t *transition_table, unsigned transition_table_count);

/*! \brief Register table of functions to be executed during StartUp transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForStartUp(const system_state_step_t *transition_table, unsigned transition_table_count);

/*! \brief Register table of functions to be executed during PowerOn transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForPowerOn(const system_state_step_t *transition_table, unsigned transition_table_count);

/*! \brief Register table of functions to be executed during PowerOff transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForPowerOff(const system_state_step_t *transition_table, unsigned transition_table_count);

/*! \brief Register table of functions to be executed during Sleep transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForSleep(const system_state_step_t *transition_table, unsigned transition_table_count);

/*! \brief Register table of functions to be executed during Shutdown transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForShutdown(const system_state_step_t *transition_table, unsigned transition_table_count);

/*! \brief Register table of functions to be executed during EmergencyShutdown transition

    \param transition_table Array of functions to be executed
    \param transition_table_count Number of functions in the array
*/
void SystemState_RegisterTableForEmergencyShutdown(const system_state_step_t *transition_table, unsigned transition_table_count);



/*! \brief Register to receive state change notifications

    \param listener Task which will be notified on every state change.
*/
void SystemState_RegisterForStateChanges(Task listener);

/*! \brief Get current state

    \return Current state
*/
system_state_state_t SystemState_GetState(void);

/*! \brief Get task for the current transition

    \return Task to which step function can send a message indicating that asynchronous step is complete.
*/
Task SystemState_GetTransitionTask(void);



/*! \brief Start Initialise transition

    Starts from system_state_powered_off.
    Goes through system_state_initialisation.
    Ends in system_state_initialised.

    \return FLASE if starting conditions weren't met e.g. requested transition can't start in current state
*/
bool SystemState_Initialise(void);

/*! \brief Start StartUp transition

    Starts from system_state_initialised.
    Goes through system_state_starting_up.
    Ends in system_state_limbo (or in system_state_active when Limbo state is removed).

    \return FLASE if starting conditions weren't met e.g. requested transition can't start in current state
*/
bool SystemState_StartUp(void);

/*! \brief Start PowerOn transition

    Starts from system_state_limbo or system_state_powering_off.
    Goes through system_state_powering_on.
    Ends in system_state_active.

    It is invalid when Limbo state is removed.

    \return FLASE if starting conditions weren't met e.g. requested transition can't start in current state
*/
bool SystemState_PowerOn(void);

/*! \brief Start PowerOff transition

    Starts from system_state_active or system_state_powering_on.
    Goes through system_state_powering_off.
    Ends in system_state_limbo.

    It is invalid when Limbo state is removed.

    \return FLASE if starting conditions weren't met e.g. requested transition can't start in current state
*/
bool SystemState_PowerOff(void);

/*! \brief Start Sleep transition

    Starts from system_state_limbo (or system_state_active when Limbo state is removed).
    Goes through system_state_going_to_sleep.
    Ends in system_state_sleeping.

    Note that in real system system_state_sleeping state is never actually reached
    as the system cease execution before reaching this state.
    That is assuming that last step function puts the system to sleep.

    \return FLASE if starting conditions weren't met e.g. requested transition can't start in current state
*/
bool SystemState_Sleep(void);

/*! \brief Start Shutdown transition

    Starts from system_state_limbo (or system_state_active when Limbo state is removed).
    Goes through system_state_shutting_down.
    Ends in system_state_powered_off.

    Note that in real system system_state_powered_off state is never actually reached
    as the system cease execution before reaching this state.
    That is assuming that last step function shuts down the system.

    \return FLASE if starting conditions weren't met e.g. requested transition can't start in current state
*/
bool SystemState_Shutdown(void);

/*! \brief Start EmergencyShutdown transition

    Starts from any state.
    Goes through system_state_shutting_down_promptly.
    Ends in system_state_powered_off.

    Note that in real system system_state_powered_off state is never actually reached
    as the system cease execution before reaching this state.
    That is assuming that last step function shuts down the system.

    \return Always TRUE
*/
bool SystemState_EmergencyShutdown(void);

/*@}*/

#endif /* SYSTEM_STATE_H_ */
