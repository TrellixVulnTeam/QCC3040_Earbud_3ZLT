/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of device test service commands for testing sensors.

            For peripherals the commands can monitor the sensor for a time, allowing
            for user interaction. Battery and temperature are read and returned
            immediately.

            The type of sensor is abstracted behind the interface. The existence of
            a sensor is controlled by build defines, so the software here is 
            conditionalised accordingly. If a particular sensor type is not supported
            then the commands will return ERROR.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_parse.h"

#include <temperature.h>
#include <battery_monitor.h>
#include <acceleration.h>
#include <proximity.h>
#include <touch.h>
#include <hall_effect.h>

#include <logging.h>
#include <stdio.h>


#ifdef INCLUDE_ACCELEROMETER
static void deviceTestServiceCommand_HandleAccelerationMessage(bool moving);
static void deviceTestServiceCommand_AccelerationTestStart(Task command_id,
                                                bool stop_on_first, unsigned timeout);
static void deviceTestServiceCommand_AccelerationTestEnded(void);

/*! The base part of the AT command response */
#define ACCELEROMETER_RESPONSE "+ACCELEROMETER:"

#define ACCELEROMETER_RESPONSE_VARIABLE_FMT "%d"

/*! Worst case variable length portion */
#define ACCELEROMETER_RESPONSE_VARIABLE_EXAMPLE "1"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_ACCELEROMETER_RESPONSE_LEN (  sizeof(ACCELEROMETER_RESPONSE) \
                                         + sizeof(ACCELEROMETER_RESPONSE_VARIABLE_EXAMPLE))
#endif /* INCLUDE_ACCELEROMETER */

#ifdef INCLUDE_PROXIMITY
static void deviceTestServiceCommand_HandleProximityMessage(bool close);
static void deviceTestServiceCommand_ProximityTestStart(Task command_id,
                                                        bool stop_on_first, unsigned timeout);
static void deviceTestServiceCommand_ProximityTestEnded(void);

/*! The base part of the AT command response */
#define PROXIMITY_RESPONSE "+PROXIMITY:"

#define PROXIMITY_RESPONSE_VARIABLE_FMT "%d"

/*! Worst case variable length portion */
#define PROXIMITY_RESPONSE_VARIABLE_EXAMPLE "1"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_PROXIMITY_RESPONSE_LEN (  sizeof(PROXIMITY_RESPONSE) \
                                     + sizeof(PROXIMITY_RESPONSE_VARIABLE_EXAMPLE))
#endif /* INCLUDE_PROXIMITY */


#ifdef INCLUDE_CAPSENSE
static void deviceTestServiceCommand_HandleTouchMessage(const TOUCH_SENSOR_ACTION_T *action_msg);
static void deviceTestServiceCommand_TouchTestStart(Task command_id,
                                                    bool stop_on_first, unsigned timeout);
static void deviceTestServiceCommand_TouchTestEnded(void);

/*! The base part of the AT command response */
#define TOUCH_RESPONSE "+TOUCH:"

#define TOUCH_RESPONSE_VARIABLE_FMT "%d,%d"

/*! Worst case variable length portion */
#define TOUCH_RESPONSE_VARIABLE_EXAMPLE "1,123"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_TOUCH_RESPONSE_LEN (  sizeof(TOUCH_RESPONSE) \
                                 + sizeof(TOUCH_RESPONSE_VARIABLE_EXAMPLE))
#endif /* INCLUDE_CAPSENSE */


#ifdef INCLUDE_HALL_EFFECT_SENSOR
static void deviceTestServiceCommand_HandleHallEffectMessage(bool magnet);
static void deviceTestServiceCommand_HallEffectTestStart(Task command_id,
                                                         bool stop_on_first, unsigned timeout);
static void deviceTestServiceCommand_HallEffectTestEnded(void);

/*! The base part of the AT command response */
#define HALLEFFECT_RESPONSE "+HALLEFFECT:"

#define HALLEFFECT_RESPONSE_VARIABLE_FMT "%d"

