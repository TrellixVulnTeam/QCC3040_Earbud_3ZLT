/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc.c

DESCRIPTION
    ANC VM Library API functions.
*/

#include "anc.h"
#include "anc_sm.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_config_read.h"

#include <stdlib.h>

/******************************************************************************/

device_ps_key_config_t device_ps_key_config;

bool AncInit(anc_mic_params_t *mic_params, anc_mode_t init_mode)
{
    anc_state_event_initialise_args_t args;
    anc_state_event_t event = {anc_state_event_initialise, NULL};

    args.mic_params = mic_params;
    args.mode = init_mode;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}
/******************************************************************************/
bool AncSetTopology(anc_filter_topology_t anc_topology)
{
    anc_state_event_t event ={anc_state_event_set_topology,NULL};
    anc_state_event_set_topology_args_t args;

    args.anc_topology=anc_topology;
    event.args=&args;

    return ancStateMachineHandleEvent(event);

}
/******************************************************************************/
#ifdef HOSTED_TEST_ENVIRONMENT
bool AncLibraryTestReset(void)
{
    return ancDataDeinitialise();
}
#endif

/******************************************************************************/
bool AncEnable(bool enable)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id=enable?anc_state_event_enable:anc_state_event_disable;
        break;

        case anc_parallel_filter_topology:
            event.id =enable?anc_state_event_enable_parallel_filter:anc_state_event_disable_parallel_filter;
        break;

        default:
            event.id =enable?anc_state_event_enable:anc_state_event_disable;
        break;

    }
    event.args = NULL;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncEnableWithUserGain(anc_user_gain_config_t *left_channel_gain, anc_user_gain_config_t *right_channel_gain)
{
    anc_state_event_enable_with_user_gain_args_t args;
    anc_state_event_t event = {anc_state_event_enable, NULL};

    args.gain_config_left = left_channel_gain;
    args.gain_config_right = right_channel_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncEnableWithMutePathGains(void)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_enable_with_mute_path_gains;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_enable_parallel_anc_with_mute_path_gains;
        break;

        default:
            event.id =anc_state_event_enable_with_mute_path_gains;
        break;

    }
    event.args=NULL;
    return ancStateMachineHandleEvent(event);
}


/******************************************************************************/
bool AncSetMode(anc_mode_t mode)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;
    anc_state_event_set_mode_args_t args;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_set_mode;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_set_parallel_mode;
        break;

        default:
            event.id =anc_state_event_set_mode;
        break;
    }

    /* Assign args to the set mode event */
    args.mode = mode;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncSetModeFilterCoefficients(anc_mode_t mode)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;
    anc_state_event_set_mode_args_t args;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_set_mode_filter_coefficients;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_set_mode_parallel_filter_coeffiecients;
        break;

        default:
            event.id =anc_state_event_set_mode_filter_coefficients;
        break;
    }

    /* Assign args to the set mode event */
    args.mode = mode;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncSetModeWithSelectedGains(anc_mode_t mode, bool enable_coarse_gains, bool enable_fine_gains)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;
    anc_state_event_set_mode_coefficients_path_gains_args_t args;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_set_mode_filter_coefficients_path_gains;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_set_mode_parallel_filter_coeffiecients_path_gains;
        break;

        default:
            event.id =anc_state_event_set_mode_filter_coefficients_path_gains;
        break;
    }

    /* Assign args to the set mode event */
    args.mode = mode;
    args.enable_coarse_gains = enable_coarse_gains;
    args.enable_fine_gains = enable_fine_gains;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncSetCurrentFilterPathGains(void)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_set_all_single_filter_path_gains;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_set_parallel_filter_path_gains;
        break;

        default:
            event.id =anc_state_event_set_all_single_filter_path_gains;
        break;
    }

    event.args=NULL;
    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncIsEnabled(void)
{
    /* Get current state to determine if ANC is enabled */
    anc_state state = ancDataGetState();

    /* If library has not been initialised then it is invalid to call this function */
    ANC_ASSERT(state != anc_state_uninitialised);

    /* ANC is enabled in any state greater than anc_state_disabled, which allows
       the above assert to be compiled out if needed and this function still 
       behave as expected. */
    return (state > anc_state_disabled) ? TRUE : FALSE;
}

/******************************************************************************/

