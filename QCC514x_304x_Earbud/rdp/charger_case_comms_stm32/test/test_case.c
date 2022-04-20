/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for case.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>

#include "unity.h"

#include "case.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_gpio.h"
#include "mock_power.h"
#include "mock_cli.h"
#include "mock_ccp.h"
#include "mock_charger.h"
#include "mock_usb.h"
#include "mock_battery.h"
#include "mock_config.h"
#include "mock_charger_comms_device.h"
#include "mock_adc.h"

#ifdef EARBUD_CURRENT_SENSES
#include "mock_current_senses.h"
#endif

#ifdef USB_ENABLED
#include "mock_case_charger.h"
#endif

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

const char earbud_letter[NO_OF_EARBUDS] = { 'L', 'R' };
volatile uint32_t ticks;

static const CLI_COMMAND test_case_command[] =
{
    { "case",     case_cmd,     2 },
    { "EBSTATUS", ats_ebstatus, 2 },
    { "LID",      atq_lid,      2 },
    { "LOOPBACK", ats_loopback, 2 },
    { "SHIP",     ats_ship,     2 },
    { NULL }
};

uint16_t run_reason;

/*-----------------------------------------------------------------------------
------------------ EXPECT FUNCTIONS -------------------------------------------
-----------------------------------------------------------------------------*/

static void expect_set_run_reason(uint16_t rr)
{
    power_set_run_reason_Expect(rr);
    run_reason |= rr;
}

static void expect_clear_run_reason(uint16_t rr)
{
    power_clear_run_reason_Expect(rr);
    run_reason &= ~rr;
}

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_case_command, CLI_SOURCE_UART, str);
}

static void do_normal_startup(void)
{
    config_get_status_time_closed_ExpectAndReturn(600);
    config_get_shipping_mode_ExpectAndReturn(false);
#ifdef SCHEME_A
    current_senses_are_present_ExpectAndReturn(true);
#endif
    ccp_init_Ignore();
    charger_comms_device_init_Expect();
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    battery_read_request_Expect(false);
    case_init();

    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, false, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();
    TEST_ASSERT_EQUAL_HEX16(POWER_RUN_BROADCAST, run_reason);

    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    case_periodic();
}

#ifdef SCHEME_A
static void do_startup_comms_disabled(void)
{
    config_get_status_time_closed_ExpectAndReturn(600);
    config_get_shipping_mode_ExpectAndReturn(false);
    current_senses_are_present_ExpectAndReturn(false);
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    battery_read_request_Expect(false);
    case_init();

    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    case_periodic();
}
#endif

static void do_shipping_mode_startup_open(void)
{
    config_get_status_time_closed_ExpectAndReturn(600);
    config_get_shipping_mode_ExpectAndReturn(true);
    power_set_standby_reason_Expect(POWER_STANDBY_SHIPPING_MODE);
    case_init();

    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    charger_connected_ExpectAndReturn(false);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    expect_set_run_reason(POWER_RUN_SHIP);
    config_get_status_time_closed_ExpectAndReturn(60);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();
}

