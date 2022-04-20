/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for ccp.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>

#include "unity.h"

#include "ccp.c"

#include "mock_wire.h"
#include "mock_case.h"
#include "mock_cli.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void cb_rx_earbud_status(
    uint8_t earbud,
    uint8_t pp,
    uint8_t chg_rate,
    uint8_t battery,
    uint8_t charging);
void cb_rx_bt_address(
        uint8_t earbud, uint16_t nap, uint8_t uap, uint32_t lap);
void cb_ack(uint8_t earbud);
void cb_nack(uint8_t earbud);
void cb_give_up(uint8_t earbud);
void cb_no_response(uint8_t earbud);
void cb_abort(uint8_t earbud);
void cb_broadcast_finished(void);
void cb_loopback(uint8_t earbud, uint8_t *data, uint16_t len);
void cb_shipping(uint8_t earbud, uint8_t sm);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

uint8_t cb_rx_status_earbud;
uint8_t cb_rx_status_pp;
uint8_t cb_rx_status_chg_rate;
uint8_t cb_rx_status_battery;
uint8_t cb_rx_status_charging;
uint16_t cb_rx_status_nap;
uint8_t cb_rx_status_uap;
uint32_t cb_rx_status_lap;
uint8_t cb_ack_earbud;
uint8_t cb_nack_earbud;
uint8_t cb_ack_ctr;
uint8_t cb_nack_ctr;
uint8_t cb_abort_ctr;
uint8_t cb_give_up_ctr;
uint8_t cb_no_response_ctr;
uint8_t cb_broadcast_finished_ctr;
uint8_t cb_loopback_ctr;
uint8_t cb_shipping_ctr;
uint8_t cb_rx_sm;

const CCP_USER_CB cb =
{
    cb_rx_earbud_status,
    cb_rx_bt_address,
    cb_ack,
    cb_nack,
    cb_give_up,
    cb_no_response,
    cb_abort,
    cb_broadcast_finished,
    cb_loopback,
    cb_shipping
};

const uint8_t wire_dest[NO_OF_EARBUDS] = { WIRE_DEST_LEFT, WIRE_DEST_RIGHT };

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void cb_rx_earbud_status(
    uint8_t earbud,
    uint8_t pp,
    uint8_t chg_rate,
    uint8_t battery,
    uint8_t charging)
{
    cb_rx_status_earbud = earbud;
    cb_rx_status_pp = pp;
    cb_rx_status_chg_rate = chg_rate;
    cb_rx_status_battery = battery;
    cb_rx_status_charging = charging;
}

void cb_rx_bt_address(
    uint8_t earbud, uint16_t nap, uint8_t uap, uint32_t lap)
{
    cb_rx_status_earbud = earbud;
    cb_rx_status_nap = nap;
    cb_rx_status_uap = uap;
    cb_rx_status_lap = lap;
}

void cb_ack(uint8_t earbud)
{
    cb_ack_earbud = earbud;
    cb_ack_ctr++;
}

void cb_nack(uint8_t earbud)
{
    cb_nack_earbud = earbud;
    cb_nack_ctr++;
}

void cb_give_up(uint8_t earbud)
{
    cb_give_up_ctr++;
}

void cb_no_response(uint8_t earbud)
{
    cb_no_response_ctr++;
}

void cb_abort(uint8_t earbud)
{
    cb_abort_ctr++;
}

void cb_broadcast_finished(void)
{
    cb_broadcast_finished_ctr++;
}

void cb_loopback(uint8_t earbud, uint8_t *data, uint16_t len)
{
    cb_loopback_ctr++;
}

void cb_shipping(uint8_t earbud, uint8_t sm)
{
    cb_rx_sm = sm;
    cb_shipping_ctr++;
}

