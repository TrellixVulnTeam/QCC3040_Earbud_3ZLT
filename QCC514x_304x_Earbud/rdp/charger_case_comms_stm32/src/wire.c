/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Wire protocol / Charger Comms protocol
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "main.h"
#include "cli.h"
#include "charger_comms.h"
#include "uart.h"
#include "timer.h"
#include "ccp.h"
#include "crc.h"
#include "bitmap.h"
#include "wire.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define WIRE_MAX_PACKET_SIZE CHARGER_COMMS_MAX_MSG_LEN

/*
* WIRE_NO_RESPONSE_TIMEOUT: Time in ticks for which we will wait for a
* response.
*/
#ifdef SCHEME_A
#define WIRE_NO_RESPONSE_TIMEOUT 200
#else
#define WIRE_NO_RESPONSE_TIMEOUT 60
#endif

/*
* WIRE_MAX_RETRIES: Number of retries that we will make before giving up.
*/
#define WIRE_MAX_RETRIES 2

/*
* WIRE_BROADCAST_SENDS: Number of times we send each broadcast message.
*/
#define WIRE_BROADCAST_SENDS 3

/*
* WIRE_BROADCAST_TIMEOUT: Time in ticks between successive broadcast message
* transmissions.
*/
#define WIRE_BROADCAST_TIMEOUT 10

/*
* Charger Comms Header field definitions.
*/
#define WIRE_HDR_MASK_SN     0x80
#define WIRE_HDR_BIT_SN      7
#define WIRE_HDR_MASK_NESN   0x40
#define WIRE_HDR_BIT_NESN    6
#define WIRE_HDR_MASK_DEST   0x30
#define WIRE_HDR_BIT_DEST    4

#ifdef SCHEME_A
#define WIRE_HDR_MASK_LENGTH 0x0F
#define WIRE_HDR_BIT_LENGTH  0
#else
#define WIRE_HDR_MASK_SRC    0x0C
#define WIRE_HDR_BIT_SRC     2
#define WIRE_HDR_MASK_LENGTH 0x03
#define WIRE_HDR_BIT_LENGTH  0
#endif

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    uint8_t tx_data[WIRE_MAX_PACKET_SIZE];
    uint16_t tx_len;
    uint8_t rx_data[WIRE_MAX_PACKET_SIZE];
    uint16_t rx_len;
    bool retry;
    uint8_t no_response_timeout;
    uint8_t retry_count;
    uint8_t nack_count;
    uint8_t sn;
    uint8_t nesn;
    bool need_answer;
}
WIRE_TRANSACTION;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint8_t broadcast_data[WIRE_MAX_PACKET_SIZE];
static uint16_t broadcast_len;
static uint8_t broadcast_timeout;
static uint8_t broadcast_count;
static WIRE_TRANSACTION wire_transaction[NO_OF_EARBUDS] = {0};
static const WIRE_USER_CB *wire_user;

const uint8_t wire_dest[NO_OF_EARBUDS] = { WIRE_DEST_LEFT, WIRE_DEST_RIGHT };

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef SCHEME_A
uint8_t wire_checksum(uint8_t *data, uint16_t len)
{
    return crc_calculate_crc8(data, len);
}
#else
uint16_t wire_checksum(uint8_t *data, uint16_t len)
{
    return crc_calculate_crc16(data, len);
}
#endif

uint16_t wire_get_payload_length(uint8_t *data)
{
#ifdef SCHEME_A
    return BITMAP_GET(WIRE_HDR, LENGTH, data[0]);
#else
    return (BITMAP_GET(WIRE_HDR, LENGTH, data[0]) << 8) + data[1];
#endif
}

#ifdef SCHEME_B
uint16_t wire_get_packet_src(uint8_t *data)
{
    return BITMAP_GET(WIRE_HDR, SRC, data[0]);
}
#endif

void wire_append_checksum(uint8_t *data, uint16_t len)
{
#ifdef SCHEME_A
    data[len] = wire_checksum(data, len);
#else
    uint16_t checksum = wire_checksum(data, len);

    data[len] = (uint8_t)((checksum >> 8) & 0xFF);
    data[len+1] = (uint8_t)(checksum & 0xFF);
#endif
}

void wire_tx_stored_message(uint8_t earbud)
{
    WIRE_TRANSACTION *wt = &wire_transaction[earbud];

    /*
    * Construct the ChargerComms header and add the checksum immediately
    * before sending.
    */
#ifdef SCHEME_A
    wt->tx_data[0] =
        BITMAP_SET(WIRE_HDR, SN, wt->sn) |
        BITMAP_SET(WIRE_HDR, NESN, wt->nesn) |
        BITMAP_SET(WIRE_HDR, LENGTH, (wt->tx_len - WIRE_HEADER_BYTES)) |
        BITMAP_SET(WIRE_HDR, DEST, wire_dest[earbud]);
#else
    wt->tx_data[0] =
        BITMAP_SET(WIRE_HDR, SN, wt->sn) |
        BITMAP_SET(WIRE_HDR, NESN, wt->nesn) |
        BITMAP_SET(WIRE_HDR, LENGTH, (((wt->tx_len - WIRE_HEADER_BYTES) >> 8) & 0x03)) |
        BITMAP_SET(WIRE_HDR, DEST, wire_dest[earbud]);

    wt->tx_data[1] = (uint8_t)((wt->tx_len - WIRE_HEADER_BYTES) & 0xFF);
#endif

    wire_append_checksum(wt->tx_data, wt->tx_len - WIRE_CRC_BYTES);

    charger_comms_transmit(wire_dest[earbud], wt->tx_data, wt->tx_len);
}