static void do_shipping_mode_startup_closed(void)
{
    config_get_status_time_closed_ExpectAndReturn(600);
    config_get_shipping_mode_ExpectAndReturn(true);
    power_set_standby_reason_Expect(POWER_STANDBY_SHIPPING_MODE);
    case_init();

    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
    expect_clear_run_reason(POWER_RUN_SHIP);
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    ticks = 0;

    lid_now = false;
    lid_before = false;
    chg_now = false;
    chg_before = false;
    case_event = true;
    case_dfu_planned = false;
    memset(case_earbud_status, 0, sizeof(case_earbud_status));
    case_status_on_timer = false;
    case_debug_mode = false; 
    in_shipping_mode = false;
    shipping_mode_lid_open_count = 0;
    run_time = 0;
    stop_set = false;
    comms_enabled = false;

    run_reason = 0;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

void test_case_lid_open(void)
{
    uint16_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+LID? with lid closed.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    cli_tx_Expect(CLI_SOURCE_UART, true, "0");
    do_cmd("LID?");

    /*
    * Display status information using 'case' command.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "Earbud  Present  Battery");
    cli_tx_Expect(CLI_SOURCE_UART, true, "L       No       255");
    cli_tx_Expect(CLI_SOURCE_UART, true, "R       No       255");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Lid : Closed");
    do_cmd("case");

    /*
    * Lid is opened, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed. The lid being
    * opened causes us to send a short status message immediately, read the
    * battery and start a status message exchange.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    charger_connected_ExpectAndReturn(false);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    battery_read_request_Expect(true);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    config_get_status_time_closed_ExpectAndReturn(1);
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(true, false, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Display status information using 'case' command.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "Earbud  Present  Battery");
    cli_tx_Expect(CLI_SOURCE_UART, true, "L       No       255");
    cli_tx_Expect(CLI_SOURCE_UART, true, "R       No       255");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Lid : Open (0s)");
    do_cmd("case");

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Battery read not yet done.
    */
    battery_read_done_ExpectAndReturn(false);
    case_periodic();

    /*
    * Battery read done, so broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(true, false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    /*
    * AT+LID? with lid open.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    cli_tx_Expect(CLI_SOURCE_UART, true, "1");
    do_cmd("LID?");

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Count the time that the lid stays open. Disable the periodic status
    * messages to make this easier.
    */

    case_enable_debug();

    for (n=1; n<=600; n++)
    {
        case_tick();
        TEST_ASSERT_EQUAL(n, lid_open_time);
    }

    for (n=1; n<=10; n++)
    {
        case_tick();
        TEST_ASSERT_EQUAL(600, lid_open_time);
    }

    /*
    * Lid is closed, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * Short status message sent, battery read request initiated.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    config_get_status_time_closed_ExpectAndReturn(600);
    battery_read_request_Expect(true);
    battery_percentage_current_ExpectAndReturn(100);
    ccp_tx_short_status_ExpectAndReturn(false, false, false, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();
}

void test_case_charger_connect_disconnect(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Charger is connected, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, true, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    /*
    * Charger is disconnected, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger disconnected");
    usb_disconnected_Expect();
    case_charger_disconnected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, false, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

void test_case_status(void)
{
    uint16_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RUN_TIME_BEFORE_STOP; n++)
    {
        case_tick();
    }

    /*
    * We have run for long enough, so the stop mode flag is set.
    */
    power_set_stop_reason_Expect(POWER_STOP_RUN_TIME);
    case_tick();

    for (n=2 + CASE_RUN_TIME_BEFORE_STOP; n<600; n++)
    {
        case_tick();
    }

    /*
    * It's time to exchange status information.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    config_get_status_time_closed_ExpectAndReturn(10);
    case_tick();

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Display status information using 'case' command.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "Earbud  Present  Battery");
    cli_tx_Expect(CLI_SOURCE_UART, true, "L       Yes      33");
    cli_tx_Expect(CLI_SOURCE_UART, true, "R       Yes      43");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Lid : Closed");
    do_cmd("case");

    /*
    * Disable the status timer.
    */
    case_enable_debug();

    /*
    * Status sequence not initiated.
    */
    for (n=0; n<20; n++)
    {
        case_tick();
    }
}

/*
* Periodic status message when lid is closed and both earbuds are fully
* charged.
*/
void test_case_status_fully_charged(void)
{
    uint32_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RUN_TIME_BEFORE_STOP; n++)
    {
        case_tick();
    }

    /*
    * We have run for long enough, so the stop mode flag is set.
    */
    power_set_stop_reason_Expect(POWER_STOP_RUN_TIME);
    case_tick();

    for (n=2 + CASE_RUN_TIME_BEFORE_STOP; n<600; n++)
    {
        case_tick();
    }

    /*
    * It's time to exchange status information.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    config_get_status_time_closed_ExpectAndReturn(10);
    case_tick();

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x64, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x64, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0x64, 0x64, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Lid is closed, and both earbuds are charged. Check that the countdown
    * has been set accordingly.
    */
    TEST_ASSERT_EQUAL(CASE_STATUS_TIME_CHARGED, case_status_countdown);

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    for (n=1; n<CASE_STATUS_TIME_CHARGED; n++)
    {
        case_tick();
    }

    /*
    * It's time to exchange status information.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    config_get_status_time_closed_ExpectAndReturn(10);
    case_tick();
}

/*
* Earbuds fail to respond to status requests.
*/
void test_case_status_no_response(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * It's time to exchange status information.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    case_start_status_sequence(false);

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Left earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (L)");
    case_no_response(EARBUD_LEFT);
    TEST_ASSERT_FALSE(case_earbud_status[EARBUD_RIGHT].present);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Right earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (R)");
    case_no_response(EARBUD_RIGHT);
    TEST_ASSERT_FALSE(case_earbud_status[EARBUD_RIGHT].present);

    /*
    * Left earbud goes back to idle state.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Right earbud goes back to idle state.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Broadcast message interrupts status.
*/
void test_case_broadcast_interrupts_status(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * It's time to exchange status information.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    case_start_status_sequence(false);

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Charger is connected interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, true, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (L)");
    case_abort(EARBUD_LEFT);

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (R)");
    case_abort(EARBUD_RIGHT);

    /*
    * Go back to the ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(50);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, true, false, false, 0x32, 0x21, 0x2B, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    /*
    * Nothing happens.
    */
    case_periodic();
}

/*
* Initiate status sequence using 'case status'.
*/
void test_case_status_cmd(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * 'case status' entered.
    */
    battery_read_request_Expect(true);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("case status"));

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    /*
    * Nothing happens.
    */
    case_periodic();
}

#ifdef SCHEME_A
/*
* 'case status' rejected because comms are disabled.
*/
void test_case_status_cmd_comms_disabled(void)
{
    /*
    * Startup.
    */
    do_startup_comms_disabled();

    /*
    * 'case status' rejected.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("case status"));
}
#endif

/*
* Initiate status sequence using AT+EBSTATUS.
*/
void test_case_at_ebstatus(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+EBSTATUS entered.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("EBSTATUS");

    /*
    * AT+EBSTATUS entered again, discarded.
    */
    do_cmd("EBSTATUS");

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (L): 33");
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (R): 43");
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    /*
    * Nothing happens.
    */
    case_periodic();
}

/*
* Status sequence initiated by AT+EBSTATUS fails.
*/
void test_case_at_ebstatus_failure(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+EBSTATUS entered.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("EBSTATUS");

    /*
    * AT+EBSTATUS entered again, discarded.
    */
    do_cmd("EBSTATUS");

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * No response from left earbud.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (L)");
    case_no_response(EARBUD_LEFT);

    /*
    * Failure message displayed, left earbud goes back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (L): Failed");
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * With nothing else to do, left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from right earbud received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (R): 43");
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Move to STATUS_BROADCAST.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_STATUS_BROADCAST, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0xFF, 0x2B, 0x00, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL_HEX16(0, run_reason);

    /*
    * Nothing happens.
    */
    case_periodic();
}

#ifdef SCHEME_A
/*
* AT+EBSTATUS rejected because comms are disabled.
*/
void test_case_at_ebstatus_comms_disabled(void)
{
    /*
    * Startup.
    */
    do_startup_comms_disabled();

    /*
    * AT+EBSTATUS entered.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("EBSTATUS"));
}
#endif

/*
* No reponse to extended status request.
*/
void test_case_xstatus_failure(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+EBSTATUS=l,0 entered.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("EBSTATUS=l,0"));

    /*
    * AT+EBSTATUS=l,2 entered, rejected because we are already handing an
    * extended status message.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("EBSTATUS=l,2"));

    /*
    * Status request message sent.
    */
    ccp_tx_xstatus_request_ExpectAndReturn(EARBUD_LEFT, 0, true);
    case_periodic();

    /*
    * No response from earbud.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (L)");
    case_no_response(EARBUD_LEFT);

    /*
    * Failure message displayed, left earbud goes back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (L): Failed");
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * With nothing else to do, left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);
}

/*
* Extended status request is interrupted by broadcast message.
*/
void test_case_broadcast_interrupts_xstatus(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+EBSTATUS=r,0 entered.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("EBSTATUS=r,0");

    /*
    * Send the status request message.
    */
    ccp_tx_xstatus_request_ExpectAndReturn(EARBUD_RIGHT, 0, true);
    case_periodic();

    /*
    * Charger is connected, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, true, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (R)");
    case_abort(EARBUD_RIGHT);

    /*
    * Go to the ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Attempt to send the status request message, but it is rejected because
    * the broadcast is in progress.
    */
    ccp_tx_xstatus_request_ExpectAndReturn(EARBUD_RIGHT, 0, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    /*
    * Re-send the status request.
    */
    ccp_tx_xstatus_request_ExpectAndReturn(EARBUD_RIGHT, 0, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (R): ABCD,EF,123456");
    case_rx_bt_address(EARBUD_RIGHT, 0xABCD, 0xEF, 0x123456);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Nothing to do, so back to idle.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Read bluetooth address using AT+EBSTATUS.
*/
void test_case_bluetooth_address(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+EBSTATUS=l,0 entered.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("EBSTATUS=l,0");

    /*
    * Attempt to send status request fails.
    */
    ccp_tx_xstatus_request_ExpectAndReturn(EARBUD_LEFT, 0, false);
    case_periodic();

    /*
    * Attempt to send status message succeeds.
    */
    ccp_tx_xstatus_request_ExpectAndReturn(EARBUD_LEFT, 0, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "EBSTATUS (L): 0002,5B,00EB21");
    case_rx_bt_address(EARBUD_LEFT, 0x0002, 0x5B, 0x00EB21);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Nothing to do, so back to idle.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Factory reset sequence.
*/
void test_case_factory_reset(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Invalid attempts to initiate a factory reset.
    */
    do_cmd("case reset");
    do_cmd("case reset 2");

    /*
    * Initiate a factory reset.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("case reset l");

    /*
    * First attempt to send the reset message fails.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Next time around, we do successfully send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Try to poll the earbud (using status request), but the attempt fails.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Next time round the poll is successful.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * No valid response from earbud.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (L)");
    case_give_up(EARBUD_LEFT);
    TEST_ASSERT_TRUE(case_earbud_status[EARBUD_LEFT].present);

    /*
    * Move to the RESET_DELAY state, to eventually trigger a retry.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll again.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Reset complete, go back to alert state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Go back to idle state.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

#ifdef SCHEME_A
/*
* Factory reset rejected because comms are disabled.
*/
void test_case_factory_reset_comms_disabled(void)
{
    /*
    * Startup.
    */
    do_startup_comms_disabled();

    /*
    * Attempt to initiate a factory reset rejected.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("case reset l"));
}
#endif

/*
* No response from earbud when attempting factory reset.
*/
void test_case_factory_reset_no_response(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Initiate a factory reset.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("case reset l");

    /*
    * Send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (L)");
    case_no_response(EARBUD_LEFT);
    TEST_ASSERT_FALSE(case_earbud_status[EARBUD_LEFT].present);

    /*
    * Reset complete, go back to ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing else to do, go back to IDLE state.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Factory reset sequence is interrupted by broadcast message.
*/
void test_case_broadcast_interrupts_factory_reset_1(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Initiate a factory reset.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("case reset l");

    /*
    * Send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Charger is connected, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, true, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (L)");
    case_abort(EARBUD_LEFT);

    /*
    * Go to the ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Attempt to send the reset message, but it is rejected because the
    * broadcast is in progress.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    /*
    * Next time around, we do successfully send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Reset complete, go back to ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing else to do, go back to IDLE state.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Factory reset sequence is interrupted by broadcast message.
*/
void test_case_broadcast_interrupts_factory_reset_2(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Initiate a factory reset.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("case reset l");

    /*
    * Send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Charger is connected, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, true, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (L)");
    case_abort(EARBUD_LEFT);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Move to the RESET_DELAY state, to eventually trigger a retry.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Reset complete, go back to ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing else to do, go back to IDLE state.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Earbud fails to respond following a Factory reset.
*/
void test_case_earbud_unresponsive_after_factory_reset(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Initiate a factory reset.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("case reset l");

    /*
    * Successfully send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * No valid response from earbud.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (L)");
    case_give_up(EARBUD_LEFT);
    TEST_ASSERT_TRUE(case_earbud_status[EARBUD_LEFT].present);

    /*
    * Move to the RESET_DELAY state, to eventually trigger a retry.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll again.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * No valid response from earbud.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (L)");
    case_give_up(EARBUD_LEFT);
    TEST_ASSERT_TRUE(case_earbud_status[EARBUD_LEFT].present);

    /*
    * Move to the RESET_DELAY state, to eventually trigger a retry.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll for the third time.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * No valid response from earbud.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (L)");
    case_give_up(EARBUD_LEFT);
    TEST_ASSERT_TRUE(case_earbud_status[EARBUD_LEFT].present);

    /*
    * That's enough attempts, go back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing to do, so go back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Firmware updates occur, disturbing the periodic status information exchange.
*/
void test_case_interrupted_by_dfu(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * We allow a DFU to take place, because nothing is happening.
    */
    TEST_ASSERT_TRUE(case_allow_dfu());

    /*
    * Reset command rejected because a DFU is in progress.
    */
    do_cmd("case reset r");

    /*
    * It's time to exchange status information, but we don't because a DFU
    * is in progress.
    */
    case_start_status_sequence(false);
    TEST_ASSERT_TRUE(case_dfu_planned);

    /*
    * DFU is finished (ie failed, because when successful we reset).
    */
    case_dfu_finished();
    TEST_ASSERT_FALSE(case_dfu_planned);

    /*
    * It's time to exchange status information.
    */
    battery_read_request_Expect(false);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    case_start_status_sequence(false);

    /*
    * Do not allow the requested DFU, because the left earbud is not in the
    * idle state.
    */
    TEST_ASSERT_FALSE(case_allow_dfu());
    TEST_ASSERT_TRUE(case_dfu_planned);

    /*
    * Request status of earbuds. Only the left succeeds at this point as
    * charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud status request now succeeds.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Response from left earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0, 0x21, 1);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from right earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0, 0x2B, 1);

    /*
    * Left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();

    /*
    * Attempt to broadcast status message fails.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, false);
    case_periodic();

    /*
    * Broadcast status message.
    */
    battery_read_done_ExpectAndReturn(true);
    battery_percentage_current_ExpectAndReturn(100);
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    case_periodic();

    /*
    * Right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Now we allow the DFU to take place.
    */
    TEST_ASSERT_TRUE(case_allow_dfu());

    /*
    * DFU is finished.
    */
    case_dfu_finished();
    TEST_ASSERT_FALSE(case_dfu_planned);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Loopback.
*/
void test_case_loopback_1(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Invalid command, shouldn't do anything.
    */
    do_cmd("case loopback");

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Invalid command (bad earbud parameter), shouldn't do anything.
    */
    do_cmd("case loopback x");

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Loopback to right earbud commanded.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("case loopback r");

    /*
    * Loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, case_earbud_status[EARBUD_RIGHT].loopback_data, 13, 13, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Looped-back data received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
    case_rx_loopback(EARBUD_RIGHT, case_earbud_status[EARBUD_RIGHT].loopback_data, 13);

    /*
    * Loopback is over, so right earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Multiple loopback messages.
*/
void test_case_loopback_2(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Request three loopback messages.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("LOOPBACK=r,3");

    for (n=0; n<3; n++)
    {
        /*
        * Loopback message is sent.
        */
        ccp_tx_loopback_ExpectWithArrayAndReturn(
            EARBUD_RIGHT, case_earbud_status[EARBUD_RIGHT].loopback_data, 13, 13, true);
        case_periodic();

        /*
        * Nothing happens.
        */
        case_periodic();

        /*
        * Looped-back data received.
        */
        cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
        case_rx_loopback(EARBUD_RIGHT, case_earbud_status[EARBUD_RIGHT].loopback_data, 13);
    }

    /*
    * Time has passed.
    */
    ticks = 100;

    /*
    * All done, so display report and go back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): Data rate = 78, NACKs = 0");

    /*
    * Loopback is over, so right earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Multiple loopback messages with a specified length.
*/
void test_case_loopback_3(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Request three loopback messages five bytes long.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("LOOPBACK=r,3,5");

    for (n=0; n<3; n++)
    {
        /*
        * Loopback message is sent.
        */
        ccp_tx_loopback_ExpectWithArrayAndReturn(
            EARBUD_RIGHT, case_earbud_status[EARBUD_RIGHT].loopback_data, 5, 5, true);
        case_periodic();

        /*
        * Nothing happens.
        */
        case_periodic();

        /*
        * Looped-back data received.
        */
        cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
        case_rx_loopback(EARBUD_RIGHT, case_earbud_status[EARBUD_RIGHT].loopback_data, 5);
    }

    /*
    * Time has passed.
    */
    ticks = 100;

    /*
    * All done, so display report and go back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): Data rate = 30, NACKs = 0");

    /*
    * Loopback is over, so right earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Multiple loopback messages with a specified data pattern.
*/
void test_case_loopback_4(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Request four loopback messages with a particular data pattern.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("LOOPBACK=r,4,0,abcdef1234");

    for (n=0; n<4; n++)
    {
        /*
        * Loopback message is sent.
        */
        ccp_tx_loopback_ExpectWithArrayAndReturn(
            EARBUD_RIGHT, "\xAB\xCD\xEF\x12\x34", 5, 5, true);
        case_periodic();

        /*
        * Nothing happens.
        */
        case_periodic();

        /*
        * Looped-back data received.
        */
        cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
        case_rx_loopback(EARBUD_RIGHT, "\xAB\xCD\xEF\x12\x34", 5);
    }

    /*
    * Time has passed.
    */
    ticks = 100;

    /*
    * All done, so display report and go back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): Data rate = 40, NACKs = 0");

    /*
    * Loopback is over, so right earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Multiple loopback messages with a specified data pattern repeated.
*/
void test_case_loopback_5(void)
{
    uint8_t n;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Request four loopback messages with a particular data pattern repeated
    * over ten bytes.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("LOOPBACK=r,4,10,aa55");

    for (n=0; n<4; n++)
    {
        /*
        * Loopback message is sent.
        */
        ccp_tx_loopback_ExpectWithArrayAndReturn(
            EARBUD_RIGHT, "\xAA\x55\xAA\x55\xAA\x55\xAA\x55\xAA\x55", 10, 10, true);
        case_periodic();

        /*
        * Nothing happens.
        */
        case_periodic();

        /*
        * Looped-back data received.
        */
        cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
        case_rx_loopback(EARBUD_RIGHT, "\xAA\x55\xAA\x55\xAA\x55\xAA\x55\xAA\x55", 10);
    }

    /*
    * Time has passed.
    */
    ticks = 100;

    /*
    * All done, so display report and go back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): Data rate = 80, NACKs = 0");

    /*
    * Loopback is over, so right earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Loopback interrupted by status broadcast.
*/
void test_case_broadcast_interrupts_loopback(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Loopback to left earbud commanded.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    do_cmd("case loopback l 0 0 abcdef01");

    /*
    * Loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_LEFT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Charger is connected interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
#ifdef USB_ENABLED
    cli_tx_Expect(CLI_BROADCAST, true, "Charger connected");
    case_charger_connected_Expect();
#endif
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(false, true, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (L)");
    case_abort(EARBUD_LEFT);

    /*
    * Go back to the ALERT state.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Attempt to send loopback message is rejected because we are
    * broadcasting.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_LEFT, "\xAB\xCD\xEF\x01", 4, 4, false);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    expect_clear_run_reason(POWER_RUN_BROADCAST);
    case_broadcast_finished();

    /*
    * Loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_LEFT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Looped-back data received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (L): OK");
    case_rx_loopback(EARBUD_LEFT, "\xAB\xCD\xEF\x01", 4);

    /*
    * Loopback is over, so left earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing else to do, so left earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Loopback fails.
*/
void test_case_loopback_failure(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Loopback to right earbud commanded.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("case loopback r 0 0 abcdef01");

    /*
    * Loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (R)");
    case_no_response(EARBUD_RIGHT);

    /*
    * Loopback failure indicated.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): Failed");
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

/*
* Send multiple loopback messages, NACKs and no responses happen.
*/
void test_case_multiple_loopbacks_bad(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Loopback to right earbud commanded.
    */
    expect_set_run_reason(POWER_RUN_STATUS_R);
    do_cmd("LOOPBACK=r,3,0,abcdef01");

    /*
    * Loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Looped-back data received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
    case_rx_loopback(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4);

    /*
    * Attempt to send second loopback message fails.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4, 4, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Second loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * The earbud didn't respond.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "No response (R)");
    case_no_response(EARBUD_RIGHT);

    /*
    * Loopback is over, so right earbud goes back to ALERT.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Second loopback message is sent again.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Looped-back data received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
    case_rx_loopback(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4);

    /*
    * Third loopback message is sent.
    */
    ccp_tx_loopback_ExpectWithArrayAndReturn(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4, 4, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * There was a NACK.
    */
    case_nack(EARBUD_RIGHT);

    /*
    * Looped-back data received.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): OK");
    case_rx_loopback(EARBUD_RIGHT, "\xAB\xCD\xEF\x01", 4);

    /*
    * Time has passed.
    */
    ticks = 100;

    /*
    * All done, so display report and go back to ALERT.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "LOOPBACK (R): Data rate = 24, NACKs = 1");
    case_periodic();
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing else to do, so right earbud goes back to IDLE.
    */
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);

    TEST_ASSERT_EQUAL_HEX16(0, run_reason);
}

#ifdef SCHEME_A
/*
* AT+LOOPBACK rejected because comms are disabled.
*/
void test_case_loopback_comms_disabled(void)
{
    /*
    * Startup.
    */
    do_startup_comms_disabled();

    /*
    * Loopback command rejected.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("LOOPBACK=r,3,0,abcdef01"));
}
#endif

/*
* Request shipping mode with AT command.
*/
void test_case_request_shipping_mode(void)
{
    uint8_t i,j;

    /*
    * Normal startup.
    */
    do_normal_startup();

    case_earbud_status[EARBUD_LEFT].present = true;
    case_earbud_status[EARBUD_RIGHT].present = true;

    /*
    * AT+SHIP entered.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    TEST_ASSERT_EQUAL(CLI_WAIT, do_cmd("SHIP"));
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Indicate shipping mode to earbuds. Only the left succeeds at this
    * point as charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_shipping_mode_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_shipping_mode_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud message now succeeds.
    */
    ccp_tx_shipping_mode_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Left earbud acccepts shipping mode.
    */
    case_rx_shipping(EARBUD_LEFT, true);

    /*
    * Display message, but don't do anything yet because the right earbud is
    * yet to report back.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "Shipping mode (L)");
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Right earbud acccepts shipping mode.
    */
    case_rx_shipping(EARBUD_RIGHT, true);

    /*
    * Sequence complete, case requests standby mode.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "Shipping mode (R)");
    cli_tx_Expect(CLI_SOURCE_UART, true, "OK");
    config_set_shipping_mode_ExpectAndReturn(true, true);
    power_set_standby_reason_Expect(POWER_STANDBY_SHIPPING_MODE);
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();

    /*
    * Nothing happens ever again.
    */
    for (i=0; i<10; i++)
    {
        case_tick();

        for (j=0; j<100; j++)
        {
            case_periodic();
        }
    }
}

/*
* Request shipping mode with AT command, but the earbuds reject it.
*/
void test_case_request_shipping_mode_rejected(void)
{
    uint8_t i,j;

    /*
    * Normal startup.
    */
    do_normal_startup();

    case_earbud_status[EARBUD_LEFT].present = true;
    case_earbud_status[EARBUD_RIGHT].present = true;

    /*
    * AT+SHIP entered.
    */
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    TEST_ASSERT_EQUAL(CLI_WAIT, do_cmd("SHIP"));
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_LEFT].state);
    TEST_ASSERT_EQUAL(CS_ALERT, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Indicate shipping mode to earbuds. Only the left succeeds at this
    * point as charger_comms will be busy by the time we get to the right.
    */
    ccp_tx_shipping_mode_ExpectAndReturn(EARBUD_LEFT, true);
    ccp_tx_shipping_mode_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * Right earbud message now succeeds.
    */
    ccp_tx_shipping_mode_ExpectAndReturn(EARBUD_RIGHT, true);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Left earbud rejects shipping mode.
    */
    case_rx_shipping(EARBUD_LEFT, false);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Right earbud rejects shipping mode.
    */
    case_rx_shipping(EARBUD_RIGHT, false);

    /*
    * Sequence complete, error reported because the earbuds rejected shipping
    * mode.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "ERROR");
    case_periodic();

    expect_clear_run_reason(POWER_RUN_STATUS_L);
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    case_periodic();
}

#ifdef SCHEME_A
/*
* Request shipping mode with AT command, charger comms disabled.
*/
void test_case_request_shipping_mode_comms_disabled(void)
{
    uint8_t i,j;

    /*
    * Startup.
    */
    do_startup_comms_disabled();

    /*
    * AT+SHIP entered. Case requests standby mode immediately.
    */
    config_set_shipping_mode_ExpectAndReturn(true, true);
    power_set_standby_reason_Expect(POWER_STANDBY_SHIPPING_MODE);
    expect_clear_run_reason(POWER_RUN_STATUS_L);
    expect_clear_run_reason(POWER_RUN_STATUS_R);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("SHIP"));

    /*
    * Nothing happens ever again.
    */
    for (i=0; i<10; i++)
    {
        case_tick();

        for (j=0; j<100; j++)
        {
            case_periodic();
        }
    }
}
#endif

/*
* Request shipping mode with AT command, fails because one or both earbuds are
* not present.
*/
void test_case_request_shipping_mode_empty(void)
{
    uint8_t i,j;

    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * AT+SHIP entered, no earbuds present.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("SHIP"));

    /*
    * AT+SHIP entered, only left earbud present.
    */
    case_earbud_status[EARBUD_LEFT].present = true;
    case_earbud_status[EARBUD_RIGHT].present = false;
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("SHIP"));

    /*
    * AT+SHIP entered, only right earbud present.
    */
    case_earbud_status[EARBUD_LEFT].present = false;
    case_earbud_status[EARBUD_RIGHT].present = true;
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("SHIP"));
}

/*
* Request shipping mode with AT command but lid is open.
*/
void test_case_request_shipping_mode_lid_open(void)
{
    /*
    * Normal startup.
    */
    do_normal_startup();

    /*
    * Lid is opened, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed. The lid being
    * opened causes us to send a short status message immediately, read the
    * battery and start a status message exchange.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    charger_connected_ExpectAndReturn(false);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    battery_read_request_Expect(true);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    config_get_status_time_closed_ExpectAndReturn(1);
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(true, false, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * AT+SHIP entered. Rejected because the lid is open.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("SHIP"));
}

/*
* Start up in shipping mode due to lid opening.
*/
void test_case_shipping_mode_startup_1(void)
{
    uint8_t i,j;

    /*
    * Start up due to lid opening.
    */
    do_shipping_mode_startup_open();

    /*
    * Lid stays open for a while, but not long enough to cause us to leave
    * shipping mode.
    */
    for (i=1; i<CASE_SHIPPING_TIME; i++)
    {
        case_periodic();
    }

    /*
    * Lid is closed, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * Process lid closure.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
    expect_clear_run_reason(POWER_RUN_SHIP);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    config_get_status_time_closed_ExpectAndReturn(600);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Nothing happens.
    */
    for (i=0; i<10; i++)
    {
        case_tick();

        for (j=0; j<100; j++)
        {
            case_periodic();
        }
    }

    /*
    * Lid is open, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * Process lid opening.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    charger_connected_ExpectAndReturn(false);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    expect_set_run_reason(POWER_RUN_SHIP);
    config_get_status_time_closed_ExpectAndReturn(60);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Lid stays open for a while, but not long enough to cause us to leave
    * shipping mode.
    */
    for (i=1; i<CASE_SHIPPING_TIME; i++)
    {
        case_periodic();
    }

    /*
    * Lid has been open long enough for us to leave shipping mode.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Leaving shipping mode");
    config_set_shipping_mode_ExpectAndReturn(false, true);
    power_clear_standby_reason_Expect(POWER_STANDBY_SHIPPING_MODE);
#ifdef SCHEME_A
    current_senses_are_present_ExpectAndReturn(true);
#endif
    ccp_init_Ignore();
    charger_comms_device_init_Expect();
    ccp_init_Ignore();
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    battery_read_request_Expect(false);
    expect_clear_run_reason(POWER_RUN_SHIP);
    case_periodic();

    /*
    * Send short status message immediately and initiate status sequence.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    charger_connected_ExpectAndReturn(false);
#ifdef EARBUD_CURRENT_SENSES
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_MONITORING);
#endif
    battery_read_request_Expect(true);
    expect_set_run_reason(POWER_RUN_STATUS_L);
    expect_set_run_reason(POWER_RUN_STATUS_R);
    config_get_status_time_closed_ExpectAndReturn(1);
    battery_percentage_current_ExpectAndReturn(0);
    ccp_tx_short_status_ExpectAndReturn(true, false, true, true);
    expect_set_run_reason(POWER_RUN_BROADCAST);
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();
}

/*
* Start up in shipping mode due to charger being connected.
*/
void test_case_shipping_mode_startup_2(void)
{
    uint8_t i,j;

    /*
    * Start up with the lid closed.
    */
    do_shipping_mode_startup_closed();

    /*
    * Nothing happens as we are still in shipping mode.
    */
    for (i=0; i<10; i++)
    {
        case_tick();

        for (j=0; j<100; j++)
        {
            case_periodic();
        }
    }

    /*
    * Charger removed, interrupt occurs.
    */
    expect_set_run_reason(POWER_RUN_CASE_EVENT);
    case_event_occurred();

    /*
    * Process charger removal.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
    expect_clear_run_reason(POWER_RUN_SHIP);
    cli_tx_Expect(CLI_BROADCAST, true, "Charger disconnected");
    usb_disconnected_Expect();
    case_charger_disconnected_Expect();
    expect_clear_run_reason(POWER_RUN_CASE_EVENT);
    case_periodic();

    /*
    * Nothing happens as we are still in shipping mode.
    */
    for (i=0; i<10; i++)
    {
        case_tick();

        for (j=0; j<100; j++)
        {
            case_periodic();
        }
    }
}
