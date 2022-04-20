/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger Comms Scheme B
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include "main.h"
#include "uart.h"
#include "wire.h"
#include "power.h"
#include "gpio.h"
#include "cli.h"
#include "charger_comms.h"
#include "vreg.h"

#ifdef CHARGER_COMMS_FAKE
#include "fake_earbud.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CHARGER_COMMS_IDLE,
    CHARGER_COMMS_WAKE_EARBUD,
    CHARGER_COMMS_CASE_START,
    CHARGER_COMMS_COMMS_MODE,
} charger_comms_b_states;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static charger_comms_b_states cc_state = CHARGER_COMMS_IDLE;

static uint8_t cc_rx_buf[CHARGER_COMMS_MAX_MSG_LEN] = {0};
static uint16_t cc_rx_buf_ctr = 0;
static uint8_t *cc_tx_buf = NULL;
static uint16_t cc_tx_len;
static uint8_t cc_dest;
static uint8_t cc_timeout;
static uint8_t cc_wait = 0;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void charger_comms_device_init(void)
{
    cc_state = CHARGER_COMMS_IDLE;
    vreg_init();
}

bool charger_comms_is_active(void)
{
    return cc_state != CHARGER_COMMS_IDLE && cc_timeout != 0;
}

static void charger_comms_end(void)
{
    cc_timeout = 0;
    cc_rx_buf_ctr = 0;
}

static void charger_comms_return_to_vchg(void)
{
    /* TODO: We should disable the pull-up and re-enable VBUS when we have
     * finished sending data.
     * Need to be very careful about timing this.*/
    vreg_off_clear_reason(VREG_REASON_OFF_COMMS);

    power_clear_run_reason(POWER_RUN_CHARGER_COMMS);
    cc_state = CHARGER_COMMS_IDLE;
}

static void charger_comms_raw_transmit(void)
{
    uart_tx(UART_DOCK, cc_tx_buf, cc_tx_len);

#ifdef CHARGER_COMMS_FAKE
#ifdef CHARGER_COMMS_FAKE_U
    earbud_rx_ready();
#else
    earbud_rx(cc_tx_buf, cc_tx_len);
#endif
#endif
}

void charger_comms_transmit(
    uint8_t dest,
    uint8_t *buf,
    uint16_t num_tx_octets)
{
    cli_tx_hex(CLI_BROADCAST, "WIRE->COMMS", buf, num_tx_octets);

    cc_tx_buf = buf;
    cc_dest = dest;
    cc_tx_len = num_tx_octets;
    cc_timeout = 2;
    
    vreg_off_set_reason(VREG_REASON_OFF_COMMS);
    power_set_run_reason(POWER_RUN_CHARGER_COMMS);

    if (cc_state == CHARGER_COMMS_IDLE)
    {
        cc_wait = 1;
        cc_state = CHARGER_COMMS_WAKE_EARBUD;
    }
    else if (cc_state == CHARGER_COMMS_COMMS_MODE)
    {
        cc_wait = 50;
        charger_comms_raw_transmit();
    }
}


void charger_comms_transmit_done(void)
{
    if (cc_dest==WIRE_DEST_BROADCAST)
    {
        charger_comms_end();
    }
}

void charger_comms_receive(uint8_t data)
{
    if (charger_comms_is_active() && (cc_dest != WIRE_DEST_BROADCAST))
    {
        if (cc_rx_buf_ctr < CHARGER_COMMS_MAX_MSG_LEN)
        {
            cc_rx_buf[cc_rx_buf_ctr++] = data;

            if (cc_rx_buf_ctr >= WIRE_NO_OF_BYTES)
            {
                if ((wire_get_payload_length(cc_rx_buf) + WIRE_HEADER_BYTES)==cc_rx_buf_ctr)
                {
                    uint8_t earbud = wire_get_packet_src(cc_rx_buf) == 1 ? EARBUD_RIGHT : EARBUD_LEFT;
                    wire_rx(
                            earbud,
                            cc_rx_buf,
                            cc_rx_buf_ctr);

                    charger_comms_end();
                }
            }
        }
    }
}

void charger_comms_periodic(void)
{
    switch(cc_state)
    {
        case CHARGER_COMMS_IDLE:
            break;

        case CHARGER_COMMS_WAKE_EARBUD:
            if (!cc_wait--)
            {
                /* Now enable the pull-up for Tcase_start*/
                gpio_enable(GPIO_DOCK_PULL_EN);
                cc_wait = 0;
                cc_state = CHARGER_COMMS_CASE_START;
            }
            break;

        case CHARGER_COMMS_CASE_START:
            if (!cc_wait--)
            {
                charger_comms_raw_transmit();
                cc_state = CHARGER_COMMS_COMMS_MODE;
            }
            break;

        case CHARGER_COMMS_COMMS_MODE:
            if (!cc_timeout--)
            {
                charger_comms_end();
            }
            if (!cc_wait--)
            {
                charger_comms_return_to_vchg();
            }
            break;

        default:
            break;
    }
}
