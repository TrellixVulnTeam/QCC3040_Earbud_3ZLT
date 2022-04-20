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
#include "earbud.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT cli_process_cmd(
    const CLI_COMMAND *cmd_table,
    uint8_t cmd_source,
    char *str)
{
    uint8_t n;
    CLI_RESULT ret = CLI_ERROR;

    for (n=0; cmd_table[n].cmd; n++)
    {
        /*
        * Is the command authorised at the moment?
        */
        if (cli_auth_level[cmd_source] >= cmd_table[n].auth_level)
        {
            /*
            * Use case-insensitive equivalent of strcmp so that the commands
            * can be case-insensitive.
            */
            if (!strcasecmp(str, cmd_table[n].cmd))
            {
                ret = cmd_table[n].fn(cmd_source);
                break;
            }
        }
    }

    return ret;
}

char *cli_get_next_token(void)
{
    return strtok(NULL, CLI_SEPARATOR);
}

CLI_RESULT cli_process_sub_cmd(
    const CLI_COMMAND *cmd_table,
    uint8_t cmd_source)
{
    char *tok = cli_get_next_token();

    return cli_process_cmd(cmd_table, cmd_source, (tok) ? tok:"");
}

bool cli_get_next_parameter(long int *param, int base)
{
    bool ret = false;
    char *tok = cli_get_next_token();

    if (tok)
    {
        *param = strtol(tok, NULL, base);
        ret = true;
    }

    return ret;
}

bool cli_get_earbud(uint8_t *earbud)
{
    bool ret = false;
    char *tok = cli_get_next_token();

    if (tok)
    {
        if (!strcasecmp(tok, "L"))
        {
            *earbud = EARBUD_LEFT;
            ret = true;
        }
        else if (!strcasecmp(tok, "R"))
        {
            *earbud = EARBUD_RIGHT;
            ret = true;
        }
    }

    return ret;
}

/*
* Decipher a string of hex data.
*/
bool cli_get_hex_data(
    uint8_t *data,
    uint8_t *data_len,
    uint8_t max_len)
{
    uint8_t ctr = 0;
    char *tok = cli_get_next_token();

    if (tok)
    {
        uint8_t n;
        uint8_t len = strlen(tok);
        char h[3];

        for (n=0; n<len; n+=2)
        {
            h[0] = tok[n];
            h[1] = tok[n+1];
            h[2] = 0;

            if (ctr < max_len)
            {
                data[ctr++] = (uint8_t)strtol(h, NULL, 16);
            }
            else
            {
                ctr = 0;
                break;
            }
        }
    }

    *data_len = ctr;
    return (ctr) ? true:false;
}
