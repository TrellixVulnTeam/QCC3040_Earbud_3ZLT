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

#include "main.h"
#include "cli.h"
#include "cli_txf.h"

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void cli_txf(uint8_t cmd_source, bool crlf, char *format, ...)
{
    char buf[80];
    va_list ap;
    int v;

    va_start(ap, format);
    v = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if (v)
    {
        cli_tx(cmd_source, crlf, buf);
    }
}
