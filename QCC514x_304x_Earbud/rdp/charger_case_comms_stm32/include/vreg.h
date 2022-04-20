/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Regulator.
*/

#ifndef VREG_H_
#define VREG_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdbool.h>
#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    VREG_REASON_OFF_LOW_BATTERY,
    VREG_REASON_OFF_COMMS,
    VREG_REASON_OFF_SHIPPING_MODE,
    VREG_REASON_OFF_OVERCURRENT,
    VREG_REASON_OFF_COMMAND,
    VREG_REASON_OFF_COUNT
} VREG_REASON_OFF;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void vreg_init(void);
void vreg_enable(void);
void vreg_disable(void);
void vreg_pwm(void);
void vreg_pfm(void);
bool vreg_is_enabled(void);
CLI_RESULT ats_regulator(uint8_t cmd_source);
void vreg_off_set_reason(VREG_REASON_OFF reason);
void vreg_off_clear_reason(VREG_REASON_OFF reason);

#endif /* VREG_H_ */
