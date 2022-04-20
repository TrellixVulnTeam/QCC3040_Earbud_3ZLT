/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger
*/

#ifndef MEMORY_H_
#define MEMORY_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define MEM_RAM_START  0x20000000

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void mem_init(void);
CLI_RESULT mem_cmd(uint8_t cmd_source);
void mem_stack_dump(uint8_t cmd_source);
void mem_cfg_standby_set(bool dw_lid, bool dw_chg);
bool mem_cfg_standby(void);
void mem_cfg_stop_set(bool dw_lid, bool dw_chg);
bool mem_cfg_stop(void);
bool mem_cfg_disable_wake_lid(void);
bool mem_cfg_disable_wake_chg(void);

#endif /* MEMORY_H_ */
