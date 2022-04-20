/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Code for handling the use of current senses for the pogo pins supplying
            each earbud.
*/

#ifndef CURRENT_SENSES_H_
#define CURRENT_SENSES_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdbool.h>
#include "cli_parse.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/**
 * Switch on the current sense due to a command to read the current sense
 */
#define CURRENT_SENSE_AMP_COMMAND 0x01

/**
 * Switch on current senses due to VCHG charger comms
 */
#define CURRENT_SENSE_AMP_COMMS   0x02

/**
 * Switch on current senses to read system load for battery reading
 * calculation
 */
#define CURRENT_SENSE_AMP_BATTERY 0x04

/**
 * Switch on current senses to monitor system load.
 */
#define CURRENT_SENSE_AMP_MONITORING 0x08
 

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/**
 * \brief Initialise the current sense module
 */
void current_senses_init(void);

/**
 * \brief Return whether the current senses are present and returning a sensible value.
 * \return True if current senses are present, false otherwise.
 */
bool current_senses_are_present(void);

/**
 * \brief AT command handler for AT+SENSE? which returns the ADC values of both left and
 *        right current sense.
 * \param cmd_source The source that sent this AT command.
 * \return If the AT command result.
 */
CLI_RESULT atq_sense(uint8_t cmd_source);

/**
 * \brief Set a reason to switch the current sense amplifier on.
 * \param reason The reason bitmask for why we need the amplifier on
 */
void current_senses_set_sense_amp(uint8_t reason);

/**
 * \brief Clear a reason for the current sense amplifier to be on.
 * \param reason The reason to clear
 */
void current_senses_clear_sense_amp(uint8_t reason);

/**
 * \brief Fetches a pointer to the left current sense ADC
 * \return A pointer to where the left earbud current sense ADC reading
 *         is written to
 */
volatile uint16_t *current_senses_left_adc_value();

/**
 * \brief Fetches a pointer to the right current sense ADC
 * \return A pointer to where the right earbud current sense ADC reading
 *         is written to
 */
volatile uint16_t *current_senses_right_adc_value();

/**
 * \brief Calculates the total load on VBUS via the left and right earbud pogo
 *        pins and returns the value in milliamps.
 * \param left_mA A pointer to where the left earbud load will be written to
 * \param right_mA A pointer to where the right earbud load will be written to
 */
void battery_fetch_load_ma(uint32_t *left_mA, uint32_t *right_mA);

/**
 * \brief Calculates the total load on VBUS and returns the value in milliamps.
 * \return Total load on VBUS in milliamps
 */
uint32_t battery_fetch_total_load_ma(void);

#endif /* CURRENT_SENSES_H_ */
