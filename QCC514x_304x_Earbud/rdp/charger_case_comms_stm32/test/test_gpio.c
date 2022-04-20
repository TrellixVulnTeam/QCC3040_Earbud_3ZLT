/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for gpio.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "gpio.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_stm32f0xx_rcc.h"
#include "mock_stm32f0xx_gpio.h"
#include "mock_cli.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR_DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define MAX_NO_OF_INIT_STRUCTS 100

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

GPIO_InitTypeDef *get_init_struct(uint16_t pin);
void update_output_registers(void);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_gpio_command[] =
{
    { "gpio", gpio_cmd, 2 },
    { NULL }
};

static const CLI_COMMAND test_ats_gpio_command[] =
{
    { "GPIO", ats_gpio, 2 },
    { NULL }
};

GPIO_InitTypeDef init_structs[MAX_NO_OF_INIT_STRUCTS];
uint8_t init_struct_ctr = 0;

GPIO_InitTypeDef default_init_struct =
{
    GPIO_Pin_All,
    GPIO_Mode_IN,
    GPIO_Speed_Level_2,
    GPIO_OType_PP,
    GPIO_PuPd_NOPULL
};

/*-----------------------------------------------------------------------------
------------------ EXPECT FUNCTIONS -------------------------------------------
-----------------------------------------------------------------------------*/

void expect_gpio_struct_init(void)
{
    GPIO_StructInit_Expect(&default_init_struct);
    GPIO_StructInit_IgnoreArg_GPIO_InitStruct();
    GPIO_StructInit_ReturnThruPtr_GPIO_InitStruct(&default_init_struct);
}

void expect_init_pd(uint16_t pin)
{
    GPIO_InitTypeDef *is = get_init_struct(pin);

    is->GPIO_PuPd = GPIO_PuPd_DOWN;

    expect_gpio_struct_init();
    GPIO_Init_Expect(GPIO_PORT(pin), is);

    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2));
    GPIO_PORT(pin)->MODER |= GPIO_Mode_IN << ((pin & 0xF) * 2);
}

void expect_init_af(uint16_t pin, uint8_t af)
{
    GPIO_InitTypeDef *is = get_init_struct(pin);

    is->GPIO_Mode = GPIO_Mode_AF;

    expect_gpio_struct_init();
    GPIO_Init_Expect(GPIO_PORT(pin), is);
    GPIO_PinAFConfig_Expect(gpio_get_port(pin), pin & 0xF, af);

    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2));
    GPIO_PORT(pin)->MODER |= GPIO_Mode_AF << ((pin & 0xF) * 2);
    GPIO_PORT(pin)->AFR[(pin & 0xF) >> 3] &= ~(0xF << ((pin & 0x7) * 4));
    GPIO_PORT(pin)->AFR[(pin & 0xF) >> 3] |= af << ((pin & 0x7) * 4);
}

void expect_init_input(uint16_t pin)
{
    GPIO_InitTypeDef *is = get_init_struct(pin);

    expect_gpio_struct_init();
    GPIO_Init_Expect(GPIO_PORT(pin), is);

    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2));
    GPIO_PORT(pin)->MODER |= GPIO_Mode_IN << ((pin & 0xF) * 2);
}

void expect_init_output(uint16_t pin)
{
    GPIO_InitTypeDef *is = get_init_struct(pin);

    is->GPIO_Mode = GPIO_Mode_OUT;

    expect_gpio_struct_init();
    GPIO_Init_Expect(GPIO_PORT(pin), is);

    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2));
    GPIO_PORT(pin)->MODER |= GPIO_Mode_OUT << ((pin & 0xF) * 2);
}

void expect_init_an(uint16_t pin)
{
    GPIO_InitTypeDef *is = get_init_struct(pin);

    is->GPIO_Mode = GPIO_Mode_AN;

    expect_gpio_struct_init();
    GPIO_Init_Expect(GPIO_PORT(pin), is);

    GPIO_PORT(pin)->MODER &= ~(0x3 << ((pin & 0xF) * 2));
    GPIO_PORT(pin)->MODER |= GPIO_Mode_AN << ((pin & 0xF) * 2);
}

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_gpio_command, CLI_SOURCE_UART, str);
}

CLI_RESULT do_at_cmd(char *str)
{
    return common_cmd(test_ats_gpio_command, CLI_SOURCE_UART, str);
}

