/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Authorisation
*/

#ifndef AUTH_H_
#define AUTH_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT at_authstart_set_cmd(uint8_t cmd_source);
CLI_RESULT at_authresp_set_cmd(uint8_t cmd_source);
CLI_RESULT at_authdisable_set_cmd(uint8_t cmd_source);

#endif /* AUTH_H_ */
