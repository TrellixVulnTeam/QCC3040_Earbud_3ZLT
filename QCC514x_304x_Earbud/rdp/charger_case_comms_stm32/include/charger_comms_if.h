/**
 * @file    charger_comms_if.h
 * @brief   Functions required to be implemented for charger_comms to work.
 */

#ifndef __CHARGER_COMMS_IF_H
#define __CHARGER_COMMS_IF_H

/**
 * @brief Drive VCHG regulator to logical high
 * @retval None
 */
void charger_comms_vreg_high(void);

/**
 * @brief Drive VCHG regulator to logical low
 * @retval None
 */
void charger_comms_vreg_low(void);

/**
 * @brief Drive VCHG regulator below the RESET threshold.
 * @retval None
 */
void charger_comms_vreg_reset(void);

#endif /* __CHARGER_COMMS_IF_H */
