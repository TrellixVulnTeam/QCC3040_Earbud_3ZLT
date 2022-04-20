/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for rtc.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "rtc.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_stm32f0xx_rcc.h"
#include "mock_cli.h"
#include "mock_case.h"
#include "mock_power.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_rtc_command[] =
{
    { "rtc", rtc_cmd, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ EXPECT FUNCTIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static void do_cmd(char *str)
{
    common_cmd(test_rtc_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    memset(test_AHBPERIPH, 0, sizeof(test_AHBPERIPH));
    memset(test_APBPERIPH, 0, sizeof(test_APBPERIPH));

    RCC_APB1PeriphClockCmd_Expect(RCC_APB1Periph_PWR, ENABLE);
    rtc_init();
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* RTC.
*/
void test_rtc(void)
{
    cli_tx_Expect(CLI_SOURCE_UART, true, "0d 00:00:00");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Alarms: 0");
    do_cmd("rtc");

    /*
    * Set the time.
    */
    do_cmd("rtc tr 123456");

    cli_tx_Expect(CLI_SOURCE_UART, true, "0d 12:34:56");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Alarms: 0");
    do_cmd("rtc");

    /*
    * RTC interrupt that is not an alarm.
    */
    RTC->ISR = 0x00000200;
    RTC_IRQHandler();
    TEST_ASSERT_EQUAL_HEX32(0x00000000, RTC->ISR);

    cli_tx_Expect(CLI_SOURCE_UART, true, "0d 12:34:56");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Alarms: 0");
    do_cmd("rtc");

    /*
    * RTC interrupt indicating alarm.
    */
    RTC->ISR = 0x00000100;
    power_set_run_reason_Expect(POWER_RUN_WATCHDOG);
    case_tick_Expect();
    RTC_IRQHandler();
    TEST_ASSERT_EQUAL_HEX32(0x00000000, RTC->ISR);

    cli_tx_Expect(CLI_SOURCE_UART, true, "0d 12:34:56");
    cli_tx_Expect(CLI_SOURCE_UART, true, "Alarms: 1");
    do_cmd("rtc");

    /*
    * Set alarm for every second.
    */
    do_cmd("rtc alarm second");
    TEST_ASSERT_EQUAL_HEX32(0x00001100, RTC->CR);
    TEST_ASSERT_EQUAL_HEX32(0x80808080, RTC->ALRMAR);

    /*
    * Set alarm for the fifth second of a minute.
    */
    do_cmd("rtc alarm second 5");
    TEST_ASSERT_EQUAL_HEX32(0x00001100, RTC->CR);
    TEST_ASSERT_EQUAL_HEX32(0x80808005, RTC->ALRMAR);

    /*
    * Set alarm for every day.
    */
    do_cmd("rtc alarm day");
    TEST_ASSERT_EQUAL_HEX32(0x00001100, RTC->CR);
    TEST_ASSERT_EQUAL_HEX32(0x80000000, RTC->ALRMAR);

    /*
    * Set alarm for the third day of the week.
    */
    do_cmd("rtc alarm day 3");
    TEST_ASSERT_EQUAL_HEX32(0x00001100, RTC->CR);
    TEST_ASSERT_EQUAL_HEX32(0x43000000, RTC->ALRMAR);

    /*
    * Disable alarm.
    */
    do_cmd("rtc alarm disable");
    TEST_ASSERT_EQUAL_HEX32(0x00001000, RTC->CR);
}
