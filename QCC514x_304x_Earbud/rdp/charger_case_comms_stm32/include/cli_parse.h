/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Command Line Interface
*/

#ifndef CLI_PARSE_H_
#define CLI_PARSE_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdbool.h>
#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINTIONS ------------------------------------
-----------------------------------------------------------------------------*/

/*
* CLI_SEPARATOR: Defines the separation character(s) that we use when parsing
* the commands using strtok.
*/
#define CLI_SEPARATOR " =,?"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT cli_process_cmd(
    const CLI_COMMAND *cmd_table, uint8_t cmd_source, char *str);

CLI_RESULT cli_process_sub_cmd(const CLI_COMMAND *cmd_table, uint8_t cmd_source);

char *cli_get_next_token(void);
bool cli_get_next_parameter(long int *param, int base);
bool cli_get_earbud(uint8_t *earbud);
bool cli_get_hex_data(uint8_t *data, uint8_t *data_len, uint8_t max_len);

#endif /* CLI_PARSE_H_ */
