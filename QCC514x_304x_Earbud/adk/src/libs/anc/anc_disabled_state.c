/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_disabled_state.c

DESCRIPTION
    Event handling functions for the ANC Disabled State.
*/

#include "anc_disabled_state.h"
#include "anc_common_state.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_configure.h"

static bool enableAncEventHandler(anc_state_event_enable_with_user_gain_args_t * args)
{
    bool enabled = FALSE;

    if(args)
        ancDataSetUserGainConfig(args->gain_config_left, args->gain_config_right);

    if(ancConfigure(TRUE))
    {
        ancDataSetState(anc_state_enabled);
        enabled = TRUE;
    }

    return enabled;
}

/******************************************************************************/
bool ancStateDisabledHandleEvent(anc_state_event_t event)
{
    /* Assume failure until proven otherwise */
    bool success = FALSE;

    switch (event.id)
    {
        case anc_state_event_enable:
        {
            success = enableAncEventHandler((anc_state_event_enable_with_user_gain_args_t *)event.args);
        }
        break;

        case anc_state_event_enable_parallel_filter:
        {
            success = ancConfigureEnableParallelAnc();
            ancDataSetState(anc_state_enabled);
        }
        break;

        case anc_state_event_set_topology:
        {
            ancDataSetTopology(((anc_state_event_set_topology_args_t *)event.args)->anc_topology);
            success = TRUE;
        }
        break;

        case anc_state_event_enable_with_mute_path_gains:
        {
            success = ancEnableWithPathGainsMuted();
            ancDataSetState(anc_state_enabled);
        }
        break;

        case anc_state_event_enable_parallel_anc_with_mute_path_gains:
        {
            success = ancConfigureEnableParallelAncPathGainsMuted();
            ancDataSetState(anc_state_enabled);
        }
        break;

        case anc_state_event_set_mode:
        case anc_state_event_set_parallel_mode:
        case anc_state_event_set_mode_filter_coefficients:
        case anc_state_event_set_mode_parallel_filter_coeffiecients:
        case anc_state_event_set_mode_filter_coefficients_path_gains:
        case anc_state_event_set_mode_parallel_filter_coeffiecients_path_gains:
        {
            /* Common processing for setting the mode */
            success = ancCommonStateHandleSetMode(event);
        }
        break;

        default:
        {
            ANC_DEBUG_INFO(("Unhandled event [%d]\n", event.id));
            ANC_PANIC();
        }
        break;
    }
    return success;
}
