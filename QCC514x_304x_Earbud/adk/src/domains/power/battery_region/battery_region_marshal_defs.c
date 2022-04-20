/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       battery_region_marshal_defs.c
\brief      Marshal definitions of battery region messages.
*/

/* local includes */
#include "battery_region.h"

/* system includes */
#include <marshal.h>
#include <marshal_common.h>

const marshal_type_descriptor_t marshal_type_descriptor_MESSAGE_BATTERY_REGION_UPDATE_STATE_T =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(sizeof(MESSAGE_BATTERY_REGION_UPDATE));

