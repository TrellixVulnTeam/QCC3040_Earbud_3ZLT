/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   system_reboot System Reboot
\brief	    Header file for the System Reboot.

*/

#ifndef SYSTEM_REBOOT_H_
#define SYSTEM_REBOOT_H_

#include <message.h>
#include <task_list.h>
#include "domain_message.h"

/*! \brief Reboot actions */
typedef enum
{
    /*!< The component initiated reboot recommends the application/client to boot
     *   as usual Power On would boot the system.*/
    reboot_action_default_state,
    /*!< The component initiated reboot recommends the application/client to move to a
     *   default state which is well defined by the application/clients */
    reboot_action_active_state,

}reboot_action_t;

/*! \brief Reboot device.

    This function is called when the power-off watchdog has expired, this
    means we have failed to shutdown after 10 seconds.

    We should now force a reboot.
*/
void SystemReboot_Reboot(void);

/*! \brief Reboots device with Action.

    This function force reboots the device and saves the action to be taken post reboot.

    We should then force a reboot.
*/
void SystemReboot_RebootWithAction(reboot_action_t reboot_action);

/*! \brief Returns the action to be taken post reboot. When app uses this function to move to a
    well defined state by application, the reboot action should be reset for the next boot
    to be as reboot_action_default_state. To achieve this the app can call SystemReboot_ResetAction.

    \return reboot action
*/
reboot_action_t SystemReboot_GetAction(void);

/*! \brief Resets the reboot action to reboot_action_default_state.

*/
void SystemReboot_ResetAction(void);

#endif
