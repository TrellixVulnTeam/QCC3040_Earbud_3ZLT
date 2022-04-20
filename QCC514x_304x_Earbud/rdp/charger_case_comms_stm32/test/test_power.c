/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for power.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "power.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_timer.h"
#include "mock_gpio.h"
#include "mock_adc.h"
#include "mock_led.h"
#include "mock_memory.h"
#include "mock_rtc.h"
#include "mock_cli.h"
#include "mock_uart.h"
#include "mock_vreg.h"
#include "mock_clock.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_power_command[] =
{
    { "power", power_cmd, 2 },
    { NULL }
};

static const CLI_COMMAND test_ats_power_command[] =
{
    { "POWER", ats_power, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ EXPECT FUNCTIONS -------------------------------------------
-----------------------------------------------------------------------------*/

void expect_go_to_sleep(void)
{
    led_sleep_Expect();
    adc_sleep_Expect();
    timer_sleep_Expect();
#ifdef VARIANT_CB
    vreg_pfm_Expect();
#endif
    gpio_clock_disable_Expect();
}

void expect_wake_up(void)
{
    gpio_clock_enable_Expect();
    timer_wake_Expect();
    led_wake_Expect();
    adc_wake_Expect();
}

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_power_command, CLI_SOURCE_UART, str);
}

static CLI_RESULT do_at_cmd(char *str)
{
    return common_cmd(test_ats_power_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    power_reason_to_run = 0;
    power_reason_to_stop = 0;
    power_reason_to_reset_stop = 0;
    power_reason_to_standby = 0;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Run mode forcing.
*/
void test_power_on_off(void)
{
    /*
    * Go to sleep and eventually wake up, as is normal.
    */
    expect_go_to_sleep();
    expect_wake_up();
    power_periodic();

    /*
    * Force run mode using the 'power on' command.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("power on"));

    /*
    * Read power status.
    */
    cli_tx_Expect(CLI_SOURCE_UART, true, "0x00002000");
    do_cmd("power");

    /*
    * Don't go to sleep, because run mode is forced on.
    */
    power_periodic();

    /*
    * Stop forcing run mode.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("power off"));

    /*
    * No reason to run, so go to sleep.
    */
    expect_go_to_sleep();
    expect_wake_up();
    power_periodic();
}

/*
* Go to sleep using the AT+POWER command.
*/
void test_power_sleep(void)
{
    /*
    * Parameter omitted.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_at_cmd("POWER"));

    /*
    * Set low power mode to sleep.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("POWER=0"));

    /*
    * No reason to run, so go to sleep.
    */
    expect_go_to_sleep();
    expect_wake_up();
    power_periodic();
}

/*
* Go to standby using the AT+POWER command.
*/
void test_power_standby(void)
{
    /*
    * Set low power mode to standby.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("POWER=1"));

    /*
    * No reason to run, so reboot ahead of entering standby.
    */
    rtc_disable_alarm_Expect();
    mem_cfg_standby_set_Expect(false, false);
    power_periodic();

    /*
    * Go into standby after reboot.
    */
    setUp();
    mem_cfg_disable_wake_lid_ExpectAndReturn(false);
    mem_cfg_disable_wake_chg_ExpectAndReturn(false);
    power_enter_standby();
}

/*
* Go to stop (via reset) using the AT+POWER command.
*/
void test_power_reset_stop(void)
{
    /*
    * Set low power mode to stop.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("POWER=2"));

    /*
    * No reason to run, so reboot ahead of entering stop.
    */
    rtc_disable_alarm_Expect();
    mem_cfg_stop_set_Expect(false, false);
    power_periodic();

    /*
    * Go into stop after reboot.
    */
    setUp();
    gpio_clock_enable_Expect();
    gpio_disable_all_Expect();
    mem_cfg_disable_wake_lid_ExpectAndReturn(false);
    mem_cfg_disable_wake_chg_ExpectAndReturn(false);
    power_enter_stop_after_reset();
}

/*
* Go to stop using the AT+POWER command.
*/
void test_power_stop(void)
{
    /*
    * Set low power mode to standby.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("POWER=3"));

    /*
    * No reason to run, so go to stop mode.
    */
    led_sleep_Expect();
    adc_stop_Expect();
    gpio_prepare_for_stop_Expect();

    /* On wake from stop mode */
#if defined(FORCE_48MHZ_CLOCK)
    clock_change_Expect(CLOCK_48MHZ);
#endif
    gpio_init_after_stop_Expect();
    adc_init_Expect();
    led_wake_Expect();
    uart_init_Expect();

    power_periodic();
}
