/*!
\copyright  Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       temperature.h
\brief      Clients of the temperature module should use this API. If a client just
            wants to read the current temperature, appTemperatureGetInstantaneous() should be used.
            If a client wants to be informed about changes in temperature with respect
            to limits, the client should register using #appTemperatureClientRegister.
            The module will then send status #temperatureMessages to the client.
*/

#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <task_list.h>
#include "domain_message.h"

/*! \brief The temperature corresponding to the first entry in #temperature_config_table */
#define TEMPERATURE_MIN -40
/*! \brief The temperature corresponding to the final entry in #temperature_config_table */
#define TEMPERATURE_MAX 85

typedef struct
{
    uint16 voltage;
    int8 temperature;
}temperature_lookup_t;

/*! Enumeration of messages the temperature module can send to its clients */
enum temperatureMessages
{
    /*! The temperature state has changed. */
    TEMPERATURE_STATE_CHANGED_IND = TEMPERATURE_MESSAGE_BASE,

    /*! This must be the final message */
    TEMPERATURE_MESSAGE_END
};

/*! Client temperature states */
typedef enum temperature_states
{
    TEMPERATURE_STATE_WITHIN_LIMITS,
    TEMPERATURE_STATE_ABOVE_UPPER_LIMIT,
    TEMPERATURE_STATE_BELOW_LOWER_LIMIT,
    TEMPERATURE_STATE_UNKNOWN,
} temperatureState;

/*! Message content for #TEMPERATURE_STATE_CHANGED_IND */
typedef struct
{
    /*! The new state */
    temperatureState state;
} TEMPERATURE_STATE_CHANGED_IND_T;

/*! \brief Temperature module state. */
typedef struct
{
    /*! Temperature module message task. */
    TaskData task;  
    /*! The measurement period. Value between 500 and 10000 ms. */
    uint16 period;      
    /*! A sub-struct to allow reset */
    struct
    {
        /*! Configurable window used for median filter. Value 3 or 5. */
        uint16 median_filter_window;
        /* latest value */
        int8 instantaneous;
    } filter;
    struct
    {
        /* smoothing factor with value between 0 and 1. stored as multiple of 100 */
        uint8 weight;
        /* last exponential moving average. */
        int32 last_ema;
        int32 current_ema;
    } average;
    /*! List of registered client tasks */
    task_list_t *clients;
} temperatureTaskData;

/*!< Task information for temperature */
extern temperatureTaskData app_temperature;

/*! Get pointer to temperature data */
#define TemperatureGetTaskData() (&app_temperature)

/*! \brief Initialise the temperature module */
extern bool appTemperatureInit(Task init_task);

/*! \brief Register with temperature to receive notifications.
    \param task The task to register.
    \param lower_limit The lower temperature limit in degrees Celsius.
    \param upper_limit The upper temperature limit in degrees Celsius.
    \return TRUE if registration was successful, FALSE if temperature
    support is not included in the built image.
    \note The temperature measurement range depends on the capabilities of the
    temperature sensor used. If a limit is selected outside the sensor measurement
    range, that limit will never be exceeded. The limit is settable in the range
    of a signed 8-bit integer (-128 to +127 degrees Celsius).
*/
#if defined(INCLUDE_TEMPERATURE)
extern bool appTemperatureClientRegister(Task task, int8 lower_limit, int8 upper_limit);
#else
#define appTemperatureClientRegister(task, lower, upper) FALSE
#endif

/*! \brief Unregister with temperature.
    \param task The task to unregister.
*/
#if defined(INCLUDE_TEMPERATURE)
extern void appTemperatureClientUnregister(Task task);
#else
#define appTemperatureClientUnregister(task) ((void)0)
#endif

/*! \brief Get the client's state.
    \param task The client's task.
    \return The client's state, or TEMPERATURE_STATE_WITHIN_LIMITS if the platform does not
            support temperature measurement.
*/
#if defined(INCLUDE_TEMPERATURE)
extern temperatureState appTemperatureClientGetState(Task task);
#else
#define appTemperatureClientGetState(task) TEMPERATURE_STATE_WITHIN_LIMITS
#endif

/*! 
    \brief Get current battery temperature.
    \return Returns current battery temperature.
*/
extern int8 appTemperatureGetInstantaneous(void);

/*! 
    \brief Get battery temperature average.
    \return Returns battery temperature average.
*/
#if defined(INCLUDE_TEMPERATURE)
extern int8 appTemperatureGetAverage(void);
#else
#define appTemperatureGetAverage() ((TEMPERATURE_MAX + TEMPERATURE_MIN)/2)
#endif

/*! 
    \brief Set test value for battery temperature.
*/
void appTemperatureSetFakeValue(int8 temperature);

/*! 
    \brief Initialize voltage->temperature config table.
    \param config_table pointer to temperature_lookup_t structure array 
    \param config_size size of temperature_lookup_t structure array
*/
extern void Temperature_SetConfigurationTable(const temperature_lookup_t* config_table,
                              unsigned config_size);

/*! \brief Unset test battery temperature to 0 and start periodic monitoring.
 */
void appTemperatureUnsetFakeValue(void);

/*! 
    \brief Inject test value for battery temperature.
    \param temperature battery temperature value to use 
*/
void appTemperatureTestInjectFakeLevel(int8 temperature);

/*! \brief Unset test battery temperature to 0 and start periodic monitoring.
 */
void appTemperatureResumeAdcMeasurements(void);

#endif /* TEMPERATURE_H */
