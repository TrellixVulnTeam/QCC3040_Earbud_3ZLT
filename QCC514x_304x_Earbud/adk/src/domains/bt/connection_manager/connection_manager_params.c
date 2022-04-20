/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
*/

#include "connection_manager_params.h"

/*! Convert a connection interval in ms into a \b valid connection
    interval value.

    Connection intervals are measured in slot pairs (1.25ms) and the 
    interval must be even.
    Valid intervals start at 7.5ms and increase in  multiples of 2.5ms.

    This macro produces the setting that is equal to or greater than
    the value requested */
#define CONN_INT_MS(_ms)  (((unsigned)(((_ms)+2.45)/1.25))&0xFFFE)


/*  Using 90mS interval to not be at same frequency as advertising interval of peer 
    using 100% duty cycle to ensure fastest connection possible.
    Note that although this is a 100% duty cycle the Bluetooth controller may interrupt 
    this to take account of other activities. */
#define LE_FIXED_PARAMS \
    .scan_interval              = 144, \
    .scan_window                = 144, \
    .supervision_timeout        = 400, \
    .conn_attempt_timeout       = 50, \
    .conn_latency_max           = 64, \
    .supervision_timeout_min    = 400, \
    .supervision_timeout_max    = 400, \
    .own_address_type           = TYPED_BDADDR_PUBLIC

/* Taken from sink app slave parameters */
#define LE_LP_CONNECTION_PARAMS \
    .conn_interval_min  = CONN_INT_MS(90), \
    .conn_interval_max  = CONN_INT_MS(110), \
    .conn_latency       = 4

static const ble_connection_params low_power_connection_params = 
{
    LE_FIXED_PARAMS, LE_LP_CONNECTION_PARAMS
};

/* Taken from sink app master initial parameters */
#define LE_LL_CONNECTION_PARAMS \
    .conn_interval_min  = CONN_INT_MS(30), \
    .conn_interval_max  = CONN_INT_MS(50), \
    .conn_latency       = 0

static const ble_connection_params low_latency_connection_params = 
{
    LE_FIXED_PARAMS, LE_LL_CONNECTION_PARAMS
};

/*! Settings used on an LE Audio link when it is idle. */
#define LEA_IDLE_CONNECTION_PARAMS \
    .conn_interval_min  = CONN_INT_MS(60), \
    .conn_interval_max  = CONN_INT_MS(90), \
    .conn_latency       = 0

static const ble_connection_params lea_idle_connection_params = 
{
    LE_FIXED_PARAMS, LEA_IDLE_CONNECTION_PARAMS
};


/* Taken from sink app GAA parameters */
#define LE_AUDIO_CONNECTION_PARAMS \
    .conn_interval_min  = CONN_INT_MS(15), \
    .conn_interval_max  = CONN_INT_MS(15), \
    .conn_latency       = 4

static const ble_connection_params audio_connection_params = 
{
    LE_FIXED_PARAMS, LE_AUDIO_CONNECTION_PARAMS
};

/* Don't use a multiple of 6 for this connection interval
   as this may cause LE transmissions to clash with eSCO
   resulting in failed peer device communication */
#define SHORT_CONNECTION_INTERVAL CONN_INT_MS(10)

/* Absolute shortest interval/lowest latency possible */
#define LE_SHORT_CONNECTION_PARAMS \
    .conn_interval_min  = SHORT_CONNECTION_INTERVAL, \
    .conn_interval_max  = SHORT_CONNECTION_INTERVAL, \
    .conn_latency       = 0

static const ble_connection_params short_connection_params = 
{
    LE_FIXED_PARAMS, LE_SHORT_CONNECTION_PARAMS
};

const ble_connection_params* const cm_qos_params[cm_qos_max] =
{
    [cm_qos_invalid]                = NULL,
    [cm_qos_low_power]              = &low_power_connection_params,
    [cm_qos_low_latency]            = &low_latency_connection_params,
    [cm_qos_lea_idle]               = &lea_idle_connection_params,
    [cm_qos_audio]                  = &audio_connection_params,
    [cm_qos_short_data_exchange]    = &short_connection_params,
    [cm_qos_passive]                = &low_power_connection_params
};