static void wire_setup_broadcast(
    uint8_t *data,
    uint16_t len,
    uint8_t timeout)
{
    broadcast_len = len + WIRE_NO_OF_BYTES;

#ifdef SCHEME_A
    broadcast_data[0] =
        BITMAP_SET(WIRE_HDR, DEST, WIRE_DEST_BROADCAST) |
        BITMAP_SET(WIRE_HDR, LENGTH, len + WIRE_CRC_BYTES);
#else
    broadcast_data[0] = BITMAP_SET(WIRE_HDR, DEST, WIRE_DEST_BROADCAST);
    broadcast_data[1] = len + WIRE_CRC_BYTES;
#endif

    if (data && len)
    {
        memcpy(&broadcast_data[WIRE_HEADER_BYTES], data, len);
    }

    wire_append_checksum(broadcast_data, len + WIRE_HEADER_BYTES);

    broadcast_timeout = timeout;
    broadcast_count = 0;
}

static void wire_send_broadcast(void)
{
    broadcast_count++;
    charger_comms_transmit(WIRE_DEST_BROADCAST, broadcast_data, broadcast_len);
}

bool wire_tx(
    WIRE_DESTINATION dest,
    uint8_t *data,
    uint16_t len)
{
    bool ret = false;

    /*
    * Currently we only allow data that will fit in a single packet, going
    * forward that will have to change.
    */
    if (len <= (WIRE_MAX_PACKET_SIZE - WIRE_NO_OF_BYTES))
    {
        if (!broadcast_len && !charger_comms_is_active())
        {
            if (dest==WIRE_DEST_BROADCAST)
            {
                /*
                * Broadcast message should cause reset of SN, NESN as well
                * as discarding everything currently in progress.
                */

                if (wire_transaction[EARBUD_LEFT].tx_len)
                {
                    wire_user->abort(EARBUD_LEFT);
                }

                if (wire_transaction[EARBUD_RIGHT].tx_len)
                {
                    wire_user->abort(EARBUD_RIGHT);
                }

                cli_tx_hex(CLI_BROADCAST, "CCP->WIRE", data, len);

                memset(wire_transaction, 0, sizeof(wire_transaction));
                wire_setup_broadcast(data, len, WIRE_BROADCAST_TIMEOUT);
                wire_send_broadcast();
                ret = true;
            }
            else
            {
                uint8_t earbud =
                    (dest==WIRE_DEST_LEFT) ? EARBUD_LEFT:EARBUD_RIGHT;
                WIRE_TRANSACTION *wt = &wire_transaction[earbud];
                WIRE_TRANSACTION other_wt = wire_transaction[earbud == EARBUD_LEFT ? EARBUD_RIGHT : EARBUD_LEFT];

                if (!wt->tx_len && !other_wt.tx_len)
                {
                    if (len)
                    {
                        cli_tx_hex(CLI_BROADCAST, "CCP->WIRE", data, len);
                    }

                    memcpy(&wt->tx_data[WIRE_HEADER_BYTES], data, len);
                    wt->tx_len = len + WIRE_NO_OF_BYTES;

                    wire_tx_stored_message(earbud);
                    wt->no_response_timeout = WIRE_NO_RESPONSE_TIMEOUT;
                    wt->retry_count = 0;
                    wt->nack_count = 0;
                    wt->retry = false;
                    wt->need_answer = true;
                    ret = true;
                }
            }
        }
    }

    return ret;
}

