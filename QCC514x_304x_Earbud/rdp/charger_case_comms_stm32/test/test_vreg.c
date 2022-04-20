/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for vreg.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "vreg.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_gpio.h"
#include "mock_cli.h"

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_command[] =
{
    { "REGULATOR", ats_regulator, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();
#ifdef SCHEME_A
    memset(GPIO_PORT(GPIO_VREG_MOD), 0, sizeof(GPIO_TypeDef));
#endif
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* AT+REGULATOR command.
*/
void test_vreg_ats_regulator(void)
{
    /*
    * Command entered without parameters, reject.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("REGULATOR="));

#ifdef SCHEME_A
    /*
    * AT+REGULATOR=0 (disable regulator).
    */
    memset(GPIO_PORT(GPIO_VREG_MOD), 0, sizeof(GPIO_TypeDef));
    gpio_disable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=0"));
    TEST_ASSERT_TRUE(GPIO_CLEARING(GPIO_VREG_MOD));
    TEST_ASSERT_FALSE(GPIO_SETTING(GPIO_VREG_MOD));
    TEST_ASSERT_EQUAL_HEX8(GPIO_Mode_OUT, GPIO_MODE(GPIO_VREG_MOD));

    /*
    * AT+REGULATOR=1 (enable regulator, level unspecified so treated as low).
    */
    memset(GPIO_PORT(GPIO_VREG_MOD), 0, sizeof(GPIO_TypeDef));
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1"));
    TEST_ASSERT_FALSE(GPIO_CLEARING(GPIO_VREG_MOD));
    TEST_ASSERT_FALSE(GPIO_SETTING(GPIO_VREG_MOD));
    TEST_ASSERT_EQUAL_HEX8(GPIO_Mode_IN, GPIO_MODE(GPIO_VREG_MOD));

    /*
    * AT+REGULATOR=1,0 (enable regulator, high voltage).
    */
    memset(GPIO_PORT(GPIO_VREG_MOD), 0, sizeof(GPIO_TypeDef));
    gpio_enable_Expect(GPIO_VREG_PFM_PWM);
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1,0"));
    TEST_ASSERT_TRUE(GPIO_CLEARING(GPIO_VREG_MOD));
    TEST_ASSERT_FALSE(GPIO_SETTING(GPIO_VREG_MOD));
    TEST_ASSERT_EQUAL_HEX8(GPIO_Mode_OUT, GPIO_MODE(GPIO_VREG_MOD));

    /*
    * AT+REGULATOR=1,1 (enable regulator, low voltage).
    */
    memset(GPIO_PORT(GPIO_VREG_MOD), 0, sizeof(GPIO_TypeDef));
    gpio_enable_Expect(GPIO_VREG_PFM_PWM);
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1,1"));
    TEST_ASSERT_FALSE(GPIO_CLEARING(GPIO_VREG_MOD));
    TEST_ASSERT_FALSE(GPIO_SETTING(GPIO_VREG_MOD));
    TEST_ASSERT_EQUAL_HEX8(GPIO_Mode_IN, GPIO_MODE(GPIO_VREG_MOD));

    /*
    * AT+REGULATOR=1,2 (enable regulator, 'reset' level).
    */
    memset(GPIO_PORT(GPIO_VREG_MOD), 0, sizeof(GPIO_TypeDef));
    gpio_enable_Expect(GPIO_VREG_PFM_PWM);
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1,2"));
    TEST_ASSERT_FALSE(GPIO_CLEARING(GPIO_VREG_MOD));
    TEST_ASSERT_TRUE(GPIO_SETTING(GPIO_VREG_MOD));
    TEST_ASSERT_EQUAL_HEX8(GPIO_Mode_OUT, GPIO_MODE(GPIO_VREG_MOD));

#endif /*SCHEME_A*/

#ifdef SCHEME_B

    /*
    * AT+REGULATOR=0.
    */
    gpio_disable_Expect(GPIO_VREG_ISO);
    gpio_disable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=0"));

    /*
    * AT+REGULATOR=1.
    */
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    gpio_enable_Expect(GPIO_VREG_ISO);
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1"));

    /*
    * AT+REGULATOR=1,0.
    */
    gpio_disable_Expect(GPIO_VREG_SEL);
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    gpio_enable_Expect(GPIO_VREG_ISO);
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1,0"));

    /*
    * AT+REGULATOR=1,1.
    */
    gpio_enable_Expect(GPIO_VREG_SEL);
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    gpio_enable_Expect(GPIO_VREG_ISO);
    gpio_enable_Expect(GPIO_VREG_EN);
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("REGULATOR=1,1"));

    /*
    * AT+REGULATOR=1,2 rejected.
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("REGULATOR=1,2"));

#endif /*SCHEME_B*/

}
