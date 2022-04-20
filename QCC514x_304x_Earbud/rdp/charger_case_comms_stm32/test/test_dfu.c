/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for dfu.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "dfu.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_stm32f0xx_rcc.h"
#include "mock_flash.h"
#include "mock_power.h"
#include "mock_cli.h"
#include "mock_case.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

volatile uint32_t ticks;

static const CLI_COMMAND test_cli_command[] =
{
    { "dfu", dfu_cmd, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static void do_cmd(char *str)
{
    common_cmd(test_cli_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    ticks = 0;

    dfu_state = DFU_IDLE;
    memset(srec_data, 0, sizeof(srec_data));
    srec_data_len = 0;
    dfu_running_image_a = true;
    dfu_image_start = IMAGE_B_START;
    dfu_image_end = IMAGE_B_END;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* DFU.
*/
void test_dfu(void)
{
    power_set_run_reason_Expect(POWER_RUN_DFU);
    cli_intercept_line_Expect(CLI_SOURCE_UART, dfu_rx);
    flash_unlock_Expect();
    flash_erase_page_ExpectAndReturn(0x8010800, true);
    flash_erase_page_ExpectAndReturn(0x8011000, true);
    flash_erase_page_ExpectAndReturn(0x8011800, true);
    flash_erase_page_ExpectAndReturn(0x8012000, true);
    flash_erase_page_ExpectAndReturn(0x8012800, true);
    flash_erase_page_ExpectAndReturn(0x8013000, true);
    flash_erase_page_ExpectAndReturn(0x8013800, true);
    flash_erase_page_ExpectAndReturn(0x8014000, true);
    flash_erase_page_ExpectAndReturn(0x8014800, true);
    flash_erase_page_ExpectAndReturn(0x8015000, true);
    flash_erase_page_ExpectAndReturn(0x8015800, true);
    flash_erase_page_ExpectAndReturn(0x8016000, true);
    flash_erase_page_ExpectAndReturn(0x8016800, true);
    flash_erase_page_ExpectAndReturn(0x8017000, true);
    flash_erase_page_ExpectAndReturn(0x8017800, true);
    flash_erase_page_ExpectAndReturn(0x8018000, true);
    flash_erase_page_ExpectAndReturn(0x8018800, true);
    flash_erase_page_ExpectAndReturn(0x8019000, true);
    flash_erase_page_ExpectAndReturn(0x8019800, true);
    flash_erase_page_ExpectAndReturn(0x801A000, true);
    flash_erase_page_ExpectAndReturn(0x801A800, true);
    flash_erase_page_ExpectAndReturn(0x801B000, true);
    flash_erase_page_ExpectAndReturn(0x801B800, true);
    flash_erase_page_ExpectAndReturn(0x801C000, true);
    flash_erase_page_ExpectAndReturn(0x801C800, true);
    flash_erase_page_ExpectAndReturn(0x801D000, true);
    flash_erase_page_ExpectAndReturn(0x801D800, true);
    flash_erase_page_ExpectAndReturn(0x801E000, true);
    flash_erase_page_ExpectAndReturn(0x801E800, true);
    flash_erase_page_ExpectAndReturn(0x801F000, true);
    flash_erase_page_ExpectAndReturn(0x801F800, true);
    cli_tx_Expect(CLI_SOURCE_UART, true, "DFU: Wait");
    do_cmd("dfu");
    TEST_ASSERT_EQUAL(DFU_WAITING, dfu_state);

    cli_tx_Expect(CLI_SOURCE_UART, true, "DFU: Busy");
    do_cmd("dfu");
}

