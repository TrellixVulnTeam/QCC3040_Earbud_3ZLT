/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Stub earbud functions used for testing 
*/

#ifndef FAKE_EARBUD_H_
#define FAKE_EARBUD_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "earbud.h"
#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void earbud_rx(uint8_t *buf, uint16_t buf_len);
void earbud_rx_ready(void);
void earbud_rxc(uint8_t data);
CLI_RESULT earbud_cmd(uint8_t cmd_source);

#endif /* FAKE_EARBUD_H_ */
