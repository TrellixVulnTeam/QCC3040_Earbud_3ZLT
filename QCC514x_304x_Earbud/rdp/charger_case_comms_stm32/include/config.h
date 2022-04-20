/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Config
*/

#ifndef CONFIG_H_
#define CONFIG_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void config_init(void);
uint64_t config_get_serial(void);
uint32_t config_get_status_time_closed(void);
uint32_t config_get_status_time_open(void);
bool config_get_shipping_mode(void);
bool config_set_shipping_mode(bool mode);
uint16_t config_get_battery_cutoff_mv(void);
uint32_t config_get_board_id(void);
CLI_RESULT config_cmd(uint8_t cmd_source);
CLI_RESULT ats_config(uint8_t cmd_source);
CLI_RESULT atq_config(uint8_t cmd_source);

#endif /* CONFIG_H_ */
