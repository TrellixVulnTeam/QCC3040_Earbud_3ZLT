/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      RTC
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_rtc.h"
#include "stm32f0xx_rcc.h"
#include "main.h"
#include "power.h"
#include "case.h"
#include "rtc.h"
#ifdef TEST
#include "test_st.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define RTC_WAIT_CTR 1000

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT rtc_cmd_status(uint8_t cmd_source);
static CLI_RESULT rtc_cmd_tr(uint8_t cmd_source);
static CLI_RESULT rtc_cmd_alarm(uint8_t cmd_source);
static CLI_RESULT rtc_cmd_alarm_second(uint8_t cmd_source);
static CLI_RESULT rtc_cmd_alarm_day(uint8_t cmd_source);
static CLI_RESULT rtc_cmd_alarm_disable(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint16_t alarm_count = 0;

static const CLI_COMMAND rtc_alarm_command[] =
{
    { "second",  rtc_cmd_alarm_second,  2 },
    { "day",     rtc_cmd_alarm_day,     2 },
    { "disable", rtc_cmd_alarm_disable, 2 },
    { NULL }
};

static const CLI_COMMAND rtc_command[] =
{
    { "",      rtc_cmd_status, 2 },
    { "tr",    rtc_cmd_tr,     2 },
    { "alarm", rtc_cmd_alarm,  2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void rtc_enable_write_access(void)
{
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
}

static void rtc_disable_write_access(void)
{
    RTC->WPR = 0xFF;
}

static void rtc_enable_init_mode(void)
{
    uint16_t ctr = 0;

    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF) && (++ctr < RTC_WAIT_CTR));
}

static void rtc_disable_init_mode(void)
{
    uint16_t ctr = 0;

    RTC->ISR &= ~RTC_ISR_INIT;
    while ((RTC->ISR & RTC_ISR_INITF) && (++ctr < RTC_WAIT_CTR));
}

/*
* Set the alarm.
*/
static void rtc_set_alarm(uint32_t mask)
{
    uint16_t ctr = 0;

    rtc_enable_write_access();

    /*
    * Disable alarm A in order to modify it.
    */
    RTC->CR &= ~RTC_CR_ALRAE;
    while (!(RTC->ISR & RTC_ISR_ALRAWF) && (++ctr < RTC_WAIT_CTR));

    /*
    * Modify alarm A mask.
    */
    RTC->ALRMAR = mask;

    /*
    * Enable alarm A and alarm A interrupt.
    */
    RTC->CR |= RTC_CR_ALRAIE | RTC_CR_ALRAE | RTC_CR_WUTE;

    /*
    * Reset the time and date registers. This means that the first alarm will
    * happen at the specified time from now.
    */
    rtc_enable_init_mode();
    RTC->TR = 0;
    RTC->DR = 0;
    rtc_disable_init_mode();

    rtc_disable_write_access();
}

void rtc_init(void)
{
    uint16_t ctr = 0;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

    /*
    * Set the DBP bit in order to enable write access to the RTC registers.
    */
    PWR->CR |= PWR_CR_DBP;

    /*
    * Enable LSI clock.
    */
    RCC->CSR |= RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY) && (++ctr < RTC_WAIT_CTR));

    /*
    * LSI clock used as RTC clock.
    */
    if ((RCC->BDCR & RCC_BDCR_RTCSEL)!=RCC_BDCR_RTCSEL_LSI)
    {
        RCC->BDCR &= ~RCC_BDCR_RTCSEL;
        RCC->BDCR |= RCC_BDCR_RTCSEL_LSI;
    }

    /*
    * Enable RTC.
    */
    RCC->BDCR |= RCC_BDCR_RTCEN;

    rtc_enable_write_access();

    /*
    * Clear all RTC interrupt flags.
    */
    RTC->ISR &= ~0x00003FF00;

    rtc_enable_init_mode();

    /*
    * Set the prescaler register to give us a calendar clock of 1Hz based on a
    * LSI value of 40kHz.
    */
    RTC->PRER = 0x007F0137;

    rtc_disable_init_mode();

    rtc_disable_write_access();

    /*
    * Set up EXTI17.
    */
    EXTI->IMR |= EXTI_IMR_MR17;
    EXTI->RTSR |= EXTI_RTSR_TR17;
    EXTI->PR |= EXTI_PR_PR17;
}

