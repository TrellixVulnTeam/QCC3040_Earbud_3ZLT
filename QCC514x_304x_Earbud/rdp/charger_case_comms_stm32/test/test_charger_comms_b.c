/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for charger_comms.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef VARIANT_ST2

#include "unity.h"

#include "charger_comms_b.c"
#include "cli_txf.c"

#include "mock_cli.h"
#include "mock_charger_comms_if.h"
#include "mock_wire.h"
#include "mock_uart.h"
#include "mock_power.h"
#include "mock_gpio.h"
#include "mock_vreg.h"

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
    memset(cc_rx_buf, 0, sizeof(cc_rx_buf));
    cc_rx_buf_ctr = 0;
    cc_state = CHARGER_COMMS_IDLE;

    charger_comms_periodic();
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Short status message is broadcast.
*/
void test_ccb_short_broadcast(void)
{
    uint16_t n;

    TEST_ASSERT_FALSE(charger_comms_is_active());

    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->COMMS", "\x30\x04\x00\x04\xCB\x92", 6, 6);
    vreg_disable_Expect();
    power_set_run_reason_Expect(POWER_RUN_CHARGER_COMMS);

    charger_comms_periodic();
    charger_comms_periodic();
    gpio_enable_Expect(GPIO_DOCK_PULL_EN);
    uart_tx_ExpectWithArray(UART_DOCK, "\x30\x04\x00\x04\xCB\x92", 6, 6);
    charger_comms_transmit(WIRE_DEST_BROADCAST, "\x30\x04\x00\x04\xCB\x92", 6);

    TEST_ASSERT_TRUE(charger_comms_is_active());

    for (n=0; n<6; n++)
    {
        charger_comms_periodic();
    }

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_transmit_done();

    TEST_ASSERT_FALSE(charger_comms_is_active());
}

/*
* Case sends status requests. The first is ignored, the second is acknowledged.
*/
void test_ccb_status_req(void)
{
    uint16_t n;

    TEST_ASSERT_FALSE(charger_comms_is_active());

    /*
    * Send a status request.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->COMMS", "\x20\x03\x03\xD0\x95", 5, 5);
    vreg_disable_Expect();
    power_set_run_reason_Expect(POWER_RUN_CHARGER_COMMS);

    charger_comms_periodic();
    charger_comms_periodic();
    gpio_enable_Expect(GPIO_DOCK_PULL_EN);
    uart_tx_ExpectWithArray(UART_DOCK, "\x20\x03\x03\xD0\x95", 5, 5);
    charger_comms_transmit(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5);

    TEST_ASSERT_TRUE(charger_comms_is_active());

    /*
    * No response from earbud.
    */
    for (n=0; n<25; n++)
    {
        charger_comms_periodic();
    }

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_periodic();

    TEST_ASSERT_FALSE(charger_comms_is_active());

    /*
    * Send another status request.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->COMMS", "\x20\x03\x03\xD0\x95", 5, 5);
    vreg_disable_Expect();
    power_set_run_reason_Expect(POWER_RUN_CHARGER_COMMS);

    charger_comms_periodic();
    charger_comms_periodic();
    gpio_enable_Expect(GPIO_DOCK_PULL_EN);
    uart_tx_ExpectWithArray(UART_DOCK, "\x20\x03\x03\xD0\x95", 5, 5);
    charger_comms_transmit(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5);

    TEST_ASSERT_TRUE(charger_comms_is_active());

    charger_comms_periodic();

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_transmit_done();

    /*
    * Earbud responds.
    */
    charger_comms_receive(0x48);
    charger_comms_receive(0x02);
    charger_comms_receive(0x46);

    wire_get_payload_length_ExpectWithArrayAndReturn("\x48\x02\x46\xD7", 4, 2);
    wire_rx_ExpectWithArray(EARBUD_LEFT, "\x48\x02\x46\xD7", 4, 4);
    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_receive(0xD7);

    TEST_ASSERT_FALSE(charger_comms_is_active());
}

