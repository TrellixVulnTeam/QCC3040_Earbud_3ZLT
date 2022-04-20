/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
*/

#ifdef INCLUDE_LATENCY_MANAGER

#include "kymera_dynamic_latency.h"
#include "kymera_latency_manager.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_data.h"
#include <bt_device.h>
#include <logging.h>
#include <bdaddr.h>
#include <acl.h>
#include <mirror_profile.h>
#include <hdma_core.h>

/*! \brief Dynamic latency state */
typedef struct
{

#ifdef INCLUDE_LATENCY_MANAGER_RELAY_RATIO
    /*! Number of packets relayed */
    uint32 relay_packet_count;

    /*! Number of packets received */
    uint32 rx_packet_count;
#endif

    /*! Current dynamic latency in milli-seconds */
    uint16 dynamic_latency;

    /*! The base latency for the current use case in milli-seconds */
    uint16 base_latency;

#ifdef INCLUDE_LATENCY_MANAGER_RELAY_RATIO
    /*! Relay Packet percentage which was recorded at last latency update.*/
    uint8 current_percentage;

    /*! Relay % threshold beyond which latency is increased */
    uint8 relay_percentage_threshold;
#endif

    /*! This flag tells if Dynamic Latency Adjustment feature is enabled. This
        flag is disabled by default. Unless it is set latency will not be
        dynamically updated. */
    unsigned dynamic_adjustment_enabled : 1;

    /*! This flag is set when dynamic adjustment is currently active */
    unsigned dynamic_adjustment_active : 1;

} kymera_dynamic_latency_data_t;

kymera_dynamic_latency_data_t kymera_dynamic_latency_data;

#define KymeraGetDynamicLatencyData() (&kymera_dynamic_latency_data)

void Kymera_DynamicLatencyInit(void)
{
    kymera_dynamic_latency_data_t *data = KymeraGetDynamicLatencyData();
    memset(data, 0, sizeof(*data));

#ifdef INCLUDE_LATENCY_MANAGER_RELAY_RATIO
    data->relay_percentage_threshold = KYMERA_LATENCY_PERCENTAGE_THRESHOLD;
#endif
}

void Kymera_DynamicLatencyEnableDynamicAdjustment(void)
{
    KymeraGetDynamicLatencyData()->dynamic_adjustment_enabled = 1;
}

void Kymera_DynamicLatencyDisableDynamicAdjustment(void)
{
    KymeraGetDynamicLatencyData()->dynamic_adjustment_enabled = 0;
}

bool Kymera_DynamicLatencyIsEnabled(void)
{
#ifdef INCLUDE_MIRRORING
    return  BtDevice_IsMyAddressPrimary() &&
            KymeraGetDynamicLatencyData()->dynamic_adjustment_enabled &&
            MirrorProfile_IsA2dpActive();
#endif

#ifdef INCLUDE_STEREO
     return KymeraGetDynamicLatencyData()->dynamic_adjustment_enabled;
#endif

}

void Kymera_DynamicLatencyStartDynamicAdjustment(uint16 base_latency)
{
    Kymera_DynamicLatencyEnableDynamicAdjustment();
    if (Kymera_DynamicLatencyIsEnabled())
    {
        kymera_dynamic_latency_data_t *data = KymeraGetDynamicLatencyData();
        DEBUG_LOG("Kymera_DynamicLatencyStartDynamicAdjustment");
#ifdef INCLUDE_LATENCY_MANAGER_RELAY_RATIO
        data->relay_packet_count = 0;
        data->rx_packet_count = 0;
        data->current_percentage = data->relay_percentage_threshold;
#endif
        data->base_latency = data->dynamic_latency = base_latency;
        data->dynamic_adjustment_active = 1;
        Kymera_DynamicLatencyHandleLatencyTimeout();
    }
}

void Kymera_DynamicLatencyStopDynamicAdjustment(void)
{
    kymera_dynamic_latency_data_t *data = KymeraGetDynamicLatencyData();
    DEBUG_LOG("Kymera_DynamicLatencyStopDynamicAdjustment");
    MessageCancelAll(KymeraGetTask(), KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT);
    data->dynamic_adjustment_active = 0;
}

uint16 Kymera_DynamicLatencyMarshal(uint8 *buf, uint16 length)
{
    uint16 written = 0;
    size_t size = sizeof(kymera_dynamic_latency_data_t);
    if (length >= size)
    {
        memcpy(buf, KymeraGetDynamicLatencyData(), size);
        written = size;
    }
    return written;
}

uint16 Kymera_DynamicLatencyUnmarshal(const uint8 *buf, uint16 length)
{
    uint16 read = 0;
    size_t size = sizeof(kymera_dynamic_latency_data_t);
    if (length >= size)
    {
        memcpy(KymeraGetDynamicLatencyData(), buf, size);
        read = size;
    }
    return read;
}

static void Kymera_DynamicLatencyScheduleNextMeasurement(void)
{
    MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT);
    MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT,
                        NULL, KYMERA_LATENCY_CHECK_DELAY);
}

void Kymera_DynamicLatencyHandoverCommit(bool is_primary)
{
    if (is_primary)
    {
        if (Kymera_DynamicLatencyIsEnabled())
        {
            if (KymeraGetDynamicLatencyData()->dynamic_adjustment_active)
            {
                Kymera_DynamicLatencyScheduleNextMeasurement();
            }
        }
    }
    else
    {
        Kymera_DynamicLatencyStopDynamicAdjustment();
    }
}

