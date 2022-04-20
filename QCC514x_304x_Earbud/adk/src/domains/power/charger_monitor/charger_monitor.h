/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Charger monitoring
*/

#ifndef CHARGER_MONITOR_H_
#define CHARGER_MONITOR_H_

#include <charger.h>
#include <usb.h>
#include <task_list.h>

#include <ui.h>
#include <ui_inputs.h>

#include "domain_message.h"

/*! \brief Power ui provider contexts */
typedef enum
{
    context_charger_disabled,
    context_charger_detached,
    context_charger_completed,
    context_charger_low,
    context_charger_fast,
    context_charger_error
} charger_provider_context_t;

/*! \brief Messages which may be sent by the charger module. */
typedef enum
{
    /*! Message to inform client the charger was attached */
    CHARGER_MESSAGE_ATTACHED = CHARGER_MESSAGE_BASE,
    /*! Message to inform client the charger was detached */
    CHARGER_MESSAGE_DETACHED,
    /*! Battery is full and the charger is in standby */
    CHARGER_MESSAGE_COMPLETED,
    /*! Battery is charging, battery voltage is above appConfigBatteryVoltageCritical (3v0) */
    CHARGER_MESSAGE_CHARGING_OK,
    /*! Battery is charging, battery voltage is below appConfigBatteryVoltageCritical (3v0) */
    CHARGER_MESSAGE_CHARGING_LOW,
    /*! Battery charging is disabled */
    CHARGER_MESSAGE_DISABLED,
    /*! Battery charging error occurred */
    CHARGER_MESSAGE_ERROR,

    /*! This must be the final message */
    CHARGER_MESSAGE_END
} chargerMessages;

/*! \brief Reasons the charger is disabled. */
typedef enum
{
    /*! No reason to disable */
    CHARGER_DISABLE_REASON_NONE = 0,
    /*! Timed-out attempting to charge the battery */
    CHARGER_DISABLE_REASON_TIMEOUT = 1,
    /*! Requested by application or another module */
    CHARGER_DISABLE_REASON_REQUEST = 2,
    /*! Error detected */
    CHARGER_DISABLE_REASON_ERROR = 4,
    /*! Maximum allowed current is zero */
    CHARGER_DISABLE_REASON_ZERO_CURRENT = 8,
    /*! No charger connected */
    CHARGER_DISABLE_REASON_NOT_CONNECTED = 16,
    /*! To be used inside charger_monitor module only */
    CHARGER_DISABLE_REASON_INTERNAL = 32,
} ChargerDisableReason;

typedef enum
{
    CHARGER_DISCONNECTED,
    CHARGER_CONNECTED,
    CHARGER_CONNECTED_NO_ERROR,
} chargerConnectionState;

/*! Access charger status
 *
 * Proxy for calling ChargerStatus trap.
 *
 * \return NO_POWER if not connected to a charger, DISABLED_ERROR if charging
 * is disabled or charger status otherwise. */
charger_status Charger_Status(void);

/*! Access attached status
 *
 * Proxy for calling UsbAttachedStatus trap.
 *
 * \return DETACHED if not connected to a charger, UNKNOWN_STATUS if charger
 * is not yet resolved or otherwise type of charger detected */
usb_attached_status Charger_AttachedStatus(void);

/*! Whether charger detection is still pending
 *
 * \return TRUE if charger detection is pending and charger type is not yet
 * resolved. */
bool Charger_AttachedStatusPending(void);


/*! /brief Check if charger is connected.
 *
 * \return TRUE if charger is connected.
 */
bool Charger_IsConnected(void);


/*! \brief Disable all checks and always say that it is ok to power off.
 *
 * \param force_allow_power_off When TRUE it always be ok to power off
*/
void Charger_ForceAllowPowerOff(bool force_allow_power_off);


/*! \brief Stops earbud from going dormant while in charger case.
 *
 * \param disallow_dormant When TRUE earbud will not go dormant in charger case
*/
void Charger_DisallowDormant(bool disallow_dormant);

/*! /brief Check if we know that the system can power off.
 *
 * Power off can only happen when there is no voltage at the
 * VCHG input, so this function checks the charger state to 
 * determine if charger has power.
 *
 * If the power status is not known, the function assumes that we cannot
 * power off. An example would be when charger detection is still pending
 * and the USB status is unknown.
 *
 * \note If charger support has not been included, then this function
 * will return FALSE as the system behaviour is not known/detectable.
 *
 * \returns TRUE if it is certain there is no external power
 */