/*! Worst case variable length portion */
#define HALLEFFECT_RESPONSE_VARIABLE_EXAMPLE "1"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_HALLEFFECT_RESPONSE_LEN (  sizeof(HALLEFFECT_RESPONSE) \
                                      + sizeof(HALLEFFECT_RESPONSE_VARIABLE_EXAMPLE))
#endif /* INCLUDE_HALL_EFFECT_SENSOR */


/*! The base part of the AT command response */
#define TEMPERATURE_RESPONSE "+TEMPERATURE:"

#define TEMPERATURE_RESPONSE_VARIABLE_FMT "%d"

/*! Worst case variable length portion */
#define TEMPERATURE_RESPONSE_VARIABLE_EXAMPLE "-127"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_TEMPERATURE_RESPONSE_LEN (  sizeof(TEMPERATURE_RESPONSE) \
                                       + sizeof(TEMPERATURE_RESPONSE_VARIABLE_EXAMPLE))

/*! The base part of the AT command response */
#define BATTERY_RESPONSE "+BATTERYLEVEL:"

#define BATTERY_RESPONSE_VARIABLE_FMT "%d"

/*! Worst case variable length portion. Allow for an unexpected value */
#define BATTERY_RESPONSE_VARIABLE_EXAMPLE "65535"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define FULL_BATTERY_RESPONSE_LEN (  sizeof(BATTERY_RESPONSE) \
                                   + sizeof(BATTERY_RESPONSE_VARIABLE_EXAMPLE))


/*! \brief Handler function for messages required in sensor access 

    \param     task     The task message sent to
    \param     id       Message identifier
    \param[in] message  The message content, if any. Frequently NULL.
*/
static void sensortest_task_handler(Task task, MessageId id, Message message);

/*! \brief Local structure used to store information required for sensor access */
struct
{
    TaskData    task;                       /*!< The message handler for this file */
    Task        running_command;            /*!< The task supplied when sent an AT command */
    unsigned    accelerometer_running:1;    /*!< Accelerometer monitoring is active */
    unsigned    proximity_running:1;        /*!< Proximity monitoring is active */
    unsigned    touch_running:1;            /*!< Touch (cap sense) monitoring is active */
    unsigned    halleffect_running:1;       /*!< Hall effect monitoring is active */
    unsigned    stop_on_response:1;         /*!< Stop when a message received during the test.
                                                 Most tests send a response when registering, 
                                                 which is ignored by this flag. */
    unsigned    initial_response_received:1;/*!< Has the initial message for sensor been received */
} sensortest_data = {.task.handler = sensortest_task_handler };

#define SENSOR_TEST_TASK() (&sensortest_data.task)


typedef enum
{
                        /*! Message used to time-out a sensor test */
    SENSORTEST_INTERNAL_TIMEOUT = INTERNAL_MESSAGE_BASE,

    /*! This must be the final message */
    SENSORTEST_INTERNAL_MESSAGE_END
} sensortest_internal_message_t;
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(SENSORTEST_INTERNAL_MESSAGE_END)

LOGGING_PRESERVE_MESSAGE_TYPE(sensortest_internal_message_t)

/* Include prototype so not neccesary to have a long #if, needed if function static */
void deviceTestServiceCommand_SetCommonTestData(Task command, bool stop_on_first, unsigned timeout);
void deviceTestServiceCommand_SetCommonTestData(Task command, bool stop_on_first, unsigned timeout)
{
    sensortest_data.stop_on_response = stop_on_first;
    sensortest_data.initial_response_received = FALSE;
    sensortest_data.running_command = command;

    MessageCancelAll(SENSOR_TEST_TASK(), SENSORTEST_INTERNAL_TIMEOUT);
    MessageSendLater(SENSOR_TEST_TASK(), SENSORTEST_INTERNAL_TIMEOUT, NULL, D_SEC(timeout));
}

/* Include prototype so not neccesary to have a long #if, needed if function static */
void deviceTestServiceCommand_ClearCommonTestDataSendOk(void);
void deviceTestServiceCommand_ClearCommonTestDataSendOk(void)
{
    Task command = sensortest_data.running_command;

    MessageCancelAll(SENSOR_TEST_TASK(), SENSORTEST_INTERNAL_TIMEOUT);

    sensortest_data.stop_on_response = FALSE;
    sensortest_data.initial_response_received = FALSE;
    sensortest_data.running_command = NULL;

    if (command)
    {
        DeviceTestService_CommandResponseOk(command);
    }
}

