/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    power_manager
\brief      Monitoring of charger status, battery voltage and temperature.
 
*/

#ifndef POWER_MANAGER_MONITORS_H_
#define POWER_MANAGER_MONITORS_H_

#include <message.h>

/*@{*/

/* \brief Register with all monitored entities. */
void appPowerRegisterMonitors(void);

/* \brief Handler for messages from all monitored entities. */
void appPowerHandleMessage(Task task, MessageId id, Message message);

/*@}*/

#endif /* POWER_MANAGER_MONITORS_H_ */
