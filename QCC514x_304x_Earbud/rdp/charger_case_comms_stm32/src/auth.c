/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Authorisation
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "timer.h"
#include "auth.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define AUTH_START_SIZE 32
#define AUTH_RESP_SIZE 32

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

char auth_start_str[CLI_NO_OF_SOURCES][AUTH_START_SIZE + 1] = {0};

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Check the AT+AUTHRESP response.
*/
static bool auth_check_response(
    uint8_t cmd_source __attribute__((unused)),
    char *authresp_str)
{
    char correct_resp[AUTH_RESP_SIZE + 1];

    /*
    * Work out what the correct response is. I don't know how to do this
    * so for now it's just 123.
    */
    strcpy(correct_resp, "123");

    return (strcasecmp(authresp_str, correct_resp)) ? false : true;
}

/*
* Handle the AT+AUTHSTART command.
*/
CLI_RESULT at_authstart_set_cmd(uint8_t cmd_source)
{
    uint8_t n;

    /*
    * Set the authorisation level to allow the +AUTHRESP command.
    */
    cli_set_auth_level(cmd_source, 1);

    /*
    * Work out a random string of 32 hex digits. The M0 doesn't have a
    * random number generator, so just use the library functions seeded from
    * a timer counter.
    */
    srand((unsigned int)timer_seed_value());
    for (n=0; n<AUTH_START_SIZE; n++)
    {
        uint8_t r = rand() & 0xF;
        auth_start_str[cmd_source][n] = (r > 9) ? r + 'a' - 10 : r + '0';
    }

    /*
    * Send the response.
    */
    PRINTF("+AUTHSTART:%s", auth_start_str[cmd_source]);

    return CLI_OK;
}

/*
* Handle the AT+AUTHRESP command.
*/
CLI_RESULT at_authresp_set_cmd(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;
    char *tok = cli_get_next_token();

    if (tok && auth_check_response(cmd_source, tok))
    {
        cli_set_auth_level(cmd_source, 2);
        ret = CLI_OK;
    }
    else
    {
        cli_set_auth_level(cmd_source, 0);
    }

    return ret;
}

/*
* Handle the AT+AUTHDISABLE command.
*/
CLI_RESULT at_authdisable_set_cmd(uint8_t cmd_source)
{
    cli_set_auth_level(cmd_source, 0);
    return CLI_OK;
}