/* Include prototype so not neccesary to have a long #if, needed if function static */
bool deviceTestServiceCommand_EndTestOnThisResponse(void);
bool deviceTestServiceCommand_EndTestOnThisResponse(void)
{
    if (sensortest_data.stop_on_response && sensortest_data.initial_response_received)
    {
        return TRUE;
    }

    /* It is safe to set this value on every message (more than once) */
    sensortest_data.initial_response_received = TRUE;
    return FALSE;
}

static void deviceTestServiceCommand_HandleTimeout(void)
{
    DEBUG_LOG_INFO("deviceTestServiceCommand_HandleTimeout");

    if (sensortest_data.running_command)
    {
#if defined(INCLUDE_ACCELEROMETER)
        if (sensortest_data.accelerometer_running)
        {
            deviceTestServiceCommand_AccelerationTestEnded();
        }
#endif
#if defined(INCLUDE_PROXIMITY)
        if (sensortest_data.proximity_running)
        {
            deviceTestServiceCommand_ProximityTestEnded();
        }
#endif
#if defined(INCLUDE_CAPSENSE)
        if (sensortest_data.touch_running)
        {
            deviceTestServiceCommand_TouchTestEnded();
        }
#endif
#if defined(INCLUDE_HALL_EFFECT_SENSOR)
        if (sensortest_data.halleffect_running)
        {
            deviceTestServiceCommand_HallEffectTestEnded();
        }
#endif

        sensortest_data.running_command = NULL;
    }
}


/*! Command handler for AT + TESTACCELEROMETER = durationS , stop_after

    This function requests accelerometer updates and returns them as 
    responses for the duration requested.

    Optionally the command may stop monitoring after the first reply.

    Errors are reported if the device test service is not authorised, or
    accelerometer is not supported.

    \param[in] task   The task to be used in command responses
    \param[in] params The parameters passed in the command
 */
void DeviceTestServiceCommand_HandleAccelerometer(Task task,
                        const struct DeviceTestServiceCommand_HandleAccelerometer *params)
{
    unsigned duration = params->durationS;
    bool stop = params->stopAfter;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAccelerometer for %ds. Stop_after:%d",
                     duration, stop);

#if !defined(INCLUDE_ACCELEROMETER)
        DeviceTestService_CommandResponseError(task);
        return;
#else
    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (duration == 0)
    {
        /* Cancelling a running command */
        deviceTestServiceCommand_AccelerationTestEnded();
    }
    else if (!appAccelerometerClientRegister(SENSOR_TEST_TASK()))
    {
        /* Failed to register for accelerometer messages */
        DeviceTestService_CommandResponseError(task);
    }
    else
    {
        deviceTestServiceCommand_AccelerationTestStart(task, stop, duration);
    }
#endif /* INCLUDE_ACCELEROMETER */
}

#ifdef INCLUDE_ACCELEROMETER
static void deviceTestServiceCommand_AccelerationTestStart(Task command_id,
                                                bool stop_on_first, unsigned timeout)
{
    deviceTestServiceCommand_SetCommonTestData(command_id, stop_on_first, timeout);
    sensortest_data.accelerometer_running = TRUE;
}

static void deviceTestServiceCommand_AccelerationTestEnded(void)
{
    sensortest_data.accelerometer_running = FALSE;
    appAccelerometerClientUnregister(SENSOR_TEST_TASK());

    deviceTestServiceCommand_ClearCommonTestDataSendOk();
}

static void deviceTestServiceCommand_HandleAccelerationMessage(bool moving)
{
    if (sensortest_data.accelerometer_running && sensortest_data.running_command)
    {
        char response[FULL_ACCELEROMETER_RESPONSE_LEN];

        sprintf(response, ACCELEROMETER_RESPONSE ACCELEROMETER_RESPONSE_VARIABLE_FMT,
                moving);
        
        DeviceTestService_CommandResponse(sensortest_data.running_command, response,
                                          FULL_ACCELEROMETER_RESPONSE_LEN);
        if (deviceTestServiceCommand_EndTestOnThisResponse())
        {
            deviceTestServiceCommand_AccelerationTestEnded();
        }
    }
}
#endif /* INCLUDE ACCLEROMETER */

