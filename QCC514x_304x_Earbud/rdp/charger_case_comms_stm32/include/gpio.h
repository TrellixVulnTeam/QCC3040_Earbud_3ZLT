/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GPIO
*/

#ifndef GPIO_H_
#define GPIO_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_gpio.h"
#include "cli_parse.h"

#ifdef VARIANT_CB
#include "cb_gpio.h"
#endif

#ifdef VARIANT_ST2
#include "st2_gpio.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define GPIO_NULL   0x00

#define GPIO_A0		0x10
#define GPIO_A1		0x11
#define GPIO_A2		0x12
#define GPIO_A3		0x13
#define GPIO_A4		0x14
#define GPIO_A5		0x15
#define GPIO_A6		0x16
#define GPIO_A7		0x17
#define GPIO_A8		0x18
#define GPIO_A9		0x19
#define GPIO_A10	0x1A
#define GPIO_A11	0x1B
#define GPIO_A12	0x1C
#define GPIO_A13	0x1D
#define GPIO_A14	0x1E
#define GPIO_A15	0x1F

#define GPIO_B0		0x20
#define GPIO_B1		0x21
#define GPIO_B2		0x22
#define GPIO_B3		0x23
#define GPIO_B4		0x24
#define GPIO_B5		0x25
#define GPIO_B6		0x26
#define GPIO_B7		0x27
#define GPIO_B8		0x28
#define GPIO_B9		0x29
#define GPIO_B10	0x2A
#define GPIO_B11	0x2B
#define GPIO_B12	0x2C
#define GPIO_B13	0x2D
#define GPIO_B14	0x2E
#define GPIO_B15	0x2F

#define GPIO_C0		0x30
#define GPIO_C1		0x31
#define GPIO_C2		0x32
#define GPIO_C3		0x33
#define GPIO_C4		0x34
#define GPIO_C5		0x35
#define GPIO_C6		0x36
#define GPIO_C7		0x37
#define GPIO_C8		0x38
#define GPIO_C9		0x39
#define GPIO_C10	0x3A
#define GPIO_C11	0x3B
#define GPIO_C12	0x3C
#define GPIO_C13	0x3D
#define GPIO_C14	0x3E
#define GPIO_C15	0x3F

#define GPIO_ACTIVE_LOW 0x8000

/*-----------------------------------------------------------------------------
------------------ MACROS -----------------------------------------------------
-----------------------------------------------------------------------------*/

#define GPIO_PORT_NUMBER(pin) \
    (((pin >> 4) & 0x3) - 1)
    
#define GPIO_PIN_NUMBER(pin) \
    (pin & 0xF)

#define GPIO_PORT(pin) \
    ((GPIO_TypeDef *)(AHB2PERIPH_BASE + ((((uint32_t)pin & 0xF0) - 0x10) << 6)))

#define GPIO_BIT(pin) \
    ((uint16_t)(1 << (pin & 0xF)))

#define GPIO_SET(pin) \
    GPIO_PORT(pin)->BSRR = GPIO_BIT(pin);

#define GPIO_RESET(pin) \
    GPIO_PORT(pin)->BRR = GPIO_BIT(pin);

#define GPIO_INPUT(pin) \
    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2));

#define GPIO_OUTPUT(pin) \
    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2)); \
    GPIO_PORT(pin)->MODER |= (0x1 << ((pin & 0xF) * 2));

#define GPIO_EXTI(pin) \
    ((uint32_t)(1 << (pin & 0xF)))

#define GPIO_MODE(pin) \
    (((GPIO_PORT(pin)->MODER) >> ((pin & 0xF) * 2)) & 0x03)

#define GPIO_SETTING(pin) \
    (((GPIO_PORT(pin)->BSRR) >> (pin & 0x0F)) ? true:false)

#define GPIO_CLEARING(pin) \
    (((GPIO_PORT(pin)->BRR) >> (pin & 0x0F)) ? true:false)

#define GPIO_AF(pin) \
    (((GPIO_PORT(pin)->AFR[(pin & 0xF) >> 3]) >> ((pin & 0x7) * 4)) & 0x07)

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void gpio_clock_enable(void);
void gpio_clock_disable(void);
void gpio_init(void);
void gpio_init_after_stop(void);
void gpio_prepare_for_stop(void);
void gpio_input(uint16_t pin);
void gpio_input_pd(uint16_t pin);
void gpio_output(uint16_t pin);
void gpio_af(uint16_t pin, uint8_t af);
void gpio_an(uint16_t pin);
void gpio_enable(uint16_t pin);
void gpio_disable(uint16_t pin);
void gpio_disable_all(void);
bool gpio_active(uint16_t pin);
CLI_RESULT gpio_cmd(uint8_t source);
CLI_RESULT ats_gpio(uint8_t cmd_source);

#endif /* GPIO_H_ */
