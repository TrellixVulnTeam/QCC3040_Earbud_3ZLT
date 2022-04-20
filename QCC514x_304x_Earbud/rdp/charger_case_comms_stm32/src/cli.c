/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Command Line Interface
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "cli.h"
#include "cli_txf.h"
#include "cli_parse.h"
#include "adc.h"
#include "gpio.h"
#include "pfn.h"
#include "uart.h"
#include "ccp.h"
#include "timer.h"
#include "memory.h"
#include "wdog.h"
#include "auth.h"
#include "flash.h"
#include "dfu.h"
#include "case.h"
#include "usb.h"
#include "ascii.h"
#include "power.h"
#include "earbud.h"
#include "debug.h"
#include "rtc.h"
#include "config.h"
#include "led.h"
#include "battery.h"
#include "case_charger.h"
#include "cmsis.h"
#include "version.h"
#include "vreg.h"

#ifdef EARBUD_CURRENT_SENSES
#include "current_senses.h"
#endif

#ifdef CHARGER_COMMS_FAKE
#include "fake_earbud.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define CLI_HISTORY

#define CLI_MAX_LINE_LENGTH 400

#ifdef CLI_HISTORY
/*
* CLI_HISTORY_ITEMS: Number of entries in the command line history.
*/
#define CLI_HISTORY_ITEMS 3

/*
* CLI_HISTORY_LENGTH: Maximum size of an entry in the command line history.
*/
#define CLI_HISTORY_LENGTH 20
#endif

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Values for AT+CCEARBUD.
*/
typedef enum
{
    CCEARBUD_NONE  = 0,
    CCEARBUD_LEFT  = 1,
    CCEARBUD_RIGHT = 2,
    CCEARBUD_BOTH  = 3
}
CCEARBUD_OPTION;

#ifdef CLI_HISTORY
typedef enum
{
    CLI_ESC_OFF,
    CLI_ESC_GOT_ESC,
    CLI_ESC_GOT_BRACKET
}
CLI_ESC_STATE;
#endif

typedef struct
{
    char line[CLI_MAX_LINE_LENGTH + 1];
    uint16_t line_ctr;
    CCEARBUD_OPTION ccearbud;
    bool discard;
    bool no_broadcast;
    void (*tx)(uint8_t *, uint16_t);
    void (*intercept)(uint8_t, char *);
#ifdef CLI_HISTORY
    char history[CLI_HISTORY_ITEMS][CLI_HISTORY_LENGTH + 1];
    uint8_t history_pos;
    CLI_ESC_STATE esc_state;
#endif
}
CLI_INFO;

