/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    power_manager
\brief      Implementation of 'go to sleep' and power off
 
*/

#ifndef POWER_MANAGER_ACTION_H_
#define POWER_MANAGER_ACTION_H_

#include <csrtypes.h>

/*@{*/

/*! \brief Enter dormant mode

    If it will fail to enter dormant mode then it will restore charger and panic.

    \param extended_wakeup_events Allow accelerometer to wake from dormant
*/
void appPowerEnterDormantMode(bool extended_wakeup_events);

/*! \brief Power off the chip

    If it will fail to power off the chip and charger detection is in progress,
    then it it will go to dormant mode.

    \return It will return FALSE if it can't power-off due to charger detection ongoing.
*/
bool appPowerDoPowerOff(void);

/*@}*/

#endif /* POWER_MANAGER_ACTION_H_ */
