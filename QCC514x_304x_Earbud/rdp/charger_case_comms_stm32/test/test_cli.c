/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for cli.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "cli.c"
#include "cli_txf.c"
#include "cli_parse.c"

#include "mock_adc.h"
#include "mock_gpio.h"
#include "mock_pfn.h"
#include "mock_uart.h"
#include "mock_ccp.h"
#include "mock_timer.h"
#include "mock_memory.h"
#include "mock_wdog.h"
#include "mock_auth.h"
#include "mock_flash.h"
#include "mock_dfu.h"
#include "mock_case.h"
#include "mock_usb.h"
#include "mock_power.h"
#include "mock_debug.h"
#include "mock_rtc.h"
#include "mock_config.h"
#include "mock_led.h"
#include "mock_battery.h"
#include "mock_charger.h"
#include "mock_case_charger.h"
#include "mock_current_senses.h"
#include "mock_vreg.h"
#ifdef CHARGER_COMMS_FAKE
#include "mock_fake_earbud.h"
#endif

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

void do_up_arrow(void)
{
    cli_rx(CLI_SOURCE_UART, ASCII_ESC);
    cli_rx(CLI_SOURCE_UART, '[');
    cli_rx(CLI_SOURCE_UART, 'A');
}

void do_down_arrow(void)
{
    cli_rx(CLI_SOURCE_UART, ASCII_ESC);
    cli_rx(CLI_SOURCE_UART, '[');
    cli_rx(CLI_SOURCE_UART, 'B');
}

