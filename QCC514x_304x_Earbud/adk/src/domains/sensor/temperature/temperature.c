/*!
\copyright  Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       temperature.c
\brief      Top level temperature sensing implementation. Uses a temperature sensor
            e.g. thermistor to actually perform the measurements.
*/

#ifdef INCLUDE_TEMPERATURE

#define DEBUG_LOG_MODULE_NAME temperature
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <panic.h>
#include <pio.h>
#include <vm.h>
#include <hydra_macros.h>
#include <limits.h>
#include <task_list.h>
#include <logging.h>

#include "adk_log.h"
#include "temperature.h"
#include "temperature_config.h"
#include "temperature_sensor.h"
#include "unexpected_message.h"

#include <stdlib.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(temperatureMessages)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TEMPERATURE, TEMPERATURE_MESSAGE_END)

#define INVALID_TEMPERATURE 100

#ifndef FAKE_BATTERY_TEMP
#define FAKE_BATTERY_TEMP INVALID_TEMPERATURE
#endif
int8 fake_battery_temperature = FAKE_BATTERY_TEMP;

bool battery_temp_test_on = FALSE;

/*! Messages sent within the temperature module. */
enum headset_temperature_internal_messages
{
    /*! Message sent to trigger a temperature measurement */
    MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER,
};


/*! The indexes in a #task_list_data_t arr_s8 to store the client data. */
enum client_indexes
{
    CLIENT_LOWER_LIMIT_INDEX,
    CLIENT_UPPER_LIMIT_INDEX,
    CLIENT_CURRENT_STATE_INDEX,
};

typedef struct
{
    const temperature_lookup_t* temperature_config_table;
    unsigned temperature_config_size;
}temp_ctx_t;

static temp_ctx_t temperature_ctx;

/*!< Task information for temperature */
temperatureTaskData app_temperature;

static int8 temperature_GetTemperature(uint16 voltage, int8 prev_temperature)
{
    uint32 prev_index, index;
    uint16 prev_mv, this_mv;

    prev_index = prev_temperature - TEMPERATURE_MIN;
    prev_mv = temperature_ctx.temperature_config_table[prev_index].voltage;

    if (voltage < prev_mv)
    {
        /* Temperature increase, search forward through lower voltages / higher temperatures */
        for (index = prev_index + 1; index < temperature_ctx.temperature_config_size; index++)
        {
            this_mv = temperature_ctx.temperature_config_table[index].voltage;

            if (voltage >= this_mv)
            {
                return temperature_ctx.temperature_config_table[index].temperature;
            }
        }
        return TEMPERATURE_MAX;
    }
    else /* voltage >= prev_mv */
    {
        /* Temperature decrease, search backwards through higher voltages / lower temperatures */
        for (index = prev_index; index > 0; index--)
        {
            this_mv = prev_mv;
            prev_mv = temperature_ctx.temperature_config_table[index-1].voltage;

            if (voltage >= this_mv && voltage < prev_mv)
            {
                return temperature_ctx.temperature_config_table[index].temperature;
            }
        }
        return TEMPERATURE_MIN;
    }
}


/*! \brief Inform a single client of temperature events */
static bool appTemperatureServiceClient(Task task, task_list_data_t *data, void *arg)
{
    int8 lower_limit = data->arr_s8[CLIENT_LOWER_LIMIT_INDEX];
    int8 upper_limit = data->arr_s8[CLIENT_UPPER_LIMIT_INDEX];
    int8 t = appTemperatureGetAverage();
    temperatureState next_state;

    next_state = (t >= upper_limit) ? TEMPERATURE_STATE_ABOVE_UPPER_LIMIT :
                    (t <= lower_limit) ? TEMPERATURE_STATE_BELOW_LOWER_LIMIT :
                        TEMPERATURE_STATE_WITHIN_LIMITS;

    if (next_state != data->arr_s8[CLIENT_CURRENT_STATE_INDEX])
    {
        MESSAGE_MAKE(ind, TEMPERATURE_STATE_CHANGED_IND_T);
        ind->state = next_state;
        data->arr_s8[CLIENT_CURRENT_STATE_INDEX] = next_state;
        MessageSend(task, TEMPERATURE_STATE_CHANGED_IND, ind);
    }

    UNUSED(arg);

    /* Iterate through every client */
    return TRUE;
}

/*! \brief Inform all clients of temperature events */
static void appTemperatureServiceClients(temperatureTaskData *temperature)
{
    TaskList_IterateWithDataRawFunction(temperature->clients, appTemperatureServiceClient, temperature);
}