anc_mic_params_t * AncGetAncMicParams(void)
{
    return ancDataGetMicParams();
}
/******************************************************************************/
bool AncConfigureFFAPathGain(audio_anc_instance instance, uint8 gain)
{
    anc_state_event_set_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_single_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.instance = instance;
    args.path = AUDIO_ANC_PATH_ID_FFA;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureFFBPathGain(audio_anc_instance instance, uint8 gain)
{
    anc_state_event_set_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_single_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.instance = instance;
    args.path = AUDIO_ANC_PATH_ID_FFB;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureFBPathGain(audio_anc_instance instance, uint8 gain)
{
    anc_state_event_set_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_single_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.instance = instance;
    args.path = AUDIO_ANC_PATH_ID_FB;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncReadFineGain(audio_anc_path_id gain_path, uint8 *gain)
{
    return ancReadFineGain(ancDataGetMode(), gain_path, gain);
}

/******************************************************************************/
bool AncReadFineGainParallelFilter(audio_anc_path_id gain_path, uint8 *instance_0_gain, uint8 *instance_1_gain)
{
    return ancReadFineGainParallelFilter(ancDataGetMode(), gain_path, instance_0_gain, instance_1_gain);
}

/******************************************************************************/
bool AncWriteFineGain(audio_anc_path_id gain_path, uint8 gain)
{
    anc_state_event_write_gain_args_t args;
    anc_state_event_t event = {anc_state_event_write_fine_gain, NULL};

    /* Assign args to the event */
    args.path = gain_path;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

bool AncWriteFineGainParallelFilter(audio_anc_path_id gain_path, uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_write_gain_parallel_filter_args_t args;
    anc_state_event_t event = {anc_state_event_write_fine_gain_parallel_filter, NULL};

    /* Assign args to the event */
    args.path = gain_path;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
void AncReadModelCoefficients(audio_anc_instance inst, audio_anc_path_id path, uint32 *denominator, uint32* numerator)
{
    if(denominator && numerator)
    {
        ancReadModelCoefficients(inst, path, denominator, numerator);
    }
}

/******************************************************************************/
void AncReadCoarseGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint16 *gain)
{
    if (gain != NULL)
    {
        ancReadCoarseGainFromInst(inst, gain_path, gain);
    }
}

/******************************************************************************/
void AncReadFineGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint8 *gain)
{
    if (gain != NULL)
    {
        ancReadFineGainFromInst(inst, gain_path, gain);
    }
}

/******************************************************************************/
void AncReadRxMixCoarseGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint16 *gain)
{
    if (gain != NULL)
    {
#ifdef ANC_UPGRADE_FILTER
        ancReadRxMixCoarseGainFromInst(inst, gain_path, gain);
#else
        UNUSED(inst);
        UNUSED(gain_path);
        *gain = 0;
#endif
    }
}

/******************************************************************************/
void AncReadRxMixFineGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint8 *gain)
{
    if (gain != NULL)
    {
#ifdef ANC_UPGRADE_FILTER
        ancReadRxMixFineGainFromInst(inst, gain_path, gain);
#else
        UNUSED(inst);
        UNUSED(gain_path);
        *gain = 0;
#endif
    }
}

/******************************************************************************/
bool AncConfigureParallelFilterFFAPathGain(uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_set_parallel_filter_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_parallel_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.path = AUDIO_ANC_PATH_ID_FFA;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}
/******************************************************************************/
bool AncConfigureParallelFilterFFBPathGain(uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_set_parallel_filter_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_parallel_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.path = AUDIO_ANC_PATH_ID_FFB;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}
/******************************************************************************/

bool AncConfigureParallelFilterFBPathGain(uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_set_parallel_filter_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_parallel_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.path = AUDIO_ANC_PATH_ID_FB;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain  = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
uint32 AncReadNumOfDenominatorCoefficients(void)
{
    return NUMBER_OF_IIR_COEFF_DENOMINATORS;
}

/******************************************************************************/
uint32 AncReadNumOfNumeratorCoefficients(void)
{
    return NUMBER_OF_IIR_COEFF_NUMERATORS;
}
/******************************************************************************/

void AncSetDevicePsKey(uint16 device_ps_key)
{
    device_ps_key_config.device_ps_key = device_ps_key;
}
/******************************************************************************/