#ifdef INCLUDE_LATENCY_MANAGER_RELAY_RATIO
static uint16 convertRelayPercentageToLatency(uint8 new_percentage)
{
    kymera_dynamic_latency_data_t * data = KymeraGetDynamicLatencyData();
    uint8 percentage_change; /* Change in relay % from current_percentage */
    uint8 units_of_change;   /* Number of steps to increase/decrease */
    uint16 value_of_change;  /* Value of latency to increase/decrease */
    uint16 latency = data->dynamic_latency;

    percentage_change = abs(data->current_percentage - new_percentage);
    if(new_percentage > data->current_percentage)
    {
        /* increase latency as relay % has further increased */
        units_of_change = percentage_change/KYMERA_LATENCY_MINIMUM_PERCENT_CHANGE;
        DEBUG_LOG_VERBOSE("Units of Latency Change (increasing): %d", units_of_change);
        value_of_change = KYMERA_LATENCY_UPDATE_UP_STEP_MS * units_of_change;
        latency = MIN(latency + value_of_change, TWS_STANDARD_LATENCY_MAX_MS);
        data->current_percentage = new_percentage;
    }
    else if (latency > data->base_latency)
    {
        /* If relay % has reduced but above threshold, decrease it proportional to change.
           If relay % is below threshold, decrease it atleast by a single step since latency
           is higher then default and needs to be reduced.
        */
        units_of_change = MAX(percentage_change/KYMERA_LATENCY_MINIMUM_PERCENT_CHANGE, 1);
        DEBUG_LOG_VERBOSE("Units of Latency Change (decreasing): %d", units_of_change);
        value_of_change = KYMERA_LATENCY_UPDATE_DOWN_STEP_MS * units_of_change;
        latency = MAX(latency - value_of_change, data->base_latency);
        data->current_percentage = MAX(new_percentage, data->relay_percentage_threshold);
    }
    return latency;
}

/*
    If relay percentage is increased by a threshold, latency is incremented by
    KYMERA_LATENCY_UPDATE_UP_STEP_MS, untill MAX is reached.

    If relay percentage is reduced by a thredhold, latency is reduced by
    KYMERA_LATENCY_UPDATE_DOWN_STEP_MS, untill MIN (i.e. DEFAULT) is reached
*/
static uint16 kymera_DynamicLatencyUpdate(void)
{
    kymera_dynamic_latency_data_t * data = KymeraGetDynamicLatencyData();
    acl_reliable_mirror_debug_statistics_t stats;
    uint32 relay_change; /*packets relayed since last time */
    uint32 rx_change; /*packets received since last time */
    uint8 relay_percentage;
    tp_bdaddr tp_addr;
    uint16 calculated_latency = 0;

    /*Get relayed and received packet counts from ACL Stats*/
    BdaddrTpFromBredrBdaddr(&tp_addr, MirrorProfile_GetMirroredDeviceAddress());
    if(AclReliableMirrorDebugStatistics(&tp_addr, &stats))
    {
        if(stats.rx_packet_count == data->rx_packet_count)
        {
            /* No packets received */
            return 0;
        }

        relay_change = stats.relay_packet_count - data->relay_packet_count;
        rx_change = stats.rx_packet_count - data->rx_packet_count;

        /* Store current stats */
        data->relay_packet_count = stats.relay_packet_count;
        data->rx_packet_count = stats.rx_packet_count;

        /* Calculate relay percentage */
        relay_percentage = (relay_change * 100)/rx_change;
        DEBUG_LOG_VERBOSE("kymera_DynamicLatencyUpdate Relay Change: %u, Rx Change: %u, Percentage: %u, Current Latency: %d",
                             relay_change, rx_change, relay_percentage, data->dynamic_latency);
        if(relay_percentage > data->relay_percentage_threshold ||
           data->current_percentage > data->relay_percentage_threshold ||
           data->dynamic_latency > data->base_latency)
        {
            calculated_latency = convertRelayPercentageToLatency(relay_percentage);
        }
    }
    return calculated_latency;
}

#endif

void Kymera_DynamicLatencyHandleLatencyTimeout(void)
{
    uint16 calculated_latency = KYMERA_LATENCY_DEFAULT;
    kymera_dynamic_latency_data_t * data = KymeraGetDynamicLatencyData();

#ifdef INCLUDE_HDMA_LINK_QUALITY_EVENT
    uint16 lq = Hdma_getLowestLinkQuality();


    if(lq != 0xffff)
    {
        DEBUG_LOG_VERBOSE("Kymera_DynamicLatencyHandleLatencyTimeout Lowest_LQ %d",lq);

        if(lq < LINK_QUALITY_AVERAGE && lq > LINK_QUALITY_LOW)
        {
            calculated_latency = KYMERA_LATENCY_INCR_100MS;
        }
        else if (lq <= LINK_QUALITY_CRITICAL)
        {
            calculated_latency = KYMERA_LATENCY_INCR_200MS;
        }
        else
        {
            /* Nothing */
        }
    }

#endif    

    if (calculated_latency && calculated_latency != data->dynamic_latency)
    {
		DEBUG_LOG_VERBOSE("Kymera_DynamicLatencyHandleLatencyTimeout Calculated_Latency %d",calculated_latency);
        data->dynamic_latency = calculated_latency;
        Kymera_LatencyManagerAdjustLatency(data->dynamic_latency);
    }

    Kymera_DynamicLatencyScheduleNextMeasurement();
}

#endif /* INCLUDE_LATENCY_MANAGER */
