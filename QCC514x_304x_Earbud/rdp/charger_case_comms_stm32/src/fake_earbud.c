/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Earbud
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "main.h"

#ifdef CHARGER_COMMS_FAKE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fake_earbud.h"
#include "power.h"
#include "ccp.h"
#include "wire.h"
#include "charger_comms.h"
#include "uart.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    uint8_t sn;
    uint8_t nesn;
    uint8_t rsp_later;
    uint8_t pattern_ctr;
    uint16_t nack_pattern;
    uint16_t corrupt_pattern;
    uint8_t rbuf[CHARGER_COMMS_MAX_MSG_LEN];
    uint16_t rbuf_len;
}
EARBUD_INFO;

/*-----------------------------------------------------------------------------
------------------ PROTOYPES --------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT earbud_cmd_nack(uint8_t cmd_source);
static CLI_RESULT earbud_cmd_corrupt(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static EARBUD_INFO earbud_info[NO_OF_EARBUDS] = {0};

static const CLI_COMMAND earbud_command[] =
{
    { "nack",    earbud_cmd_nack,    2 },
    { "corrupt", earbud_cmd_corrupt, 2 },
    { NULL }
};

#ifdef CHARGER_COMMS_FAKE_U
static uint8_t eb_rx_buf[CHARGER_COMMS_MAX_MSG_LEN] = {0};
static uint16_t eb_rx_buf_ctr = 0;
#endif

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void earbud_rx(uint8_t *buf, uint16_t buf_len)
{
    WIRE_DESTINATION dest = (WIRE_DESTINATION)((buf[0] & 0x30) >> 4);

    cli_tx_hex(CLI_BROADCAST, "EARBUD RX", buf, buf_len);

    if (dest == WIRE_DEST_BROADCAST)
    {
        earbud_info[EARBUD_LEFT].sn = 0;
        earbud_info[EARBUD_LEFT].nesn = 0;
        earbud_info[EARBUD_RIGHT].sn = 0;
        earbud_info[EARBUD_RIGHT].nesn = 0;
    }
    else
    {
        uint8_t earbud = (dest==WIRE_DEST_LEFT) ? EARBUD_LEFT:EARBUD_RIGHT;
        EARBUD_INFO *info = &earbud_info[earbud];
        uint8_t rbuf[CHARGER_COMMS_MAX_MSG_LEN];
        uint16_t len = WIRE_CRC_BYTES;
        uint8_t pkt_nesn = (buf[0] & 0x40) ? 1:0;
        uint8_t pkt_sn = (buf[0] & 0x80) ? 1:0;

        if ((1 << info->pattern_ctr) & info->nack_pattern)
        {
            /*
            * Signal a NACK by not toggling NESN.
            */
            PRINT_B("NACK response");
        }
        else
        {
            if (pkt_sn == info->nesn)
            {
                info->nesn = (info->nesn) ? 0:1;
            }

            if (pkt_nesn != info->sn)
            {
                info->sn = (info->sn) ?  0:1;
            }

            if (buf_len <= WIRE_NO_OF_BYTES)
            {
                if (info->rsp_later)
                {
                    info->rsp_later--;
                    if (!info->rsp_later)
                    {
                        memcpy(&rbuf[WIRE_HEADER_BYTES], info->rbuf, info->rbuf_len);
                        len = info->rbuf_len + WIRE_CRC_BYTES;
                    }
                }
                else
                {
                    len = 0;
                }
            }
            else
            {
                CCP_MESSAGE msg = (CCP_MESSAGE)(buf[WIRE_HEADER_BYTES] & 0xF);

                switch (msg)
                {
                    case CCP_MSG_STATUS_REQ:
                        info->rbuf[0] = CCP_MSG_EARBUD_STATUS;

                        if (buf_len > 3)
                        {
                            uint8_t info_type = buf[2];

                            info->rbuf[1] = info_type | 0x80;

                            switch (info_type)
                            {
                                case CCP_IT_BT_ADDRESS:
                                    info->rbuf[2] = (uint8_t)rand();
                                    info->rbuf[3] = (uint8_t)rand();
                                    info->rbuf[4] = (uint8_t)rand();
                                    info->rbuf[5] = (uint8_t)rand();
                                    info->rbuf[6] = (uint8_t)rand();
                                    info->rbuf[7] = (uint8_t)rand();
                                    info->rbuf[8] = (uint8_t)rand();
                                    info->rbuf_len = 9;
                                    info->rsp_later = 3;
                                    break;

                                default:
                                    break;
                            }
                        }
                        else
                        {
                            info->rbuf[1] = 0x00;
                            info->rbuf[2] = (uint8_t)(rand() % 100);
                            info->rbuf_len = 3;
                            info->rsp_later = 3;
                        }
                        break;

                    case CCP_MSG_LOOPBACK:
                        info->rbuf[0] = CCP_MSG_LOOPBACK;
                        memcpy(&info->rbuf[1], &buf[2], buf_len - 3);
                        info->rbuf_len = buf_len - 2;
                        info->rsp_later = 1;
                        break;

                    case CCP_MSG_EARBUD_CMD:
                        info->rbuf[0] = CCP_MSG_EARBUD_RSP;
                        info->rbuf[1] = CCP_EC_SHIPPING_MODE;
                        info->rbuf[2] = 0x01;
                        info->rbuf_len = 3;
                        info->rsp_later = 1;
                        break;

                    default:
                        break;
                }
            }
        }

        if (len)
        {
#ifdef SCHEME_A
            rbuf[0] = (uint8_t)len;
#else
            rbuf[0] = (uint8_t)((len >> 8) & 0x03) |
                (earbud==EARBUD_LEFT) ? 0x08:0x04;
            rbuf[1] = (uint8_t)(len & 0xFF);
#endif

            if (info->sn)
            {
                rbuf[0] |= 0x80;
            }

            if (info->nesn)
            {
                rbuf[0] |= 0x40;
            }

            wire_append_checksum(rbuf, len + WIRE_HEADER_BYTES - WIRE_CRC_BYTES);

            if ((1 << info->pattern_ctr) & info->corrupt_pattern)
            {
                /*
                * Corrupt the data so that it will be rejected due to a
                * checksum mismatch.
                */
                PRINT_B("Corrupt response");
                rbuf[0] ^= 0xFF;
            }

#ifdef SCHEME_A
            wire_rx(earbud, rbuf, len + WIRE_HEADER_BYTES);
#else
            cli_tx_hex(CLI_BROADCAST, "EARBUD TX", rbuf, len + WIRE_HEADER_BYTES);
#ifdef CHARGER_COMMS_FAKE_U
            uart_tx(UART_EARBUD, rbuf, len + WIRE_HEADER_BYTES);
#else
            {
                uint8_t n;

                for (n=0; n < (len + WIRE_HEADER_BYTES); n++)
                {
                    charger_comms_receive(rbuf[n]);
                }
            }
#endif
#endif
        }

        info->pattern_ctr = (info->pattern_ctr + 1) & 0xF;
    }