void setUp(void)
{
    cb_ack_ctr = 0;
    cb_nack_ctr = 0;
    cb_give_up_ctr = 0;
    cb_no_response_ctr = 0;
    cb_abort_ctr = 0;
    cb_broadcast_finished_ctr = 0;
    cb_loopback_ctr = 0;
    cb_shipping_ctr = 0;
    cb_rx_sm = 0;
    cb_rx_status_nap = 0;
    cb_rx_status_uap = 0;
    cb_rx_status_lap = 0;

    ccp_user = NULL;
    memset(ccp_transaction, 0, sizeof(ccp_transaction));
    
    wire_init_Ignore();
    ccp_init(&cb);    
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Broadcast short status messasge.
*/
void test_ccp_broadcast(void)
{
    /*
    * Nothing happens.
    */
    ccp_periodic();

    /*
    * Open lid, so short status message gets broadcast.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_BROADCAST, "\x00\x05", 2, 2, true);
    TEST_ASSERT_TRUE(ccp_tx_short_status(true, false, true));

    /*
    * Nothing happens.
    */
    ccp_periodic();
    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Receive indication from wire that the broadcast is finished, check that
    * we notified the case module.
    */
    ccp_broadcast_finished();
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
}

/*
* Successful exchange of status.
*/
void test_ccp_status_request(void)
{
    uint8_t n;

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_LEFT));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x01\x00\x21", 3, 3);
    ccp_rx(EARBUD_LEFT, "\x01\x00\x21", 3, true);
    TEST_ASSERT_EQUAL(EARBUD_LEFT, cb_rx_status_earbud);
    TEST_ASSERT_EQUAL(0, cb_rx_status_pp);
    TEST_ASSERT_EQUAL_HEX8(0x21, cb_rx_status_battery);
    TEST_ASSERT_EQUAL(0, cb_rx_status_charging);

    /*
    * Send status request to right earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_RIGHT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_RIGHT));

    /*
    * Receive ACK from right earbud. We don't pass the ACK to case because
    * we want a proper response.
    */
    ccp_ack(EARBUD_RIGHT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_RIGHT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x01\x00\x2B", 3, 3);
    ccp_rx(EARBUD_RIGHT, "\x01\x00\x2B", 3, true);
    TEST_ASSERT_EQUAL(EARBUD_RIGHT, cb_rx_status_earbud);
    TEST_ASSERT_EQUAL(0, cb_rx_status_pp);
    TEST_ASSERT_EQUAL_HEX8(0x2B, cb_rx_status_battery);
    TEST_ASSERT_EQUAL(0, cb_rx_status_charging);

    /*
    * Status message gets broadcast.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_BROADCAST, "\x00\x01\xE4\x21\x2B", 5, 5, true);
    TEST_ASSERT_TRUE(ccp_tx_status(true, false, true, false, 0x64, 0x21, 0x2B, false, false));

    /*
    * Nothing happens.
    */
    ccp_periodic();
    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Receive indication from wire that the broadcast is finished, check that
    * we notified the case module.
    */
    ccp_broadcast_finished();
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
}

/*
* Successful bluetooth address read.
*/
void test_ccp_bt_address(void)
{
    uint8_t n;

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03\x00", 2, 2, true);
    TEST_ASSERT_TRUE(ccp_tx_xstatus_request(EARBUD_LEFT, CCP_IT_BT_ADDRESS));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x01\x80\x21\xEB\x00\x5B\x02\x00", 8, 8);
    ccp_rx(EARBUD_LEFT, "\x01\x80\x21\xEB\x00\x5B\x02\x00", 8, true);
    TEST_ASSERT_EQUAL_HEX16(0x0002, cb_rx_status_nap);
    TEST_ASSERT_EQUAL_HEX8(0x5B, cb_rx_status_uap);
    TEST_ASSERT_EQUAL_HEX32(0x00EB21, cb_rx_status_lap);
}

/*
* Factory reset.
*/
void test_ccp_factory_reset(void)
{
    uint8_t n;

    /*
    * Send reset command to right earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_RIGHT, "\x02\x01", 2, 2, true);
    TEST_ASSERT_TRUE(ccp_tx_reset(EARBUD_RIGHT, true));

    /*
    * Receive ACK from right earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_RIGHT);
    TEST_ASSERT_EQUAL(1, cb_ack_ctr);

    /*
    * Send status request to right earbud, this is our poll to see if the
    * reset has been completed.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_RIGHT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_RIGHT));

    /*
    * Receive ACK from right earbud. We don't pass the ACK to case because
    * we want a proper response.
    */
    ccp_ack(EARBUD_RIGHT);
    TEST_ASSERT_EQUAL(1, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_RIGHT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x01\x00\x2B", 3, 3);
    ccp_rx(EARBUD_RIGHT, "\x01\x00\x2B", 3, true);
    TEST_ASSERT_EQUAL(EARBUD_RIGHT, cb_rx_status_earbud);
    TEST_ASSERT_EQUAL(0, cb_rx_status_pp);
    TEST_ASSERT_EQUAL_HEX8(0x2B, cb_rx_status_battery);
    TEST_ASSERT_EQUAL(0, cb_rx_status_charging);

    /*
    * Status message gets broadcast.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_BROADCAST, "\x00\x01\xE4\x21\x2B", 5, 5, true);
    TEST_ASSERT_TRUE(ccp_tx_status(true, false, true, false, 0x64, 0x21, 0x2B, false, false));

    /*
    * Nothing happens.
    */
    ccp_periodic();
    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Receive indication from wire that the broadcast is finished, check that
    * we notified the case module.
    */
    ccp_broadcast_finished();
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
}

/*
* Status request is acknowledged by earbud, but the actual answer is never
* delivered (no response to the polls).
*/
void test_ccp_status_request_no_answer(void)
{
    uint8_t n;

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_LEFT));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Second poll fails.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, false);
    ccp_periodic();

    /*
    * Second poll.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Third poll.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Fourth poll.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Fifth and final poll.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    TEST_ASSERT_EQUAL(0, cb_give_up_ctr);

    /*
    * Give up.
    */
    ccp_periodic();
    TEST_ASSERT_EQUAL(1, cb_give_up_ctr);
}

