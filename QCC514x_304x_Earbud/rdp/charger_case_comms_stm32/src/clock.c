/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Clock
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx.h"
#include "main.h"
#include "adc.h"
#include "timer.h"
#include "gpio.h"
#include "uart.h"
#include "clock.h"

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

uint32_t SystemCoreClock;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void clock_select_48mhz_hse_pll(void)
{
    /*
    * Enable HSE.
    */
    RCC->CR |= ((uint32_t)RCC_CR_HSEON);
    while (!(RCC->CR & RCC_CR_HSERDY));

    /*
    * Disable PLL.
    */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE_DIV1 |
        RCC_CFGR_PLLSRC_PREDIV1 | RCC_CFGR_PLLXTPRE_PREDIV1 |
#if (HSE_VALUE==8000000)
        RCC_CFGR_PLLMULL6);
#else
        RCC_CFGR_PLLMULL3);
#endif

    /*
    * Enable PLL.
    */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /*
    * Select PLL as system clock.
    */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while (!(RCC->CFGR & RCC_CFGR_SWS_PLL));

    FLASH->ACR |= FLASH_ACR_LATENCY;
}

static void clock_select_8mhz_hsi(void)
{
    FLASH->ACR &= ~FLASH_ACR_LATENCY;

    /*
    * Select HSI as system clock.
    */
    RCC->CFGR &= ~RCC_CFGR_SW;
    while (RCC->CFGR & RCC_CFGR_SWS);

    /*
    * Disable PLL.
    */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    /*
    * Disable HSE.
    */
    RCC->CR &= ~((uint32_t)RCC_CR_HSEON);
    while (RCC->CR & RCC_CR_HSERDY);
}

void clock_change(CLOCK_MODE clock_mode)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, DISABLE);
    adc_sleep();
    timer_clock_disable();
    uart_clock_disable();
    gpio_clock_disable();

    if (clock_mode==CLOCK_48MHZ)
    {
        clock_select_48mhz_hse_pll();
        SystemCoreClock = 48000000;
    }
    else
    {
        clock_select_8mhz_hsi();
        SystemCoreClock = 8000000;
    }

    gpio_clock_enable();
    uart_init();
    timer_init();
    adc_wake();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
}

void SystemInit(void)
{
    /* Set HSION bit */
    RCC->CR |= RCC_CR_HSION;

    /* Reset SW[1:0], HPRE[3:0], PPRE[2:0], ADCPRE, MCOSEL[2:0], MCOPRE[2:0], */
    /* PLLNODIV, PLLSRC, PLLXTPRE and PLLMUL[3:0] bits */
    RCC->CFGR &= ~(RCC_CFGR_SW | RCC_CFGR_HPRE | RCC_CFGR_PPRE |
        RCC_CFGR_ADCPRE | RCC_CFGR_MCO | RCC_CFGR_MCO_PRE | RCC_CFGR_PLLNODIV |
        RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMUL);

    /* Reset HSEBYP, HSEON, CSSON and PLLON bits */
    RCC->CR &= ~(RCC_CR_HSEBYP | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);

    /* Reset PREDIV1[3:0] bits */
    RCC->CFGR2 &= ~RCC_CFGR2_PREDIV1;

    /* Reset USARTSW[1:0], I2CSW, CECSW and ADCSW bits */
    RCC->CFGR3 &= ~(RCC_CFGR3_USART1SW | RCC_CFGR3_I2C1SW |
        RCC_CFGR3_CECSW | RCC_CFGR3_ADCSW);

    /* Reset HSI14 bit */
    RCC->CR2 &= ~RCC_CR2_HSI14ON;

    /* Disable all interrupts */
    RCC->CIR = 0x00000000;

    FLASH->ACR = FLASH_ACR_PRFTBE;
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
}

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 8000000;
}

void clock_init(void)
{
    SystemInit();
    SystemCoreClockUpdate();

#ifdef FORCE_48MHZ_CLOCK
    clock_change(CLOCK_48MHZ);
#endif
}
