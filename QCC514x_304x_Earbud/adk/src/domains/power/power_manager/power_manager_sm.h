/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    power_manager
\brief      Power manager state machine
 
*/

#ifndef POWER_MANAGER_SM_H_
#define POWER_MANAGER_SM_H_

#include "power_manager.h"

/*@{*/

/*! \brief Set new state

    It will execute exit function of the old state and entry function of the new state.

    \param new_state New state
*/
void appPowerSetState(powerState new_state);

/*! \brief Check if system should go to sleep, power off or nothing */
void appPowerHandlePowerEvent(void);

/*@}*/

#endif /* POWER_MANAGER_SM_H_ */