static void wire_process_rx(uint8_t earbud)
{
    WIRE_TRANSACTION *wt = &wire_transaction[earbud];

    if (wt->rx_len)
    {
        uint8_t pkt_sn = BITMAP_GET(WIRE_HDR, SN, wt->rx_data[0]);
        uint8_t pkt_nesn = BITMAP_GET(WIRE_HDR, NESN, wt->rx_data[0]);

#ifndef TEST
        /*
        * Remove these eventually.
        */
        PRINTF_B("Process RX (%c)", earbud_letter[earbud]);
        cli_tx_hex(CLI_BROADCAST, "WIRE RX", wt->rx_data, wt->rx_len);
        PRINTF_B("PKT SN=%d NESN=%d", pkt_sn, pkt_nesn);
        PRINTF_B("Local SN=%d NESN=%d", wt->sn, wt->nesn);
#endif

        if (pkt_sn == wt->nesn)
        {
            wt->nesn = (wt->nesn) ? 0:1;
        }

        if (pkt_nesn == wt->sn)
        {
            PRINT_B("NACK!");
            wt->retry = true;
            wt->nack_count++;
            wire_user->nack(earbud);
        }
        else
        {
            wt->sn = (wt->sn) ? 0:1;

            if (wt->rx_len > WIRE_NO_OF_BYTES)
            {
                /*
                * The message is long enough to contain a CCP payload,
                * so pass it on.
                */
                wire_user->rx(
                    earbud, &wt->rx_data[WIRE_HEADER_BYTES], wt->rx_len-WIRE_NO_OF_BYTES,
                    true);

                /*
                * Cause ack message to be sent to earbud (the actual contents
                * of this message, which is just a header and a checksum, will
                * be created on the point of sending).
                */
                wt->tx_len = WIRE_NO_OF_BYTES;
                wt->retry_count = 0;
                wt->retry = true;
                wt->need_answer = false;
            }
            else
            {
                /*
                * Even though there is no CCP payload, pass on an indication
                * that the message was acknowledged.
                */
                wire_user->ack(earbud);
                wt->tx_len = 0;
            }
        }

        wt->rx_len = 0;
    }
}

void wire_rx(uint8_t earbud, uint8_t *data, uint16_t len)
{
    WIRE_TRANSACTION *wt = &wire_transaction[earbud];

    cli_tx_hex(CLI_BROADCAST, "COMMS->WIRE", data, len);

    /*
    * Currently we only allow data that will fit in a single packet, going
    * forward that will have to change.
    */
    if (len <= WIRE_MAX_PACKET_SIZE && len >= WIRE_NO_OF_BYTES)
    {
#ifdef SCHEME_A
        if (wire_checksum(data, len-1) == data[len-1])
#else
        if (wire_checksum(data, len-2) == (((uint16_t)(data[len-2])) << 8) + data[len-1])
#endif
        {
            /*
            * Buffer the received message, to be dealt with in the periodic
            * function.
            */
            memcpy(wt->rx_data, data, len);
            wt->rx_len = len;
        }
        else
        {
            /*
            * Checksum error, so this data cannot be trusted. Ignore it, shortly
            * we will retry.
            */
            PRINT_B("Invalid checksum");
            wt->retry = true;
        }
    }
}

void wire_init(const WIRE_USER_CB *user_cb)
{
    wire_user = user_cb;
}

static void wire_manage_transaction(uint8_t earbud)
{
    WIRE_TRANSACTION *wt = &wire_transaction[earbud];

    wire_process_rx(earbud);

    if (wt->tx_len)
    {
        if (wt->retry)
        {
            if (wt->retry_count < WIRE_MAX_RETRIES)
            {
                if (!charger_comms_is_active())
                {
                    wire_tx_stored_message(earbud);
                    wt->no_response_timeout = WIRE_NO_RESPONSE_TIMEOUT;
                    wt->retry_count++;
                    wt->retry = false;

                    if (!wt->need_answer)
                    {
                        wt->tx_len = 0;
                    }
                }
            }
            else
            {
                if (wt->nack_count)
                {
                    /*
                    * Persistent NACKs, so issue an empty broadcast message
                    * to hopefully reset everything before trying again.
                    */
                    uint8_t e;

                    for (e=0; e<NO_OF_EARBUDS; e++)
                    {
                        WIRE_TRANSACTION *wtc = &wire_transaction[e];

                        wtc->sn = 0;
                        wtc->nesn = 0;
                        wtc->retry_count = 0;
                        wtc->retry = true;
                        wtc->nack_count = 0;
                    }
                    wire_setup_broadcast(NULL, 0, 0);
                }
                else
                {
                    wt->tx_len = 0;
                    wire_user->give_up(earbud);
                }
            }
        }
        else
        {
            if (!wt->no_response_timeout)
            {
                /* If we get no response, retry. */
                if (wt->retry_count < WIRE_MAX_RETRIES)
                {
                    wt->retry = true;
                }
                else
                {
                    wt->tx_len = 0;
                    wire_user->no_response(earbud);
                }
            }
            else
            {
                wt->no_response_timeout--;
            }
        }
    }
}

void wire_periodic(void)
{
    if (broadcast_len)
    {
        if (!broadcast_timeout)
        {
            if (!charger_comms_is_active())
            {
                wire_send_broadcast();

                if (broadcast_count < WIRE_BROADCAST_SENDS)
                {
                    broadcast_timeout = WIRE_BROADCAST_TIMEOUT;
                }
                else
                {
                    broadcast_len = 0;
                    wire_user->broadcast_finished();
                }
            }
        }
        else
        {
            broadcast_timeout--;
        }
    }
    else
    {
        wire_manage_transaction(EARBUD_LEFT);
        wire_manage_transaction(EARBUD_RIGHT);
    }
}
