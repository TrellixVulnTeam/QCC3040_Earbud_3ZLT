/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for battery.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>

#include "unity.h"

#include "battery.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_adc.h"
#include "mock_gpio.h"
#include "mock_led.h"
#include "mock_cli.h"
#include "mock_timer.h"
#include "mock_case_charger.h"
#include "mock_config.h"
#include "mock_power.h"
#include "mock_current_senses.h"
#include "mock_vreg.h" 

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    memset(&battery_status, 0, sizeof(battery_status));
    battery_status.state = BATTERY_IDLE;
    battery_status.cmd_source = CLI_SOURCE_NONE;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Battery read and LED indication requested and executed.
*/
void test_battery_led_indication(void)
{
    int n;

    power_clear_run_reason_Ignore();
    /*
    * Nothing happens.
    */

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Read with LED indication is requested.
    */
    battery_read_request(true);

    TEST_ASSERT_FALSE(battery_read_done());

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);

    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 6813);
#ifdef SCHEME_A
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    config_get_battery_cutoff_mv_ExpectAndReturn(3000);
    charger_set_reason_Expect(CHARGER_OFF_BATTERY_READ);
    power_set_run_reason_Expect(POWER_RUN_BATTERY_READ);
    battery_periodic();

    for (n=0; n<BATTERY_READ_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 6813);
#ifdef SCHEME_A
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    power_clear_standby_reason_Expect(POWER_STANDBY_LOW_BATTERY);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    charger_clear_reason_Expect(CHARGER_OFF_BATTERY_READ);
    led_indicate_battery_Expect(100);
    battery_periodic();

    TEST_ASSERT_EQUAL(100, battery_percentage_current());

    /*
    * Nothing happens.
    */
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Another read is requested.
    */
    battery_read_request(true);

    TEST_ASSERT_FALSE(battery_read_done());

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    
    adc_start_measuring_ExpectAndReturn(true);
    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    config_get_battery_cutoff_mv_ExpectAndReturn(3000);
    charger_set_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    for (n=0; n<BATTERY_READ_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_start_measuring_ExpectAndReturn(true);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

#ifdef SCHEME_A
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    
    adc_start_measuring_ExpectAndReturn(true);
#endif
    power_clear_standby_reason_Expect(POWER_STANDBY_LOW_BATTERY);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    charger_clear_reason_Expect(CHARGER_OFF_BATTERY_READ);
    led_indicate_battery_Expect(50);
    battery_periodic();

    TEST_ASSERT_TRUE(battery_read_done());
    TEST_ASSERT_EQUAL(50, battery_percentage_current());
}

/*
* Battery read without LED indication requested and executed.
*/
void test_battery_no_led_indication(void)
{
    int n;

    /*
    * Nothing happens.
    */
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Read without LED indication is requested.
    */
    battery_read_request(false);

    TEST_ASSERT_FALSE(battery_read_done());

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    config_get_battery_cutoff_mv_ExpectAndReturn(3000);
    charger_set_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    for (n=0; n<BATTERY_READ_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_start_measuring_ExpectAndReturn(true);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Another read is requested, but as we already are reading we ignore it.
    */
    battery_read_request(false);

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 3949);
#ifdef SCHEME_A
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_L, 3300, 180);
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_R, 3300, 180);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    power_clear_standby_reason_Expect(POWER_STANDBY_LOW_BATTERY);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    charger_clear_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    TEST_ASSERT_TRUE(battery_read_done());
    TEST_ASSERT_EQUAL(50, battery_percentage_current());

    /*
    * Nothing happens.
    */
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();
}

/*
* Battery read without LED indication to begin with, but mid-way through
* the operation, an LED indication does get requested.
*/
void test_battery_belated_led_indication(void)
{
    int n;

    /*
    * Nothing happens.
    */
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Read without LED indication is requested.
    */
    battery_read_request(false);

    TEST_ASSERT_FALSE(battery_read_done());

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    config_get_battery_cutoff_mv_ExpectAndReturn(3000);
    charger_set_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    for (n=0; n<BATTERY_READ_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_start_measuring_ExpectAndReturn(true);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Another read - this time with LED indication - requested.
    */
    battery_read_request(true);

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 3949);
#ifdef SCHEME_A
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_L, 3300, 180);
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_R, 3300, 180);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    power_clear_standby_reason_Expect(POWER_STANDBY_LOW_BATTERY);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    charger_clear_reason_Expect(CHARGER_OFF_BATTERY_READ);
    led_indicate_battery_Expect(50);
    battery_periodic();

    TEST_ASSERT_TRUE(battery_read_done());
    TEST_ASSERT_EQUAL(50, battery_percentage_current());
}

