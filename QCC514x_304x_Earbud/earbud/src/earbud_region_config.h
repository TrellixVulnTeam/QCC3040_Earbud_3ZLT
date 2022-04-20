/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_region_config.h
\brief     Application specific battery operating region configuration
*/

#ifndef EARBUD_REGION_CONFIG_H_
#define EARBUD_REGION_CONFIG_H_

#include "battery_region.h"
#include "charger_monitor_config.h"

#define TRICKLE appConfigChargerTrickleCurrent()
#define PRE     appConfigChargerPreCurrent()
#define FAST    appConfigChargerFastCurrent()

#define Vpre   2100
#define Vcrit  appConfigChargerCriticalThresholdVoltage()
#define Vfast  appConfigChargerPreFastThresholdVoltage()
#define Vfloat appConfigChargerTerminationVoltage()

#ifdef BATTERY_ZJ1454
/* The QCC5141 and QCC5151 RDPs have battery charger ZJ1454
 * which specifies 2 charging regions. Below 15deg C, the max charge
 * is 1C, and below 45deg C we can use the full 2C rate */
#define BATTERY_REGION_HALF_CHARGE
#define minHalfFastRegionTemp (0)
#define minFastRegionTemp (15)
#define maxFastRegionTemp (45)
#else
#define minFastRegionTemp (0)
#define maxFastRegionTemp (45)
#endif

/* Enable short timeouts for charger/battery platform testing */
#ifdef CF133_BATT
#define CHARGING_TIMER_TIMEOUT 15
#else
#define CHARGING_TIMER_TIMEOUT 509
#endif

/*! \brief Return the charge mode region configuration table for the earbud application.

    The configuration table can be passed directly to the battery region component in
    domains.

    \param table_length - used to return the number of rows in the config table.

    \return Application specific battery operating region configuration
*/
const charge_region_t* EarbudRegion_GetChargeModeConfigTable(unsigned* table_length);

/*! \brief Return the discharge mode region configuration table for the earbud application.

    The configuration table can be passed directly to the battery region component in
    domains.

    \param table_length - used to return the number of rows in the config table.

    \return Application specific battery operating region configuration
*/
const charge_region_t* EarbudRegion_GetDischargeModeConfigTable(unsigned* table_length);

/*! \brief Return battery operating region state handler structure for the earbud application.

    The configuration table can be passed directly to the battery region component in
    domains.

    \return Application specific battery operating region handler functions
*/
const battery_region_handlers_t* EarbudRegion_GetRegionHandlers(void);

#endif /* EARBUD_REGION_CONFIG_H_ */

