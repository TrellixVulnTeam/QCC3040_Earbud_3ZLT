/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Battery charging on LD20-11114 using the BQ24230 charger.   
             See https://www.ti.com/lit/ds/symlink/bq24232.pdf
*/

#ifndef CHARGER_H_
#define CHARGER_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stdbool.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * The charging current modes that are supported by BQ24230.
 * They are internally set by the IC with the exception of ILIM which
 * is set by an external resistor.
 */
typedef enum
{
    /** 100 mA. USB100 mode */
    CHARGER_CURRENT_MODE_100MA,

    /** 500 mA. USB500 mode */
    CHARGER_CURRENT_MODE_500MA,

    /** Set by an external resistor from ILIM to VSS */
    CHARGER_CURRENT_MODE_ILIM,
    
    /** Standby (USB suspend mode) */
    CHARGER_CURRENT_MODE_STANDBY

} CHARGER_CURRENT_MODE;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * \brief Initialise the operation of the on-board battery charger
 */
extern void charger_init(void);

/**
 * \brief Enable or disable the battery charger
 * \param enable true to enable the charger, false to disable it.
 */
void charger_enable(bool enable);

/**
 * \brief Set the maximum current that the battery charger will operate at
 * \param mode The current mode to use.
 */
void charger_set_current(CHARGER_CURRENT_MODE mode);

/**
 * \brief Return the current mode that the charger is in.
 * \return The current mode the charger is in.
 */
CHARGER_CURRENT_MODE charger_current_mode(void);

/**
 * \brief Determine if there is a charger connected, i.e VCHG is present
 * \return true if a charger is connected, false otherwise.
 */
bool charger_connected(void);

/**
 * \brief Determine if the charger is currently charging the battery.
 *        There are scenarios where the charger is physically connected but
 *        the battery is full so no charging is occurring.
 * \return true if the battery is being charged, false otherwise.
 */
bool charger_is_charging(void);

#endif /* CHARGER_H_ */
