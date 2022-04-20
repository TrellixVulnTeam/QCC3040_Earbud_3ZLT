/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Common test code relating to command handling.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

uint8_t cli_auth_level[CLI_NO_OF_SOURCES];
static char cmd_buf[100];

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT common_cmd(const CLI_COMMAND *cmd_table, uint8_t source, char *str)
{
    strcpy(cmd_buf, str);

    return cli_process_cmd(
        cmd_table,
        source,
        strtok(cmd_buf, CLI_SEPARATOR));
}

void common_cmd_init(void)
{
    uint8_t n;

    for (n=0; n<CLI_NO_OF_SOURCES; n++)
    {
        cli_auth_level[n] = 2;
    }
}
