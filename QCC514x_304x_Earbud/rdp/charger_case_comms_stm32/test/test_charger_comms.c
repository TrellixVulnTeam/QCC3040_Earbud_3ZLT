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

#ifdef VARIANT_CB

#include "unity.h"

#include "charger_comms.c"
#include "cli_txf.c"

#include "mock_cli.h"
#include "mock_charger_comms_if.h"
#include "mock_wire.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

earbud_channel left_earbud;
earbud_channel right_earbud;
bool received_charger_comm_packet;

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    memset(&left_earbud, 0, sizeof(left_earbud));
    memset(&right_earbud, 0, sizeof(right_earbud));
    received_charger_comm_packet = false;

    cfg = NULL;
    charger_comm_state = CHARGER_COMMS_NOT_ACTIVE;
    charger_comm_vreg_state = CHARGER_COMMS_VREG_HIGH;
    start_idx = 0;
    write_idx = 0;
    bit_idx = 0;
    timer_idx = 0;
    num_tx_bits = 0;
    memset(tx_buf, 0, sizeof(tx_buf));
    expect_reply = false;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Charger comms.
*/
void test_charger_comms(void)
{
}

#endif /*VARIANT_CB*/
