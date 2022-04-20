/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_enabled_state.c

DESCRIPTION
    Event handling functions for the ANC Enabled State.
*/

#include "anc_enabled_state.h"
#include "anc_common_state.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_configure.h"
#include "anc_config_write.h"
#include "anc_configure_coefficients.h"

static bool disableAncEventHandler(void)
{
    bool disabled = FALSE;
    if(ancConfigure(FALSE))
    {
        ancDataSetState(anc_state_disabled);
        disabled = TRUE;
    }
    return disabled;
}

static bool disableParallelAncEventHandler(void)
{
    bool disabled =FALSE;
    if(ancConfigureDisableParallelAnc())
    {
        ancDataSetState(anc_state_disabled);
        disabled = TRUE;
    }
    return disabled;
}

static bool setFilterPathGainEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancConfigureFilterPathGain(((anc_state_event_set_path_gain_args_t *)event.args)->instance,
                                          ((anc_state_event_set_path_gain_args_t *)event.args)->path,
                                          ((anc_state_event_set_path_gain_args_t *)event.args)->gain);

    return gain_set;
}

static bool setParallelFilterPathGainEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancConfigureParallelFilterPathGain(((anc_state_event_set_parallel_filter_path_gain_args_t *)event.args)->path,
                                                  ((anc_state_event_set_parallel_filter_path_gain_args_t *)event.args)->instance_0_gain,
                                                  ((anc_state_event_set_parallel_filter_path_gain_args_t *)event.args)->instance_1_gain);
    return gain_set;
}

static bool writeFineGainToPsEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancWriteFineGain(ancDataGetMode(),
                                          ((anc_state_event_write_gain_args_t *)event.args)->path,
                                          ((anc_state_event_write_gain_args_t *)event.args)->gain);

    return gain_set;
}

static bool writeFineGainToPsParallelFilterEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancWriteFineGainParallelFilter(ancDataGetMode(),
                                            ((anc_state_event_write_gain_parallel_filter_args_t *)event.args)->path,
                                            ((anc_state_event_write_gain_parallel_filter_args_t *)event.args)->instance_0_gain,
                                            ((anc_state_event_write_gain_parallel_filter_args_t *)event.args)->instance_1_gain);

    return gain_set;
}


/******************************************************************************/
bool ancStateEnabledHandleEvent(anc_state_event_t event)
{
    bool success = FALSE;

    switch (event.id)
    {
        case anc_state_event_disable:
        {
            success = disableAncEventHandler();
        }
        break;

        case anc_state_event_disable_parallel_filter:
        {
            success =disableParallelAncEventHandler();
        }
        break;

        case anc_state_event_set_mode:
        {
            if (ancCommonStateHandleSetMode(event))
            {
                success = ancConfigureAfterModeChange();
            }
        }
        break;

        case anc_state_event_set_parallel_mode:
        {
            if(ancCommonStateHandleSetMode(event))
            {
                success = ancConfigureParallelFilterAfterModeChange();
            }

        }
        break;

        case anc_state_event_set_mode_filter_coefficients:
        {
            if (ancCommonStateHandleSetMode(event))
            {
                success = ancConfigureFilterCoefficientsAfterModeChange();
            }
        }
        break;

        case anc_state_event_set_mode_parallel_filter_coeffiecients:
        {
            if(ancCommonStateHandleSetMode(event))
            {
                success = ancConfigureParallelFilterCoefAfterModeChange();
            }

        }
        break;

        case anc_state_event_set_mode_filter_coefficients_path_gains:
        {
            if (ancCommonStateHandleSetMode(event))
            {
                success = ancConfigureFilterCoefficientsPathGainsAfterModeChange(
                        ((anc_state_event_set_mode_coefficients_path_gains_args_t *) event.args)->enable_coarse_gains,
                        ((anc_state_event_set_mode_coefficients_path_gains_args_t *) event.args)->enable_fine_gains
                        );
            }
        }
        break;

        case anc_state_event_set_mode_parallel_filter_coeffiecients_path_gains:
        {
            if(ancCommonStateHandleSetMode(event))
            {
                success = ancConfigureParallelFilterCoefPathGainsAfterModeChange(
                        ((anc_state_event_set_mode_coefficients_path_gains_args_t *) event.args)->enable_coarse_gains,
                        ((anc_state_event_set_mode_coefficients_path_gains_args_t *) event.args)->enable_fine_gains
                        );
            }
        }
        break;

        case anc_state_event_set_single_filter_path_gain:
        {
            success = setFilterPathGainEventHandler(event);
        }
        break;
               
        case anc_state_event_write_fine_gain:
        {
            success = writeFineGainToPsEventHandler(event);
        }
        break;
        
        case anc_state_event_write_fine_gain_parallel_filter:
        {
            success = writeFineGainToPsParallelFilterEventHandler(event);
        }
        break;

        case anc_state_event_set_all_single_filter_path_gains:
        {
            ancConfigureFilterPathGains();
#ifdef ANC_UPGRADE_FILTER
            setRxMixGains(anc_single_filter_topology);
#endif
            success = TRUE;
        }
        break;

        case anc_state_event_set_parallel_filter_path_gain:
        {
            success = setParallelFilterPathGainEventHandler(event);
        }
        break;

        case anc_state_event_set_parallel_filter_path_gains:
        {
             ancConfigureParallelFilterPathGains(TRUE, TRUE);
#ifdef ANC_UPGRADE_FILTER
            setRxMixGains(anc_parallel_filter_topology);
#endif
            success = TRUE;
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
