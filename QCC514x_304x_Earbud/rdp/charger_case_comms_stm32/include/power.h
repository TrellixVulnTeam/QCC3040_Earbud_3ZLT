/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power modes
*/

#ifndef POWER_H_
#define POWER_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* Reasons to run.
*/
#define POWER_RUN_UART_RX       0x00000001
#define POWER_RUN_UART_TX       0x00000002
#define POWER_RUN_CHARGER_COMMS 0x00000004
#define POWER_RUN_DEBUG         0x00000008
#define POWER_RUN_CASE_EVENT    0x00000010
#define POWER_RUN_STATUS_L      0x00000020
#define POWER_RUN_STATUS_R      0x00000040
#define POWER_RUN_WATCHDOG      0x00000080
#define POWER_RUN_DFU           0x00000100
#define POWER_RUN_BROADCAST     0x00000200
#define POWER_RUN_USB_RX        0x00000400
#define POWER_RUN_USB_TX        0x00000800
#define POWER_RUN_LED           0x00001000
#define POWER_RUN_FORCE_ON      0x00002000
#define POWER_RUN_CHG_CONNECTED 0x00004000
#define POWER_RUN_SHIP          0x00008000
#ifdef SCHEME_B
#define POWER_RUN_UART_CC_RX    0x00010000
#define POWER_RUN_UART_CC_TX    0x00020000
#define POWER_RUN_UART_EB_RX    0x00040000
#define POWER_RUN_UART_EB_TX    0x00080000
#endif
#define POWER_RUN_BATTERY_READ  0x00100000
#define POWER_RUN_CURRENT_MON   0x00200000

/*
* Reasons to go to standby.
*/
#define POWER_STANDBY_COMMAND       0x01
#define POWER_STANDBY_SHIPPING_MODE 0x02
#define POWER_STANDBY_LOW_BATTERY   0x04

/*
* Reasons to go to stop.
*/
#define POWER_STOP_COMMAND       0x01
#define POWER_STOP_RUN_TIME      0x02
#define POWER_STOP_FULLY_CHARGED 0x04
#define POWER_STOP_CASE_EMPTY    0x08

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void power_enter_standby(void);
void power_enter_stop_after_reset(void);
void power_periodic(void);
void power_set_run_reason(uint32_t reason);
void power_clear_run_reason(uint32_t reason);
void power_set_standby_reason(uint8_t reason);
void power_clear_standby_reason(uint8_t reason);
void power_set_stop_reason(uint8_t reason);
void power_clear_stop_reason(uint8_t reason);
CLI_RESULT power_cmd_standby(uint8_t cmd_source);
CLI_RESULT power_cmd(uint8_t cmd_source);
CLI_RESULT ats_power(uint8_t cmd_source);

#endif /* POWER_H_ */
