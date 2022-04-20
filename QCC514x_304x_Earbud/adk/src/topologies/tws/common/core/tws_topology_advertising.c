/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      tws_topology control of advertising parameters.
*/

#include "tws_topology_private.h"
#include "tws_topology_config.h"
#include "tws_topology_advertising.h"

#include <bt_device.h>
#include <pairing.h>
#include <le_advertising_manager.h>
#include <logging.h>

void twsTopology_UpdateAdvertisingParams(void)
{
    tws_topology_le_adv_params_set_type_t next = LE_ADVERTISING_PARAMS_SET_TYPE_UNSET;
    tws_topology_le_adv_params_set_type_t current = TwsTopologyGetTaskData()->advertising_params;

    if (BtDevice_IsPairedWithPeer())
    {
        if (!appPairingIsIdle())
        {
            next = LE_ADVERTISING_PARAMS_SET_TYPE_FAST;
        }
        else if (!appDeviceIsPeerConnected())
        {
            next = LE_ADVERTISING_PARAMS_SET_TYPE_FAST;
        }
        else
        {
            if (appDeviceIsBredrHandsetConnected())
            {
                next = LE_ADVERTISING_PARAMS_SET_TYPE_SLOW;
            }
            else
            {
                next = LE_ADVERTISING_PARAMS_SET_TYPE_FAST_FALLBACK;
            }
        }
    }

    if (next != current)
    {
        DEBUG_LOG_INFO("twsTopology_UpdateAdvertisingParams "
                       "enum:tws_topology_le_adv_params_set_type_t:%d->"
                       "enum:tws_topology_le_adv_params_set_type_t:%d", current, next);

        LeAdvertisingManager_ParametersSelect(next);

        TwsTopologyGetTaskData()->advertising_params = next;
    }
}

