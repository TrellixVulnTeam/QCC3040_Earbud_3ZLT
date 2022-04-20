/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power modes
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include "stm32f0xx.h"
#include "stm32f0xx_rcc.h"
#include "stm32f0xx_pwr.h"
#include "main.h"
#include "timer.h"
#include "gpio.h"
#include "adc.h"
#include "led.h"
#include "memory.h"
#include "cmsis.h"
#include "rtc.h"
#include "power.h"
#include "uart.h"
#include "vreg.h"
#include "clock.h"

#ifdef EARBUD_CURRENT_SENSES
#include "current_senses.h"
#endif

#ifdef TEST
#include "test_st.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT power_cmd_status(uint8_t cmd_source);
CLI_RESULT power_cmd_on(uint8_t cmd_source);
CLI_RESULT power_cmd_off(uint8_t cmd_source);
CLI_RESULT power_cmd_sleep(uint8_t cmd_source);
CLI_RESULT power_cmd_stop(uint8_t cmd_source);
CLI_RESULT power_cmd_standby(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint32_t power_reason_to_run = 0;
static uint8_t power_reason_to_stop = 0;
static uint8_t power_reason_to_reset_stop = 0;
static uint8_t power_reason_to_standby = 0;

static const CLI_COMMAND power_command[] =
{
    { "",        power_cmd_status,  2 },
    { "on",      power_cmd_on,      2 },
    { "off",     power_cmd_off,     2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void power_resume(void)
{
    /* Resume Tick interrupt if disabled prior to sleep mode entry*/
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

    /* Re-enable peripherals */
    gpio_clock_enable();
    timer_wake();
    led_wake();
    adc_wake();
}

static void power_setup_wakeup_sources(void)
{
    /* Enable the power block */
    RCC->APB1ENR |= RCC_APB1Periph_PWR;

    /*Disable all used wakeup sources: PA0, PC13 */
    PWR->CSR &= ~(PWR_CSR_EWUP1 | PWR_CSR_EWUP2);

    /*Clear all related wakeup flags*/
    PWR->CR |= (PWR_FLAG_WU << 2);

    /*Re-enable all used wakeup sources: PA0, PC13 */
    if (!mem_cfg_disable_wake_lid())
    {
        PWR->CSR |= PWR_CSR_EWUP1;
    }

    if (!mem_cfg_disable_wake_chg())
    {
        PWR->CSR |= PWR_CSR_EWUP2;
    }
}

static void power_enter_sleep(void)
{
    DISABLE_IRQ();

    led_sleep();
    adc_sleep();
    timer_sleep();

#ifdef VARIANT_CB
    /* Regulator PFM/PWM. */
    vreg_pfm();
#endif

    gpio_clock_disable();

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    /* Request to enter SLEEP mode */
    /* Clear SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    ENABLE_IRQ();

    /* Request Wait For Interrupt */
    WFI();

    power_resume();
}

void power_enter_stop_after_reset(void)
{
    /* Put all GPIOs into high-Z mode to reduce power consumption during STOP,
     * except the regulator enable so we can power the earbuds. */
    gpio_clock_enable();
    vreg_enable();
    gpio_prepare_for_stop();

    /* Enable the PWR block and set-up wakeup sources */
    power_setup_wakeup_sources();

    /* Request to enter STOP mode */
    PWR->CR &= ~PWR_CR_PDDS;
    PWR->CR |= PWR_CR_LPDS;

    /* Wake up in a day */ 
    rtc_set_alarm_day(1);

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* Request Wait For Interrupt */
    WFI();

    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    gpio_init_after_stop();
}

static void power_enter_stop(void)
{
    DISABLE_IRQ();

    led_sleep();
    adc_stop();

#ifdef EARBUD_CURRENT_SENSES  
    /* No need to sense in STOP mode */
    current_senses_clear_sense_amp(CURRENT_SENSE_AMP_MONITORING);
#endif
    gpio_prepare_for_stop();

    /* Pause the tick interrupt */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    /* Request to enter STOP mode */
    PWR->CR &= ~PWR_CR_PDDS;
    PWR->CR |= PWR_CR_LPDS;

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    ENABLE_IRQ();

    /* Request Wait For Interrupt */
    WFI();

    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

#if defined(FORCE_48MHZ_CLOCK)
    clock_change(CLOCK_48MHZ);
#endif
    gpio_init_after_stop();

#ifdef EARBUD_CURRENT_SENSES  
    /* Enable current senses for monitoring */
    current_senses_set_sense_amp(CURRENT_SENSE_AMP_MONITORING);
#endif

    adc_init();
    led_wake();
    uart_init();

    /* Resume Tick interrupt */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

static void power_reset_to_standby(void)
{
    /* Force the voltage regulator OFF */
#ifdef SCHEME_A
    vreg_pwm();
    charger_comms_vreg_reset();
    delay_ms(30);
#endif
    vreg_disable();

    rtc_disable_alarm();
    mem_cfg_standby_set(
        (power_reason_to_standby & POWER_STANDBY_LOW_BATTERY) ? true:false,
        false);
    NVIC_SYSTEM_RESET();
}

static void power_reset_to_stop(void)
{
    rtc_disable_alarm();
    mem_cfg_stop_set(false, false);
    NVIC_SYSTEM_RESET();
}

void power_enter_standby(void)
{
    /* Enable the PWR block and set-up wakeup sources */
    power_setup_wakeup_sources();

    /* Select STANDBY mode */
    PWR->CR |= PWR_CR_PDDS;

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* Request Wait For Interrupt */
    WFI();
}

void power_set_run_reason(uint32_t reason)
{
    DISABLE_IRQ();
    power_reason_to_run |= reason;
    ENABLE_IRQ();
}

void power_clear_run_reason(uint32_t reason)
{
    DISABLE_IRQ();
    power_reason_to_run &= ~reason;
    ENABLE_IRQ();
}

void power_set_standby_reason(uint8_t reason)
{
    DISABLE_IRQ();
    power_reason_to_standby |= reason;
    ENABLE_IRQ();
}

void power_clear_standby_reason(uint8_t reason)
{
    DISABLE_IRQ();
    power_reason_to_standby &= ~reason;
    ENABLE_IRQ();
}

void power_set_stop_reason(uint8_t reason)
{
    DISABLE_IRQ();
    power_reason_to_stop |= reason;
    ENABLE_IRQ();
}

void power_clear_stop_reason(uint8_t reason)
{
    DISABLE_IRQ();
    power_reason_to_stop &= ~reason;
    ENABLE_IRQ();
}

void power_set_reset_stop_reason(uint8_t reason)
{
    DISABLE_IRQ();
    power_reason_to_reset_stop |= reason;
    ENABLE_IRQ();
}

void power_clear_reset_stop_reason(uint8_t reason)
{
    DISABLE_IRQ();
    power_reason_to_reset_stop &= ~reason;
    ENABLE_IRQ();
}

void power_periodic(void)
{
    if (!power_reason_to_run)
    {
        /*
        * No reason left to stay in run mode, so go to a lower power mode.
        */
        if (power_reason_to_standby)
        {
            power_reset_to_standby();
        }
        else if (power_reason_to_reset_stop)
        {
            power_reset_to_stop();
        }
        else if (power_reason_to_stop)
        {
            power_enter_stop();
        }
        else
        {
            power_enter_sleep();
        }
    }
}

CLI_RESULT power_cmd_on(uint8_t cmd_source __attribute__((unused)))
{
    power_set_run_reason(POWER_RUN_FORCE_ON);
    return CLI_OK;
}

CLI_RESULT power_cmd_off(uint8_t cmd_source __attribute__((unused)))
{
    power_clear_run_reason(POWER_RUN_FORCE_ON);
    return CLI_OK;
}

CLI_RESULT power_cmd_sleep(uint8_t cmd_source __attribute__((unused)))
{
    power_clear_standby_reason(POWER_STANDBY_COMMAND);
    power_clear_stop_reason(POWER_STOP_COMMAND);
    power_clear_reset_stop_reason(POWER_STOP_COMMAND);
    return CLI_OK;
}

CLI_RESULT power_cmd_stop(uint8_t cmd_source __attribute__((unused)))
{
    power_set_stop_reason(POWER_STOP_COMMAND);
    return CLI_OK;
}

CLI_RESULT power_cmd_reset_stop(uint8_t cmd_source __attribute__((unused)))
{
    power_set_reset_stop_reason(POWER_STOP_COMMAND);
    return CLI_OK;
}

CLI_RESULT power_cmd_standby(uint8_t cmd_source __attribute__((unused)))
{
    power_set_standby_reason(POWER_STANDBY_COMMAND);
    return CLI_OK;
}

CLI_RESULT power_cmd_status(uint8_t cmd_source)
{
    PRINTF("0x%08x", power_reason_to_run);
    return CLI_OK;
}

CLI_RESULT power_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(power_command, cmd_source);
}

CLI_RESULT ats_power(uint8_t cmd_source)
{
    bool ret = CLI_ERROR;
    long int mode;

    if (cli_get_next_parameter(&mode, 10))
    {
        switch (mode)
        {
            default:
            case 0:
                ret = power_cmd_sleep(cmd_source);
                break;

            case 1:
                ret = power_cmd_standby(cmd_source);
                break;

            case 2:
                ret = power_cmd_reset_stop(cmd_source);
                break;

            case 3:
                ret = power_cmd_stop(cmd_source);
                break;
        }
    }

    return ret;
}
