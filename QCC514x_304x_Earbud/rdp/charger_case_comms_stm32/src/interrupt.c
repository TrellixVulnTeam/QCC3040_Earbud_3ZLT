/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interrupts
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx.h"
#include "stm32f0xx_misc.h"
#include "stm32f0xx_exti.h"
#include "stm32f0xx_syscfg.h"
#include "stm32f0xx_rcc.h"
#include "main.h"
#include "case.h"
#include "gpio.h"
#include "wdog.h"
#include "memory.h"
#include "uart.h"
#include "interrupt.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void interrupt_nvic_init(IRQn_Type irqn, uint8_t priority)
{
    NVIC_InitTypeDef nvicStructure;

    nvicStructure.NVIC_IRQChannel = irqn;
    nvicStructure.NVIC_IRQChannelPriority = priority;
    nvicStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvicStructure);
    NVIC_EnableIRQ(irqn);
}

void interrupt_init(void)
{
    /*
    * Set external interrupts on lid and charger detection pins
    * to trigger on rising and falling edges.
    */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(GPIO_PORT_NUMBER(GPIO_MAG_SENSOR),
                          GPIO_PIN_NUMBER(GPIO_MAG_SENSOR));
    SYSCFG_EXTILineConfig(GPIO_PORT_NUMBER(GPIO_CHG_SENSE),
                          GPIO_PIN_NUMBER(GPIO_CHG_SENSE));

#ifdef FAST_TIMER_INTERRUPT
    interrupt_nvic_init(TIM14_IRQn, 0);
#endif
#ifdef USB_ENABLED
    interrupt_nvic_init(USB_IRQn, 0);
#endif
    interrupt_nvic_init(ADC1_IRQn, 1);
    interrupt_nvic_init(EXTI0_1_IRQn, 1);
    interrupt_nvic_init(EXTI4_15_IRQn, 1);
#ifdef SCHEME_A
    interrupt_nvic_init(TIM3_IRQn, 0);
#endif
#ifdef SCHEME_B
    interrupt_nvic_init(USART3_4_IRQn, 3);
#endif
    interrupt_nvic_init(USART1_IRQn, 3);
    interrupt_nvic_init(TIM17_IRQn, 3);
    interrupt_nvic_init(RTC_IRQn, 3);

    EXTI->IMR |= (GPIO_EXTI(GPIO_MAG_SENSOR) | GPIO_EXTI(GPIO_CHG_SENSE));
    EXTI->EMR &= ~(GPIO_EXTI(GPIO_MAG_SENSOR) | GPIO_EXTI(GPIO_CHG_SENSE));
    EXTI->RTSR |= GPIO_EXTI(GPIO_MAG_SENSOR) | GPIO_EXTI(GPIO_CHG_SENSE);
    EXTI->FTSR |= GPIO_EXTI(GPIO_MAG_SENSOR) | GPIO_EXTI(GPIO_CHG_SENSE);

    /*
    * Clear EXTI pending register for charger and lid.
    */
    EXTI->PR |= GPIO_EXTI(GPIO_CHG_SENSE) | GPIO_EXTI(GPIO_MAG_SENSOR);
}

void EXTI0_1_IRQHandler(void)
{
    uint32_t pr = EXTI->PR & 0x00000003;

    EXTI->PR = pr;

    if (pr & GPIO_EXTI(GPIO_MAG_SENSOR))
    {
        /*
        * Lid open/close detected.
        */
        case_event_occurred();
    }
}

void EXTI4_15_IRQHandler(void)
{
    uint32_t pr = EXTI->PR & 0x0000FFF0;

    EXTI->PR = pr;

    if (pr & GPIO_EXTI(GPIO_CHG_SENSE))
    {
        /*
        * Charger connection/disconnection detected.
        */
        case_event_occurred();
    }
}

void HardFault_Handler(void)
{
    uint8_t cmd_source = 0;

    /*
    * Kick watchdog so we definitely have enough time to get debug messages
    * out.
    */
    wdog_kick();

    /*
    * Output information of interest.
    */
    PRINT("HARD FAULT");
    mem_stack_dump(cmd_source);

    /*
    * Loop until watchdog reset.
    */
    while (1)
    {
        uart_dump();
    }
}
