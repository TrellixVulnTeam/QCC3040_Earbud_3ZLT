/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Watchdog
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_iwdg.h"
#include "main.h"
#include "power.h"
#include "wdog.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define IWDG_TIMEOUT_SECONDS 5

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void wdog_init(void)
{
    IWDG->KR = IWDG_WriteAccess_Enable;
    IWDG->PR = IWDG_Prescaler_64;
    IWDG->RLR = (uint16_t)(IWDG_TIMEOUT_SECONDS * LSI_VALUE / 64);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

void wdog_kick(void)
{
    IWDG_ReloadCounter();
}

void wdog_periodic(void)
{
    wdog_kick();
    power_clear_run_reason(POWER_RUN_WATCHDOG);
}

/*
* Test the watchdog by going into a tight loop so that eventually we will
* reset.
*/
CLI_RESULT wdog_cmd(uint8_t cmd_source __attribute__((unused)))
{
    while (1);
}
