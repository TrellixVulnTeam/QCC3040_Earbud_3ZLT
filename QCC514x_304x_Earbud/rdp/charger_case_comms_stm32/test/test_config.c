/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for config.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "config.c"
#include "crc.c"
#include "cli_txf.c"
#include "cli_parse.c"
#include "common_cmd.c"

#include "mock_cli.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND test_config_command[] =
{
    { "config", ats_config, 2 },
    { NULL }
};

bool test_flash_is_locked;
bool test_erase_page_fail;

/*-----------------------------------------------------------------------------
------------------ EXPECT FUNCTIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT do_cmd(char *str)
{
    return common_cmd(test_config_command, CLI_SOURCE_UART, str);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    common_cmd_init();

    memset(_flash_cfg, 0xFF, sizeof(_flash_cfg));
    test_flash_is_locked = true;
    test_erase_page_fail = false;
}

void tearDown(void)
{
}

/*
* Simulation of the flash_lock() function.
*/
void flash_lock(void)
{
    test_flash_is_locked = true;
}

/*
* Simulation of the flash_unlock() function.
*/
void flash_unlock(void)
{
    test_flash_is_locked = false;
}

/*
* Simulation of the flash_erase_page() function. The real function can erase
* any page in flash, but here we are only interested in its use for erasing
* the config page.
*/
bool flash_erase_page(uint32_t page_address)
{
    bool ret = false;

    if (page_address != (uint32_t)_flash_cfg)
    {
        TEST_FAIL();
    }

    if (!test_erase_page_fail && !test_flash_is_locked)
    {
        memset(_flash_cfg, 0xFF, sizeof(_flash_cfg));
        ret = true;
    }

    return ret;
}

/*
* Simulation of the flash_write() function.
*/
bool flash_write(uint32_t address, uint32_t data)
{
    bool ret = false;

    if (!test_flash_is_locked)
    {
        *((uint32_t *)address) = data;
        ret = true;
    }

    return ret;
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Verbose config display.
*/
void test_config_verbose(void)
{
    uint8_t cfg[] =
    {
        0xE1, 0xAC, 0x6F, 0xD0, 0x17, 0x00, 0x04, 0xEF,
        0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0xF4,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x0B, 0x01, 0x00, 0x00, 0x00
    };

    memcpy(_flash_cfg, cfg, sizeof(cfg));

    config_init();

    cli_tx_Expect(CLI_SOURCE_UART, false, "serial : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "stc    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "sto    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "bco    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "id     : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config");
}

/*
* Terse config display using AT+CONFIG?
*/
void test_config_terse(void)
{
    uint8_t cfg[] =
    {
        0xE1, 0xAC, 0x6F, 0xD0, 0x17, 0x00, 0x04, 0xEF,
        0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0xF4,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x0B, 0x01, 0x00, 0x00, 0x00
    };

    memcpy(_flash_cfg, cfg, sizeof(cfg));

    config_init();

    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, false, ",");
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, false, ",");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, false, ",");
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, false, ",");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    atq_config(CLI_SOURCE_UART);
}

/*
* Saved config has a different code to the current firmware, so
* default values are assumed (except for the serial number).
*/
void test_config_version_changed(void)
{
    uint8_t cfg[] =
    {
        0xE0, 0xAC, 0x6F, 0xD0, 0x17, 0x00, 0x04, 0xEF,
        0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0xF4,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x0B, 0x01, 0x00, 0x00, 0x00
    };

    memcpy(_flash_cfg, cfg, sizeof(cfg));

    config_init();

    cli_tx_Expect(CLI_SOURCE_UART, false, "serial : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "stc    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "sto    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "bco    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "id     : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config");
}

/*
* Saved config has an incorrect checksum, so default values are assumed.
*/
void test_config_checksum_error(void)
{
    uint8_t cfg[] =
    {
        0xE1, 0xAC, 0x6F, 0xD0, 0x17, 0x00, 0x03, 0xEF,
        0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0xF4,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x0B, 0x01, 0x00, 0x00, 0x00
    };

    memcpy(_flash_cfg, cfg, sizeof(cfg));

    config_init();

    cli_tx_Expect(CLI_SOURCE_UART, false, "serial : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "stc    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "sto    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "bco    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "id     : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config");
}

/*
* Valid config stored, but there is more of it than expected.
*/
void test_config_unexpectedly_large(void)
{
    uint8_t cfg[] =
    {
        0xE1, 0xAC, 0x6F, 0xD0, 0x39, 0x00, 0x43, 0xEF,
        0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0xF4,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x0B, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00
    };

    memcpy(_flash_cfg, cfg, sizeof(cfg));

    config_init();

    cli_tx_Expect(CLI_SOURCE_UART, false, "serial : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "stc    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "sto    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "bco    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "id     : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config");
}