static int32 temperature_ExponentialAverage(temperatureTaskData *temp, int8 reading)
{
    int32 ema;
    static uint8 t = 1;

    /* for first time reading is the average */
    if (t == 1)
    {
        ema = reading * 100;
        t++;
    }
    else
    {
        ema = (int32)((temp)->average.weight * reading) + 
            (int32)(((100 - (temp)->average.weight) * temp->average.last_ema) / 100);
    }

    return ema;
}

static int8 temperature_MedianFiltering(temperatureTaskData *temp, int8 reading)
{
    uint8 i,j, k;
    static int8 *input_arr = NULL;
    int8 *sort_arr = NULL;
    static uint8 index = 0;
    int8 temp_value;
    int8 median_val;
    
    if (input_arr == NULL)
    {
        input_arr = (int8*)PanicUnlessMalloc(sizeof(int8)*temp->filter.median_filter_window);
    }

    /* copy reading into input array */
    input_arr[index] = reading;

    /* copy array into sorting array */
    sort_arr = (int8*)PanicUnlessMalloc(sizeof(int8)*(index + 1));

    memcpy(sort_arr, input_arr, sizeof(int8)*(index + 1));
    
    /* sort input array in increasing order */
    for(j=0; j<(index + 1); j++)
    {       
        for(k=j+1; k<(index + 1); k++)
        {
            /* swap */
            if(sort_arr[j] > sort_arr[k])
            {
                temp_value = sort_arr[j];
                sort_arr[j] = sort_arr[k];
                sort_arr[k] = temp_value;
            }
        }
    }       

    median_val = sort_arr[((index + 1) - 1)/2];

    /* free sort array */
    free(sort_arr);

    if (index == (temp->filter.median_filter_window - 1))
    {
        /* shift input_arr values */
        for(i=0; i<(temp->filter.median_filter_window - 1); i++)
        {
            input_arr[i] = input_arr[i+1];
        }
        input_arr[temp->filter.median_filter_window - 1] = 0;  
    }
    else
    {
        index++;
    }

    /* return median value */
    return median_val;
}

static void handleTemperatureReading(temperatureTaskData *temperature, int8 reading)
{
    int8 median_reading;
    int32 ema_reading;
    
    median_reading = temperature_MedianFiltering(temperature, reading);

    temperature->filter.instantaneous = median_reading;

    /* calculate average */
    temperature->average.last_ema = temperature->average.current_ema;
    ema_reading = temperature_ExponentialAverage(temperature, median_reading);

    temperature->average.current_ema = ema_reading;

    DEBUG_LOG_DEBUG("temp: %d median %d avg %d",
            reading, median_reading, ema_reading / 100);

    appTemperatureServiceClients(temperature);
}

static uint16 Temperature_AdcResultHandler(const MessageAdcResult* result)
{
    /*! The latest vref measurement, which is required to calculate the voltage.
        vref must be measured before the ADC voltage. */
    static uint16 vref_raw;
    uint16 volt_mv = 0;

    uint16 reading = result->reading;

    if (adcsel_vref_hq_buff == result->adc_source)
    {
        vref_raw = reading;
    }
    else if (appTemperatureSensorAdcSource() == result->adc_source)
    {
        if (vref_raw != 0)
        {
            uint32 vref_const = VmReadVrefConstant();
            volt_mv = (uint16)(vref_const * reading / vref_raw);
        }
        else
        {
            DEBUG_LOG_WARN("Temperature_AdcResultHandler, vref_raw reading is 0.");
        }
        
        return volt_mv;
    }
    else
    {
        DEBUG_LOG_WARN("Temperature_AdcResultHandler unexpected source - %d",result->adc_source);
    }

    return 0;
}

/*! \brief Handle temperature messages */
static void appTemperatureHandleMessage(Task task, MessageId id, Message message)
{
    temperatureTaskData *temperature = (temperatureTaskData *)task;
    uint16 voltage_mv;
    
    switch (id)
    {
        case MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER:
        {
            appTemperatureSensorRequestMeasurement(task);
        }
        break;
                
        case MESSAGE_ADC_RESULT:
        {
            if (!battery_temp_test_on &&
                fake_battery_temperature == INVALID_TEMPERATURE)
            {
                voltage_mv = Temperature_AdcResultHandler((const MessageAdcResult*)message);
                if (voltage_mv != 0)
                {
                    int8 t_new = temperature_GetTemperature(voltage_mv, 
                                            temperature->filter.instantaneous);
                    handleTemperatureReading(temperature, t_new);
                    MessageSendLater(&temperature->task,
                             MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER, NULL,
                             temperature->period); 
                }
            }
        }
        break;

        default:
        {
            /* An unexpected message has arrived - must handle it */
            UnexpectedMessage_HandleMessage(id);
        }
        break;
    }
}