/*! Command handler for AT + TESTPROXIMITY = durationS , stop_after

    This function requests updates for the procimity sensor and 
    returns them as responses for the duration requested.

    Optionally the command may stop monitoring after the first reply.

    Errors are reported if the device test service is not authorised, or
    proximity sensor is not supported.

    \param[in] task   The task to be used in command responses
    \param[in] params The parameters passed in the command
 */
void DeviceTestServiceCommand_HandleProximitySensor(Task task,
                        const struct DeviceTestServiceCommand_HandleProximitySensor *params)
{
    unsigned duration = params->durationS;
    bool stop = params->stopAfter;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleProximitySensor for %ds. Stop_after:%d",
                     duration, stop);

#if !defined(INCLUDE_PROXIMITY)
    DeviceTestService_CommandResponseError(task);
    return;
#else
    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (duration == 0)
    {
        /* Cancelling a running command */
        deviceTestServiceCommand_ProximityTestEnded();
    }
    else if (!appProximityClientRegister(SENSOR_TEST_TASK()))
    {
        /* Failed to register for accelerometer messages */
        DeviceTestService_CommandResponseError(task);
    }
    else
    {
        deviceTestServiceCommand_ProximityTestStart(task, stop, duration);
    }
#endif /* INCLUDE_PROXIMITY */
}

#ifdef INCLUDE_PROXIMITY
static void deviceTestServiceCommand_ProximityTestStart(Task command_id,
                                                bool stop_on_first, unsigned timeout)
{
    deviceTestServiceCommand_SetCommonTestData(command_id, stop_on_first, timeout);

    sensortest_data.proximity_running = TRUE;
}

static void deviceTestServiceCommand_ProximityTestEnded(void)
{
    sensortest_data.proximity_running = FALSE;
    appProximityClientUnregister(SENSOR_TEST_TASK());

    deviceTestServiceCommand_ClearCommonTestDataSendOk();
}

static void deviceTestServiceCommand_HandleProximityMessage(bool close)
{
    if (sensortest_data.proximity_running && sensortest_data.running_command)
    {
        char response[FULL_PROXIMITY_RESPONSE_LEN];

        sprintf(response, PROXIMITY_RESPONSE PROXIMITY_RESPONSE_VARIABLE_FMT,
                close);
        
        DeviceTestService_CommandResponse(sensortest_data.running_command, response,
                                          FULL_PROXIMITY_RESPONSE_LEN);
        if (deviceTestServiceCommand_EndTestOnThisResponse())
        {
            deviceTestServiceCommand_ProximityTestEnded();
        }
    }
}
#endif /* INCLUDE_PROXIMITY */


/*! Command handler for AT + TESTTOUCH = durationS , stop_after

    This function requests touch sensor updates and returns them as 
    responses for the duration requested.

    Optionally the command may stop monitoring after the first reply.

    Errors are reported if the device test service is not authorised, or
    no touch sensor is supported.

    \param[in] task   The task to be used in command responses
    \param[in] params The parameters passed in the command
 */
void DeviceTestServiceCommand_HandleTouchSensor(Task task,
                        const struct DeviceTestServiceCommand_HandleTouchSensor *params)
{
    unsigned duration = params->durationS;
    bool stop = params->stopAfter;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleTouchSensor for %ds. Stop_after:%d",
                     duration, stop);

#if !defined(INCLUDE_CAPSENSE)
    DeviceTestService_CommandResponseError(task);
    return;
#else
    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (duration == 0)
    {
        /* Cancelling a running command */
        deviceTestServiceCommand_TouchTestEnded();
    }
    else if (!TouchSensorActionClientRegister(SENSOR_TEST_TASK()))
    {
        /* Failed to register for accelerometer messages */
        DeviceTestService_CommandResponseError(task);
    }
    else
    {
        deviceTestServiceCommand_TouchTestStart(task, stop, duration);
    }