#ifdef SCHEME_A
    power_clear_run_reason(POWER_RUN_CHARGER_COMMS);
#endif
}

#ifdef CHARGER_COMMS_FAKE_U
void earbud_rx_ready(void)
{
    eb_rx_buf_ctr = 0;
}

void earbud_rxc(uint8_t data)
{
    if (eb_rx_buf_ctr < CHARGER_COMMS_MAX_MSG_LEN)
    {
        eb_rx_buf[eb_rx_buf_ctr++] = data;

        if (eb_rx_buf_ctr >= WIRE_NO_OF_BYTES)
        {
            if ((wire_get_payload_length(eb_rx_buf) + WIRE_HEADER_BYTES)==eb_rx_buf_ctr)
            {
                earbud_rx(eb_rx_buf, eb_rx_buf_ctr);
                eb_rx_buf_ctr = 0;
            }
        }
    }
}
#endif

static CLI_RESULT earbud_cmd_nack(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint8_t earbud;

    if (cli_get_earbud(&earbud))
    {
        long int x;

        if (cli_get_next_parameter(&x, 16))
        {
            earbud_info[earbud].nack_pattern = (uint16_t)x;
            ret = CLI_OK;
        }
    }

    return ret;
}

static CLI_RESULT earbud_cmd_corrupt(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint8_t earbud;

    if (cli_get_earbud(&earbud))
    {
        long int x;

        if (cli_get_next_parameter(&x, 16))
        {
            earbud_info[earbud].corrupt_pattern = (uint16_t)x;
            ret = CLI_OK;
        }
    }

    return ret;
}

CLI_RESULT earbud_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(earbud_command, cmd_source);
}

#endif