bool appTemperatureInit(Task init_task)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();

    DEBUG_LOG("appTemperatureInit");

    temperature->clients = TaskList_WithDataCreate();
    temperature->task.handler = appTemperatureHandleMessage;

    /* these need to be set from headset or earbud init */
    temperature->period = appConfigTemperatureReadingPeriodMs();
    temperature->filter.median_filter_window = appConfigTemperatureMedianFilterWindow();
    temperature->average.weight = appConfigTemperatureSmoothingWeight();

    appTemperatureSensorInit();
    if (fake_battery_temperature == INVALID_TEMPERATURE)
    {
        appTemperatureSensorRequestMeasurement(&temperature->task);
    }
    
    UNUSED(init_task);
    
    return TRUE;
}
 
bool appTemperatureClientRegister(Task task, int8 lower_limit, int8 upper_limit)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();
    task_list_data_t data = {0};

    DEBUG_LOG("appTemperatureClientRegister Task=%p (%d, %d)", task, lower_limit, upper_limit);

    data.arr_s8[CLIENT_LOWER_LIMIT_INDEX] = lower_limit;
    data.arr_s8[CLIENT_UPPER_LIMIT_INDEX] = upper_limit;
    data.arr_s8[CLIENT_CURRENT_STATE_INDEX] = TEMPERATURE_STATE_UNKNOWN;
    appTemperatureServiceClient(task, &data, temperature);
    PanicFalse(TaskList_AddTaskWithData(temperature->clients, task, &data));
    return TRUE;
}

void appTemperatureClientUnregister(Task task)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();

    DEBUG_LOG("appTemperatureClientUnregister Task=%p", task);

    PanicFalse(TaskList_RemoveTask(temperature->clients, task));
}

temperatureState appTemperatureClientGetState(Task task)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();
    task_list_data_t *data;
    PanicFalse(TaskList_GetDataForTaskRaw(temperature->clients, task, &data));
    return (temperatureState)data->arr_s8[CLIENT_CURRENT_STATE_INDEX];
}

int8 appTemperatureGetAverage(void)
{
   if (fake_battery_temperature != INVALID_TEMPERATURE)
   {
       return fake_battery_temperature;
   }
   else
   {
       temperatureTaskData *temperature = TemperatureGetTaskData();
       return (temperature->average.current_ema / 100);
   }
}

int8 appTemperatureGetInstantaneous(void)
{
    if (fake_battery_temperature != INVALID_TEMPERATURE)
    {
        return fake_battery_temperature;
    }
    else
    {
        temperatureTaskData *temperature = TemperatureGetTaskData();
        return temperature->filter.instantaneous;
    }
}

void Temperature_SetConfigurationTable(const temperature_lookup_t* config_table,
                              unsigned config_size)
{
    DEBUG_LOG("Temperature_SetConfigurationTable, set voltage->temperature lookup configuration table");

    temperature_ctx.temperature_config_table = config_table;
    temperature_ctx.temperature_config_size = config_size;
}

void appTemperatureSetFakeValue(int8 temperature)
{
    temperatureTaskData *temperature_data = TemperatureGetTaskData();
    DEBUG_LOG("appTemperatureSetFakeValue, set test temperature: %d ", temperature);
    
    fake_battery_temperature = temperature;
    appTemperatureServiceClients(temperature_data);    
}

void appTemperatureUnsetFakeValue(void)
{
    DEBUG_LOG("appTemperatureUnsetFakeValue, reset test temperature");
    temperatureTaskData *temperature = TemperatureGetTaskData();
    
    fake_battery_temperature = INVALID_TEMPERATURE;
    MessageSendLater(&temperature->task,
                     MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER, NULL,
                     0);
}

void appTemperatureTestInjectFakeLevel(int8 temperature)
{
    temperatureTaskData *temperature_data = TemperatureGetTaskData();
    DEBUG_LOG("appTemperatureTestInjectFakeLevel, inject test temperature: %d ", temperature);
    battery_temp_test_on = TRUE;

    handleTemperatureReading(temperature_data, temperature);
}

void appTemperatureResumeAdcMeasurements(void)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();
    
    DEBUG_LOG("appTemperatureResumeAdcMeasurements");
    battery_temp_test_on = FALSE;    
    
    MessageSendLater(&temperature->task,
                     MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER, NULL,
                     0);
}

#endif
