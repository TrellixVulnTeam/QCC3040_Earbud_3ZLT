/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Watchdog
*/

#ifndef WDOG_H_
#define WDOG_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void wdog_init(void);
void wdog_periodic(void);
CLI_RESULT wdog_cmd(uint8_t source);
void wdog_kick(void);

#endif /* WDOG_H_ */
