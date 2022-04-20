/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GPIO
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f0xx_exti.h"
#include "stm32f0xx_rcc.h"
#include "main.h"
#include "gpio.h"
#ifdef TEST
#include "test_st.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define NO_OF_PORTS 3

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT gpio_cmd_h(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_l(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_i(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_ipd(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_o(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_rd(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_af(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_display(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND gpio_command[] =
{
    { "",    gpio_cmd_display, 2 },
    { "h",   gpio_cmd_h,       2 },
    { "l",   gpio_cmd_l,       2 },
    { "i",   gpio_cmd_i,       2 },
    { "o",   gpio_cmd_o,       2 },
    { "ipd", gpio_cmd_ipd,     2 },
    { "rd",  gpio_cmd_rd,      2 },
    { "af",  gpio_cmd_af,      2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static GPIO_TypeDef *gpio_get_port(uint16_t pin)
{
    return GPIO_PORT(pin);
}

static uint16_t gpio_get_bit(uint16_t pin)
{
    return GPIO_BIT(pin);
}

static void gpio_init_pin(uint16_t pin, GPIO_InitTypeDef *init_struct)
{
    init_struct->GPIO_Pin = (uint32_t)gpio_get_bit(pin);
    GPIO_Init(gpio_get_port(pin), init_struct);
}

static void gpio_set(uint16_t pin)
{
    GPIO_SET(pin);
}

static void gpio_reset(uint16_t pin)
{
    GPIO_RESET(pin);
}

void gpio_input(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    gpio_init_pin(pin, &init_struct);
}

void gpio_input_pd(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_PuPd = GPIO_PuPd_DOWN;
    gpio_init_pin(pin, &init_struct);
}

void gpio_output(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init_pin(pin, &init_struct);
}

void gpio_af(uint16_t pin, uint8_t af)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_Mode = GPIO_Mode_AF;
    gpio_init_pin(pin, &init_struct);

    GPIO_PinAFConfig(gpio_get_port(pin), pin & 0xF, af);
}

void gpio_an(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_Mode = GPIO_Mode_AN;
    gpio_init_pin(pin, &init_struct);
}

void gpio_enable(uint16_t pin)
{
    if (pin & GPIO_ACTIVE_LOW)
    {
        gpio_reset(pin);
    }
    else
    {
        gpio_set(pin);
    }
}

void gpio_disable(uint16_t pin)
{
    if (pin & GPIO_ACTIVE_LOW)
    {
        gpio_set(pin);
    }
    else
    {
        gpio_reset(pin);
    }
}

bool gpio_active(uint16_t pin)
{
    bool bitstatus =
        ((gpio_get_port(pin)->IDR) & gpio_get_bit(pin)) ? true:false;

    if (pin & GPIO_ACTIVE_LOW)
    {
        return (bitstatus) ? false:true;
    }
    else
    {
        return bitstatus;
    }
}

void gpio_clock_enable(void)
{
    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        ENABLE);
}

void gpio_clock_disable(void)
{
    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        DISABLE);
}

void gpio_disable_all(void)
{
    /* Set all pins to analogue high-Z mode (GPIO_Mode_AN) */
    GPIOA->MODER=0xFFFFFFFF;
    GPIOB->MODER=0xFFFFFFFF;
    GPIOC->MODER=0xFFFFFFFF;
    GPIOD->MODER=0xFFFFFFFF;
    GPIOE->MODER=0xFFFFFFFF;
}

void gpio_prepare_for_stop(void)
{
#if defined(VARIANT_CB) 
    /*
     * Keep
     * - GPIO_A0 as input (GPIO_MAG_SENSOR) 
     * - GPIO_A7 as output (GPIO_VREG_EN)
     * - GPIO_B9 as output (GPIO_VREG_MOD)
     * 
     * All other GPIOs put into analogue mode
     */
    GPIOA->MODER=0xFFFF7FFC;
    GPIOB->MODER=0xFFF7FFFF;

    /* Put banks C, D and E in analogue mode */
    GPIOC->MODER=0xFFFFFFFF;
    GPIOD->MODER=0xFFFFFFFF;
    GPIOE->MODER=0xFFFFFFFF;

#elif defined(VARIANT_ST2)
    /*
     * Keep
     * - GPIO_A0 as input (GPIO_MAG_SENSOR)
     * - GPIO_A10 as output (GPIO_VREG_ISO)
     * - GPIO_B8 as output (GPIO_DOCK_PULL_EN)
     * - GPIO_B14 as output (GPIO_VREG_EN)
     * - GPIO_B15 as output (GPIO_VREG_SEL)
     * 
     * All other GPIOs put into analogue mode
     */
    GPIOA->MODER=0xFFDFFFFC;
    GPIOB->MODER=0x5FFDFFFF;

    /* Put banks C, D and E in analogue mode */
    GPIOC->MODER=0xFFFFFFFF;
    GPIOD->MODER=0xFFFFFFFF;
    GPIOE->MODER=0xFFFFFFFF;

#else
#error "No GPIO state for STOP mode defined for this variant"
#endif
}

void gpio_init(void)
{
    /* Initialise all the GPIOs that are re-initialised after exiting from STOP */
    gpio_init_after_stop();

    /* During initialisation ensure the voltage regulator is off to avoid waking
     * the earbuds up unless we explicitly need to. */
    gpio_disable(GPIO_VREG_EN);
    gpio_output(GPIO_VREG_EN);

#ifdef VARIANT_ST2
    /* By default enable the Comms pull-up.
     * TODO: This shouldn't be the default behaviour but it useful for early
     * development. */
    gpio_enable(GPIO_DOCK_PULL_EN);
    gpio_output(GPIO_DOCK_PULL_EN);

    gpio_disable(GPIO_VREG_ISO);
    gpio_output(GPIO_VREG_ISO);

    gpio_disable(GPIO_VREG_SEL);
    gpio_output(GPIO_VREG_SEL);
#endif
}

void gpio_init_after_stop(void)
{
    /*
    * Enable clock for all the ports in one go.
    */
    gpio_clock_enable();

    /* Magnetic sensor - input.*/
    gpio_input_pd(GPIO_MAG_SENSOR);

    /*
    * Set the USART1 GPIO pins to their alternate function.
    */
    gpio_af(GPIO_UART_TX, GPIO_AF_0);
    gpio_af(GPIO_UART_RX, GPIO_AF_0);

#ifdef VARIANT_ST2
    /*
    * Set the USART3 GPIO pins to their alternate function.
    */
    gpio_af(GPIO_DOCK_DATA_TX, GPIO_AF_4);
    gpio_af(GPIO_DOCK_DATA_RX, GPIO_AF_4);

#ifdef CHARGER_COMMS_FAKE_U
    /*
    * Set the USART4 GPIO pins to their alternate function.
    */
    gpio_af(GPIO_C10, GPIO_AF_0);
    gpio_af(GPIO_C11, GPIO_AF_0);
#endif

#endif

#ifdef EARBUD_CURRENT_SENSES
    /* Power for both current sense amplifiers */
    gpio_enable(GPIO_CURRENT_SENSE_AMP);
    gpio_output(GPIO_CURRENT_SENSE_AMP);
#endif

#ifdef EARBUD_CURRENT_SENSES
    /* Left earbud current sense */
    gpio_an(GPIO_L_CURRENT_SENSE);

    /* Right earbud current sense */
    gpio_an(GPIO_R_CURRENT_SENSE);
#endif

    /* VBAT monitor reading */
    gpio_an(GPIO_VBAT_MONITOR);

    /* VBAT monitor on/off */
    gpio_disable(GPIO_VBAT_MONITOR_ON_OFF);
    gpio_output(GPIO_VBAT_MONITOR_ON_OFF);

    /* LEDs */
    gpio_disable(GPIO_LED_RED);
    gpio_output(GPIO_LED_RED);

    gpio_disable(GPIO_LED_GREEN);
    gpio_output(GPIO_LED_GREEN);

    gpio_disable(GPIO_LED_BLUE);
    gpio_output(GPIO_LED_BLUE);

#ifdef VARIANT_CB
    /* Regulator PFM/PWM. */
    gpio_disable(GPIO_VREG_PFM_PWM);
    gpio_output(GPIO_VREG_PFM_PWM);

    /* Regulator power good and LED red*/
    gpio_input(GPIO_VREG_PG);

    /* Regulator modulate */
    gpio_disable(GPIO_VREG_MOD);
    gpio_output(GPIO_VREG_MOD);
#endif

    /* Charger sense. */
    gpio_input(GPIO_CHG_SENSE);

#ifdef CHARGER_BQ24230
    /* BQ24230 charger */
    gpio_output(GPIO_CHG_EN2);
    gpio_output(GPIO_CHG_EN1);
    gpio_output(GPIO_CHG_CE_N);
    gpio_input(GPIO_CHG_STATUS_N);
#endif

    /* VBAT NTC thermistor */
    gpio_an(GPIO_NTC_MONITOR);
    gpio_output(GPIO_NTC_MONITOR_ON_OFF);
}

CLI_RESULT gpio_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(gpio_command, cmd_source);
}

static uint16_t gpio_pin_input(void)
{
    uint16_t ret = GPIO_NULL;
    char *token = cli_get_next_token();

    if (token && (strlen(token) >= 2))
    {
        char port = token[0] & ~0x20;

        if ((port >= 'A') && (port <= 'C'))
        {
            long int pin = strtol(&token[1], NULL, 10);

            if (pin <= 15)
            {
                ret = (uint16_t)(((port - 'A' + 1) << 4) + pin);
            }
        }
    }

    return ret;
}

static CLI_RESULT gpio_cmd_h(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint16_t pin = gpio_pin_input();
    GPIO_TypeDef *port = gpio_get_port(pin);
    uint16_t bit = gpio_get_bit(pin);

    if (pin != GPIO_NULL)
    {
        port->BSRR |= bit;
        ret = CLI_OK;
    }

    return ret;
}

static CLI_RESULT gpio_cmd_l(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint16_t pin = gpio_pin_input();
    GPIO_TypeDef *port = gpio_get_port(pin);
    uint16_t bit = gpio_get_bit(pin);

    if (pin != GPIO_NULL)
    {
        port->BRR |= bit;
        ret = CLI_OK;
    }

    return ret;
}

static CLI_RESULT gpio_cmd_i(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        gpio_input(pin);
        ret = CLI_OK;
    }

    return ret;
}

static CLI_RESULT gpio_cmd_o(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        gpio_output(pin);
        ret = CLI_OK;
    }

    return ret;
}

static CLI_RESULT gpio_cmd_ipd(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        gpio_input_pd(pin);
        ret = CLI_OK;
    }

    return ret;
}

static CLI_RESULT gpio_cmd_af(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    long int af;

    if (cli_get_next_parameter(&af, 10))
    {
        uint16_t pin = gpio_pin_input();

        if (pin != GPIO_NULL)
        {
            gpio_af(pin, (uint8_t)af);
            ret = CLI_OK;
        }
    }

    return ret;
}

static void gpio_pin_text(GPIO_TypeDef *port, uint8_t pin_no, char *pin_str)
{
    switch ((port->MODER >> (2*pin_no)) & 0x3)
    {
        case GPIO_Mode_IN:
            *pin_str++ = 'i';
            *pin_str++ = (port->IDR & (1 << pin_no)) ? '1':'0';
            break;

        case GPIO_Mode_OUT:
            *pin_str++ = 'o';
            *pin_str++ = (port->ODR & (1 << pin_no)) ? '1':'0';
            break;

        case GPIO_Mode_AF:
            *pin_str++ = 'a';
            *pin_str++ = 'f';
            break;

        case GPIO_Mode_AN:
            *pin_str++ = 'a';
            *pin_str++ = 'n';
            break;
    }

    *pin_str++ = 0;
}

static CLI_RESULT gpio_cmd_rd(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        char pin_str[3];

        gpio_pin_text(gpio_get_port(pin), (uint8_t)(pin & 0xF), pin_str);
        PRINT(pin_str);
        ret = CLI_OK;
    }

    return ret;
}

static CLI_RESULT gpio_cmd_display(uint8_t cmd_source)
{
    uint32_t p;

    PRINT("       0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15");

    for (p=0; p<NO_OF_PORTS; p++)
    {
        uint8_t pin;
        GPIO_TypeDef *port = (GPIO_TypeDef *)(AHB2PERIPH_BASE + (p << 10));

        PRINTF_U("GPIO%c", p + 'A');

        for (pin=0; pin<16; pin++)
        {
            char pin_str[4];

            pin_str[0] = ' ';
            gpio_pin_text(port, pin, &pin_str[1]);

            PRINTF_U(pin_str);
        }

        PRINT("");
    }

    return CLI_OK;
}

CLI_RESULT ats_gpio(uint8_t cmd_source)
{
    return gpio_cmd(cmd_source);
}
