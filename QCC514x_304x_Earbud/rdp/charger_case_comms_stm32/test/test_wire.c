/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for wire.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>

#include "unity.h"

#include "wire.c"
#include "crc.c"

#include "mock_cli.h"
#include "mock_charger_comms.h"
#include "mock_uart.h"
#include "mock_timer.h"
#include "mock_ccp.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void cb_rx(uint8_t earbud, uint8_t *data, uint16_t len, bool final_piece);
void cb_ack(uint8_t earbud);
void cb_nack(uint8_t earbud);
void cb_give_up(uint8_t earbud);
void cb_no_response(uint8_t earbud);
void cb_abort(uint8_t earbud);
void cb_broadcast_finished(void);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

uint8_t cb_rx_earbud;
uint8_t *cb_rx_data;
uint8_t cb_rx_len;
bool cb_rx_final_piece;
uint8_t cb_ack_ctr;
uint8_t cb_nack_ctr;
uint8_t cb_abort_ctr;
uint8_t cb_give_up_ctr;
uint8_t cb_no_response_ctr;
uint8_t cb_broadcast_finished_ctr;

const WIRE_USER_CB cb =
{
    cb_rx,
    cb_ack,
    cb_nack,
    cb_give_up,
    cb_no_response,
    cb_abort,
    cb_broadcast_finished
};

const char earbud_letter[NO_OF_EARBUDS] = { 'L', 'R' };

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void cb_rx(uint8_t earbud, uint8_t *data, uint16_t len, bool final_piece)
{
    cb_rx_earbud = earbud;
    cb_rx_data = data;
    cb_rx_len = len;
    cb_rx_final_piece = final_piece;
}

void cb_ack(uint8_t earbud)
{
    cb_ack_ctr++;
}

