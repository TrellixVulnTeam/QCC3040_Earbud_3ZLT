/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Bootloader
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wdeprecated" /* for __set_MSP() */
#include "stm32f0xx.h"
#include "stm32f0xx_gpio.h"
#include "main.h"
#include "uart.h"
#include "memory.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define IMAGE_A_START 0x08001000
#define IMAGE_B_START 0x08010800
#define VECTOR_TABLE_SIZE 192

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*
* This structure exists to facilitate reading certain items from the vector
* table: the stack pointer, the program counter and the image count (which
* occupies an unused location in the table).
*/
typedef struct
{
    uint32_t sp;
    uint32_t pc;
    uint32_t x2;
    uint32_t x3;
    uint32_t x4;
    uint32_t x5;
    uint32_t x6;
    uint32_t image_count;
}
VT_MAP;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void power_set_run_reason(uint8_t rr __attribute__((unused)))
{
}

void power_clear_run_reason(uint8_t rr __attribute__((unused)))
{
}

void cli_uart_rx(uint8_t data __attribute__((unused)))
{
}

int main(void)
{
    VT_MAP *vta = (VT_MAP *)IMAGE_A_START;
    VT_MAP *vtb = (VT_MAP *)IMAGE_B_START;
    bool use_a = false;
    bool use_b = false;

    if (vta->image_count==0xFFFFFFFF)
    {
        if (vtb->image_count!=0xFFFFFFFF)
        {
            /*
            * Only image B is present.
            */
            use_b = true;
        }
    }
    else if (vtb->image_count==0xFFFFFFFF)
    {
        /*
        * Only image A is present.
        */
        use_a = true;
    }
    else
    {
        /*
        * Both images are present, so use the one with the higher count.
        */
        if ((vta->image_count > 0xFFFFFFF0) &&
            (vtb->image_count < 0x0000000F))
        {
            use_b = true;
        }
        if ((vtb->image_count > 0xFFFFFFF0) &&
            (vta->image_count < 0x0000000F))
        {
            use_a = true;
        }
        else if (vta->image_count > vtb->image_count)
        {
            use_a = true;
        }
        else
        {
            use_b = true;
        }
    }

    if (use_a || use_b)
    {
        VT_MAP *vt = (use_a) ? vta : vtb;

        /*
        * Copy the vector table from the image we intend to run into RAM.
        */
        memcpy(
            (void *)MEM_RAM_START,
            (const void *)vt,
            (size_t)VECTOR_TABLE_SIZE);

         /*
         * Map SRAM to 0x00000000. This is in order to start using the RAM
         * copy of the vector table.
         */
         RCC->APB2ENR |= RCC_APB2Periph_SYSCFG;
         SYSCFG->CFGR1 &= ~SYSCFG_CFGR1_MEM_MODE;
         SYSCFG->CFGR1 |= SYSCFG_MemoryRemap_SRAM;

         /*
         * Jump to the image.
         */
         __set_MSP(vt->sp);
         ((void (*)(void))vt->pc)();
    }

    /*
    * No image present, so all we can do is stay in the bootloader. It is
    * still to be decided what, if anything, should happen now.
    */

    /*
    * Enable GPIO clock.
    */
    RCC->AHBENR |= RCC_AHBPeriph_GPIOB;

    /*
    * Set the USART1 GPIO pins to their alternate function.
    */
    GPIOB->MODER |= 0x0000A000;

    uart_init();

    /*
    * Enable USART1 interrupt.
    */
    NVIC->ISER[0] = (uint32_t)0x01 << (USART1_IRQn & (uint8_t)0x1F);

    uart_tx(UART_CLI, (uint8_t *)"No image\r\n", 10);

    /*
    * Main loop.
    */
    while (1)
    {
        uart_tx_periodic();
    }
}