void do_cmd_adc(void)
{
    uart_tx_Expect(UART_CLI, "a", 1);
    cli_rx(CLI_SOURCE_UART, 'a');

    uart_tx_Expect(UART_CLI, "d", 1);
    cli_rx(CLI_SOURCE_UART, 'd');

    uart_tx_Expect(UART_CLI, "c", 1);
    cli_rx(CLI_SOURCE_UART, 'c');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    adc_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

void do_cmd_gpio(void)
{
    uart_tx_Expect(UART_CLI, "g", 1);
    cli_rx(CLI_SOURCE_UART, 'g');

    uart_tx_Expect(UART_CLI, "p", 1);
    cli_rx(CLI_SOURCE_UART, 'p');

    uart_tx_Expect(UART_CLI, "i", 1);
    cli_rx(CLI_SOURCE_UART, 'i');

    uart_tx_Expect(UART_CLI, "o", 1);
    cli_rx(CLI_SOURCE_UART, 'o');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    gpio_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

void do_cmd_pfn(void)
{
    uart_tx_Expect(UART_CLI, "p", 1);
    cli_rx(CLI_SOURCE_UART, 'p');

    uart_tx_Expect(UART_CLI, "f", 1);
    cli_rx(CLI_SOURCE_UART, 'f');

    uart_tx_Expect(UART_CLI, "n", 1);
    cli_rx(CLI_SOURCE_UART, 'n');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    pfn_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

void do_at_cmd_ccearbud(uint8_t value, CLI_RESULT result)
{
    char val_str[2];

    val_str[0] = value + 0x30;
    val_str[1] = 0;

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "E", 1);
    cli_rx(CLI_SOURCE_UART, 'E');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "R", 1);
    cli_rx(CLI_SOURCE_UART, 'R');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "U", 1);
    cli_rx(CLI_SOURCE_UART, 'U');

    uart_tx_Expect(UART_CLI, "D", 1);
    cli_rx(CLI_SOURCE_UART, 'D');

    uart_tx_Expect(UART_CLI, "=", 1);
    cli_rx(CLI_SOURCE_UART, '=');

    uart_tx_Expect(UART_CLI, val_str, 1);
    cli_rx(CLI_SOURCE_UART, val_str[0]);

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);

    if (result==CLI_OK)
    {
        uart_tx_Expect(UART_CLI, "OK", 2);
    }
    else
    {
        uart_tx_Expect(UART_CLI, "ERROR", 5);
    }

    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    memset(cli_info, 0, sizeof(cli_info));
    memset(cli_auth_level, 0, sizeof(cli_auth_level));

    cli_init();
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* User enters the adc command via the UART.
*/
void test_cli_enter_command(void)
{
    uart_tx_Expect(UART_CLI, "a", 1);
    cli_rx(CLI_SOURCE_UART, 'a');

    uart_tx_Expect(UART_CLI, "d", 1);
    cli_rx(CLI_SOURCE_UART, 'd');

    uart_tx_Expect(UART_CLI, "c", 1);
    cli_rx(CLI_SOURCE_UART, 'c');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    adc_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

#ifdef USB_ENABLED
/*
* User enters the adc command via the USB.
*/
void test_cli_enter_command_usb(void)
{
    usb_tx_Expect("a", 1);
    cli_rx(CLI_SOURCE_USB, 'a');

    usb_tx_Expect("d", 1);
    cli_rx(CLI_SOURCE_USB, 'd');

    usb_tx_Expect("c", 1);
    cli_rx(CLI_SOURCE_USB, 'c');

    usb_tx_Expect("\x0D", 1);
    usb_tx_Expect("\x0A", 1);
    adc_cmd_ExpectAndReturn(CLI_SOURCE_USB, CLI_OK);
    cli_rx(CLI_SOURCE_USB, ASCII_CR);

    cli_rx(CLI_SOURCE_USB, ASCII_LF);
}
#endif

/*
* User enters the adc command via the UART, but makes a mistake half way
* through and corrects it.
*/
void test_cli_enter_corrected_command(void)
{
    /*
    * Nothing typed yet, so BS or DEL shouldn't do anything.
    */
    cli_rx(CLI_SOURCE_UART, ASCII_BS);
    cli_rx(CLI_SOURCE_UART, ASCII_DEL);

    uart_tx_Expect(UART_CLI, "a", 1);
    cli_rx(CLI_SOURCE_UART, 'a');

    uart_tx_Expect(UART_CLI, "f", 1);
    cli_rx(CLI_SOURCE_UART, 'f');

    uart_tx_Expect(UART_CLI, "\x08", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, "\x08", 1);
    cli_rx(CLI_SOURCE_UART, ASCII_BS);

    uart_tx_Expect(UART_CLI, "d", 1);
    cli_rx(CLI_SOURCE_UART, 'd');

    uart_tx_Expect(UART_CLI, "c", 1);
    cli_rx(CLI_SOURCE_UART, 'c');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    adc_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

/*
* Command history.
*/
void test_cli_command_history(void)
{
    char *big_cmd = "adc too big to go in the command history";
    size_t big_cmd_len;
    int n;

    /*
    * No commands entered so far, so up and down shouldn't do anything.
    */
    do_up_arrow();
    do_down_arrow();

    /*
    * Three commands.
    */
    do_cmd_adc();
    do_cmd_gpio();
    do_cmd_pfn();

    /*
    * A long command is entered, which is too big for the command history.
    */
    big_cmd_len = strlen(big_cmd);
    for (n=0; n<big_cmd_len; n++)
    {
        char echo_str[2];

        echo_str[0] = big_cmd[n];
        echo_str[1] = 0;

        uart_tx_Expect(UART_CLI, echo_str, 1);
        cli_rx(CLI_SOURCE_UART, big_cmd[n]);
    }

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    adc_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    /*
    * Start to type another command.
    */
    uart_tx_Expect(UART_CLI, "x", 1);
    cli_rx(CLI_SOURCE_UART, 'x');

    uart_tx_Expect(UART_CLI, "y", 1);
    cli_rx(CLI_SOURCE_UART, 'y');

    /*
    * Press up arrow.
    */
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "pfn", 3);
    do_up_arrow();

    /*
    * Press up arrow.
    */
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "gpio", 4);
    do_up_arrow();

    /*
    * Press up arrow.
    */
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "adc", 3);
    do_up_arrow();

    /*
    * Press up arrow - but nothing happens because we've got to the end of the
    * command history.
    */
    do_up_arrow();

    /*
    * Press down arrow.
    */
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "gpio", 4);
    do_down_arrow();

    /*
    * Press down arrow.
    */
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, " ", 1);
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "pfn", 3);
    do_down_arrow();

    /*
    * Press down arrow - but nothing happens because we've got to the end of the
    * command history.
    */
    do_down_arrow();

    /*
    * Press return and execute the command.
    */
    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    pfn_cmd_ExpectAndReturn(CLI_SOURCE_UART, CLI_OK);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);

    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

/*
* Escape sequence that we ignore (eg right arrow).
*/
void test_cli_disregarded_escape_sequence(void)
{
    /*
    * Right arrow pressed, which we don't care about so do nothing.
    */
    cli_rx(CLI_SOURCE_UART, ASCII_ESC);
    cli_rx(CLI_SOURCE_UART, '[');
    cli_rx(CLI_SOURCE_UART, 'C');

    /*
    * Execute a command to check that the sequence has not caused any trouble.
    */
    do_cmd_adc();
}

/*
* Invalid characters entered.
*/
void test_cli_invalid_characters(void)
{
    cli_rx(CLI_SOURCE_UART, 0x02);
    cli_rx(CLI_SOURCE_UART, 0x80);

    /*
    * Execute a command to check that the invalid characters have not caused
    * any trouble.
    */
    do_cmd_adc();
}