void cb_nack(uint8_t earbud)
{
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

void setUp(void)
{
    cb_ack_ctr = 0;
    cb_nack_ctr = 0;
    cb_give_up_ctr = 0;
    cb_no_response_ctr = 0;
    cb_abort_ctr = 0;
    cb_broadcast_finished_ctr = 0;

    memset(broadcast_data, 0, sizeof(broadcast_data));
    broadcast_len = 0;
    broadcast_timeout = 0;
    broadcast_count = 0;
    memset(wire_transaction, 0, sizeof(wire_transaction));
    wire_user = NULL;

    wire_init(&cb);
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Send a broadcast message.
*/
void test_wire_broadcast(void)
{
    int n;

    /*
    * Nothing happens.
    */    
    wire_periodic();

    /*
    * Request to send broadcast message (short status).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x00\x01", 2, 2);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x33\x00\x01\x3D", 4, 4);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x04\x00\x01\x9B\x37", 6, 6);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x01", 2));

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Broadcast message sent again.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x33\x00\x01\x3D", 4, 4);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x04\x00\x01\x9B\x37", 6, 6);
#endif
    wire_periodic();

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Try to send broadcast for a final time, but the charger_comms layer is
    * busy.
    */
    charger_comms_is_active_ExpectAndReturn(true);
    wire_periodic();

    /*
    * Broadcast message sent for a final time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x33\x00\x01\x3D", 4, 4);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x04\x00\x01\x9B\x37", 6, 6);
#endif
    wire_periodic();

    /*
    * Check that we sent a notification that the broadcast was finished.
    */
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
    TEST_ASSERT_EQUAL(0, broadcast_len);

    /*
    * No more messages are sent.
    */
    for (n=0; n<100; n++)
    {
        wire_periodic();
    }
}

/*
* Successful exchange of status.
*/
void test_wire_status_request(void)
{
    uint8_t n;

    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBB", 2, 2);
    wire_rx(EARBUD_LEFT, "\x41\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7E", 4, 4);
    wire_rx(EARBUD_LEFT, "\x40\x02\xCF\x7E", 4);
#endif

    /*
    * ACK passed up to CCP.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(1, cb_ack_ctr);

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Poll from CCP.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\xE1\xE1", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\xE0\x02\xD2\x00", 4, 4);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, NULL, 0));

    /*
    * Receive status response from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x84\x01\x00\x21\x94", 5, 5);
    wire_rx(EARBUD_LEFT, "\x84\x01\x00\x21\x94", 5);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x80\x05\x01\x00\x21\x73\x15", 7, 7);
    wire_rx(EARBUD_LEFT, "\x80\x05\x01\x00\x21\x73\x15", 7);
#endif

    /*
    * ACK the status response and pass its contents to CCP.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x21\x8D", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x02\xC4\x54", 4, 4);
#endif
    wire_periodic();

    TEST_ASSERT_EQUAL(EARBUD_LEFT, cb_rx_earbud);
    TEST_ASSERT_EQUAL_MEMORY("\x01\x00\x21", cb_rx_data, 3);
    TEST_ASSERT_EQUAL(3, cb_rx_len);
    TEST_ASSERT_TRUE(cb_rx_final_piece);

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Broadcast status.
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x00\x00\xE4\x21\x7F", 5, 5);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x7F\x0D", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x7F\x59\xA3", 9, 9);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x00\xE4\x21\x7F", 5));

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Broadcast message sent again.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x7F\x0D", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x7F\x59\xA3", 9, 9);
#endif
    wire_periodic();

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Broadcast message sent for a final time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x7F\x0D", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x7F\x59\xA3", 9, 9);
#endif
    wire_periodic();

    /*
    * Check that we sent a notification that the broadcast was finished.
    */
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
    TEST_ASSERT_EQUAL(0, broadcast_len);

    /*
    * Request to send message (status request), this time for the right earbud.
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_RIGHT, "\x12\x03\x18", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_RIGHT, "\x10\x03\x03\x15\x30", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_RIGHT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive ACK from right earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBB", 2, 2);
    wire_rx(EARBUD_RIGHT, "\x41\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7E", 4, 4);
    wire_rx(EARBUD_RIGHT, "\x40\x02\xCF\x7E", 4);
#endif

    /*
    * ACK passed up to CCP.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(2, cb_ack_ctr);

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Poll from CCP.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_RIGHT, "\xD1\xFA", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_RIGHT, "\xD0\x02\xD7\x95", 4, 4);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_RIGHT, NULL, 0));

    /*
    * Receive status response from right earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x84\x01\x00\x2B\xCB", 5, 5);
    wire_rx(EARBUD_RIGHT, "\x84\x01\x00\x2B\xCB", 5);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x80\x05\x01\x00\x2B\xD2\x5F", 7, 7);
    wire_rx(EARBUD_RIGHT, "\x80\x05\x01\x00\x2B\xD2\x5F", 7);
#endif

    /*
    * ACK the status response and pass its contents to CCP.
    */
    charger_comms_is_active_ExpectAndReturn(false);

#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_RIGHT, "\x11\x96", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_RIGHT, "\x10\x02\xC1\xC1", 4, 4);
#endif

    wire_periodic();

    TEST_ASSERT_EQUAL(EARBUD_RIGHT, cb_rx_earbud);
    TEST_ASSERT_EQUAL_MEMORY("\x01\x00\x2B", cb_rx_data, 3);
    TEST_ASSERT_EQUAL(3, cb_rx_len);
    TEST_ASSERT_TRUE(cb_rx_final_piece);

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Broadcast status.
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x00\x00\xE4\x21\x2B", 5, 5);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x2B\x06", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x2B\x43\xD2", 9, 9);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x00\xE4\x21\x2B", 5));

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Broadcast message sent again.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x2B\x06", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x2B\x43\xD2", 9, 9);
#endif
    wire_periodic();

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);

    /*
    * Broadcast message sent for a final time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x2B\x06", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x2B\x43\xD2", 9, 9);
#endif
    wire_periodic();

    /*
    * Check that we sent a notification that the broadcast was finished.
    */
    TEST_ASSERT_EQUAL(2, cb_broadcast_finished_ctr);
    TEST_ASSERT_EQUAL(0, broadcast_len);
}

