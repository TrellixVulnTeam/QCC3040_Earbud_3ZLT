/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_power.h
\brief      Management of battery and charger events and states

*/

#ifndef RAFS_POWER_H
#define RAFS_POWER_H

#include "charger_monitor.h"
#include "battery_region.h"

typedef struct {
    MessageId           charger_event;  /*!< The most recent charger status event */
    battery_region_state_t battery_state;  /*!< The most recent battery state */
} rafs_power_t;

/*!
 * \brief Rafs_UpdateChargerState
 * This function tracks the state of the battery charger.
 * \param id        The charger message ID
 */
void Rafs_UpdateChargerState(MessageId id);

/*!
 * \brief Rafs_UpdateBatteryState
 * This function tracks the state of the battery.
 * \param msg       The battery level update state message.
 */
void Rafs_UpdateBatteryState(MESSAGE_BATTERY_REGION_UPDATE_STATE_T *msg);

/*!
 * \brief Rafs_IsWriteSafe
 * Determine if it is safe to start a new write operation.
 *      - the battery is OK
 *      - or the charger is plugged in and charging
 * \return RAFS_OK if the device is adequately powered or RAFS_LOW_POWER
 */
rafs_errors_t Rafs_IsWriteSafe(void);

#endif /* RAFS_POWER_H */