/*
* Valid config stored, but there is less of it than expected.
*/
void test_config_unexpectedly_small(void)
{
    uint8_t cfg[] =
    {
        0xE1, 0xAC, 0x6F, 0xD0, 0x0C, 0x00, 0x41, 0xEF,
        0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12, 0xF4,
        0x01, 0x00, 0x00
    };

    memcpy(_flash_cfg, cfg, 19);

    config_init();

    cli_tx_Expect(CLI_SOURCE_UART, false, "serial : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "stc    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "sto    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "bco    : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    cli_tx_Expect(CLI_SOURCE_UART, false, "id     : ");
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config");
}

/*
* Serial number.
*/
void test_config_serial(void)
{
    uint64_t cgs;

    config_init();

    /*
    * Display the default serial number.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "FFFFFFFFFFFFFFFF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config serial");

    /*
    * Try to set the serial number, but the page erase fails.
    */
    test_erase_page_fail = true;
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("config serial 1234567890abcdef"));
    test_erase_page_fail = false;

    /*
    * Set the serial number.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("config serial 1234567890abcdef"));

    TEST_ASSERT_EQUAL_HEX8(
        crc_calculate_crc8((uint8_t *)&config.data, sizeof(CONFIG_DATA)),
        config.checksum);

    /*
    * Read back contents of flash.
    */
    config_init();

    /*
    * Display the serial number.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "1234567890ABCDEF");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config serial");

    /*
    * Test config_get_serial().
    */
    cgs = config_get_serial();
    if (cgs != 0x1234567890ABCDEF)
    {
        printf("config_get_serial() returned %llX, expected 0x1234567890ABCDEF", cgs);
        TEST_FAIL();
    }
}

/*
* Status Timeout (Lid Closed).
*/
void test_config_stc(void)
{
    config_init();

    /*
    * Display the default timeout.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "60");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config stc");

    /*
    * Attempt to set the timeout (too low).
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("config stc 0"));

    /*
    * Set the timeout.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("config stc 2"));

    TEST_ASSERT_EQUAL_HEX8(
        crc_calculate_crc8((uint8_t *)&config.data, sizeof(CONFIG_DATA)),
        config.checksum);

    /*
    * Read back contents of flash.
    */
    config_init();

    /*
    * Display the serial number.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "2");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config stc");

    /*
    * Test config_get_status_time_closed().
    */
    TEST_ASSERT_EQUAL(2, config_get_status_time_closed());
}

/*
* Status Timeout (Lid Open).
*/
void test_config_sto(void)
{
    config_init();

    /*
    * Display the default timeout.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config sto");

    /*
    * Attempt to set the timeout (too low).
    */
    TEST_ASSERT_EQUAL(CLI_ERROR, do_cmd("config sto 0"));

    /*
    * Set the timeout.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("config sto 3"));

    TEST_ASSERT_EQUAL_HEX8(
        crc_calculate_crc8((uint8_t *)&config.data, sizeof(CONFIG_DATA)),
        config.checksum);

    /*
    * Read back contents of flash.
    */
    config_init();

    /*
    * Display the serial number.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "3");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config sto");

    /*
    * Test config_get_status_time_open().
    */
    TEST_ASSERT_EQUAL(3, config_get_status_time_open());
}

/*
* Battery cutoff.
*/
void test_config_bco(void)
{
    config_init();

    /*
    * Display the default battery cutoff level.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "3200");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config bco");

    /*
    * Set the battery cutoff level.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("config bco 2000"));

    TEST_ASSERT_EQUAL_HEX8(
        crc_calculate_crc8((uint8_t *)&config.data, sizeof(CONFIG_DATA)),
        config.checksum);

    /*
    * Read back contents of flash.
    */
    config_init();

    /*
    * Display the battery cutoff level.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "2000");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config bco");
}

/*
* Board ID.
*/
void test_config_id(void)
{
    config_init();

    /*
    * Display the default board ID.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "0");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config id");

    /*
    * Set the board ID.
    */
    TEST_ASSERT_EQUAL(CLI_OK, do_cmd("config id 1"));

    TEST_ASSERT_EQUAL_HEX8(
        crc_calculate_crc8((uint8_t *)&config.data, sizeof(CONFIG_DATA)),
        config.checksum);

    /*
    * Read back contents of flash.
    */
    config_init();

    /*
    * Display the board ID.
    */
    cli_tx_Expect(CLI_SOURCE_UART, false, "1");
    cli_tx_Expect(CLI_SOURCE_UART, true, "");
    do_cmd("config id");
}
