/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       battery_monitor.c
\brief      Battery monitoring
*/

#ifndef HAVE_NO_BATTERY

#define DEBUG_LOG_MODULE_NAME battery_monitor
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <adc.h>

#include "app_task.h"
#include "battery_monitor.h"
#include "battery_monitor_config.h"
#include "system_state.h"
#include "hydra_macros.h"
#include "panic.h"
#include "unexpected_message.h"

#include <logging.h>
#include <stdlib.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(battery_messages)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */

ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(BATTERY_APP, BATTERY_APP_MESSAGE_END)
#endif

#ifndef FAKE_BATTERY_LEVEL
#define FAKE_BATTERY_LEVEL (0)
#endif
uint16 fake_battery_level = FAKE_BATTERY_LEVEL;

#define FAKE_BATTERY_LEVEL_DISABLED 0

bool battery_level_test_on = FALSE;

#if defined(QCC3020_FF_ENTRY_LEVEL_AA) || (defined HAVE_RDP_HW_YE134) || (defined HAVE_RDP_HW_18689)
#define CHARGED_BATTERY_FULL_OFFSET_mV   (200)
#else
#define CHARGED_BATTERY_FULL_OFFSET_mV   (0)
#endif

/*! Battery task structure */
typedef struct
{
    /*! Battery task */
    TaskData task;
    /*! The measurement period. Value between 500 and 10000 ms. */
    uint16 period;
    /*! Store the vref measurement, which is required to calculate vbat */
    uint16 vref_raw;
    /* track cfm sending */
    bool cfm_sent;
    /*! A sub-struct to allow reset */
    struct
    {
        /*! Configurable window used for median filter. Value 3 or 5. */
        uint16 median_filter_window;
        /* latest value */
        uint16 instantaneous;
    } filter;
    struct
    {
        /* smoothing factor with value between 0 and 1 */
        uint8 weight;
        /* last exponential moving average. */
        uint32 last_ema;
        uint32 current_ema;
    } average;
    /*! A linked-list of clients */
    batteryRegisteredClient *client_list;
} batteryTaskData;

/*! \brief Battery component task data. */
batteryTaskData app_battery;

/*! \brief Access the battery task data. */
#define GetBattery()    (&app_battery)

/*! Enumerated type for messages sent within the battery
    handler. */
enum battery_internal_messages
{
    /*! Message sent to trigger an intermittent battery measurement */
    MESSAGE_BATTERY_INTERNAL_MEASUREMENT_TRIGGER = 1,

    MESSAGE_BATTERY_TEST_PROCESS_READING,
};

/* TRUE if the current value is less than the threshold taking into account hysteresis */
static bool ltThreshold(uint16 current, uint16 threshold, uint16 hysteresis)
{
    return current < (threshold - hysteresis);
}

/* TRUE if the current value is greater than the threshold taking into account hysteresis. */
static bool gtThreshold(uint16 current, uint16 threshold, uint16 hysteresis)
{
    return current > (threshold + hysteresis);
}

/* TRUE if the current value is outside the threshold taking into account hysteresis */
static bool thresholdExceeded(uint16 current, uint16 threshold, uint16 hysteresis)
{
    return ltThreshold(current, threshold, hysteresis) ||
           gtThreshold(current, threshold, hysteresis);
}

/*! Add a client to the list of clients */
static bool appBatteryClientAdd(batteryTaskData *battery, batteryRegistrationForm *form)
{
    batteryRegisteredClient *new = calloc(1, sizeof(*new));
    if (new)
    {
        new->form = *form;
        new->next = battery->client_list;
        battery->client_list = new;
        return TRUE;
    }
    return FALSE;
}

/*! Remove a client from the list of clients */
static void appBatteryClientRemove(batteryTaskData *battery, Task task)
{
    batteryRegisteredClient **head;
    for (head = &battery->client_list; *head != NULL; head = &(*head)->next)
    {
        if ((*head)->form.task == task)
        {
            batteryRegisteredClient *toremove = *head;
            *head = (*head)->next;
            free(toremove);
            break;
        }
    }
}

/*! Iterate through the list of clients, sending battery level messages when
    the representation criteria is met */
static void appBatteryServiceClients(batteryTaskData *battery)
{
    batteryRegisteredClient *client = NULL;
    uint16 voltage = appBatteryGetVoltageAverage();
    for (client = battery->client_list; client != NULL; client = client->next)
    {
        uint16 hysteresis = client->form.hysteresis;
        switch (client->form.representation)
        {
            case battery_level_repres_voltage:
            {
                if(thresholdExceeded(voltage, client->voltage, hysteresis))
                {
                    MESSAGE_MAKE(msg, MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T);
                    msg->voltage_mv = voltage;
                    client->voltage = voltage;
                    MessageSend(client->form.task, MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE, msg);
                }
            }
            break;
        }
    }
}

