/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_region_config.c
\brief     operating region configuration tables and state handlers

    This file contains battery operating configuration tables
*/

#include "earbud_region_config.h"
#include "battery_region.h"

/*! \brief charge mode config table*/
const charge_region_t earbud_charge_mode_config_table[] =
{
    {0,       Vfloat,   4350, 100, -40, 0,  1, NORMAL_REGION, 0},
    {0,       Vfloat,   4350, 100,  0,  45, 1, NORMAL_REGION, 0},
    {0,       Vfloat,   4350, 100,  45, 85, 1, NORMAL_REGION, 0},
    {0,         3600, Vfloat, 50,  -40, 0,  1, NORMAL_REGION, 0},
#ifdef BATTERY_REGION_HALF_CHARGE
    {FAST/2,    3600, Vfloat, 50,  minHalfFastRegionTemp, minFastRegionTemp,
                                            1, NORMAL_REGION, CHARGING_TIMER_TIMEOUT},
#endif
    {FAST,      3600, Vfloat, 50,  minFastRegionTemp, maxFastRegionTemp,
                                            1, NORMAL_REGION, CHARGING_TIMER_TIMEOUT},
    {0,         3600, Vfloat, 50,   45, 85, 1, NORMAL_REGION, 0},
    {0,        Vcrit,   3600, 50,  -40, 0,  1, NORMAL_REGION, 0},
#ifdef BATTERY_REGION_HALF_CHARGE
    {FAST/2,    Vcrit,   3600, 50, minHalfFastRegionTemp, minFastRegionTemp,
                                            1, NORMAL_REGION, CHARGING_TIMER_TIMEOUT},
#endif
    {FAST,     Vcrit,   3600, 50,  minFastRegionTemp, maxFastRegionTemp,
                                            1, NORMAL_REGION, CHARGING_TIMER_TIMEOUT},
    {0,        Vcrit,   3600, 50,   45, 85, 1, NORMAL_REGION, 0},
    {0,        Vfast,  Vcrit, 50,  -40, 0,  1, CRITICAL_REGION, 0},
#ifdef BATTERY_REGION_HALF_CHARGE
    {FAST/2,   Vfast,  Vcrit, 50,  minHalfFastRegionTemp, minFastRegionTemp,
                                            1, CRITICAL_REGION, 0},
#endif
    {FAST,     Vfast,  Vcrit, 50,  minFastRegionTemp, maxFastRegionTemp,
                                            1, CRITICAL_REGION, 0},
    {0,        Vfast,  Vcrit, 50,   45, 85, 1, CRITICAL_REGION, 0},    
    {0,         Vpre,  Vfast, 50,  -40, 0,  1, CRITICAL_REGION, 0},
    {PRE,       Vpre,  Vfast, 50,    0, 45, 1, CRITICAL_REGION, 0},
    {0,         Vpre,  Vfast, 50,   45, 85, 1, CRITICAL_REGION, 0},
    {0,            0,   Vpre, 50,  -40, 0,  1, CRITICAL_REGION, 0},
    {TRICKLE,      0,   Vpre, 50,    0, 45, 1, CRITICAL_REGION, 0},
    {0,            0,   Vpre, 50,   45, 85, 1, CRITICAL_REGION, 0},
};

/*! \brief discharge mode config table*/
const charge_region_t earbud_discharge_mode_config_table[] =
{
    {0, 4200, 4350, 100, -40, -20, 1, NORMAL_REGION, 0},
    {0, 4200, 4350, 100, -20,  60, 1, NORMAL_REGION, 0},
    {0, 4200, 4350, 100,  60,  85, 1, NORMAL_REGION, 0},      
    {0, 3300, 4200, 50,  -40, -20, 1, NORMAL_REGION, 0},
    {0, 3300, 4200, 50,  -20,  60, 1, NORMAL_REGION, 0},
    {0, 3300, 4200, 50,   60,  85, 1, NORMAL_REGION, 0},
    {0, 3000, 3300, 50,  -40, -20, 1, CRITICAL_REGION, 0},
    {0, 3000, 3300, 50,  -20,  60, 1, CRITICAL_REGION, 0},
    {0, 3000, 3300, 50,   60,  85, 1, CRITICAL_REGION, 0},
    {0,    0, 3000, 50,  -40, -20, 1, SAFETY_REGION, 0},
    {0,    0, 3000, 50,  -20,  60, 1, SAFETY_REGION, 0},
    {0,    0, 3000, 50,   60,  85, 1, SAFETY_REGION, 0},
};

/*! \brief battery region component various state handlers*/
const battery_region_handlers_t earbud_region_handlers =
{
    {NULL},
    {NULL},
    {NULL}
};

const charge_region_t* EarbudRegion_GetChargeModeConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(earbud_charge_mode_config_table);
    return earbud_charge_mode_config_table;
}

const charge_region_t* EarbudRegion_GetDischargeModeConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(earbud_discharge_mode_config_table);
    return earbud_discharge_mode_config_table;
}

const battery_region_handlers_t* EarbudRegion_GetRegionHandlers(void)
{
    return &earbud_region_handlers;
}