#endif /* INCLUDE_CAPSENSE */
}

#ifdef INCLUDE_CAPSENSE
static void deviceTestServiceCommand_TouchTestStart(Task command_id,
                                                    bool stop_on_first, unsigned timeout)
{
    deviceTestServiceCommand_SetCommonTestData(command_id, stop_on_first, timeout);

    sensortest_data.touch_running = TRUE;
}

static void deviceTestServiceCommand_TouchTestEnded(void)
{
    TouchSensorActionClientUnRegister(SENSOR_TEST_TASK());
    sensortest_data.touch_running = FALSE;

    deviceTestServiceCommand_ClearCommonTestDataSendOk();
}

static unsigned deviceTestServiceCommand_TouchTypeFromAction(touch_action_t action)
{
    if (SINGLE_PRESS <= action && action < SLIDE_UP)
    {
        /* Indicated a touch */
        return 1;
    }
    if (SLIDE_UP <= action && action <= SLIDE_RIGHT)
    {
        /* Indicates a slide */
        return 2;
    }
    if (HAND_COVER <= action && action <= HAND_COVER_RELEASE)
    {
        /* Indicates a hand cover */
        return 3;
    }

    DEBUG_LOG_VERBOSE("deviceTestServiceCommand_TouchTypeFromAction. Did not map action enum:touch_action_t:%x",
                        action);

    return 0;
}

static void deviceTestServiceCommand_HandleTouchMessage(const TOUCH_SENSOR_ACTION_T *action_msg)
{
    if (sensortest_data.touch_running && sensortest_data.running_command)
    {
        touch_action_t action = action_msg->action;
        char response[FULL_TOUCH_RESPONSE_LEN];
        unsigned action_type = deviceTestServiceCommand_TouchTypeFromAction(action);

        sprintf(response, TOUCH_RESPONSE TOUCH_RESPONSE_VARIABLE_FMT,
                action_type, action_msg->action);

        DeviceTestService_CommandResponse(sensortest_data.running_command, response,
                                          FULL_TOUCH_RESPONSE_LEN);
        if (sensortest_data.stop_on_response)
        {
            deviceTestServiceCommand_TouchTestEnded();
        }
    }
}
#endif /* INCLUDE_CAPSENSE */


/*! Command handler for AT + TESTHALLEFFECT = durationS , stop_after

    This function requests updates for the hall effect sensor and 
    returns them as responses for the duration requested.

    Optionally the command may stop monitoring after the first reply.

    Errors are reported if the device test service is not authorised, or
    hall effect sensor is not supported.

    \param[in] task   The task to be used in command responses
    \param[in] params The parameters passed in the command
 */
void DeviceTestServiceCommand_HandleHallEffectSensor(Task task,
                        const struct DeviceTestServiceCommand_HandleHallEffectSensor *params)
{
    unsigned duration = params->durationS;
    bool stop = params->stopAfter;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleHallEffectSensor for %ds. Stop_after:%d",
                     duration, stop);

#if !defined(INCLUDE_HALL_EFFECT_SENSOR)
    DeviceTestService_CommandResponseError(task);
    return;
#else
    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (duration == 0)
    {
        /* Cancelling a running command */
        deviceTestServiceCommand_HallEffectTestEnded();
    }
    else if (!HallEffectSensor_ClientRegister(SENSOR_TEST_TASK()))
    {
        /* Failed to register for accelerometer messages */
        DeviceTestService_CommandResponseError(task);
    }
    else
    {
        deviceTestServiceCommand_HallEffectTestStart(task, stop, duration);
    }
#endif /* INCLUDE_HALL_EFFECT_SENSOR */
}

#ifdef INCLUDE_HALL_EFFECT_SENSOR
static void deviceTestServiceCommand_HallEffectTestStart(Task command_id,
                                                         bool stop_on_first, unsigned timeout)
{
    deviceTestServiceCommand_SetCommonTestData(command_id, stop_on_first, timeout);

    sensortest_data.halleffect_running = TRUE;
}