static void appBatteryScheduleNextMeasurement(batteryTaskData *battery,
                                                       uint32 delay)
{
    MessageSendLater(&battery->task, MESSAGE_BATTERY_INTERNAL_MEASUREMENT_TRIGGER,
                        NULL, delay);
}

static uint32 batteryMonitor_ExponentialAverage(batteryTaskData *battery, uint16 reading)
{
    uint32 ema;
    static uint8 t = 1;

    /* for first time reading is the average */
    if (t == 1)
    {
        ema = reading * 100;
        t++;
    }
    else
    {
        ema = (uint32)((battery)->average.weight * reading) + 
            (uint32)(((100 - (battery)->average.weight) * battery->average.last_ema) / 100);
    }

    return ema;
}

static uint16 batteryMonitor_MedianFiltering(batteryTaskData *battery, uint16 reading)
{
    uint8 i,j, k;
    static uint16 *input_arr = NULL;
    uint16 *sort_arr = NULL;
    static uint8 index = 0;
    uint16 temp_value;
    uint16 median_val;

    if (input_arr == NULL)
    {
        input_arr = (uint16*)PanicUnlessMalloc(sizeof(uint16)*battery->filter.median_filter_window);
    }

    /* copy reading into input array */
    input_arr[index] = reading;

    /* copy array into sorting array */
    sort_arr = (uint16*)PanicUnlessMalloc(sizeof(uint16)*(index + 1));

    memcpy(sort_arr, input_arr, sizeof(uint16)*(index + 1));  
    
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

    if (index == ((battery)->filter.median_filter_window - 1))
    {
        /* shift input_arr values */
        for(i=0; i<((battery)->filter.median_filter_window - 1); i++)
        {
            input_arr[i] = input_arr[i+1];
        }
        input_arr[((battery)->filter.median_filter_window - 1)] = 0;
    }
    else
    {
        index++;
    }    

    /* return median value */
    return median_val;
}

/*! Return TRUE if a new voltage is available,with enough samples that the result
    should be stable. This waits for the filter to be full. */
static bool appBatteryAdcResultHandler(batteryTaskData *battery, MessageAdcResult* result)
{
    uint16 reading = result->reading;
    uint16 median_reading = 0;
    uint32 ema_reading;

    switch (result->adc_source)
    {
        case adcsel_pmu_vbat_sns:
        {
            if (!battery->cfm_sent)
            {                           
                battery->cfm_sent = TRUE;
                MessageSend(SystemState_GetTransitionTask(), MESSAGE_BATTERY_INIT_CFM, NULL);
            }

            if (battery->vref_raw != 0)
            {           
                uint16 vbatt_mv = (uint16)((uint32)VmReadVrefConstant() * reading / battery->vref_raw);

                /* apply median filtering before storing.
                  */
                median_reading = batteryMonitor_MedianFiltering(battery, vbatt_mv);

                battery->filter.instantaneous = median_reading;

                /* calculate average */
                battery->average.last_ema = battery->average.current_ema;
                ema_reading = batteryMonitor_ExponentialAverage(battery, median_reading);

                DEBUG_LOG_DEBUG("battMon: %d median %d avg %d",
                        vbatt_mv, median_reading, ema_reading / 100);


                battery->average.current_ema = ema_reading;
            }
            else
            {
                DEBUG_LOG_WARN("battery_monitor, vref_raw reading is 0.");
            }
            if (!battery_level_test_on)
            {
                appBatteryScheduleNextMeasurement(battery, battery->period);
            }
            return (median_reading != 0);
        }

        case adcsel_vref_hq_buff:
            battery->vref_raw = reading;
            break;

        default:
            DEBUG_LOG("appBatteryAdcResultHandler unexpected source - %d",result->adc_source);
            break;
    }
    return FALSE;
}