/*
* Battery read without LED indication requested and executed. The battery's
* charge has fallen below the configured cutoff level.
*/
void test_battery_below_cutoff(void)
{
    int n;

    /*
    * Nothing happens.
    */
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Read without LED indication is requested.
    */
    battery_read_request(false);

    TEST_ASSERT_FALSE(battery_read_done());

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    config_get_battery_cutoff_mv_ExpectAndReturn(3000);
    charger_set_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    for (n=0; n<BATTERY_READ_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    /*
    * Attempt to start measuring, but there is already a measurement in
    * progress.
    */
    adc_start_measuring_ExpectAndReturn(false);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    /*
    * Start measuring.
    */
    adc_start_measuring_ExpectAndReturn(true);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    /*
    * Reading is below the cutoff level, kick off a second read.
    */
    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 2999);
#ifdef SCHEME_A
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_L, 3300, 180);
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_R, 3300, 180);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    battery_periodic();

    adc_start_measuring_ExpectAndReturn(true);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    /*
    * Reading is below the cutoff level, kick off a third read.
    */
    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 2999);
#ifdef SCHEME_A
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_L, 3300, 180);
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_R, 3300, 180);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    battery_periodic();

    adc_start_measuring_ExpectAndReturn(true);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    /*
    * We accept the third reading regardless, but set the standby flag.
    */
    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 2999);
#ifdef SCHEME_A
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_L, 3300, 180);
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_R, 3300, 180);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    power_set_standby_reason_Expect(POWER_STANDBY_LOW_BATTERY);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    charger_clear_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    TEST_ASSERT_TRUE(battery_read_done());
    TEST_ASSERT_EQUAL(0, battery_percentage_current());
}

/*
* Battery percentage calculation.
*/
void test_battery_percentage(void)
{
    battery_status.current_battery_mv = 30000;
    TEST_ASSERT_EQUAL(100, battery_percentage());

    battery_status.current_battery_mv = 4200;
    TEST_ASSERT_EQUAL(100, battery_percentage());

    battery_status.current_battery_mv = 3949;
    TEST_ASSERT_EQUAL(50, battery_percentage());

    battery_status.current_battery_mv = 3300;
    TEST_ASSERT_EQUAL(0, battery_percentage());

    battery_status.current_battery_mv = 0;
    TEST_ASSERT_EQUAL(0, battery_percentage());
}

/*
* AT+NTC?
*/
void test_battery_atq_ntc(void)
{
    gpio_enable_Expect(GPIO_NTC_MONITOR_ON_OFF);
    adc_blocking_measure_Expect();
    adc_read_mv_ExpectAndReturn(ADC_NTC, 3300, 0x600);
    gpio_disable_Expect(GPIO_NTC_MONITOR_ON_OFF);
    cli_tx_Expect(CLI_SOURCE_UART, true, "1536");
    atq_ntc(CLI_SOURCE_UART);
}

/*
* AT+BATTERY?
*/
void test_battery_atq_battery(void)
{
    int n;

    /*
    * AT+BATTERY? entered.
    */
    TEST_ASSERT_EQUAL(CLI_WAIT, atq_battery(CLI_SOURCE_UART));

    /*
    * Attempted command from another source rejected.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, atq_battery(CLI_SOURCE_USB));

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_set_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    config_get_battery_cutoff_mv_ExpectAndReturn(3000);
    charger_set_reason_Expect(CHARGER_OFF_BATTERY_READ);
    battery_periodic();

    for (n=0; n<BATTERY_READ_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
    battery_periodic();

    for (n=0; n<BATTERY_ADC_DELAY_TIME; n++)
    {
        battery_fetch_total_load_ma_IgnoreAndReturn(0);
        adc_start_measuring_ExpectAndReturn(true);
        battery_periodic();
    }

    adc_read_mv_ExpectAndReturn(ADC_VBAT, 6600, 3949);
#ifdef SCHEME_A
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_L, 3300, 180);
    adc_read_mv_ExpectAndReturn(ADC_CURRENT_SENSE_R, 3300, 180);
    battery_fetch_total_load_ma_IgnoreAndReturn(0);
    adc_start_measuring_ExpectAndReturn(true);
#endif
    power_clear_standby_reason_Expect(POWER_STANDBY_LOW_BATTERY);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
#ifdef SCHEME_A
    current_senses_clear_sense_amp_Expect(CURRENT_SENSE_AMP_BATTERY);
#endif
    charger_clear_reason_Expect(CHARGER_OFF_BATTERY_READ);
    cli_tx_Expect(CLI_SOURCE_UART, true, "3949,50");
    cli_tx_Expect(CLI_SOURCE_UART, true, "OK");
    battery_periodic();
}
