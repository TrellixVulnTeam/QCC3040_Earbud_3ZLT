/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for crc.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "crc.h"

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

void test_crc8(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x18, crc_calculate_crc8("\x12\x03", 2));
    TEST_ASSERT_TRUE(crc_verify_crc8("\x12\x03\x18", 3));
    TEST_ASSERT_FALSE(crc_verify_crc8("\x12\x03\x17", 3));    
}

void test_crc16(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xD095, crc_calculate_crc16("\x20\x03\x03", 3));
    TEST_ASSERT_TRUE(crc_verify_crc16("\x20\x03\x03\xD0\x95", 5));
    TEST_ASSERT_FALSE(crc_verify_crc16("\x20\x03\x03\xD0\x94", 5));

    TEST_ASSERT_EQUAL_HEX16(0xCB92, crc_calculate_crc16("\x30\x04\x00\x04", 4));
    TEST_ASSERT_EQUAL_HEX16(0x1530, crc_calculate_crc16("\x10\x03\x03", 3));
    TEST_ASSERT_EQUAL_HEX16(0xD200, crc_calculate_crc16("\xE0\x02", 2));
    TEST_ASSERT_EQUAL_HEX16(0xC454, crc_calculate_crc16("\x20\x02", 2));
    TEST_ASSERT_EQUAL_HEX16(0xCE88, crc_calculate_crc16("\x30\x07\x00\x04\x80\x21\x2B", 7));
}
