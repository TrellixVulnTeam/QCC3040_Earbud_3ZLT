/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       battery_monitor.c
\brief      Battery monitoring
*/

#include <logging.h>

#include <adc.h>

#include "app_task.h"
#include "hydra_macros.h"

#include "charger_data.h"
#include "charger_monitor_config.h"
#include "charger_detect.h"

#ifdef INCLUDE_CHARGER_DETECT

#define CHG_MON_REQUEST_READING 0xffff

void Charger_VChgMonitorStart(chargerTaskData *data)
{
    if (!data->vchg_monitor_enabled)
    {
        DEBUG_LOG_INFO("Charger: vchg monitor start");
        data->vchg_monitor_enabled = 1;

        data->chg_mon_reading = CHG_MON_REQUEST_READING;

        MessageSend(&data->task, CHARGER_INTERNAL_VCHG_MEASUREMENT, NULL);
    }
}

void Charger_VChgMonitorStop(chargerTaskData *data)
{
    if (data->vchg_monitor_enabled)
    {
        DEBUG_LOG_INFO("Charger: vchg monitor stop");
        data->vchg_monitor_enabled = 0;
        (void) MessageCancelAll(&data->task,
                                CHARGER_INTERNAL_VCHG_MEASUREMENT);
    }
}

void Charger_VchgMonitorPeriodic(chargerTaskData *data)
{
    if (!data->vchg_monitor_enabled ||
            data->vchg_monitor_read_pending)
    {
        return;
    }

    bool adc_read_requested = FALSE;

    if (data->vref_reading == 0)
    {
        adc_read_requested = AdcReadRequest(&data->task, adcsel_vref_hq_buff, 0, 0);
    }
    else if (data->chg_mon_reading == CHG_MON_REQUEST_READING)
    {
        adc_read_requested = AdcReadRequest(&data->task, adcsel_chg_mon, 0, 0);
    }
    else
    {
        adc_read_requested = AdcReadRequest(&data->task, adcsel_pmu_vchg_sns, 0, 0);
    }

    if (!adc_read_requested)
    {
        /* try again soon */
        MessageSendLater(&data->task, CHARGER_INTERNAL_VCHG_MEASUREMENT, NULL, 10);
    }
    else
    {
        data->vchg_monitor_read_pending = 1;
        /* cleared in Charger_VChgMonitorReading() */
    }
}

void Charger_VChgMonitorReading(chargerTaskData *data, MessageAdcResult *message)
{
    switch (message->adc_source)
    {
        case adcsel_vref_hq_buff:
            data->vchg_monitor_read_pending = 0;

            data->vref_reading = message->reading;
            DEBUG_LOG_INFO("Charger: vref = %d", data->vref_reading);

            /* back to the main handler */
            MessageSend(&data->task, CHARGER_INTERNAL_VCHG_MEASUREMENT, NULL);
            break;

        case adcsel_chg_mon:
            data->vchg_monitor_read_pending = 0;

            if (data->vchg_monitor_enabled &&
                    data->vref_reading)
            {
                data->chg_mon_reading = (uint16)
                    ((uint32)Charger_GetFastCurrent() * message->reading / data->vref_reading);

                DEBUG_LOG_DEBUG("Charger: chg mon = %dmA", data->chg_mon_reading);

                /* back to the main handler */
                MessageSend(&data->task, CHARGER_INTERNAL_VCHG_MEASUREMENT, NULL);
            }
            break;

        case adcsel_pmu_vchg_sns:
            data->vchg_monitor_read_pending = 0;

            if (data->vchg_monitor_enabled &&
                    data->vref_reading)
            {
                uint16 reading_mv = (uint16)
                    ((uint32)VmReadVrefConstant() * message->reading / data->vref_reading);

                DEBUG_LOG_INFO("Charger: vchg %dmV current %dmA",
                        reading_mv, data->chg_mon_reading);

                /* pass new readings into ChargerDetect */
                ChargerDetect_VchgReading(reading_mv, data->chg_mon_reading);
                /* request new adcsel_chg_mon reading */
                data->chg_mon_reading = CHG_MON_REQUEST_READING;

                /* schedule next reading */
                MessageSendLater(&data->task, CHARGER_INTERNAL_VCHG_MEASUREMENT, NULL,
                        appConfigChargerVchgPollingPeriodMs());
            }
            break;


        default:
            DEBUG_LOG_INFO("Charger_VChgMonitorReading unexpected source - %d",
                    message->adc_source);
            break;
    }
}

#endif /* INCLUDE_CHARGER_DETECT */