/*
* Check that we discard an unexpected S-record.
*/
void test_cli_stray_s_record(void)
{
    int n;

    /*
    * S-record received and ignored.
    */
    cli_rx(CLI_SOURCE_UART, 'S');
    cli_rx(CLI_SOURCE_UART, '3');
    for (n=0; n<200; n++)
    {
        cli_rx(CLI_SOURCE_UART, n & 0x37);
    }
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
    cli_rx(CLI_SOURCE_UART, ASCII_LF);

    /*
    * Execute a command to check that the discarded S-record has not caused
    * any trouble.
    */
    do_cmd_pfn();
}

/*
* AT+REBOOT
*/
void test_cli_reboot_cmd(void)
{
    uart_tx_Expect(UART_CLI, "a", 1);
    cli_rx(CLI_SOURCE_UART, 'a');

    uart_tx_Expect(UART_CLI, "t", 1);
    cli_rx(CLI_SOURCE_UART, 't');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "r", 1);
    cli_rx(CLI_SOURCE_UART, 'r');

    uart_tx_Expect(UART_CLI, "e", 1);
    cli_rx(CLI_SOURCE_UART, 'e');

    uart_tx_Expect(UART_CLI, "b", 1);
    cli_rx(CLI_SOURCE_UART, 'b');

    uart_tx_Expect(UART_CLI, "o", 1);
    cli_rx(CLI_SOURCE_UART, 'o');

    uart_tx_Expect(UART_CLI, "o", 1);
    cli_rx(CLI_SOURCE_UART, 'o');

    uart_tx_Expect(UART_CLI, "t", 1);
    cli_rx(CLI_SOURCE_UART, 't');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    uart_tx_Expect(UART_CLI, "OK", 2);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

/*
* AT+ID?
*/
void test_cli_id_cmd(void)
{
    uart_tx_Expect(UART_CLI, "a", 1);
    cli_rx(CLI_SOURCE_UART, 'a');

    uart_tx_Expect(UART_CLI, "t", 1);
    cli_rx(CLI_SOURCE_UART, 't');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "i", 1);
    cli_rx(CLI_SOURCE_UART, 'i');

    uart_tx_Expect(UART_CLI, "d", 1);
    cli_rx(CLI_SOURCE_UART, 'd');

    uart_tx_Expect(UART_CLI, "?", 1);
    cli_rx(CLI_SOURCE_UART, '?');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    config_get_board_id_ExpectAndReturn(3);
#ifdef VARIANT_CB
    uart_tx_Expect(UART_CLI, "\"CB\",3,0,\"UNRELEASED\"", 21);
#else
    uart_tx_Expect(UART_CLI, "\"ST2\",3,0,\"UNRELEASED\"", 22);
#endif
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    uart_tx_Expect(UART_CLI, "OK", 2);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
    cli_rx(CLI_SOURCE_UART, ASCII_LF);
}

/*
* AT+CCEARBUD.
*/
void test_cli_at_ccearbud_set(void)
{
    do_at_cmd_ccearbud(0, CLI_OK);
    do_at_cmd_ccearbud(1, CLI_OK);
    do_at_cmd_ccearbud(2, CLI_OK);
    do_at_cmd_ccearbud(3, CLI_OK);
    do_at_cmd_ccearbud(4, CLI_ERROR);
}

/*
* User enters AT+CCX=1 via the UART.
*/
void test_cli_enter_at_set_command(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "X", 1);
    cli_rx(CLI_SOURCE_UART, 'X');

    uart_tx_Expect(UART_CLI, "=", 1);
    cli_rx(CLI_SOURCE_UART, '=');

    uart_tx_Expect(UART_CLI, "1", 1);
    cli_rx(CLI_SOURCE_UART, '1');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    uart_tx_Expect(UART_CLI, "ERROR", 5);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters AT+CCX? via the UART.
*/
void test_cli_enter_at_query_command(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "X", 1);
    cli_rx(CLI_SOURCE_UART, 'X');

    uart_tx_Expect(UART_CLI, "?", 1);
    cli_rx(CLI_SOURCE_UART, '?');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    uart_tx_Expect(UART_CLI, "ERROR", 5);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters AT+CCX=? via the UART.
*/
void test_cli_enter_at_test_command(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "X", 1);
    cli_rx(CLI_SOURCE_UART, 'X');

    uart_tx_Expect(UART_CLI, "=", 1);
    cli_rx(CLI_SOURCE_UART, '=');

    uart_tx_Expect(UART_CLI, "?", 1);
    cli_rx(CLI_SOURCE_UART, '?');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    uart_tx_Expect(UART_CLI, "ERROR", 5);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters AT+ABC and it is treated as being for the case due to the