/*
* Broadcast message interrupts status message exchange, which is consequently
* aborted.
*/
void test_ccp_broadcast_interrupting(void)
{
    uint8_t n;

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_LEFT));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Open lid, so short status message gets broadcast.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_BROADCAST, "\x00\x01", 2, 2, true);
    TEST_ASSERT_TRUE(ccp_tx_short_status(true, false, false));

    /*
    * Abort notification from wire is forwarded to case.
    */
    TEST_ASSERT_EQUAL(0, cb_abort_ctr);
    ccp_abort(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(1, cb_abort_ctr);

    /*
    * Nothing happens.
    */
    ccp_periodic();
    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Receive indication from wire that the broadcast is finished, check that
    * we notified the case module.
    */
    ccp_broadcast_finished();
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
}

/*
* Reject attempts to send a message if we are already busy.
*/
void test_ccp_busy_reject(void)
{
    uint8_t n;

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_LEFT));

    /*
    * Attempted factory reset is rejected because we are busy with the status
    * request.
    */
    TEST_ASSERT_FALSE(ccp_tx_reset(EARBUD_LEFT, true));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x01\x00\x21", 3, 3);
    ccp_rx(EARBUD_LEFT, "\x01\x00\x21", 3, true);
    TEST_ASSERT_EQUAL(EARBUD_LEFT, cb_rx_status_earbud);
    TEST_ASSERT_EQUAL(0, cb_rx_status_pp);
    TEST_ASSERT_EQUAL_HEX8(0x21, cb_rx_status_battery);
    TEST_ASSERT_EQUAL(0, cb_rx_status_charging);
}

/*
* Unexpected responses from the earbud.
*/
void test_ccp_unknown_responses(void)
{
    uint8_t n;

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_LEFT));

    /*
    * Response has unknown channel.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x71\x00\x21", 3, 3);
    ccp_rx(EARBUD_LEFT, "\x71\x00\x21", 3, true);

    /*
    * Send status request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x03", 1, 1, true);
    TEST_ASSERT_TRUE(ccp_tx_status_request(EARBUD_LEFT));

    /*
    * Response has unknown message ID.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x0F\x00\x21", 3, 3);
    ccp_rx(EARBUD_LEFT, "\x0F\x00\x21", 3, true);
}

/*
* Successful loopback.
*/
void test_ccp_loopback(void)
{
    uint8_t n;

    /*
    * Send loopback message to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x04\x12\x34\x56\x78", 5, 5, true);
    TEST_ASSERT_TRUE(ccp_tx_loopback(EARBUD_LEFT, "\x12\x34\x56\x78", 4));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x04\x12\x34\x56\x78", 5, 5);
    ccp_rx(EARBUD_LEFT, "\x04\x12\x34\x56\x78", 5, true);
    TEST_ASSERT_EQUAL(1, cb_loopback_ctr);
}

/*
* Shipping mode request.
*/
void test_ccp_shipping_mode(void)
{
    uint8_t n;

    /*
    * Send shipping mode request to left earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_LEFT, "\x05\x02", 2, 2, true);
    TEST_ASSERT_TRUE(ccp_tx_shipping_mode(EARBUD_LEFT));

    /*
    * Receive ACK from left earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_LEFT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_LEFT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x06\x02\x01", 3, 3);
    ccp_rx(EARBUD_LEFT, "\x06\x02\x01", 3, true);
    TEST_ASSERT_EQUAL(1, cb_rx_sm);
    TEST_ASSERT_EQUAL(1, cb_shipping_ctr);

    /*
    * Send shipping mode request to right earbud.
    */
    wire_tx_ExpectWithArrayAndReturn(WIRE_DEST_RIGHT, "\x05\x02", 2, 2, true);
    TEST_ASSERT_TRUE(ccp_tx_shipping_mode(EARBUD_RIGHT));

    /*
    * Receive ACK from right earbud. We don't pass the ACK to case because
    * we want a response.
    */
    ccp_ack(EARBUD_RIGHT);
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Nothing happens for a bit.
    */
    for (n=1; n<CCP_POLL_TIMEOUT; n++)
    {
        ccp_periodic();
    }

    /*
    * Poll the earbud for a response.
    */
    wire_tx_ExpectAndReturn(WIRE_DEST_RIGHT, NULL, 0, true);
    ccp_periodic();

    /*
    * Earbud responds.
    */
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "WIRE->CCP", "\x06\x02\x00", 3, 3);
    ccp_rx(EARBUD_RIGHT, "\x06\x02\x00", 3, true);
    TEST_ASSERT_EQUAL(0, cb_rx_sm);
    TEST_ASSERT_EQUAL(2, cb_shipping_ctr);
}
