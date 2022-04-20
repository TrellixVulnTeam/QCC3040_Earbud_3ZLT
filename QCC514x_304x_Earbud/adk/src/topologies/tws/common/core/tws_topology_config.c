/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Configuration / parameters used by the topology.
*/

#include "tws_topology_config.h"
#include "rtime.h"

static const bredr_scan_manager_scan_parameters_set_t inquiry_scan_params_set[] =
{
    {
        {
            [SCAN_MAN_PARAMS_TYPE_SLOW] = { .interval = US_TO_BT_SLOTS(2560000), .window = US_TO_BT_SLOTS(11250) },
            [SCAN_MAN_PARAMS_TYPE_FAST] = { .interval = US_TO_BT_SLOTS(320000),  .window = US_TO_BT_SLOTS(11250) },
        },
    },
};

static const bredr_scan_manager_scan_parameters_set_t page_scan_params_set[] =
{
    {
        {
            [SCAN_MAN_PARAMS_TYPE_SLOW] = { .interval = US_TO_BT_SLOTS(1280000), .window = US_TO_BT_SLOTS(11250), .scan_type = hci_scan_type_interlaced },
            [SCAN_MAN_PARAMS_TYPE_FAST] = { .interval = US_TO_BT_SLOTS(100000),  .window = US_TO_BT_SLOTS(11250), .scan_type = hci_scan_type_interlaced },
            [SCAN_MAN_PARAMS_TYPE_THROTTLE] = { .interval = US_TO_BT_SLOTS(640000),  .window = US_TO_BT_SLOTS(5000), .scan_type = hci_scan_type_standard },
        },
    },
};

const bredr_scan_manager_parameters_t inquiry_scan_params =
{
    inquiry_scan_params_set, ARRAY_DIM(inquiry_scan_params_set)
};

const bredr_scan_manager_parameters_t page_scan_params =
{
    page_scan_params_set, ARRAY_DIM(page_scan_params_set)
};

#define MSEC_TO_LE_TIMESLOT(x)	((x)*1000/625)
#ifdef USE_AGGRESSIVE_FAST_ADVERTISING
#define FAST_ADVERTISING_INTERVAL_MIN_SLOTS MSEC_TO_LE_TIMESLOT(30)
#define FAST_ADVERTISING_INTERVAL_MAX_SLOTS MSEC_TO_LE_TIMESLOT(40)
#else
#define FAST_ADVERTISING_INTERVAL_MIN_SLOTS MSEC_TO_LE_TIMESLOT(90)
#define FAST_ADVERTISING_INTERVAL_MAX_SLOTS MSEC_TO_LE_TIMESLOT(100)
#endif
#define SLOW_ADVERTISING_INTERVAL_MIN_SLOTS MSEC_TO_LE_TIMESLOT(225)
#define SLOW_ADVERTISING_INTERVAL_MAX_SLOTS MSEC_TO_LE_TIMESLOT(250)

static const le_adv_parameters_set_t params_set =
{
    {
        /* This is an ordered list, do not make any assumptions when changing the order of the list items */
        {SLOW_ADVERTISING_INTERVAL_MIN_SLOTS, SLOW_ADVERTISING_INTERVAL_MAX_SLOTS},
        {FAST_ADVERTISING_INTERVAL_MIN_SLOTS, FAST_ADVERTISING_INTERVAL_MAX_SLOTS}
    }
};

#define TIMEOUT_FALLBACK_IN_SECONDS 10


static const le_adv_parameters_config_table_t config_table =
{
    {
        /* This is an ordered list, do not make any assumptions when changing the order of the list items */
        [LE_ADVERTISING_PARAMS_SET_TYPE_FAST] = {le_adv_preset_advertising_interval_fast, 0},
        [LE_ADVERTISING_PARAMS_SET_TYPE_FAST_FALLBACK] = {le_adv_preset_advertising_interval_fast, TIMEOUT_FALLBACK_IN_SECONDS},
        [LE_ADVERTISING_PARAMS_SET_TYPE_SLOW] = {le_adv_preset_advertising_interval_slow, 0}
    }
};

const le_adv_parameters_t le_adv_params = {.sets = &params_set, .table = &config_table };