/*
* Case receives lots of meaningless data from the earbud.
*/
void test_ccb_rx_too_much(void)
{
    uint16_t n;

    TEST_ASSERT_FALSE(charger_comms_is_active());

    /*
    * Send a status request.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->COMMS", "\x10\x03\x03\x15\x30", 5, 5);
    vreg_disable_Expect();
    power_set_run_reason_Expect(POWER_RUN_CHARGER_COMMS);

    charger_comms_periodic();
    charger_comms_periodic();
    gpio_enable_Expect(GPIO_DOCK_PULL_EN);
    uart_tx_ExpectWithArray(UART_DOCK, "\x10\x03\x03\x15\x30", 5, 5);
    charger_comms_transmit(WIRE_DEST_RIGHT, "\x10\x03\x03\x15\x30", 5);

    TEST_ASSERT_TRUE(charger_comms_is_active());

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_transmit_done();

    /*
    * Earbud responds with nonsense.
    */
    for (n=1; n<WIRE_NO_OF_BYTES; n++)
    {
        charger_comms_receive(0xAB);
    }

    for (n=4; n<=CHARGER_COMMS_MAX_MSG_LEN; n++)
    {
        wire_get_payload_length_ExpectAndReturn("\xAB\xAB", n);
        charger_comms_receive(0xAB);
    }

    for (n=0; n<100; n++)
    {
        charger_comms_receive(0xAB);
    }

    /*
    * Eventually the case gives up expecting a valid response.
    */
    for (n=0; n<100; n++)
    {
        charger_comms_periodic();
    }

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_periodic();

    TEST_ASSERT_FALSE(charger_comms_is_active());

    /*
    * Send another status request.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->COMMS", "\x10\x03\x03\x15\x30", 5, 5);
    vreg_disable_Expect();
    power_set_run_reason_Expect(POWER_RUN_CHARGER_COMMS);

    charger_comms_periodic();
    charger_comms_periodic();
    gpio_enable_Expect(GPIO_DOCK_PULL_EN);
    uart_tx_ExpectWithArray(UART_DOCK, "\x10\x03\x03\x15\x30", 5, 5);
    charger_comms_transmit(WIRE_DEST_RIGHT, "\x10\x03\x03\x15\x30", 5);

    TEST_ASSERT_TRUE(charger_comms_is_active());

    charger_comms_periodic();

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_transmit_done();

    /*
    * Earbud responds.
    */
    charger_comms_receive(0x44);
    charger_comms_receive(0x02);
    charger_comms_receive(0x03);

    wire_get_payload_length_ExpectWithArrayAndReturn("\x44\x02\x03\xBA", 4, 2);
    wire_rx_ExpectWithArray(EARBUD_RIGHT, "\x44\x02\x03\xBA", 4, 4);
    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_receive(0xBA);

    TEST_ASSERT_FALSE(charger_comms_is_active());
}

/*
* Unexpected data received from earbud.
*/
void test_ccb_rx_unexpected(void)
{
    TEST_ASSERT_FALSE(charger_comms_is_active());

    /*
    * Unexpected data from earbud, discarded.
    */
    charger_comms_receive(0xAA);
    TEST_ASSERT_EQUAL(0, cc_rx_buf_ctr);

    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->COMMS", "\x30\x04\x00\x04\xCB\x92", 6, 6);
    vreg_disable_Expect();
    power_set_run_reason_Expect(POWER_RUN_CHARGER_COMMS);

    charger_comms_periodic();
    charger_comms_periodic();
    gpio_enable_Expect(GPIO_DOCK_PULL_EN);
    uart_tx_ExpectWithArray(UART_DOCK, "\x30\x04\x00\x04\xCB\x92", 6, 6);
    charger_comms_transmit(WIRE_DEST_BROADCAST, "\x30\x04\x00\x04\xCB\x92", 6);

    TEST_ASSERT_TRUE(charger_comms_is_active());

    charger_comms_periodic();

    /*
    * Unexpected data from earbud, discarded.
    */
    charger_comms_receive(0xBB);
    TEST_ASSERT_EQUAL(0, cc_rx_buf_ctr);

    vreg_enable_Expect();
    gpio_disable_Expect(GPIO_DOCK_PULL_EN);
    power_clear_run_reason_Expect(POWER_RUN_CHARGER_COMMS);
    charger_comms_transmit_done();

    TEST_ASSERT_FALSE(charger_comms_is_active());
}

#endif /*VARIANT_ST2*/
