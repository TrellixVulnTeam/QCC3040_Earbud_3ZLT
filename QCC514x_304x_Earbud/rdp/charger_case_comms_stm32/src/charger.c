/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Battery charging on LD20-11114 using the BQ24230 charger.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include "main.h"
#include "charger.h"
#include "gpio.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * The mode that the charger is currently in.
 * It is in 100mA mode by default through external pull down resistors
 * on the charger IC.
 */
CHARGER_CURRENT_MODE current_mode = CHARGER_CURRENT_MODE_100MA;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef CHARGER_BQ24230

void charger_init(void)
{
}

void charger_enable(bool enable)
{
    if (enable)
    {
        gpio_enable(GPIO_CHG_CE_N);
    }
    else
    {
        gpio_disable(GPIO_CHG_CE_N);
    }
}

void charger_set_current(CHARGER_CURRENT_MODE mode)
{
    if (mode == current_mode)
    {
        return;
    }
    current_mode = mode;

    /* Based on Table 1 from the datasheet */
    switch (current_mode)
    {
        case CHARGER_CURRENT_MODE_100MA:
            gpio_disable(GPIO_CHG_EN2); 
            gpio_disable(GPIO_CHG_EN1); 
            break;

        case CHARGER_CURRENT_MODE_500MA:
            gpio_disable(GPIO_CHG_EN2); 
            gpio_enable(GPIO_CHG_EN1); 
            break;

        case CHARGER_CURRENT_MODE_ILIM:
            gpio_enable(GPIO_CHG_EN2); 
            gpio_disable(GPIO_CHG_EN1); 
            break;
    
        case CHARGER_CURRENT_MODE_STANDBY:
            gpio_enable(GPIO_CHG_EN2); 
            gpio_enable(GPIO_CHG_EN1); 
            break;

        default:
            break;
    }
}

CHARGER_CURRENT_MODE charger_current_mode(void)
{
    return current_mode;
}

bool charger_connected(void)
{
    return gpio_active(GPIO_CHG_SENSE);
}

bool charger_is_charging(void)
{
    /* TODO: the status pin indicator is far more complicated than this.
     * Need to distinguish between 6 different states as described in
     * 8.4.1.6 of the datasheet. */
    return gpio_active(GPIO_CHG_STATUS_N);
}

#else /*CHARGER_BQ24230*/

/** Stub functions for charger with no controls */

void charger_init(void) { }
void charger_enable(bool enable __attribute__((unused))) {}
void charger_set_current(CHARGER_CURRENT_MODE mode __attribute__((unused))) {}

bool charger_is_charging(void)
{
    return charger_connected();
}

bool charger_connected(void)
{
    return gpio_active(GPIO_CHG_SENSE);
}


#endif /*CHARGER_BQ24230*/
