/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for led.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "led.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_timer.h"
#include "mock_gpio.h"
#include "mock_power.h"
#include "mock_cli.h"
#include "mock_charger.h"
#include "mock_case_charger.h"
#include "mock_battery.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_led_command[] =
{
    { "LED", ats_led, 2 },
    { NULL }
};

static const LED_SEQUENCE led_seq_test_1 =
{
    10, 1,
    {
        { LED_COLOUR_CYAN, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_test_2 =
{
    10, 1,
    {
        { LED_COLOUR_MAGENTA, LED_PHASE_FOREVER },
    }
};

/*-----------------------------------------------------------------------------
------------------ EXPECT FUNCTIONS -------------------------------------------
-----------------------------------------------------------------------------*/

void expect_red(void)
{
    gpio_enable_Expect(GPIO_LED_RED);
    gpio_disable_Expect(GPIO_LED_GREEN);
    gpio_disable_Expect(GPIO_LED_BLUE);
}

void expect_green(void)
{
    gpio_disable_Expect(GPIO_LED_RED);
    gpio_enable_Expect(GPIO_LED_GREEN);
    gpio_disable_Expect(GPIO_LED_BLUE);
}

void expect_amber(void)
{
    gpio_enable_Expect(GPIO_LED_RED);
    gpio_enable_Expect(GPIO_LED_GREEN);
    gpio_disable_Expect(GPIO_LED_BLUE);
}

void expect_blue(void)
{
    gpio_disable_Expect(GPIO_LED_RED);
    gpio_disable_Expect(GPIO_LED_GREEN);
    gpio_enable_Expect(GPIO_LED_BLUE);
}

void expect_magenta(void)
{
    gpio_enable_Expect(GPIO_LED_RED);
    gpio_disable_Expect(GPIO_LED_GREEN);
    gpio_enable_Expect(GPIO_LED_BLUE);
}

void expect_cyan(void)
{
    gpio_disable_Expect(GPIO_LED_RED);
    gpio_enable_Expect(GPIO_LED_GREEN);
    gpio_enable_Expect(GPIO_LED_BLUE);
}

void expect_white(void)
{
    gpio_enable_Expect(GPIO_LED_RED);
    gpio_enable_Expect(GPIO_LED_GREEN);
    gpio_enable_Expect(GPIO_LED_BLUE);
}

void expect_off(void)
{
    gpio_disable_Expect(GPIO_LED_RED);
    gpio_disable_Expect(GPIO_LED_GREEN);
    gpio_disable_Expect(GPIO_LED_BLUE);
}

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_led_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    led_ctr = 0;
    led_overall_ctr = 0;
    led_phase_ctr = 0;
    led_colour = LED_COLOUR_OFF;
    led_seq = NULL;
    led_event_seq = NULL;
    memset(led_event_queue, 0, sizeof(led_event_queue));
    led_queue_head = 0;
    led_queue_tail = 0;

    led_init();
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Battery level indications.
*/
void test_led_battery_indications(void)
{
    int i;

    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Battery indication requested.
    */
    power_set_run_reason_Expect(POWER_RUN_LED);
    led_indicate_battery(50);

    expect_amber();
    led_periodic();

    for (i=1; i<500; i++)
    {
        led_periodic();
    }

    expect_off();
    power_clear_run_reason_Expect(POWER_RUN_LED);
    led_periodic();

    /*
    * Battery indication requested.
    */
    power_set_run_reason_Expect(POWER_RUN_LED);
    led_indicate_battery(20);

    expect_red();
    led_periodic();

    for (i=1; i<500; i++)
    {
        led_periodic();
    }

    expect_off();
    power_clear_run_reason_Expect(POWER_RUN_LED);
    led_periodic();

    /*
    * Battery indication requested.
    */
    power_set_run_reason_Expect(POWER_RUN_LED);
    led_indicate_battery(99);

    expect_green();
    led_periodic();

    for (i=1; i<500; i++)
    {
        led_periodic();
    }

    expect_off();
    power_clear_run_reason_Expect(POWER_RUN_LED);
    led_periodic();
}

/*
* Queued indications.
*/
void test_led_queued_indications(void)
{
    int i;

    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Battery indication requested.
    */
    power_set_run_reason_Expect(POWER_RUN_LED);
    led_indicate_battery(20);

    expect_red();
    led_periodic();

    for (i=1; i<100; i++)
    {
        led_periodic();
    }

    /*
    * Battery indication requested, queued because we are still displaying
    * the previous indication.
    */
    led_indicate_battery(50);

    /*
    * Battery indication requested, not queued because it is identical to
    * an indication already in the queue.
    */
    led_indicate_battery(50);

    for (i=0; i<100; i++)
    {
        led_periodic();
    }

    /*
    * Battery indication requested, also queued.
    */
    led_indicate_battery(99);

    /*
    * Battery indication requested, not queued because it is identical to
    * an indication already in the queue.
    */
    led_indicate_battery(50);

    for (i=0; i<300; i++)
    {
        led_periodic();
    }

    /*
    * Attempt to queue two more indications. That's one too many, so the last
    * should be disregarded.
    */
    led_indicate_event(&led_seq_test_1);
    led_indicate_event(&led_seq_test_2);

    /*
    * Finished displaying the first indication.
    */
    expect_off();
    led_periodic();

    /*
    * Display the next indication in the queue.
    */
    expect_amber();
    led_periodic();

    for (i=1; i<500; i++)
    {
        led_periodic();
    }

    /*
    * Finished displaying the second indication.
    */
    expect_off();
    led_periodic();

    /*
    * Display the next indication in the queue.
    */
    expect_green();
    led_periodic();

    for (i=1; i<500; i++)
    {
        led_periodic();
    }

    /*
    * Finished displaying the third indication.
    */
    expect_off();
    led_periodic();

    /*
    * Display the next indication in the queue.
    */
    expect_cyan();
    led_periodic();

    for (i=1; i<10; i++)
    {
        led_periodic();
    }

    /*
    *  Finished displaying the fourth and final indication.
    */
    expect_off();
    power_clear_run_reason_Expect(POWER_RUN_LED);
    led_periodic();

    /*
    * Back to normal.
    */
    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }
}

/*
* Indication requested, but we are already displaying that indication so it
* is ignored.
*/
void test_led_same_indication(void)
{
    int i;

    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Battery indication requested.
    */
    power_set_run_reason_Expect(POWER_RUN_LED);
    led_indicate_battery(20);

    expect_red();
    led_periodic();

    for (i=1; i<100; i++)
    {
        led_periodic();
    }

    /*
    * Battery indication requested, ignored because we are still displaying
    * that exact indication.
    */
    led_indicate_battery(20);

    for (i=0; i<400; i++)
    {
        led_periodic();
    }

    /*
    *  Finished displaying the indication.
    */
    expect_off();
    power_clear_run_reason_Expect(POWER_RUN_LED);
    led_periodic();

    /*
    * Back to normal.
    */
    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }
}

/*
* Background (permanent) indications.
*/
void test_led_background(void)
{
    int i;

    /*
    * Charger not connected, LED off.
    */
    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Charger connected, but not yet charging, LED stays off.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(10);
    charger_is_charging_ExpectAndReturn(false);
    led_periodic();

    /*
    * Charging begins, LED begins to flash red.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(10);
    charger_is_charging_ExpectAndReturn(true);
    expect_red();
    led_periodic();

    for (i=1; i<100; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(11);
        charger_is_charging_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(12);
    charger_is_charging_ExpectAndReturn(true);
    expect_off();
    led_periodic();

    for (i=1; i<100; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(13);
        charger_is_charging_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(14);
    charger_is_charging_ExpectAndReturn(true);
    expect_red();
    led_periodic();

    /*
    * Charged to 30%, LED begins to flash green.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(30);
    charger_is_charging_ExpectAndReturn(true);
    expect_green();
    led_periodic();

    for (i=1; i<100; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(31);
        charger_is_charging_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(32);
    charger_is_charging_ExpectAndReturn(true);
    expect_off();
    led_periodic();

    for (i=1; i<100; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(33);
        charger_is_charging_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(34);
    charger_is_charging_ExpectAndReturn(true);
    expect_green();
    led_periodic();

    for (i=1; i<100; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(31);
        charger_is_charging_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(32);
    charger_is_charging_ExpectAndReturn(true);
    expect_off();
    led_periodic();

    /*
    * Charged to 95%, LED goes solid green.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(95);
    charger_is_charging_ExpectAndReturn(true);
    expect_green();
    led_periodic();

    for (i=1; i<200; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(98);
        charger_is_charging_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Stopped charging, LED stays solid green.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(98);
    charger_is_charging_ExpectAndReturn(false);
    led_periodic();

    for (i=1; i<200; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(98);
        charger_is_charging_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Temperature out of range, LED flashes red.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(true);
    expect_red();
    led_periodic();

    for (i=1; i<10; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(true);
    expect_off();
    led_periodic();

    for (i=1; i<10; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(true);
        led_periodic();
    }

    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(true);
    expect_red();
    led_periodic();
}

/*
* AT+LED.
*/
void test_led_at_command(void)
{
    int i;

    /*
    * Charger not connected, LED off.
    */
    for (i=0; i<500; i++)
    {
        charger_connected_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Charger connected, fully charged, LED goes solid green.
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(98);
    charger_is_charging_ExpectAndReturn(false);
    expect_green();
    led_periodic();

    /*
    * AT+LED=0 (off).
    */
    expect_off();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=0"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=1 (red).
    */
    expect_red();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=1"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=2 (green).
    */
    expect_green();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=2"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=3 (amber).
    */
    expect_amber();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=3"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=4 (blue).
    */
    expect_blue();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=4"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=5 (magenta).
    */
    expect_magenta();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=5"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=6 (cyan).
    */
    expect_cyan();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=6"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED=7 (white).
    */
    expect_white();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED=7"));

    for (i=0; i<10; i++)
    {
        led_periodic();
    }

    /*
    * AT+LED with no parameters (stop forcing the LED colour).
    */
    expect_off();
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("LED"));

    /*
    * Background indication returns (solid green for being fully charged).
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(98);
    charger_is_charging_ExpectAndReturn(false);
    expect_green();
    led_periodic();

    for (i=0; i<10; i++)
    {
        charger_connected_ExpectAndReturn(true);
        case_charger_temperature_fault_ExpectAndReturn(false);
        battery_percentage_current_ExpectAndReturn(98);
        charger_is_charging_ExpectAndReturn(false);
        led_periodic();
    }

    /*
    * Go to sleep.
    */
    expect_off();
    led_sleep();

    /*
    * Wake up.
    */
    led_wake();

    /*
    * Background indication returns (solid green for being fully charged).
    */
    charger_connected_ExpectAndReturn(true);
    case_charger_temperature_fault_ExpectAndReturn(false);
    battery_percentage_current_ExpectAndReturn(98);
    charger_is_charging_ExpectAndReturn(false);
    expect_green();
    led_periodic();
}