static CLI_RESULT rtc_cmd_status(uint8_t cmd_source)
{
    uint32_t tr = RTC->TR;

    PRINTF("%dd %d%d:%d%d:%d%d",
        (RTC->DR & RTC_DR_WDU) >> 13,
        (tr & RTC_TR_HT) >> 20,  (tr & RTC_TR_HU) >> 16,
        (tr & RTC_TR_MNT) >> 12,  (tr & RTC_TR_MNU) >> 8,
        (tr & RTC_TR_ST) >> 4,  (tr & RTC_TR_SU));

    PRINTF("Alarms: %d", alarm_count);

    return CLI_OK;
}

/*
* Set the alarm for the specified day of the week.
*/
void rtc_set_alarm_day(uint8_t day)
{
    rtc_set_alarm(RTC_ALRMAR_WDSEL | (day << 24));
}

/*
* Set the alarm for the specified second.
*/
void rtc_set_alarm_second(uint8_t second)
{
    rtc_set_alarm(
        RTC_ALRMAR_MSK4 | RTC_ALRMAR_MSK3 | RTC_ALRMAR_MSK2 | second);
}

/*
* Set alarm for every second.
*/
void rtc_set_alarm_every_second(void)
{
    rtc_set_alarm(
        RTC_ALRMAR_MSK4 | RTC_ALRMAR_MSK3 |
        RTC_ALRMAR_MSK2 | RTC_ALRMAR_MSK1);
}

/*
* Disable the alarm.
*/
void rtc_disable_alarm(void)
{
    rtc_enable_write_access();
    RTC->CR &=~ RTC_CR_ALRAE;
    RTC->ALRMAR = 0;
    rtc_disable_write_access();
}

/*
* Command to set alarm for the specified second.
*/
static CLI_RESULT rtc_cmd_alarm_second(uint8_t cmd_source __attribute__((unused)))
{
    long int s;

    /*
    * Base 16 is used for the input as that makes it easy to copy into the
    * register, which is in BCD.
    */
    if (cli_get_next_parameter(&s, 16))
    {
        rtc_set_alarm_second(s);
    }
    else
    {
        /*
        * No second specified, so set alarm for every second.
        */
        rtc_set_alarm(
            RTC_ALRMAR_MSK4 | RTC_ALRMAR_MSK3 |
            RTC_ALRMAR_MSK2 | RTC_ALRMAR_MSK1);
    }

    return CLI_OK;
}

/*
* Command to set alarm for the specified day.
*/
static CLI_RESULT rtc_cmd_alarm_day(uint8_t cmd_source __attribute__((unused)))
{
    long int d;

    /*
    * Base 16 is used for the input as that makes it easy to copy into the
    * register, which is in BCD.
    */
    if (cli_get_next_parameter(&d, 16))
    {
        rtc_set_alarm_day(d);
    }
    else
    {
        /*
        * No day specified, so set alarm for every day.
        */
        rtc_set_alarm(RTC_ALRMAR_MSK4);
    }

    return CLI_OK;
}

/*
* Command to disable the alarm.
*/
static CLI_RESULT rtc_cmd_alarm_disable(uint8_t cmd_source __attribute__((unused)))
{
    rtc_disable_alarm();
    return CLI_OK;
}

/*
* Command to set the TR register.
*/
static CLI_RESULT rtc_cmd_tr(uint8_t cmd_source __attribute__((unused)))
{
    long int tr;

    /*
    * Base 16 is used for the input as that makes it easy to copy into the
    * register, which is in BCD.
    */
    if (cli_get_next_parameter(&tr, 16))
    {
        rtc_enable_write_access();
        rtc_enable_init_mode();
        RTC->TR = (uint32_t)tr;
        rtc_disable_init_mode();
        rtc_disable_write_access();
    }

    return CLI_OK;
}

CLI_RESULT rtc_cmd_alarm(uint8_t cmd_source)
{
    return cli_process_sub_cmd(rtc_alarm_command, cmd_source);
}

CLI_RESULT rtc_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(rtc_command, cmd_source);
}

void RTC_IRQHandler(void)
{
    uint32_t isr = RTC->ISR & 0x00003FF00;

    RTC->ISR &= ~isr;

    if (isr & RTC_ISR_ALRAF | RTC_ISR_WUTF)
    {
        EXTI->PR |= EXTI_PR_PR17;
        alarm_count++;

        /*
        * We don't want to kick the watchdog at this point, because we will
        * always keep coming back here even if stuck in a periodic function
        * or lower-priority interrupt. Instead, force run mode so that
        * wdog_periodic() will eventually do it.
        */
        power_set_run_reason(POWER_RUN_WATCHDOG);

        /*
        * Provide the case tick.
        */
        case_tick();
    }
}
