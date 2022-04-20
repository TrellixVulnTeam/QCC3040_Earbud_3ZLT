/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Debug commands for the LD20-11114 board
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "debug.h"
#include "case.h"
#include "power.h"
#include "pfn.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void debug_enable_test_mode(bool enable, uint8_t cmd_source)
{
    if (enable)
    {
        cli_broadcast_disable(cmd_source);
        pfn_set_runnable("led", false);
        case_enable_debug();
        power_set_run_reason(POWER_RUN_DEBUG);
    }
    else
    {
        cli_broadcast_enable(cmd_source);
        pfn_set_runnable("led", true);
        case_disable_debug();
        power_clear_run_reason(POWER_RUN_DEBUG);
    }
}

CLI_RESULT ats_test(uint8_t cmd_source)
{
    long int x;

    if (cli_get_next_parameter(&x, 10))
    {
        debug_enable_test_mode((bool)x, cmd_source);
    }

    return CLI_OK;
}
