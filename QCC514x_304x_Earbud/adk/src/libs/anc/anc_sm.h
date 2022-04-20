/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_sm.h

DESCRIPTION
    Entry point to the ANC VM Library State Machine.
*/

#ifndef ANC_SM_H_
#define ANC_SM_H_

#include "anc.h"

#include <csrtypes.h>

/* ANC VM Library States. The order is important due to the way the API
   functions are implemented */
typedef enum
{
    anc_state_uninitialised,
    anc_state_disabled,
    anc_state_enabled
} anc_state;

/* ANC VM Library State Events */
typedef enum
{
    anc_state_event_initialise,
    anc_state_event_set_topology,
    anc_state_event_enable,
    anc_state_event_enable_parallel_filter,
    anc_state_event_enable_with_mute_path_gains,
    anc_state_event_enable_parallel_anc_with_mute_path_gains,
    anc_state_event_set_parallel_filter_path_gains,
    anc_state_event_disable,
    anc_state_event_disable_parallel_filter,
    anc_state_event_set_mode,
    anc_state_event_set_parallel_mode,
    anc_state_event_set_mode_filter_coefficients,
    anc_state_event_set_mode_parallel_filter_coeffiecients,
    anc_state_event_set_single_filter_path_gain,
    anc_state_event_set_all_single_filter_path_gains,
    anc_state_event_set_parallel_filter_path_gain,
    anc_state_event_write_fine_gain,
    anc_state_event_write_fine_gain_parallel_filter,
    anc_state_event_set_mode_filter_coefficients_path_gains,
    anc_state_event_set_mode_parallel_filter_coeffiecients_path_gains
} anc_state_event_id;

/* Event structure that is used to inject an event and any corresponding
   arguments into the state machine. */
typedef struct
{
    anc_state_event_id id;
    void *args;
} anc_state_event_t;

/* Event arguments for the initialise event */
typedef struct
{
    anc_mic_params_t *mic_params;
    anc_mode_t mode;
} anc_state_event_initialise_args_t;

/* Event arguments for the set_mode event */
typedef struct
{
    anc_mode_t mode;
} anc_state_event_set_mode_args_t;

/* Event arguments for the set_mode event */
typedef struct
{
    anc_mode_t mode;
    bool enable_coarse_gains;
    bool enable_fine_gains;
} anc_state_event_set_mode_coefficients_path_gains_args_t;

typedef struct
{
    anc_filter_topology_t anc_topology;
}anc_state_event_set_topology_args_t;

/* Event arguments for the set_mode event */
typedef struct
{
    audio_anc_instance instance;
    audio_anc_path_id path;
    unsigned gain;
} anc_state_event_set_path_gain_args_t;

typedef struct
{
    audio_anc_path_id path;
    unsigned instance_0_gain;
    unsigned instance_1_gain;
}anc_state_event_set_parallel_filter_path_gain_args_t;

/* Event arguments for the write gain to PS event */
typedef struct
{
    audio_anc_path_id path;
    unsigned gain;
} anc_state_event_write_gain_args_t;

/* Event arguments for the write gain to PS event for parallel filter */
typedef struct
{
    audio_anc_path_id path;
    unsigned instance_0_gain;
    unsigned instance_1_gain;
} anc_state_event_write_gain_parallel_filter_args_t;

/* Event arguments for the enable event with user gain configuration */
typedef struct
{
    anc_user_gain_config_t *gain_config_left;
    anc_user_gain_config_t *gain_config_right;
} anc_state_event_enable_with_user_gain_args_t;

/******************************************************************************
NAME
    ancStateMachineHandleEvent

DESCRIPTION
    Main entry point to the ANC VM library State Machine. All events will be
    injected using this function that will then determine which state specific
    handler should process the event.

RETURNS
    Bool indicating if the event was successfully processed.
*/
bool ancStateMachineHandleEvent(anc_state_event_t event);

#endif

