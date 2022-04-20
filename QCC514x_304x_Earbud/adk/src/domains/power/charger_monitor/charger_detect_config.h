/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Configuration related definitions for charger detection.
*/

#ifndef CHARGER_DETECT_CONFIG_H_
#define CHARGER_DETECT_CONFIG_H_

#include "charger_data.h"

/*! Return charger config for detected charger */
const charger_config_t *ChargerDetect_GetConfig(MessageChargerDetected *msg);
/*! Return charger config for new charger connection state */
const charger_config_t *ChargerDetect_GetConnectedConfig(bool charger_connected);

#endif /* CHARGER_DETECT_CONFIG_H_ */