static void deviceTestServiceCommand_HallEffectTestEnded(void)
{
    HallEffectSensor_ClientUnRegister(SENSOR_TEST_TASK());
    sensortest_data.halleffect_running = FALSE;

    deviceTestServiceCommand_ClearCommonTestDataSendOk();
}

static void deviceTestServiceCommand_HandleHallEffectMessage(bool magnet)
{
    if (sensortest_data.halleffect_running && sensortest_data.running_command)
    {
        char response[FULL_HALLEFFECT_RESPONSE_LEN];

        sprintf(response, HALLEFFECT_RESPONSE HALLEFFECT_RESPONSE_VARIABLE_FMT,
                magnet);
        
        DeviceTestService_CommandResponse(sensortest_data.running_command, response,
                                          FULL_HALLEFFECT_RESPONSE_LEN);
        if (deviceTestServiceCommand_EndTestOnThisResponse())
        {
            deviceTestServiceCommand_HallEffectTestEnded();
        }
    }
}
#endif /* INCLUDE_HALL_EFFECT_SENSOR */


/*! Command handler for AT + TEMPERATURE ?

    This function reads the current temperature and returns as a response.
    
    Errors are reported if the device test service is not authorised, or
    there is no temperature sensor.

    \param[in] task   The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleTemperature(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleTemperature");

#if !defined(INCLUDE_TEMPERATURE)
    DeviceTestService_CommandResponseError(task);
    return;
#else

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    char response[FULL_TEMPERATURE_RESPONSE_LEN];

    sprintf(response, TEMPERATURE_RESPONSE TEMPERATURE_RESPONSE_VARIABLE_FMT,
            appTemperatureGetAverage());

    DeviceTestService_CommandResponse(task, response, FULL_TEMPERATURE_RESPONSE_LEN);
    DeviceTestService_CommandResponseOk(task);
#endif /* INCLUDE TEMPERATURE */
}


/*! Command handler for AT + BATTERYLEVEL ? 

    This function requests the current battery level and returns as 
    a response.

    An errors is reported if the device test service is not authorised.

    \param[in] task   The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleBatteryLevel(Task task)
{
    char response[FULL_BATTERY_RESPONSE_LEN];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleBatteryLevel");

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    sprintf(response, BATTERY_RESPONSE BATTERY_RESPONSE_VARIABLE_FMT,
            appBatteryGetVoltageAverage());

    DeviceTestService_CommandResponse(task, response, FULL_BATTERY_RESPONSE_LEN);
    DeviceTestService_CommandResponseOk(task);
}

static void sensortest_task_handler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    UNUSED(sensortest_data.task);

    switch (id)
    {
#ifdef INCLUDE_ACCELEROMETER
        case ACCELEROMETER_MESSAGE_IN_MOTION:
        case ACCELEROMETER_MESSAGE_NOT_IN_MOTION:
            deviceTestServiceCommand_HandleAccelerationMessage(id == ACCELEROMETER_MESSAGE_IN_MOTION);
            break;
#endif

#ifdef INCLUDE_PROXIMITY
        case PROXIMITY_MESSAGE_IN_PROXIMITY:
        case PROXIMITY_MESSAGE_NOT_IN_PROXIMITY:
            deviceTestServiceCommand_HandleProximityMessage(id == PROXIMITY_MESSAGE_IN_PROXIMITY);
            break;
#endif

#ifdef INCLUDE_CAPSENSE
        case TOUCH_SENSOR_ACTION:
            deviceTestServiceCommand_HandleTouchMessage((const TOUCH_SENSOR_ACTION_T *) message);
            break;
#endif

#ifdef INCLUDE_HALL_EFFECT_SENSOR
        case HALL_EFFECT_MESSAGE_MAGNET_PRESENT:
        case HALL_EFFECT_MESSAGE_MAGNET_NOT_PRESENT:
            deviceTestServiceCommand_HandleHallEffectMessage(id == HALL_EFFECT_MESSAGE_MAGNET_PRESENT);
            break;
#endif

        case SENSORTEST_INTERNAL_TIMEOUT:
            deviceTestServiceCommand_HandleTimeout();
            break;
    }
}

