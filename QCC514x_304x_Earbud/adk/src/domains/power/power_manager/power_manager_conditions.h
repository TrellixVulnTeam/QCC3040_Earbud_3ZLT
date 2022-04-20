/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    power_manager
\brief      Functions used to decide if system should go to sleep or power off.

Decision is mostly based on charger status.

*/

#ifndef POWER_MANAGER_CONDITIONS_H_
#define POWER_MANAGER_CONDITIONS_H_

/*@{*/

/*! \brief Check if system can power off

    \return TRUE if system can power off
*/
bool appPowerCanPowerOff(void);

/*! \brief Check if system can go to sleep

    \return TRUE if system can go to sleep
*/
bool appPowerCanSleep(void);

/*! \brief Check if system needs to power off

    \return TRUE if system needs to power off
*/
bool appPowerNeedsToPowerOff(void);

/*@}*/

#endif /* POWER_MANAGER_CONDITIONS_H_ */
