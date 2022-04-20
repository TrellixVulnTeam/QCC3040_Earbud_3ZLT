/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_hardware.h
\brief      Header file for functions pertinent to earbuds hardware or PCB specifc information
*/

#ifndef EARBUD_HARDWARE_H
#define EARBUD_HARDWARE_H

/*! \brief Allow the app to control the external power supplies.
    The mask is hardware dependent. 
    
    \param enable_mask - conrols which power supplies from the project definitions are to be accessed
    \param enables     - If a bit is set in enables_mask, then a corresponding bit '1' in enables
                         will enable that power supply. A '0' in enables will request the power
                         supply to be disabled.
*/
void EarbudHardware_SetSensorPowerSupplies(uint16 enable_mask, uint16 enables);

typedef enum {
#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
    /* This mask is project dependent. The earbud_hardware.c file can
     * match bits here to actual specific PIOs. Multiple PIOs may
     * be needed to enable individual power supplies; other
     * supplies may be controlled via i2c or SPI.
     */
    en_1v8_ldo_mask = 1 << 0,
    en_3v0_ldo_mask = 1 << 1,
    all_supplies_mask = 3
#else
    all_supplies_mask = 0
#endif
} rdp_power_supply_masks ;

#endif /* EARBUD_HARDWARE_H */
