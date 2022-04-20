/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Wire protocol
*/

#ifndef WIRE_H_
#define WIRE_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "earbud.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#ifdef SCHEME_A
#define WIRE_HEADER_BYTES 1
#define WIRE_CRC_BYTES 1
#else
#define WIRE_HEADER_BYTES 2
#define WIRE_CRC_BYTES 2
#endif

/*
* WIRE_NO_OF_BYTES: Number of bytes in the message that relate to the wire
* protocol layer.
*/
#define WIRE_NO_OF_BYTES (WIRE_HEADER_BYTES + WIRE_CRC_BYTES)

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    WIRE_DEST_CASE      = 0,
    WIRE_DEST_RIGHT     = 1,
    WIRE_DEST_LEFT      = 2,
    WIRE_DEST_BROADCAST = 3
}
WIRE_DESTINATION;

typedef struct
{
    void (*rx)(uint8_t earbud, uint8_t *data, uint16_t len, bool final_piece);
    void (*ack)(uint8_t earbud);
    void (*nack)(uint8_t earbud);
    void (*give_up)(uint8_t earbud);
    void (*no_response)(uint8_t earbud);
    void (*abort)(uint8_t earbud);
    void (*broadcast_finished)(void);
}
WIRE_USER_CB;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

extern const uint8_t wire_dest[NO_OF_EARBUDS];

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void wire_init(const WIRE_USER_CB *user_cb);
uint16_t wire_get_payload_length(uint8_t *data);
void wire_periodic(void);
bool wire_tx(WIRE_DESTINATION dest, uint8_t *data, uint16_t len);
void wire_rx(uint8_t earbud, uint8_t *data, uint16_t len);

#ifdef SCHEME_A
uint8_t wire_checksum(uint8_t *data, uint16_t len);
#else
uint16_t wire_checksum(uint8_t *data, uint16_t len);
#endif

void wire_append_checksum(uint8_t *data, uint16_t len);

#ifdef SCHEME_B
uint16_t wire_get_packet_src(uint8_t *data);
#endif

#endif /* WIRE_H_ */