CLI_RESULT at_ccearbud_set_cmd(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT ats_reboot(uint8_t cmd_source);
static CLI_RESULT atq_id(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND cli_command[] =
{
    { "adc",    adc_cmd,    2 },
    { "case",   case_cmd,   2 },
    { "config", config_cmd, 2 },
    { "dfu",    dfu_cmd,    2 },
    { "flash",  flash_cmd,  2 },
#ifdef CHARGER_COMMS_FAKE
    { "earbud", earbud_cmd, 2 },
#endif
    { "gpio",   gpio_cmd,   2 },
    { "led",    led_cmd,    2 },
    { "mem",    mem_cmd,    2 },
    { "pfn",    pfn_cmd,    2 },
    { "power",  power_cmd,  2 },
    { "rtc",    rtc_cmd,    2 },
    { "timer",  timer_cmd,  2 },
    { "wdog",   wdog_cmd,   2 },
    { NULL }
};

static const CLI_COMMAND cli_command_ats[] =
{
    { "AUTHSTART",   at_authstart_set_cmd,   0 },
    { "AUTHRESP",    at_authresp_set_cmd,    1 },
    { "AUTHDISABLE", at_authdisable_set_cmd, 1 },
    { "CCEARBUD",    at_ccearbud_set_cmd,    1 },
    { "CHARGER",     ats_charger,            2 },
    { "CONFIG",      ats_config,             2 },
    { "EBSTATUS",    ats_ebstatus,           2 },
    { "GPIO",        ats_gpio,               2 },
    { "LED",         ats_led,                2 },
    { "LOOPBACK",    ats_loopback,           2 },
    { "POWER",       ats_power,              2 },
    { "REBOOT",      ats_reboot,             2 },
    { "REGULATOR",   ats_regulator,          2 },
    { "SHIP",        ats_ship,               2 },
    { "TEST",        ats_test,               2 },
    { NULL }
};

static const CLI_COMMAND cli_command_atq[] =
{
    { "BATTERY", atq_battery, 2 },
    { "CHARGER", atq_charger, 2 },
    { "CONFIG",  atq_config,  2 },
    { "ID",      atq_id,      2 },
    { "LID",     atq_lid,     2 },
    { "NTC",     atq_ntc,     2 },
#ifdef EARBUD_CURRENT_SENSES
    { "SENSE",   atq_sense,   2 },
#endif
    { NULL }
};

static const CLI_COMMAND cli_command_att[] =
{
    { NULL }
};

static CLI_INFO cli_info[CLI_NO_OF_SOURCES] = {0};
uint8_t cli_auth_level[CLI_NO_OF_SOURCES] = {0};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void cli_uart_tx(uint8_t *data, uint16_t len)
{
    uart_tx(UART_CLI, data, len);
}

void cli_uart_rx(uint8_t data)
{
    cli_rx(CLI_SOURCE_UART, (char)data);
}

void cli_init(void)
{
    cli_info[CLI_SOURCE_UART].tx = cli_uart_tx;
    cli_auth_level[CLI_SOURCE_UART] = 2;

#ifdef USB_ENABLED
    cli_info[CLI_SOURCE_USB].tx = usb_tx;
    cli_auth_level[CLI_SOURCE_USB] = 2;
#endif
}

void cli_broadcast_enable(uint8_t cmd_source)
{
    cli_info[cmd_source].no_broadcast = false;
}

void cli_broadcast_disable(uint8_t cmd_source)
{
    cli_info[cmd_source].no_broadcast = true;
}

CLI_RESULT at_ccearbud_set_cmd(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;
    long int e;

    if (cli_get_next_parameter(&e, 10))
    {
        if ((e >= CCEARBUD_NONE) && (e <= CCEARBUD_BOTH))
        {
            cli_info[cmd_source].ccearbud = (CCEARBUD_OPTION)e;
            ret = CLI_OK;
        }
    }

    return ret;
}

void cli_set_auth_level(uint8_t cmd_source, uint8_t level)
{
    cli_auth_level[cmd_source] = level;
}

void cli_tx(uint8_t cmd_source, bool crlf, char *str)
{
    uint8_t s;

    for (s=0; s<CLI_NO_OF_SOURCES; s++)
    {
        if (((cmd_source==CLI_BROADCAST) && !cli_info[s].no_broadcast) ||
            (cmd_source==s))
        {
            cli_info[s].tx((uint8_t *)str, strlen(str));

            if (crlf)
            {
                uint8_t buf[2] = { ASCII_CR, ASCII_LF };
                cli_info[s].tx(buf, 2);
            }
        }
    }
}

void cli_txc(uint8_t cmd_source, char ch)
{
    char buf[2];
    buf[0] = ch;
    buf[1] = 0;

    cli_tx(cmd_source, false, buf);
}

void cli_tx_hex(uint8_t cmd_source, char *heading, uint8_t *data, uint8_t len)
{
    uint8_t i;

    cli_txf(cmd_source, false, "%s:", heading);

    for (i=0; i<len; i++)
    {
        cli_txf(cmd_source, false, " %02x", data[i]);
    }

    cli_tx(cmd_source, true, "");
}

static void cli_case_at_command(uint8_t cmd_source)
{
    CLI_INFO *cli = &cli_info[cmd_source];
    CLI_RESULT at_result;

    if (cli->line[cli->line_ctr - 1]=='?')
    {
        if (cli->line[cli->line_ctr - 2]=='=')
        {
            /*
            * Command ends in =? so it is a test command.
            */
            at_result = cli_process_cmd(
                cli_command_att, cmd_source,
                strtok(&cli->line[3], CLI_SEPARATOR));
        }
        else
        {
            /*
            * Command ends in ? so it is a query.
            */
            at_result = cli_process_cmd(
                cli_command_atq, cmd_source,
                strtok(&cli->line[3], CLI_SEPARATOR));
        }
    }
    else
    {
        /*
        * Command doesn't end in ? so it is a set command.
        */
        at_result = cli_process_cmd(
            cli_command_ats, cmd_source,
            strtok(&cli->line[3], CLI_SEPARATOR));
    }

    switch (at_result)
    {
        case CLI_OK:
            PRINT("OK");
            break;

        case CLI_ERROR:
            PRINT("ERROR");
            break;

        default:
            break;
    }
}

static void cli_left_earbud_at_command(uint8_t cmd_source, char *cmd)
{
    if (!ccp_at_command(cmd_source, WIRE_DEST_LEFT, cmd))
    {
        PRINT("L: ERROR");
    }
}

static void cli_right_earbud_at_command(uint8_t cmd_source, char *cmd)
{
    if (!ccp_at_command(cmd_source, WIRE_DEST_RIGHT, cmd))
    {
        PRINT("R: ERROR");
    }
}

static void cli_both_earbuds_at_command(uint8_t cmd_source, char *cmd)
{
    cli_left_earbud_at_command(cmd_source, cmd);
    cli_right_earbud_at_command(cmd_source, cmd);
}

#ifdef CLI_HISTORY
/*
* Add the current command line into the command history. The idea is that
* the new command should end up the first item in the array, and there should
* be no duplicate entries.
*/
static void cli_store_history(uint8_t cmd_source)
{
    CLI_INFO *cli = &cli_info[cmd_source];

    if (cli->line_ctr && (cli->line_ctr <= CLI_HISTORY_LENGTH))
    {
        uint8_t n;

        for (n=0; n<CLI_HISTORY_ITEMS; n++)
        {
            if (!strcmp(cli->line, cli->history[n]))
            {
                break;
            }
        }

        if (n>=CLI_HISTORY_ITEMS)
        {
            n = CLI_HISTORY_ITEMS - 1;
        }

        memmove(cli->history[1], cli->history, (CLI_HISTORY_LENGTH + 1) * n);
        strcpy(cli->history[0], cli->line);
    }
}

/*
* Replace the current command line.
*/
static void cli_replace_line(uint8_t cmd_source, char *replacement)
{
    CLI_INFO *cli = &cli_info[cmd_source];
    size_t len = strlen(replacement);

    if (len && (len < sizeof(cli->line)))
    {
        size_t n;

        /*
        * Rub out the existing line.
        */
        cli_txc(cmd_source, ASCII_CR);
        for (n=0; n<cli->line_ctr; n++)
        {
            cli_txc(cmd_source, ' ');
        }
        cli_txc(cmd_source, ASCII_CR);

        /*
        * Insert the replacement line.
        */
        strcpy(cli->line, replacement);
        cli->line_ctr = len;

        /*
        * Display the replacement line.
        */
        cli_tx(cmd_source, false, cli->line);
    }
}
#endif /*CLI_HISTORY*/

void cli_rx(uint8_t cmd_source, char ch)
{
    CLI_INFO *cli = &cli_info[cmd_source];

#ifdef CLI_HISTORY
    bool escape_seq = true;

    /*
    * Escape sequence handling.
    */
    switch (cli->esc_state)
    {
        default:
        case CLI_ESC_OFF:
            if (ch==ASCII_ESC)
            {
                cli->esc_state = CLI_ESC_GOT_ESC;
            }
            else
            {
                cli->esc_state = CLI_ESC_OFF;
                escape_seq = false;
            }
            break;

        case CLI_ESC_GOT_ESC:
            if (ch=='[')
            {
                cli->esc_state = CLI_ESC_GOT_BRACKET;
            }
            else
            {
                cli->esc_state = CLI_ESC_OFF;
                escape_seq = false;
            }
            break;

        case CLI_ESC_GOT_BRACKET:
            cli->esc_state = CLI_ESC_OFF;
            if (ch=='A')
            {
                if (cli->history_pos < CLI_HISTORY_ITEMS)
                {
                    cli_replace_line(cmd_source, cli->history[cli->history_pos]);
                    cli->history_pos++;
                }
            }
            else if (ch=='B')
            {
                if (cli->history_pos > 1)
                {
                    cli_replace_line(cmd_source, cli->history[cli->history_pos - 2]);
                    cli->history_pos--;
                }
            }
            break;
    }

    if (!escape_seq)
    {
        /*
        * Treat the character as normal, not as part of an escape sequence.
        */
        cli->history_pos = 0;
#else
    {
#endif /*CLI_HISTORY*/
        switch (ch)
        {
            case ASCII_LF:
                break;

            case ASCII_CR:
                if (cli->intercept)
                {
                    cli->intercept(cmd_source, cli->line);
                }
                else if (!cli->discard)
                {
#ifdef CLI_HISTORY
                    cli_store_history(cmd_source);
#endif
                    /*
                    * Echo CRLF.
                    */
                    cli_txc(cmd_source, ASCII_CR);
                    cli_txc(cmd_source, ASCII_LF);

                    if ((cli->line_ctr >= 2) &&
                        ((cli->line[0] | 0x20) =='a') &&
                        ((cli->line[1] | 0x20) =='t'))
                    {
                        /*
                        * This is an AT command.
                        */
                        if ((cli->line_ctr >= 5) && (cli->line[2] == '+') &&
                            ((cli->line[3] | 0x20) =='c') &&
                            ((cli->line[4] | 0x20) =='c'))
                        {
                            /*
                            * The command starts AT+CC, so it is for the case.
                            */
                            cli_case_at_command(cmd_source);
                        }
                        else
                        {
                            char ec = cli->line[2] | 0x20;

                            switch (ec)
                            {
                                case 'l':
                                    cli_left_earbud_at_command(
                                        cmd_source, &cli->line[3]);
                                    break;

                                case 'r':
                                    cli_right_earbud_at_command(
                                        cmd_source, &cli->line[3]);
                                    break;

                                case 'b':
                                    cli_both_earbuds_at_command(
                                        cmd_source, &cli->line[3]);
                                    break;

                                default:
                                    /*
                                    * Recipient of this AT command depends on the
                                    * +CCEARBUD setting.
                                    */
                                    switch (cli->ccearbud)
                                    {
                                        case CCEARBUD_LEFT:
                                            cli_left_earbud_at_command(
                                                cmd_source, &cli->line[2]);
                                            break;

                                        case CCEARBUD_RIGHT:
                                            cli_right_earbud_at_command(
                                                cmd_source, &cli->line[2]);
                                            break;

                                        case CCEARBUD_BOTH:
                                            cli_both_earbuds_at_command(
                                                cmd_source, &cli->line[2]);
                                            break;

                                        default:
                                        case CCEARBUD_NONE:
                                            cli_case_at_command(cmd_source);
                                            break;
                                    }
                                    break;
                            }
                        }
                    }
                    else
                    {
                        /*
                        * Doesn't begin with AT, so treat as a standard CLI command.
                        */
                        if (cli->line_ctr)
                        {
                            if (cli_process_cmd(
                                cli_command, cmd_source,
                                strtok(cli->line, CLI_SEPARATOR)) != CLI_OK)
                            {
                                PRINT("ERROR");
                            }
                        }
                    }
                }
                cli->line_ctr = 0;
                cli->discard = false;
                memset(cli->line, 0, sizeof(cli->line));
                break;

            case ASCII_BS:
            case ASCII_DEL:
                if (!cli->discard && cli->line_ctr)
                {
                    cli->line[cli->line_ctr] = 0;
                    cli->line_ctr--;
                    cli_txc(cmd_source, ASCII_BS);
                    cli_txc(cmd_source, ' ');
                    cli_txc(cmd_source, ASCII_BS);
                }
                break;

            default:
                if (!cli->discard)
                {
                    if ((ch >= 0x20) && (ch < 0x7F))
                    {
                        if (!cli->intercept && (ch=='S') && (cli->line_ctr==0))
                        {
                            /*
                            * This is interpreted as being an S-record from a
                            * failed firmware update. Discard the entire line.
                            */
                            cli->discard = true;
                        }
                        else if (cli->line_ctr < CLI_MAX_LINE_LENGTH)
                        {
                            if (!cli->intercept)
                            {
                                /*
                                * Echo the character entered.
                                */
                                cli_txc(cmd_source, ch);
                            }

                            /*
                            * Add the character to the line buffer.
                            */
                            cli->line[cli->line_ctr++] = ch;
                        }
                    }
                }
                break;
        }
    }
}

void cli_intercept_line(
    uint8_t cmd_source,
    void (*fn)(uint8_t, char *))
{
    cli_info[cmd_source].intercept = fn;
}

static CLI_RESULT ats_reboot(uint8_t cmd_source __attribute__((unused)))
{
    /* Force the voltage regulator OFF */
#ifdef SCHEME_A
    vreg_pwm();
    charger_comms_vreg_reset();
#endif
    delay_ms(30);
    vreg_disable();
    delay_ms(30);

    NVIC_SYSTEM_RESET();
    return CLI_OK;
}

static CLI_RESULT atq_id(uint8_t cmd_source)
{
    PRINTF("\"%s\",%d,%d,\"%s\"",
        VARIANT_NAME,
        config_get_board_id(),
        SW_VERSION_NUMBER, SW_VERSION_STRING);

    return CLI_OK;
}
