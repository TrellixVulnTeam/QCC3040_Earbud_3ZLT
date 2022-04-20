/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Module for the earbud application hardware specific configuration
*/

#include "earbud_hardware.h"
#include <pio_common.h>

/*! \brief Allow the app to control the external power supplies. */
void EarbudHardware_SetSensorPowerSupplies(uint16 enable_mask, uint16 enables)
{
#if defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689
    uint16 mask, bank;

     /* To allow control of the LDOs when we are in dormant mode, we set the PIOs
      * as input and pull them high or low. PIOs cannot be driven in dormant. 
      * To avoid glitches, we set the PIO pull direction and strenth first, before
      * setting the PIO as an input.
      */
#if (RDP_PIO_LDO1V8 != PIO_UNUSED)
    {
        if (enable_mask & en_1v8_ldo_mask)
        {
            bool enabled = enables & en_1v8_ldo_mask;
            bank = PioCommonPioBank(RDP_PIO_LDO1V8);
            mask = PioCommonPioMask(RDP_PIO_LDO1V8);
            PioSetMapPins32Bank(bank, mask, mask);         /* Ensure app has control */
            PioSet32Bank(bank, mask, enabled  ? mask : 0); /* pull up or down */
            PioSetStrongBias32Bank(bank, mask, mask);      /* strong pull */
            PioSetDir32Bank(bank, mask, 0);		           /* Set as an input */
        }
    }
#endif
#if (RDP_PIO_LDO3V != PIO_UNUSED)
    {
        if (enable_mask & en_3v0_ldo_mask)
        {
            bool enabled = enables & en_3v0_ldo_mask;
            /* now set 3v0 LDO as an input but pulled high */
            bank = PioCommonPioBank(RDP_PIO_LDO3V);
            mask = PioCommonPioMask(RDP_PIO_LDO3V);
            PioSetMapPins32Bank(bank, mask, mask);
            PioSet32Bank(bank, mask, enabled ? mask : 0); /* pull up or down */
            PioSetStrongBias32Bank(bank, mask, mask);     /* strong pull */
            PioSetDir32Bank(bank, mask, 0);		          /* input */
        }
    }
#endif
#else
UNUSED (enable_mask);
UNUSED (enables);
#endif /* defined HAVE_RDP_HW_YE134 || defined HAVE_RDP_HW_18689 */
}