* default CCEARBUD setting.
*/
void test_cli_enter_at_command_for_case(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    uart_tx_Expect(UART_CLI, "ERROR", 5);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters AT+ABC and it is treated as being for the left earbud due to the
* CCEARBUD setting.
*/
void test_cli_enter_at_command_for_left_earbud(void)
{
    do_at_cmd_ccearbud(CCEARBUD_LEFT, CLI_OK);

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_LEFT, "+ABC", true);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters AT+ABC and it is treated as being for the right earbud due to the
* CCEARBUD setting.
*/
void test_cli_enter_at_command_for_right_earbud(void)
{
    do_at_cmd_ccearbud(CCEARBUD_RIGHT, CLI_OK);

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_RIGHT, "+ABC", true);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters AT+ABC and it is treated as being for both earbuds due to the
* CCEARBUD setting.
*/
void test_cli_enter_at_command_for_both_earbuds(void)
{
    do_at_cmd_ccearbud(CCEARBUD_BOTH, CLI_OK);

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_LEFT, "+ABC", true);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_RIGHT, "+ABC", true);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters ATL+ABC via the UART.
*/
void test_cli_enter_atl_command(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "L", 1);
    cli_rx(CLI_SOURCE_UART, 'L');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_LEFT, "+ABC", true);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters ATR+ABC via the UART.
*/
void test_cli_enter_atr_command(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "R", 1);
    cli_rx(CLI_SOURCE_UART, 'R');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_RIGHT, "+ABC", true);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* User enters ATB+ABC via the UART.
*/
void test_cli_enter_atb_command(void)
{
    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "T", 1);
    cli_rx(CLI_SOURCE_UART, 'T');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "+", 1);
    cli_rx(CLI_SOURCE_UART, '+');

    uart_tx_Expect(UART_CLI, "A", 1);
    cli_rx(CLI_SOURCE_UART, 'A');

    uart_tx_Expect(UART_CLI, "B", 1);
    cli_rx(CLI_SOURCE_UART, 'B');

    uart_tx_Expect(UART_CLI, "C", 1);
    cli_rx(CLI_SOURCE_UART, 'C');

    uart_tx_Expect(UART_CLI, "\x0D", 1);
    uart_tx_Expect(UART_CLI, "\x0A", 1);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_LEFT, "+ABC", true);
    ccp_at_command_ExpectAndReturn(CLI_SOURCE_UART, WIRE_DEST_RIGHT, "+ABC", true);
    cli_rx(CLI_SOURCE_UART, ASCII_CR);
}

/*
* Test cli_tx_hex()
*/
void test_cli_tx_hex(void)
{
    uart_tx_Expect(UART_CLI, "heading:", 8);
    uart_tx_Expect(UART_CLI, " 12", 3);
    uart_tx_Expect(UART_CLI, " 34", 3);
    uart_tx_Expect(UART_CLI, " 56", 3);
    uart_tx_Expect(UART_CLI, " 78", 3);
    uart_tx_Expect(UART_CLI, "", 0);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_tx_hex(CLI_SOURCE_UART, "heading", "\x12\x34\x56\x78", 4);
}

/*
* Test cli_tx()
*/
void test_cli_tx(void)
{
    /*
    * TX on UART without CRLF.
    */
    uart_tx_Expect(UART_CLI, "abc", 3);
    cli_tx(CLI_SOURCE_UART, false, "abc");

    /*
    * TX on UART with CRLF.
    */
    uart_tx_Expect(UART_CLI, "def", 3);
    uart_tx_Expect(UART_CLI, "\x0D\x0A", 2);
    cli_tx(CLI_SOURCE_UART, true, "def");

#ifdef USB_ENABLED
    /*
    * TX on USB without CRLF.
    */
    usb_tx_Expect("ghi", 3);
    cli_tx(CLI_SOURCE_USB, false, "ghi");

    /*
    * TX on USB with CRLF.
    */
    usb_tx_Expect("jkl", 3);
    usb_tx_Expect("\x0D\x0A", 2);
    cli_tx(CLI_SOURCE_USB, true, "jkl");
#endif

    /*
    * Broadcast message.
    */
    uart_tx_Expect(UART_CLI, "mno", 3);
#ifdef USB_ENABLED
    usb_tx_Expect("mno", 3);
#endif
    cli_tx(CLI_BROADCAST, false, "mno");

    /*
    * Disable broadcast messages to the UART, as might happen when in test
    * mode.
    */
    cli_broadcast_disable(CLI_SOURCE_UART);

    /*
    * Broadcast message not sent to UART.
    */
#ifdef USB_ENABLED
    usb_tx_Expect("pqr", 3);
#endif
    cli_tx(CLI_BROADCAST, false, "pqr");

    uart_tx_Expect(UART_CLI, "stu", 3);
    cli_tx(CLI_SOURCE_UART, false, "stu");

    /*
    * Enable broadcast messages again.
    */
    cli_broadcast_enable(CLI_SOURCE_UART);

    /*
    * Check that broadcast message is sent to UART.
    */
    uart_tx_Expect(UART_CLI, "vwx", 3);
#ifdef USB_ENABLED
    usb_tx_Expect("vwx", 3);
#endif
    cli_tx(CLI_BROADCAST, false, "vwx");
}
