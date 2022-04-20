/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       wired_audio_source_interface.c
\brief      wired audio source interface definitions
*/

#include "wired_audio_source_interface.h"
#include "kymera_adaptation_audio_protected.h"
#include "wired_audio_private.h"
#include "audio_sources.h"
#include "volume_system.h"
#include "wired_audio_source.h"
#include "logging.h"

#include <stdlib.h>
#include <panic.h>

static bool wiredAudio_GetWiredAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params);
static void wiredAudio_FreeWiredAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params);
static bool wiredAudio_GetWiredAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);
static void wiredAudio_FreeWiredAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params);
static bool wiredAudio_IsAudioRouted(audio_source_t source);
static source_status_t wiredAudio_SetState(audio_source_t source, source_state_t state);

static const audio_source_audio_interface_t wired_source_audio_interface =
{
    .GetConnectParameters = wiredAudio_GetWiredAudioConnectParameters,
    .ReleaseConnectParameters = wiredAudio_FreeWiredAudioConnectParameters,
    .GetDisconnectParameters = wiredAudio_GetWiredAudioDisconnectParameters,
    .ReleaseDisconnectParameters = wiredAudio_FreeWiredAudioDisconnectParameters,
    .IsAudioRouted = wiredAudio_IsAudioRouted,
    .SetState = wiredAudio_SetState
};


static bool wiredAudio_GetWiredAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    switch(source)
    {
        case audio_source_line_in:
            {
                wired_analog_connect_parameters_t *conn_param = (wired_analog_connect_parameters_t*)malloc(sizeof(wired_analog_connect_parameters_t));
                
                PanicNull(conn_param);
                
                conn_param->rate = WiredAudioSourceGetTaskData()->rate;
                conn_param->max_latency = WiredAudioSourceGetTaskData()->max_latency;
                conn_param->min_latency = WiredAudioSourceGetTaskData()->min_latency;
                conn_param->target_latency = WiredAudioSourceGetTaskData()->target_latency;
                conn_param->volume = AudioSources_CalculateOutputVolume(audio_source_line_in);

                source_params->data = (wired_analog_connect_parameters_t*)conn_param;
                source_params->data_length = sizeof(wired_analog_connect_parameters_t);
                
            }
            break;
        
         default:
            return FALSE;
    }
   return TRUE;
}

static void wiredAudio_FreeWiredAudioConnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
    switch(source)
    {
        case audio_source_line_in:
            {
                PanicNull(source_params);
                PanicFalse(source_params->data_length == sizeof(wired_analog_connect_parameters_t));
                if(source_params->data_length)
                {
                    free(source_params->data);
                    source_params->data = (void *)NULL;
                    source_params->data_length = 0;
                }
            }
            break;
        
        default:
            break;
    }
}

static bool wiredAudio_GetWiredAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
   UNUSED(source);
   UNUSED(source_params);

   return TRUE;
}

static void wiredAudio_FreeWiredAudioDisconnectParameters(audio_source_t source, source_defined_params_t * source_params)
{
   UNUSED(source);
   UNUSED(source_params);

   return;
}

static bool wiredAudio_IsAudioRouted(audio_source_t source)
{
#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
    return WiredAudioSource_IsAudioAvailable(source);
#else
    UNUSED(source);
    return FALSE;
#endif
}

const audio_source_audio_interface_t * WiredAudioSource_GetWiredAudioInterface(void)
{
    return &wired_source_audio_interface;
}

/***************************************************************************
DESCRIPTION
    Set the wired audio state
*/
static source_status_t wiredAudio_SetState(audio_source_t source, source_state_t state)
{
    DEBUG_LOG("wiredAudio_SetState source=%d state=%d", source, state);
    /* Function logic to be implemented. Update unit test too*/
    return source_status_ready;
}
