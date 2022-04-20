/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for case_charger.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "case_charger.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_cli.h"
#include "mock_usb.h"
#include "mock_charger_detect.h"
#include "mock_charger.h"
#include "mock_power.h"
#include "mock_battery.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_command[] =
{
    { "CHARGER", ats_charger, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    case_charger_status.state = CASE_CHARGER_IDLE;
    requested_mode = CHARGER_CURRENT_MODE_100MA;
    charger_reason = 0;
    charger_enabled_now = false;
    temperature_ok = true;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Normal connect and disconnect.
*/
void test_case_charger(void)
{
    uint8_t n;

    case_charger_periodic();

    usb_chg_detected_Expect();
    case_charger_connected();

    power_set_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    charger_enable_Expect(true);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_USB_ENUMERATION_TIMEOUT; n++)
    {
        usb_has_enumerated_ExpectAndReturn(false);
        case_charger_periodic();
    }

    case_charger_periodic();

    charger_connected_ExpectAndReturn(true);
    charger_detect_get_type_ExpectAndReturn(CHARGER_DETECT_TYPE_SDP);
    cli_tx_Expect(CLI_BROADCAST, true, "USB type = 1");
    usb_has_enumerated_ExpectAndReturn(false);
    case_charger_periodic();

    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    case_charger_periodic();

    case_charger_disconnected();

    charger_enable_Expect(false);
    charger_detect_cancel_Expect();
    power_clear_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    case_charger_periodic();
}

/*
* Temperature gets too low, charging turned off.
*/
void test_case_charger_too_cold(void)
{
    uint8_t n;

    case_charger_periodic();

    usb_chg_detected_Expect();
    case_charger_connected();

    TEST_ASSERT_FALSE(case_charger_temperature_fault());

    power_set_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    charger_enable_Expect(true);
    case_charger_periodic();

    usb_has_enumerated_ExpectAndReturn(true);
    case_charger_periodic();

    charger_connected_ExpectAndReturn(true);
    charger_detect_get_type_ExpectAndReturn(CHARGER_DETECT_TYPE_SDP);
    cli_tx_Expect(CLI_BROADCAST, true, "USB type = 1");
    usb_has_enumerated_ExpectAndReturn(true);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_500MA);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    battery_read_ntc_ExpectAndReturn(2000);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    TEST_ASSERT_FALSE(case_charger_temperature_fault());

    battery_read_ntc_ExpectAndReturn(2400);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_STANDBY);
    charger_enable_Expect(false);
    case_charger_periodic();

    TEST_ASSERT_TRUE(case_charger_temperature_fault());

    case_charger_disconnected();

    charger_detect_cancel_Expect();
    power_clear_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    case_charger_periodic();
}

/*
* Temperature gets too high, charging turned off.
*/
void test_case_charger_too_hot(void)
{
    uint8_t n;

    case_charger_periodic();

    usb_chg_detected_Expect();
    case_charger_connected();

    TEST_ASSERT_FALSE(case_charger_temperature_fault());

    power_set_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    charger_enable_Expect(true);
    case_charger_periodic();

    usb_has_enumerated_ExpectAndReturn(true);
    case_charger_periodic();

    charger_connected_ExpectAndReturn(true);
    charger_detect_get_type_ExpectAndReturn(CHARGER_DETECT_TYPE_SDP);
    cli_tx_Expect(CLI_BROADCAST, true, "USB type = 1");
    usb_has_enumerated_ExpectAndReturn(true);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_500MA);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    TEST_ASSERT_FALSE(case_charger_temperature_fault());

    battery_read_ntc_ExpectAndReturn(1000);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_STANDBY);
    charger_enable_Expect(false);
    case_charger_periodic();

    TEST_ASSERT_TRUE(case_charger_temperature_fault());

    case_charger_disconnected();

    charger_detect_cancel_Expect();
    power_clear_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    case_charger_periodic();
}

/*
* Battery reading causes charging to be temporarily disabled.
*/
void test_case_charger_battery_read(void)
{
    uint8_t n;

    case_charger_periodic();

    usb_chg_detected_Expect();
    case_charger_connected();

    power_set_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    charger_enable_Expect(true);
    case_charger_periodic();

    usb_has_enumerated_ExpectAndReturn(true);
    case_charger_periodic();

    charger_connected_ExpectAndReturn(true);
    charger_detect_get_type_ExpectAndReturn(CHARGER_DETECT_TYPE_SDP);
    cli_tx_Expect(CLI_BROADCAST, true, "USB type = 1");
    usb_has_enumerated_ExpectAndReturn(true);
    case_charger_periodic();

    for (n=0; n<CASE_CHARGER_MONITOR_PERIOD; n++)
    {
        case_charger_periodic();
    }

    battery_read_ntc_ExpectAndReturn(1500);
    charger_set_current_Expect(CHARGER_CURRENT_MODE_500MA);
    case_charger_periodic();

    charger_set_current_Expect(CHARGER_CURRENT_MODE_STANDBY);
    charger_set_reason(CHARGER_OFF_BATTERY_READ);

    case_charger_periodic();

    charger_clear_reason(CHARGER_OFF_BATTERY_READ);

    case_charger_disconnected();

    charger_enable_Expect(false);
    charger_detect_cancel_Expect();
    power_clear_run_reason_Expect(POWER_RUN_CHG_CONNECTED);
    case_charger_periodic();
}

/*
* AT+CHARGER and AT+CHARGER? commands.
*/
void test_case_charger_at_commands(void)
{
    /*
    * AT+CHARGER without parameters.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER"));

    /*
    * AT+CHARGER=0.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=0"));

    /*
    * AT+CHARGER=1.
    */
    charger_enable_Expect(true);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=1"));

    /*
    * AT+CHARGER=1,0.
    */
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=1,0"));

    /*
    * AT+CHARGER=1,1.
    */
    charger_set_current_Expect(CHARGER_CURRENT_MODE_500MA);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=1,1"));

    /*
    * AT+CHARGER=1,2.
    */
    charger_set_current_Expect(CHARGER_CURRENT_MODE_ILIM);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=1,2"));

    /*
    * AT+CHARGER=1,3.
    */
    charger_set_current_Expect(CHARGER_CURRENT_MODE_STANDBY);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=1,3"));

    /*
    * AT+CHARGER=1,0.
    */
    charger_set_current_Expect(CHARGER_CURRENT_MODE_100MA);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("CHARGER=1,0"));

    /*
    * AT+CHARGER?
    */
    charger_current_mode_ExpectAndReturn(CHARGER_CURRENT_MODE_100MA);
    charger_is_charging_ExpectAndReturn(false);
    charger_connected_ExpectAndReturn(true);
    cli_tx_Expect(CLI_SOURCE_UART, true, "1,0,0");
    TEST_ASSERT_EQUAL(CLI_OK, atq_charger(CLI_SOURCE_UART));
}
