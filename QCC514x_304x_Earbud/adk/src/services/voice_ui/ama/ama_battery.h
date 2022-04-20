/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_battery.h
\brief  File consists of function decalration for Amazon Voice Service's battery handling.
*/
#ifndef AMA_BATTERY_H
#define AMA_BATTERY_H

#include "accessories.pb-c.h"

/*! \brief Update the battery level
    \param[in] uint8 Battery level as a percentage
*/
void AmaBattery_Update(uint8 battery_level);

/*! \brief Initialize the AMA battery module.
*/
bool AmaBattery_Init(void);

/*! \brief Get the AMA battery information.
    \return Pointer to the DeviceBattery strucure containing the AMA battery information.
*/
DeviceBattery *AmaBattery_GetDeviceBattery(void);

#endif

