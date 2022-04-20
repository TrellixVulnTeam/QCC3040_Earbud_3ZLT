/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Command Line Interface.
*/

#ifndef CLI_H_
#define CLI_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "cli_txf.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINTIONS ------------------------------------
-----------------------------------------------------------------------------*/

#define CLI_BROADCAST 0xFF

#define PUTCHAR(ch) \
    cli_txc(cmd_source, ch);

#define PRINTF_B(...) \
    cli_txf(CLI_BROADCAST, true, __VA_ARGS__);

#define PRINTF_BU(...) \
    cli_txf(CLI_BROADCAST, false, __VA_ARGS__);

#define PRINT_B(x) \
    cli_tx(CLI_BROADCAST, true, x);

#define PRINT_BU(x) \
    cli_tx(CLI_BROADCAST, false, x);

#define PRINTF(...) \
    cli_txf(cmd_source, true, __VA_ARGS__);

#define PRINTF_U(...) \
    cli_txf(cmd_source, false, __VA_ARGS__);

#define PRINT(x) \
    cli_tx(cmd_source, true, x);

#define PRINT_U(x) \
    cli_tx(cmd_source, false, x);

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CLI_SOURCE_UART,
#ifdef USB_ENABLED
    CLI_SOURCE_USB,
#endif
    CLI_NO_OF_SOURCES,
    CLI_SOURCE_NONE
}
CLI_SOURCE;

typedef enum
{
    CLI_OK,
    CLI_ERROR,
    CLI_WAIT
}
CLI_RESULT;

typedef struct
{
    char *cmd;
    CLI_RESULT (*fn)(uint8_t source);
    uint8_t auth_level;
}
CLI_COMMAND;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

extern uint8_t cli_auth_level[CLI_NO_OF_SOURCES];

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void cli_init(void);
void cli_broadcast_enable(uint8_t cmd_source);
void cli_broadcast_disable(uint8_t cmd_source);
void cli_set_auth_level(uint8_t cmd_source, uint8_t level);
void cli_tx(uint8_t cmd_source, bool crlf, char *str);
void cli_rx(uint8_t cmd_source, char ch);
void cli_txc(uint8_t cmd_source, char ch);

void cli_tx_hex(uint8_t cmd_source, char *heading, uint8_t *data, uint8_t len);
void cli_intercept_line(uint8_t cmd_source, void (*fn)(uint8_t, char *));
void cli_uart_rx(uint8_t data);

#endif /* CLI_H_ */
