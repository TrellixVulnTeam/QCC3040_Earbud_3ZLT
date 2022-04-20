/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for charger_detect.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "charger_detect.c"

#include "mock_usb.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

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

/*
* Charger detect start and cancel.
*/
void test_charger_detect_start_and_cancel(void)
{
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    usb_activate_bcd_Expect();
    charger_detect_start();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START);

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD);

    usb_deactivate_bcd_Expect();
    charger_detect_cancel();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);
}

/*
* 5V applied to VBUS and no contact to the USB data lines.
*/
void test_charger_detect_floating_wall_charger(void)
{
    uint8_t n;

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    usb_activate_bcd_Expect();
    charger_detect_start();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START);

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD);

    for (n=0; n<CHARGER_DETECT_DCD_TIMEOUT_TICKS; n++)
    {
        usb_dcd_ExpectAndReturn(false);
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD);
    }

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);

    for (n=0; n<CHARGER_DETECT_DCD_REREAD_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);
    }

    usb_dcd_ExpectAndReturn(false);
    usb_dcd_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_FINISH);

    usb_deactivate_bcd_Expect();
    usb_start_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    TEST_ASSERT_EQUAL(CHARGER_DETECT_TYPE_FLOATING, charger_detect_get_type());
}

/*
* SDP detected.
*/
void test_charger_detect_sdp(void)
{
    uint8_t n;

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    usb_activate_bcd_Expect();
    charger_detect_start();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START);

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD);

    usb_dcd_ExpectAndReturn(true);
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);

    for (n=0; n<CHARGER_DETECT_DCD_REREAD_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);
    }

    usb_dcd_ExpectAndReturn(true);
    usb_dcd_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_PRIMARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_PRIMARY_DETECTION);
    }

    usb_primary_detection_enable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_PRIMARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_PRIMARY_DETECTION);
    }

    usb_pdet_ExpectAndReturn(false);
    usb_primary_detection_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_FINISH);

    usb_deactivate_bcd_Expect();
    usb_start_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    TEST_ASSERT_EQUAL(CHARGER_DETECT_TYPE_SDP, charger_detect_get_type());
}

/*
* DCP detected.
*/
void test_charger_detect_dcp(void)
{
    uint8_t n;

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    usb_activate_bcd_Expect();
    charger_detect_start();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START);

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD);

    usb_dcd_ExpectAndReturn(true);
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);

    for (n=0; n<CHARGER_DETECT_DCD_REREAD_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);
    }

    usb_dcd_ExpectAndReturn(true);
    usb_dcd_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_PRIMARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_PRIMARY_DETECTION);
    }

    usb_primary_detection_enable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_PRIMARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_PRIMARY_DETECTION);
    }

    usb_pdet_ExpectAndReturn(true);
    usb_primary_detection_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_SECONDARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_SECONDARY_DETECTION);
    }

    usb_secondary_detection_enable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_SECONDARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_SECONDARY_DETECTION);
    }

    usb_sdet_ExpectAndReturn(true);
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_FINISH);

    usb_deactivate_bcd_Expect();
    usb_start_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    TEST_ASSERT_EQUAL(CHARGER_DETECT_TYPE_DCP, charger_detect_get_type());
}

/*
* CDP detected.
*/
void test_charger_detect_cdp(void)
{
    uint8_t n;

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    usb_activate_bcd_Expect();
    charger_detect_start();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START);

    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD);

    usb_dcd_ExpectAndReturn(true);
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);

    for (n=0; n<CHARGER_DETECT_DCD_REREAD_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_DCD_REREAD);
    }

    usb_dcd_ExpectAndReturn(true);
    usb_dcd_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_PRIMARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_PRIMARY_DETECTION);
    }

    usb_primary_detection_enable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_PRIMARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_PRIMARY_DETECTION);
    }

    usb_pdet_ExpectAndReturn(true);
    usb_primary_detection_disable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_SECONDARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_START_SECONDARY_DETECTION);
    }

    usb_secondary_detection_enable_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_SECONDARY_DETECTION);

    for (n=0; n<CHARGER_DETECT_MODE_CHANGE_TICKS; n++)
    {
        charger_detect_periodic();
        TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_SECONDARY_DETECTION);
    }

    usb_sdet_ExpectAndReturn(false);
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_FINISH);

    usb_deactivate_bcd_Expect();
    usb_start_Expect();
    charger_detect_periodic();
    TEST_ASSERT_EQUAL(charger_detect_state, CHARGER_DETECT_IDLE);

    TEST_ASSERT_EQUAL(CHARGER_DETECT_TYPE_CDP, charger_detect_get_type());
}