/*
* Earbud fails to respond to a status request altogether. We assume that it is
* therefore not present, and give up without retrying.
*/
void test_wire_no_response(void)
{
    uint8_t n;

    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_NO_RESPONSE_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Not given up yet.
    */
    TEST_ASSERT_EQUAL(0, cb_no_response_ctr);

    /*
    * Now we give up.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(1, cb_no_response_ctr);
}

#ifdef SCHEME_A
/*
* Attempt to send a message that is too big.
*/
void test_wire_tx_too_big(void)
{
    /*
    * Request to send broadcast message of 15 characters, which is one too
    * many.
    */
    TEST_ASSERT_FALSE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x00\xE4\x21\x7F\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF", 15));

    /*
    * Nothing happens.
    */
    wire_periodic();

    /*
    * Request to send broadcast message of 14 characters, which is OK.
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x00\x00\xE4\x21\x7F\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE", 14, 14);
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x3F\x00\x00\xE4\x21\x7F\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\x5D", 16, 16);
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x00\xE4\x21\x7F\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE", 14));
}
#endif

/*
* Receiving corrupt message (checksum failure).
*/
void test_wire_receive_corrupt_message(void)
{
    uint8_t n;

    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive corrupted ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBA", 2, 2);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x41\xBA", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7F", 4, 4);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x40\x02\xCF\x7F", 4);
#endif
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Re-send the status request.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    wire_periodic();

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive ACK from left earbud, this time not corrupted.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBB", 2, 2);
    wire_rx(EARBUD_LEFT, "\x41\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7E", 4, 4);
    wire_rx(EARBUD_LEFT, "\x40\x02\xCF\x7E", 4);
#endif

    /*
    * ACK passed up to CCP.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(1, cb_ack_ctr);
}

/*
* Keep receiving corrupt messages (checksum failure) until we give up.
*/
void test_wire_keep_receiving_corrupt_messages(void)
{
    uint8_t n;

    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive corrupted ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\xBB", 2, 2);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x40\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x00\xCF\x7E", 4, 4);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x40\x00\xCF\x7E", 4);
#endif

    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Send the status request for a second time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    wire_periodic();

    /*
    * Receive corrupted ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xFB", 2, 2);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x41\xFB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xDF\x7E", 4, 4);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x40\x02\xDF\x7E", 4);
#endif
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Attempt to send the status request for a third time, but charger_comms
    * is busy.
    */
    charger_comms_is_active_ExpectAndReturn(true);
    wire_periodic();

    /*
    * Send the status request for a third time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    wire_periodic();

    /*
    * Receive corrupted ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x61\xBB", 2, 2);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x61\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x60\x02\xCF\x7E", 4, 4);
    cli_tx_Expect(CLI_BROADCAST, true, "Invalid checksum");
    wire_rx(EARBUD_LEFT, "\x60\x02\xCF\x7E", 4);
#endif
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);
    TEST_ASSERT_EQUAL(0, cb_give_up_ctr);

    /*
    * Now we give up.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(1, cb_give_up_ctr);
}

/*
* Receive NACK from earbud.
*/
void test_wire_receive_nack(void)
{
    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive NACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x01\x0E", 2, 2);
    cli_tx_Expect(CLI_BROADCAST, true, "NACK!");
    wire_rx(EARBUD_LEFT, "\x01\x0E", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x00\x02\xC2\xB2", 4, 4);
    cli_tx_Expect(CLI_BROADCAST, true, "NACK!");
    wire_rx(EARBUD_LEFT, "\x00\x02\xC2\xB2", 4);
#endif
    TEST_ASSERT_EQUAL(0, cb_ack_ctr);

    /*
    * Re-send the status request.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x62\x03\x3B", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x60\x03\x03\xCD\x38", 5, 5);
#endif
    wire_periodic();

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBB", 2, 2);
    wire_rx(EARBUD_LEFT, "\x41\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7E", 4, 4);
    wire_rx(EARBUD_LEFT, "\x40\x02\xCF\x7E", 4);
#endif

    /*
    * ACK passed up to CCP.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(1, cb_ack_ctr);
}

/*
* Repeated NACKs.
*/
void test_wire_repeated_nack(void)
{
    uint8_t n;
    uint8_t i;

    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive NACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x01\x0E", 2, 2);
    wire_rx(EARBUD_LEFT, "\x01\x0E", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x00\x02\xC2\xB2", 4, 4);
    wire_rx(EARBUD_LEFT, "\x00\x02\xC2\xB2", 4);
#endif

    /*
    * Retransmit the status request.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "NACK!");
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x62\x03\x3B", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x60\x03\x03\xCD\x38", 5, 5);
#endif
    wire_periodic();
    TEST_ASSERT_EQUAL(1, wire_transaction[EARBUD_LEFT].nack_count);

    /*
    * Receive second NACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x01\x0E", 2, 2);
    wire_rx(EARBUD_LEFT, "\x01\x0E", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x00\x02\xC2\xB2", 4, 4);
    wire_rx(EARBUD_LEFT, "\x00\x02\xC2\xB2", 4);
#endif

    /*
    * Retransmit the status request.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "NACK!");
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x62\x03\x3B", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x60\x03\x03\xCD\x38", 5, 5);
#endif
    wire_periodic();
    TEST_ASSERT_EQUAL(2, wire_transaction[EARBUD_LEFT].nack_count);

    /*
    * Receive third NACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x01\x0E", 2, 2);
    wire_rx(EARBUD_LEFT, "\x01\x0E", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x00\x02\xC2\xB2", 4, 4);
    wire_rx(EARBUD_LEFT, "\x00\x02\xC2\xB2", 4);
#endif

    /*
    * Three NACKs received, so setup empty broadcast message.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "NACK!");
    wire_periodic();
    TEST_ASSERT_EQUAL(WIRE_NO_OF_BYTES, broadcast_len);

    /*
    * Send the empty broadcast message.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x31\x15", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x02\xC7\x27", 4, 4);
#endif
    wire_periodic();
    TEST_ASSERT_EQUAL(1, broadcast_count);

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Send the empty broadcast message again.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x31\x15", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x02\xC7\x27", 4, 4);
#endif
    wire_periodic();
    TEST_ASSERT_EQUAL(2, broadcast_count);

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Send the empty broadcast message for a third time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x31\x15", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x02\xC7\x27", 4, 4);
#endif
    wire_periodic();
    TEST_ASSERT_EQUAL(3, broadcast_count);

    /*
    * Retransmit the status message.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    wire_periodic();

    /*
    * This time we receive ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBB", 2, 2);
    wire_rx(EARBUD_LEFT, "\x41\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7E", 4, 4);
    wire_rx(EARBUD_LEFT, "\x40\x02\xCF\x7E", 4);
#endif

    /*
    * ACK passed up to CCP.
    */
    wire_periodic();
    TEST_ASSERT_EQUAL(1, cb_ack_ctr);

    /*
    * Poll from CCP.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\xE1\xE1", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\xE0\x02\xD2\x00", 4, 4);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, NULL, 0));

    /*
    * Receive status response from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x84\x01\x00\x21\x94", 5, 5);
    wire_rx(EARBUD_LEFT, "\x84\x01\x00\x21\x94", 5);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x80\x05\x01\x00\x21\x73\x15", 7, 7);
    wire_rx(EARBUD_LEFT, "\x80\x05\x01\x00\x21\x73\x15", 7);
#endif

    /*
    * ACK the status response and pass its contents to CCP.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x21\x8D", 2, 2);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x02\xC4\x54", 4, 4);
#endif
    wire_periodic();

    TEST_ASSERT_EQUAL(EARBUD_LEFT, cb_rx_earbud);
    TEST_ASSERT_EQUAL_MEMORY("\x01\x00\x21", cb_rx_data, 3);
    TEST_ASSERT_EQUAL(3, cb_rx_len);
    TEST_ASSERT_TRUE(cb_rx_final_piece);

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Broadcast status.
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x00\x00\xE4\x21\x7F", 5, 5);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x36\x00\x00\xE4\x21\x7F\x0D", 7, 7);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x07\x00\x00\xE4\x21\x7F\x59\xA3", 9, 9);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x00\xE4\x21\x7F", 5));
}

/*
* Broadcast message interrupts status message exchange.
*/
void test_wire_broadcast_interrupting(void)
{
    uint8_t n;

    /*
    * Request to send message (status request).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x03", 1, 1);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x22\x03\x0F", 3, 3);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_LEFT, "\x20\x03\x03\xD0\x95", 5, 5);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Do nothing.
    */
    wire_periodic();

    /*
    * Receive ACK from left earbud.
    */
#ifdef SCHEME_A
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x41\xBB", 2, 2);
    wire_rx(EARBUD_LEFT, "\x41\xBB", 2);
#else
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "COMMS->WIRE", "\x40\x02\xCF\x7E", 4, 4);
    wire_rx(EARBUD_LEFT, "\x40\x02\xCF\x7E", 4);
#endif
    TEST_ASSERT_EQUAL(0, cb_abort_ctr);

    /*
    * Request to send broadcast message (short status).
    */
    charger_comms_is_active_ExpectAndReturn(false);
    cli_tx_hex_ExpectWithArray(CLI_BROADCAST, "CCP->WIRE", "\x00\x02", 2, 2);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x33\x00\x02\xFE", 4, 4);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x04\x00\x02\xAB\x54", 6, 6);
#endif
    TEST_ASSERT_TRUE(wire_tx(WIRE_DEST_BROADCAST, "\x00\x02", 2));

    /*
    * Check that the status message sequence was aborted.
    */
    TEST_ASSERT_EQUAL(1, cb_abort_ctr);

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    /*
    * Attempt to send status request rejected, because we are still
    * broadcasting.
    */
    TEST_ASSERT_FALSE(wire_tx(WIRE_DEST_LEFT, "\x03", 1));

    /*
    * Broadcast message sent again.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x33\x00\x02\xFE", 4, 4);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x04\x00\x02\xAB\x54", 6, 6);
#endif
    wire_periodic();

    /*
    * Do nothing for a bit.
    */
    for (n=0; n<WIRE_BROADCAST_TIMEOUT; n++)
    {
        wire_periodic();
    }

    TEST_ASSERT_EQUAL(0, cb_broadcast_finished_ctr);

    /*
    * Try to send broadcast for a final time, but the charger_comms layer is
    * busy.
    */
    charger_comms_is_active_ExpectAndReturn(true);
    wire_periodic();

    /*
    * Broadcast message sent for a final time.
    */
    charger_comms_is_active_ExpectAndReturn(false);
#ifdef SCHEME_A
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x33\x00\x02\xFE", 4, 4);
#else
    charger_comms_transmit_ExpectWithArray(WIRE_DEST_BROADCAST, "\x30\x04\x00\x02\xAB\x54", 6, 6);
#endif
    wire_periodic();

    /*
    * Check that we sent a notification that the broadcast was finished.
    */
    TEST_ASSERT_EQUAL(1, cb_broadcast_finished_ctr);
    TEST_ASSERT_EQUAL(0, broadcast_len);

    /*
    * No more messages are sent.
    */
    for (n=0; n<100; n++)
    {
        wire_periodic();
    }
}