static void appBatteryHandleMessage(Task task, MessageId id, Message message)
{
    batteryTaskData *battery = (batteryTaskData *)task;
    if (battery->period != 0)
    {
        switch (id)
        {
            case MESSAGE_ADC_RESULT:
                if (!battery_level_test_on &&
                    fake_battery_level == FAKE_BATTERY_LEVEL_DISABLED)
                {
                    if (appBatteryAdcResultHandler(battery, (MessageAdcResult*)message))
                    {
                        appBatteryServiceClients(battery);
                    }
                }
                break;

            case MESSAGE_BATTERY_TEST_PROCESS_READING:
            {                
                if (appBatteryAdcResultHandler(battery, (MessageAdcResult*)message))
                {
                    appBatteryServiceClients(battery);
                }
            }
                break;

            case MESSAGE_BATTERY_INTERNAL_MEASUREMENT_TRIGGER:
                /* Start immediate battery reading, note vref is read first */
                if (!AdcReadRequest(&battery->task, adcsel_vref_hq_buff, 0, 0))
                {
                    DEBUG_LOG("AdcReadRequest() for adcsel_vref_hq_buff returned False."
                        "Schedule next set of measurements.");
                    appBatteryScheduleNextMeasurement(battery, battery->period);
                    break;
                }
                if (!AdcReadRequest(&battery->task, adcsel_pmu_vbat_sns, 0, 0))
                {
                    DEBUG_LOG("AdcReadRequest() for adcsel_pmu_vbat_sns returned False."
                        "Schedule next set of measurements.");
                    battery->vref_raw = 0;
                    appBatteryScheduleNextMeasurement(battery, battery->period);
                }                
                break;

            default:
                /* An unexpected message has arrived - must handle it */
                UnexpectedMessage_HandleMessage(id);
                break;
        }
    }
}

bool appBatteryInit(Task init_task)
{
    DEBUG_LOG("appBatteryInit");
    batteryTaskData *battery = GetBattery();
    memset(battery, 0, sizeof(*battery));

    UNUSED(init_task);

    /* Set up task handler */
    battery->task.handler = appBatteryHandleMessage;

    /* these need to be set from headset or earbud init. temporarily in config file. */
    battery->period = appConfigBatteryReadingPeriodMs();
    battery->filter.median_filter_window = appConfigBatteryMedianFilterWindow();
    battery->average.weight = appConfigBatterySmoothingWeight();

    if (fake_battery_level == FAKE_BATTERY_LEVEL_DISABLED)
    {
        appBatteryScheduleNextMeasurement(battery, 0);
    }
    else
    {
        battery->cfm_sent = TRUE;
        MessageSend(SystemState_GetTransitionTask(), MESSAGE_BATTERY_INIT_CFM, NULL);
    }

    return TRUE;
}

uint16 appBatteryGetVoltageAverage(void)
{
    if (fake_battery_level != FAKE_BATTERY_LEVEL_DISABLED)
    {
        return fake_battery_level;
    }
    else
    {
        batteryTaskData *battery = GetBattery();
        return (battery->average.current_ema / 100);
    }
}

bool BatteryMonitor_IsGood(void)
{
    return appBatteryGetVoltageAverage() > appConfigBatteryVoltageLow();
}

uint16 appBatteryGetVoltageInstantaneous(void)
{
    if (fake_battery_level != FAKE_BATTERY_LEVEL_DISABLED)
    {
        return fake_battery_level;
    }
    else
    {
        batteryTaskData *battery = GetBattery();
        return battery->filter.instantaneous;
    }
}

bool appBatteryRegister(batteryRegistrationForm *client)
{
    batteryTaskData *battery = GetBattery();
    if (appBatteryClientAdd(battery, client))
    {
        appBatteryServiceClients(battery);
        return TRUE;
    }
    return FALSE;
}

void appBatteryUnregister(Task task)
{
    batteryTaskData *battery = GetBattery();
    appBatteryClientRemove(battery, task);
}

void appBatteryTestSetFakeVoltage(uint16 voltage)
{
    batteryTaskData *battery = GetBattery();
    
    DEBUG_LOG("appBatteryTestSetFakeVoltage, set test voltage: %d ", voltage);
    fake_battery_level = voltage;
    appBatteryServiceClients(battery);
}

void appBatteryTestUnsetFakeVoltage(void)
{
    DEBUG_LOG("appBatteryTestUnsetFakeVoltage, reset test voltage to %d", FAKE_BATTERY_LEVEL_DISABLED);
    batteryTaskData *battery = GetBattery();
    
    fake_battery_level = FAKE_BATTERY_LEVEL_DISABLED;
    appBatteryScheduleNextMeasurement(battery, 0);
}

void appBatteryTestInjectFakeLevel(uint16 voltage)
{   
    DEBUG_LOG("appBatteryTestInjectFakeLevel, inject test voltage: %d ", voltage);
    battery_level_test_on = TRUE;
    
    /* create MessageAdcResult message */
    MESSAGE_MAKE(msg, MessageAdcResult);
    msg->reading = voltage;
    msg->adc_source = adcsel_pmu_vbat_sns;
    
    MessageSend(&GetBattery()->task, MESSAGE_BATTERY_TEST_PROCESS_READING, msg);
}

void appBatteryTestResumeAdcMeasurements(void)
{
    batteryTaskData *battery = GetBattery();
    
    DEBUG_LOG("appBatteryTestResumeAdcMeasurements");
    battery_level_test_on = FALSE;    
    
    appBatteryScheduleNextMeasurement(battery, 0);
}

#endif /* !HAVE_NO_BATTERY */
