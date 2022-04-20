/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage the insertion and removal of chargers and the battery charger
*/

#ifndef CASE_CHARGER_H_
#define CASE_CHARGER_H_

#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*
* These should go in order of priority, lowest first. If any changes are made,
* the charger_reason_on[] array must be updated.
*/
typedef enum
{
    CHARGER_ON_CONNECTED,
    CHARGER_OFF_BATTERY_READ,
    CHARGER_ON_COMMAND,
    CHARGER_OFF_COMMAND,
    CHARGER_OFF_TEMPERATURE,
    CHARGER_NO_OF_REASONS
}
CHARGER_REASON;

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * \brief Set a reason for the charger to be on or off.
 * \param reason
 */
void charger_set_reason(CHARGER_REASON reason);

/**
 * \brief Clear a reason for the charger to be on or off.
 * \param reason
 */
void charger_clear_reason(CHARGER_REASON reason);

/**
 * \brief Indicate that a power source has been attached which will start the
 * detection sequence and set the battery charger up.
 */
void case_charger_connected(void);

/**
 * \brief Indicate that the power source has been removed which will clean up
 * the detection state and battery charger.
 */
void case_charger_disconnected(void);

/**
 * \brief The periodic function that drives the detection and power source
 * monitoring.
 */
void case_charger_periodic(void);

/**
 * \brief Returns true if the temperature is too high or too low, false
 * otherwise.
 */
bool case_charger_temperature_fault(void);

/**
 * \brief Returns true if the charger type has been fully detected
 */
bool case_charger_is_resolved(void);

/**
 * \brief AT command handling for charger control
 * 
 * \param cmd_source The source that sent this AT command
 *
 * \return Whether this AT command was successful or not
 */
CLI_RESULT ats_charger(uint8_t cmd_source);

/**
 * \brief AT command handling for requesting the charger state
 * 
 * \param cmd_source The source that sent this AT command
 *
 * \return Whether this AT command was successful or not
 */
CLI_RESULT atq_charger(uint8_t cmd_source);

#endif /* CASE_CHARGER_H_ */
