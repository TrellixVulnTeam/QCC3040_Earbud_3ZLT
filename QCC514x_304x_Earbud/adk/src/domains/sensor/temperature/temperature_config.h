/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       temperature_config.h
\brief      Configuration related definitions for the temperature module.
*/

#ifndef TEMPERATURE_CONFIG_H_
#define TEMPERATURE_CONFIG_H_


/*! The temperature sensor measurement interval in milli-seconds. */
#define appConfigTemperatureReadingPeriodMs() D_SEC(10)
#define appConfigTemperatureMedianFilterWindow() 5
/* smoothing factor stored as multiple of 100 */
#define appConfigTemperatureSmoothingWeight() 50

#endif /* TEMPERATURE_CONFIG_H_ */