void do_gpio_init(void)
{
    RCC_AHBPeriphClockCmd_Expect(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        ENABLE);

    expect_init_pd(GPIO_MAG_SENSOR);
    expect_init_af(GPIO_UART_TX, GPIO_AF_0);
    expect_init_af(GPIO_UART_RX, GPIO_AF_0);

#ifdef VARIANT_ST2
    expect_init_af(GPIO_DOCK_DATA_TX, GPIO_AF_4);
    expect_init_af(GPIO_DOCK_DATA_RX, GPIO_AF_4);
#endif

#ifdef VARIANT_CB
    expect_init_an(GPIO_L_CURRENT_SENSE);
    expect_init_an(GPIO_R_CURRENT_SENSE);
#endif

    expect_init_an(GPIO_VBAT_MONITOR);
    expect_init_output(GPIO_VBAT_MONITOR_ON_OFF);
    expect_init_output(GPIO_LED_RED);
    expect_init_output(GPIO_LED_GREEN);
    expect_init_output(GPIO_LED_BLUE);

#ifdef VARIANT_CB
    expect_init_output(GPIO_VREG_PFM_PWM);
    expect_init_input(GPIO_VREG_PG);
    expect_init_output(GPIO_VREG_MOD);
#endif

    expect_init_input(GPIO_CHG_SENSE);

#ifdef CHARGER_BQ24230
    expect_init_output(GPIO_CHG_EN2);
    expect_init_output(GPIO_CHG_EN1);
    expect_init_output(GPIO_CHG_CE_N);
    expect_init_input(GPIO_CHG_STATUS_N);
#endif

    expect_init_an(GPIO_NTC_MONITOR);
    expect_init_output(GPIO_NTC_MONITOR_ON_OFF);

    expect_init_output(GPIO_VREG_EN);

#ifdef EARBUD_CURRENT_SENSES
    expect_init_output(GPIO_CURRENT_SENSE_AMP);
#endif

#ifdef VARIANT_ST2
    expect_init_output(GPIO_DOCK_PULL_EN);
    expect_init_output(GPIO_VREG_ISO);
    expect_init_output(GPIO_VREG_SEL);
#endif

    gpio_init();

    update_output_registers();

    /*
    * Go to sleep.
    */
    RCC_AHBPeriphClockCmd_Expect(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        DISABLE);
    gpio_clock_disable();

    /*
    * Wake up.
    */
    RCC_AHBPeriphClockCmd_Expect(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        ENABLE);
    gpio_clock_enable();

    update_output_registers();
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();
    init_struct_ctr = 0;
    memset(test_AHB2PERIPH, 0, sizeof(test_AHB2PERIPH));

    do_gpio_init();
}

void tearDown(void)
{
}

GPIO_InitTypeDef *get_init_struct(uint16_t pin)
{
    GPIO_InitTypeDef *is;

    TEST_ASSERT_LESS_THAN(MAX_NO_OF_INIT_STRUCTS, init_struct_ctr);

    is = &init_structs[init_struct_ctr++];
    memcpy(is, &default_init_struct, sizeof(GPIO_InitTypeDef));
    is->GPIO_Pin = GPIO_BIT(pin);

    return is;
}

void update_output_registers(void)
{
    GPIOA->ODR |= GPIOA->BSRR;
    GPIOB->ODR |= GPIOB->BSRR;
    GPIOC->ODR |= GPIOC->BSRR;

    GPIOA->ODR &= ~GPIOA->BRR;
    GPIOB->ODR &= ~GPIOB->BRR;
    GPIOC->ODR &= ~GPIOC->BRR;

    GPIOA->BSRR = 0;
    GPIOB->BSRR = 0;
    GPIOC->BSRR = 0;

    GPIOA->BRR = 0;
    GPIOB->BRR = 0;
    GPIOC->BRR = 0;
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* GPIO display.
*/
void test_gpio_display(void)
{
    /*
    * Make one input pin high.
    */
    GPIOA->IDR |= GPIO_Pin_0;

    cli_tx_Expect(CLI_SOURCE_UART, true, "       0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15");

#ifdef VARIANT_ST2

    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOA");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i1");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOB");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " af");
    cli_tx_Expect(CLI_SOURCE_UART, false, " af");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " af");
    cli_tx_Expect(CLI_SOURCE_UART, false, " af");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o1");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOC");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");

#else

    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOA");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i1");
    cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOB");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o1");
    cli_tx_Expect(CLI_SOURCE_UART, false, " af");
    cli_tx_Expect(CLI_SOURCE_UART, false, " af");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " o0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOC");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");
    cli_tx_Expect(CLI_SOURCE_UART, false, " i0");

#endif /*VARIANT_ST2*/

    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("gpio");
}

