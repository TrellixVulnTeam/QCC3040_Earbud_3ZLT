/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for charger_comm.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "pfn.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_timer.h"
#include "mock_ccp.h"
#include "mock_led.h"
#include "mock_uart.h"
#include "mock_wire.h"
#include "mock_wdog.h"
#include "mock_power.h"
#include "mock_case.h"
#include "mock_charger_comms_device.h"
#include "mock_dfu.h"
#include "mock_usb.h"
#include "mock_battery.h"
#include "mock_charger_detect.h"
#include "mock_case_charger.h"
#include "mock_cli.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_pfn_command[] =
{
    { "pfn", pfn_cmd, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_pfn_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    memset(pfn_status, 0, sizeof(pfn_status));
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Periodic functions.
*/
void test_pfn(void)
{
    /*
    * All periodic functions called.
    */
    wdog_periodic_Expect();
    uart_tx_periodic_Expect();
    uart_rx_periodic_Expect();
    led_periodic_Expect();
    ccp_periodic_Expect();
    dfu_periodic_Expect();
    usb_tx_periodic_Expect();
    usb_rx_periodic_Expect();
    charger_detect_periodic_Expect();
    case_charger_periodic_Expect();
    wire_periodic_Expect();
    battery_periodic_Expect();
    case_periodic_Expect();
    power_periodic_Expect();
    charger_comms_periodic_Expect();
    pfn_periodic();

    /*
    * Status display.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "wdog           0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "uart_tx        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "uart_rx        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "led            0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "ccp            0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "dfu            0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "usb_tx         0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "usb_rx         0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "chg_det        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "charger        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "wire           0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "battery        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "case           0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "power          0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "c_comms        0      0      0");
    do_cmd("pfn");

    /*
    * Stop the LED module.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("pfn stop led"));

    /*
    * All periodic functions called except the LED module because it has been
    * stopped.
    */
    wdog_periodic_Expect();
    uart_tx_periodic_Expect();
    uart_rx_periodic_Expect();
    ccp_periodic_Expect();
    dfu_periodic_Expect();
    usb_tx_periodic_Expect();
    usb_rx_periodic_Expect();
    charger_detect_periodic_Expect();
    case_charger_periodic_Expect();
    wire_periodic_Expect();
    battery_periodic_Expect();
    case_periodic_Expect();
    power_periodic_Expect();
    charger_comms_periodic_Expect();
    pfn_periodic();

    /*
    * Status display shows that the LED module is stopped.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "wdog           0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "uart_tx        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "uart_rx        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "led      STOP  0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "ccp            0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "dfu            0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "usb_tx         0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "usb_rx         0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "chg_det        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "charger        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "wire           0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "battery        0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "case           0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "power          0      0      0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "c_comms        0      0      0");
    do_cmd("pfn");

    /*
    * Start the LED module.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("pfn start led"));

    /*
    * All periodic functions called.
    */
    wdog_periodic_Expect();
    uart_tx_periodic_Expect();
    uart_rx_periodic_Expect();
    led_periodic_Expect();
    ccp_periodic_Expect();
    dfu_periodic_Expect();
    usb_tx_periodic_Expect();
    usb_rx_periodic_Expect();
    charger_detect_periodic_Expect();
    case_charger_periodic_Expect();
    wire_periodic_Expect();
    battery_periodic_Expect();
    case_periodic_Expect();
    power_periodic_Expect();
    charger_comms_periodic_Expect();
    pfn_periodic();

    /*
    * Check that pfn reset causes the data to be reset.
    */
    TEST_ASSERT_EQUAL(3, pfn_status[0].runs);
    do_cmd("pfn reset");
    TEST_ASSERT_EQUAL(0, pfn_status[0].runs);

    /*
    * Module absent or invalid, do nothing.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("pfn stop"));
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("pfn stop xxxxx"));
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("pfn start"));
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("pfn start xxxxx"));
}
