/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Battery monitoring
*/

#ifndef BATTERY_MONITOR_H_
#define BATTERY_MONITOR_H_

#include "domain_message.h"

#include <marshal.h>

/*! Battery level updates messages. The message a client receives depends upon
    the batteryRegistrationForm::representation set when registering by calling
    #appBatteryRegister.  */
enum battery_messages
{
    /*! Message signalling the battery module initialisation is complete */
    MESSAGE_BATTERY_INIT_CFM = BATTERY_APP_MESSAGE_BASE,
    /*! Message signalling the battery voltage has changed. */
    MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE,

    /*! This must be the final message */
    BATTERY_APP_MESSAGE_END
};

/*! Options for representing the battery voltage */
enum battery_level_representation
{
    /*! As a voltage */
    battery_level_repres_voltage
};

/*! Message #MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE content. */
typedef struct
{
    /*! The updated battery voltage in milli-volts. */
    uint16 voltage_mv;
} MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T;

/*! Battery client registration form */
typedef struct
{
    /*! The task that will receive battery status messages */
    Task task;
    /*! The representation method requested by the client */
    enum battery_level_representation representation;

    /*! The reporting hysteresis
    */
    uint16 hysteresis;

} batteryRegistrationForm;

/*! Structure used internally to the battery module to store per-client state */
typedef struct battery_registered_client_item
{
    /*! The next client in the list */
    struct battery_registered_client_item *next;
    /*! The client's registration information */
    batteryRegistrationForm form;
    /*! The last battery voltage sent to the client */
    
    /*! As a voltage */
    uint16 voltage;
    
} batteryRegisteredClient;

#ifdef HAVE_NO_BATTERY

/* make sure these return TRUE and dereference the parameter */
#define appBatteryInit(init_task) (init_task == init_task)
#define appBatteryRegister(form) (form == form)
#define appBatteryUnregister(task)
#define appBatteryGetVoltageAverage() (0)
#define BatteryMonitor_IsGood() (FALSE)
#define appBatteryGetVoltageInstantaneous() (0)

#else

/*! Start monitoring the battery voltage */
bool appBatteryInit(Task init_task);

/*! @brief Register to receive battery change notifications.

    @note The first notification after registering will only be
    sent when sufficient battery readings have been taken after
    power on to ensure that the notification represents a stable
    value.

    @param form The client's registration form.

    @return TRUE on successful registration, otherwise FALSE.
*/
bool appBatteryRegister(batteryRegistrationForm *form);

/*! @brief Unregister a task from receiving battery change notifications.
    @param task The client task to unregister.
    Silently ignores unregister requests for a task not previously registered
*/
void appBatteryUnregister(Task task);

/*! @brief Read the averaged battery voltage in mV.
    @return The battery voltage. */
uint16 appBatteryGetVoltageAverage(void);

/*! @brief Check if battery voltage is good enough to power device.
    @return TRUE if battery voltage is good, otherwise FALSE if battery is low. */
bool BatteryMonitor_IsGood(void);

/*! @brief Read the filtered battery voltage in mV.
    @return The battery voltage. */
uint16 appBatteryGetVoltageInstantaneous(void);

/*! \brief Override the battery level for test purposes.

    After calling this function actual battery measurements will be ignored,
    and voltage value will be used instead.

    \param voltage Voltage level to be used.
*/
void appBatteryTestSetFakeVoltage(uint16 voltage);

/*! @brief Unset test battery value to 0 and start periodic monitoring.
 */
void appBatteryTestUnsetFakeVoltage(void);

/*! \brief Inject new battery level for test purposes.

    After calling this function actual battery measurements will be ignored,
    and voltage value will be used instead.

    \param voltage Voltage level to be used.
*/
void appBatteryTestInjectFakeLevel(uint16 voltage);

/*! @brief Unset test battery value to 0 and start periodic monitoring.
 */
void appBatteryTestResumeAdcMeasurements(void);

#endif /* !HAVE_NO_BATTERY */

#endif /* BATTERY_MONITOR_H_ */