/*
* Read the state of an individual pin using the AT command.
*/
void test_gpio_read(void)
{
    /*
    * Make one input pin high.
    */
    GPIOA->IDR |= GPIO_Pin_0;

    cli_tx_Expect(CLI_SOURCE_UART, true, "i1");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,A0"));

    cli_tx_Expect(CLI_SOURCE_UART, true, "i0");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,C14"));

#ifdef VARIANT_ST2
    cli_tx_Expect(CLI_SOURCE_UART, true, "o0");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,B8"));
#else
    cli_tx_Expect(CLI_SOURCE_UART, true, "o1");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,B5"));
#endif

    cli_tx_Expect(CLI_SOURCE_UART, true, "o0");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,B4"));

    cli_tx_Expect(CLI_SOURCE_UART, true, "an");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,A6"));

    cli_tx_Expect(CLI_SOURCE_UART, true, "af");
    TEST_ASSERT_EQUAL(CLI_OK, do_at_cmd("GPIO=RD,B7"));

    /*
    * Reject invalid commands.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_at_cmd("GPIO=RD"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_at_cmd("GPIO=RD,X8"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_at_cmd("GPIO=RD,C16"));
}

/*
* Pin manipulation commands.
*/
void test_gpio_commands(void)
{
    /*
    * Reject invalid commands.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio h"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio l x9"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio i a"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio o 12"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio ipd a16"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio af"));
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("gpio af 0"));

    /*
    * Set a pin high.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("gpio h c0"));
    update_output_registers();
    TEST_ASSERT_EQUAL(GPIOC->ODR & GPIO_Pin_0, GPIO_Pin_0);

    /*
    * Set a pin low.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("gpio l c0"));
    update_output_registers();
    TEST_ASSERT_EQUAL(GPIOC->ODR & GPIO_Pin_0, 0);

    /*
    * Configure a pin to be an input.
    */
    expect_init_input(GPIO_C15);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("gpio i c15"));
    TEST_ASSERT_EQUAL(GPIO_Mode_IN, GPIO_MODE(GPIO_C15));

    /*
    * Configure a pin to be an output.
    */
    expect_init_output(GPIO_C15);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("gpio o c15"));
    TEST_ASSERT_EQUAL(GPIO_Mode_OUT, GPIO_MODE(GPIO_C15));

    /*
    * Configure a pin to be an input with pull-down.
    */
    expect_init_pd(GPIO_C15);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("gpio ipd C15"));
    TEST_ASSERT_EQUAL(GPIO_Mode_IN, GPIO_MODE(GPIO_C15));

    /*
    * Configure a pin for alternate function.
    */
    expect_init_af(GPIO_C15, 2);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("gpio af 2 C15"));
    TEST_ASSERT_EQUAL(GPIO_Mode_AF, GPIO_MODE(GPIO_C15));
    TEST_ASSERT_EQUAL(2, GPIO_AF(GPIO_C15));
}

/*
* Enable/disable.
*/
void test_gpio_enable_disable(void)
{
    /*
    * Enable an active high pin.
    */
    gpio_enable(GPIO_A2);
    TEST_ASSERT_EQUAL(GPIOA->BSRR & GPIO_Pin_2, GPIO_Pin_2);
    update_output_registers();
    TEST_ASSERT_EQUAL(GPIOA->ODR & GPIO_Pin_2, GPIO_Pin_2);

    /*
    * Disable an active high pin.
    */
    gpio_disable(GPIO_A2);
    TEST_ASSERT_EQUAL(GPIOA->BRR & GPIO_Pin_2, GPIO_Pin_2);
    update_output_registers();
    TEST_ASSERT_EQUAL(GPIOA->ODR & GPIO_Pin_2, 0);

    /*
    * Enable an active low pin.
    */
    gpio_enable(GPIO_B3 | GPIO_ACTIVE_LOW);
    TEST_ASSERT_EQUAL(GPIOB->BRR & GPIO_Pin_3, GPIO_Pin_3);
    update_output_registers();
    TEST_ASSERT_EQUAL(GPIOB->ODR & GPIO_Pin_3, 0);

    /*
    * Disable an active low pin.
    */
    gpio_disable(GPIO_B3 | GPIO_ACTIVE_LOW);
    TEST_ASSERT_EQUAL(GPIOB->BSRR & GPIO_Pin_3, GPIO_Pin_3);
    update_output_registers();
    TEST_ASSERT_EQUAL(GPIOB->ODR & GPIO_Pin_3, GPIO_Pin_3);
}

/*
* Test gpio_active().
*/
void test_gpio_active(void)
{
    /*
    * High and active high = active.
    */
    GPIOA->IDR |= GPIO_Pin_0;
    TEST_ASSERT_TRUE(gpio_active(GPIO_A0));

    /*
    * Low and active high = inactive.
    */
    GPIOA->IDR &= ~GPIO_Pin_0;
    TEST_ASSERT_FALSE(gpio_active(GPIO_A0));

    /*
    * High and active low = inactive.
    */
    GPIOB->IDR |= GPIO_Pin_2;
    TEST_ASSERT_FALSE(gpio_active(GPIO_B2 | GPIO_ACTIVE_LOW));

    /*
    * Low and active low = active.
    */
    GPIOB->IDR &= ~GPIO_Pin_2;
    TEST_ASSERT_TRUE(gpio_active(GPIO_A0 | GPIO_ACTIVE_LOW));
}

/*
* Test gpio_disable_all().
*/
void test_gpio_disable_all(void)
{
    uint8_t n;

    gpio_disable_all();

    cli_tx_Expect(CLI_SOURCE_UART, true, "       0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOA");
    for (n=0; n<16; n++)
    {
        cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    }
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOB");
    for (n=0; n<16; n++)
    {
        cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    }
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "GPIOC");
    for (n=0; n<16; n++)
    {
        cli_tx_Expect(CLI_SOURCE_UART, false, " an");
    }
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("gpio");
}
