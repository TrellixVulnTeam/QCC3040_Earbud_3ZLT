/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Periodic functions
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "main.h"
#include "timer.h"
#include "ccp.h"
#include "led.h"
#include "uart.h"
#include "wire.h"
#include "wdog.h"
#include "power.h"
#include "case.h"
#include "pfn.h"
#include "charger_comms_device.h"
#include "dfu.h"
#include "usb.h"
#include "battery.h"
#include "charger_detect.h"
#include "case_charger.h"
#ifdef TEST
#include "test_st.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    char *name;
    void (*fn)(void);
}
PERIODIC_FN;

typedef struct
{
    uint64_t total;
    uint32_t slowest;
    uint32_t runs;
    uint32_t slow;
    bool stopped;
}
PERIODIC_STATUS;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT pfn_cmd_status(uint8_t cmd_source);
static CLI_RESULT pfn_cmd_start(uint8_t cmd_source);
static CLI_RESULT pfn_cmd_stop(uint8_t cmd_source);
static CLI_RESULT pfn_cmd_reset(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const PERIODIC_FN pfn[] =
{
    { "wdog",    wdog_periodic           },
    { "uart_tx", uart_tx_periodic        },
    { "uart_rx", uart_rx_periodic        },
    { "led",     led_periodic            },
    { "ccp",     ccp_periodic            },
    { "dfu",     dfu_periodic            },
#ifdef USB_ENABLED
    { "usb_tx",  usb_tx_periodic         },
    { "usb_rx",  usb_rx_periodic         },
    { "chg_det", charger_detect_periodic },
    { "charger", case_charger_periodic   },
#endif
    { "wire",    wire_periodic           },
    { "battery", battery_periodic        },
    { "case",    case_periodic           },
    { "power",   power_periodic          },
    { "c_comms", charger_comms_periodic  },
    { NULL }
};

static PERIODIC_STATUS pfn_status[sizeof(pfn) / sizeof(PERIODIC_FN)];

static const CLI_COMMAND pfn_command[] =
{
    { "",      pfn_cmd_status, 2 },
    { "start", pfn_cmd_start,  2 },
    { "stop",  pfn_cmd_stop,   2 },
    { "reset", pfn_cmd_reset,  2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void pfn_set_runnable(char *module_name, bool running)
{
    uint8_t n;

    for (n=0; pfn[n].fn; n++)
    {
        if (!strcmp(module_name, pfn[n].name))
        {
            pfn_status[n].stopped = (running) ? false:true;
            break;
        }
    }
}

static CLI_RESULT pfn_cmd_status(uint8_t cmd_source __attribute__((unused)))
{
    uint8_t n;

    for (n=0; pfn[n].fn; n++)
    {
        PRINTF("%-7s  %s  %-5u  %-5u  %lu",
            pfn[n].name,
            (pfn_status[n].stopped) ? "STOP":"    ",
            (uint32_t)(pfn_status[n].total / pfn_status[n].runs),
            pfn_status[n].slow,
            pfn_status[n].slowest);
    }

    return CLI_OK;
}

static CLI_RESULT pfn_cmd_start(uint8_t cmd_source __attribute__((unused)))
{
    char *module = cli_get_next_token();
    if (module)
    {
        pfn_set_runnable(module, true);
    }
    return CLI_OK;
}

static CLI_RESULT pfn_cmd_stop(uint8_t cmd_source __attribute__((unused)))
{
    char *module = cli_get_next_token();
    if (module)
    {
        pfn_set_runnable(module,false);
    }
    return CLI_OK;
}

static CLI_RESULT pfn_cmd_reset(uint8_t cmd_source __attribute__((unused)))
{
    memset(pfn_status, 0, sizeof(pfn_status));
    return CLI_OK;
}

CLI_RESULT pfn_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(pfn_command, cmd_source);
}


void pfn_periodic(void)
{
    uint8_t n;

    for (n=0; pfn[n].fn; n++)
    {
#ifdef FAST_TIMER_INTERRUPT
        uint64_t t_start = global_time_us;
        uint64_t t_run;
#else
        uint32_t t_start = TIM14->CNT;
        uint32_t t_now;
        uint32_t t_run;
#endif

        if (pfn_status[n].stopped)
        {
            continue;
        }

        pfn[n].fn();

#ifdef FAST_TIMER_INTERRUPT
        t_run = global_time_us - t_start;
#else
        t_now = TIM14->CNT;
        t_run = t_now - t_start;

        if (t_now < t_start)
        {
            t_run = (0xFFFF - t_start) + t_now + 1;
        }
        else
        {
            t_run = t_now - t_start;
        }
#endif
        pfn_status[n].runs++;
        pfn_status[n].total += t_run;

        if (t_run > pfn_status[n].slowest)
        {
            pfn_status[n].slowest = (uint32_t)t_run;
        }

        if (t_run > 500)
        {
            pfn_status[n].slow++;
        }
    }
}