bool Charger_CanPowerOff(void);

/*! /brief Check if we know that the system can enter dormant.
 *
 * Dormant can only happen even if the charger is connected, but not
 * charging. This function therefore checks the charger state to determine 
 * if charger has power.
 *
 * The presence of power at the charger interface does not prevent the 
 * system entering dormant. 
 * 
 * If the power status is not known, the function assumes that we cannot
 * power off. An example would be when charger detection is still pending
 * and the USB status is unknown.
 *
 * \note If charger support has not been included, then this function
 * will return FALSE as the system behaviour is not known/detectable.
 *
 * \returns TRUE if the charger is not currently charging and optionally 
 * no VCHG voltage is detected.
 */
bool Charger_CanEnterDormant(void);

#ifdef INCLUDE_CHARGER

/*! \brief Initialise the application handling of charger
 *
 * This function should be called during application initialisation.
 * If charger support is not required, the function ensures that
 * the charger is disabled.
*/
bool Charger_Init(Task init_task);


/*! Add reason for the charger to be disabled */
void Charger_DisableReasonAdd(ChargerDisableReason reason);

/*! Clear reason for the charger to be disabled */
void Charger_DisableReasonClear(ChargerDisableReason reason);


/*! \brief Make sure the charger is disabled
 *
 * This function should be called from power off code to make sure that
 * the charger is off.
*/
void Charger_ForceDisable(void);

/*! \brief Restore the charger, if there are no other reasons for the charger
 * to be disabled.
 *
 * This function should only be called after a call to
 * \ref Charger_ForceDisable().
*/
void Charger_RestoreState(void);

/*! /brief Check if charger is enabled.
 *
 * Normally, if not prevented by temperature, battery state or user request,
 * charging is enabled when a charger is attached.
 *
 * \return TRUE if charger is enabled.
 */
bool Charger_IsEnabled(void);

/*! /brief Check if charger is actively charging
*
* Charger might be enabled and not charging, for example because the battery
* is full or due to an error.
*
* \return TRUE if charger is actively charging.
*/
bool Charger_IsCharging(void);


/*! Reconfigure charger current
 *
 * Should be called every time charger current needs to be re-evaluated,
 * for example when charger is detected, battery region changed, etc. */
void Charger_UpdateCurrent(void);

/*! Get currently configured fast charge current
 *
 * \return Configured fast charge current in mA. */
unsigned Charger_GetFastCurrent(void);

/*! Handle charger change
 *
 * Should be called every time charger type or state changes to handle
 * PSU source switch and USB suspend transitions. */
void Charger_HandleChange(void);

/*! Notify charger monitor that charger is connected or detached */
void Charger_UpdateConnected(bool charger_is_connected);


/*! Enable or disable test mode
 *
 * When enabled hardware state transitions are ignored and firmware acts
 * only on the test input */
void Charger_TestModeControl(bool enabled);
/*! Test input: MessageChargerChanged */
void Charger_TestChargerConnected(bool is_connected);
/*! Test input: MessageChargerDetected */
void Charger_TestChargerDetected(usb_attached_status attached_status,
                                       uint16 charger_dp_millivolts,
                                       uint16 charger_dm_millivolts,
                                       usb_type_c_advertisement cc_status);
/*! Test input: MessageChargerStatus */
void Charger_TestChargerStatus(charger_status chg_status);


/*! \brief Register a client to receive status messages from the charger module.
 * \param client_task The task to register.
 *
 * \return TRUE if successfully registered.
 * FALSE if registration not successful of if the charger support is not
 * compiled into the application.
*/
bool Charger_ClientRegister(Task client_task);

/*! \brief Unregister a client.
 *
 * \param client_task The task to unregister.
*/
void Charger_ClientUnregister(Task client_task);

#else

#define Charger_DisableReasonAdd(reason)
#define Charger_DisableReasonClear(reason)
#define Charger_ForceDisable(void)
#define Charger_RestoreState(void)
#define Charger_IsEnabled(void) (FALSE)
#define Charger_IsCharging(void) (FALSE)
#define Charger_ClientRegister(task) (FALSE)
#define Charger_ClientUnregister(task)

#endif /* !INCLUDE_CHARGER */

#endif /* CHARGER_MONITOR_H_ */
