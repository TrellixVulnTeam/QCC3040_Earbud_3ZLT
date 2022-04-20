/*****************************************************************************
Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.

FILE NAME
    ama_state.c
*/

#ifdef INCLUDE_AMA
#include <bdaddr.h>
#include <boot.h>
#include <connection.h>
#include <file.h>
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <led.h>
#include <message.h>
#include <panic.h>
#include <pio.h>
#include <region.h>
#include <service.h>
#include <sink.h>
#include <source.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stream.h>
#include <util.h>
#include <vm.h>
#include <psu.h>
#include <charger.h>
#include <a2dp.h>
#include <audio_plugin_music_variants.h>
#include <transform.h>
#include <loader.h>
#include <partition.h>
#include <micbias.h>
#include <vmal.h>
#include <gatt_ama_server.h>
#include "ama_send_command.h"
#include <logging.h>

#include "ama_debug.h"
#include "ama_state.h"
#include "ama_eq.h"

typedef struct __AmaFeatureState
{
    bool auxiliaryConnected;
    bool bluetoothA2dpEnabled;
    bool bluetoothHfpEnabled;

}AmaFeatureState;

static AmaFeatureState amaFeatureState;

void AmaState_Init(void)
{
    amaFeatureState.auxiliaryConnected = FALSE;
    amaFeatureState.bluetoothA2dpEnabled = TRUE;
    amaFeatureState.bluetoothHfpEnabled = TRUE;
}

ama_error_code_t AmaState_GetState(uint32 feature, uint32* Pstate, ama_state_value_case_t* pValueCase)
{
    *pValueCase = AMA_STATE_VALUE_NOT_SET;

    ama_error_code_t errorCode = ama_error_code_success;

    DEBUG_LOG("AmaState_GetState feature %x", feature);

    switch(feature)
    {
        case AMA_FEATURE_AUXILIARY_CONNECTED:
            *Pstate = (uint32)amaFeatureState.auxiliaryConnected;
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_A2DP_ENABLED:
            *Pstate = (uint32)amaFeatureState.bluetoothA2dpEnabled;
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_HFP_ENABLED:
            *Pstate = (uint32)amaFeatureState.bluetoothHfpEnabled;
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_A2DP_CONNECTED:
            // TBD *Pstate = (uint32)amaFeatureState.bluetoothHfpEnabled;
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_HFP_CONNECTED:
            // TBD *Pstate = (uint32)amaFeatureState.bluetoothHfpEnabled;
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_CLASSIC_DISCOVERABLE:
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;
        case AMA_FEATURE_DEVICE_CALIBRATION_REQUIRED:
            *pValueCase = AMA_STATE_VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_DEVICE_THEME:
            *pValueCase = AMA_STATE_VALUE_INTEGER;
            break;

#ifdef INCLUDE_AMA_DEVICE_CONTROLS
        case AMA_FEATURE_EQUALIZER_BASS:
            *Pstate = Ama_EqGetEqualizerBass();
            *pValueCase = AMA_STATE_VALUE_INTEGER;
            break;

        case AMA_FEATURE_EQUALIZER_MID:
            *Pstate = Ama_EqGetEqualizerMid();
            *pValueCase = AMA_STATE_VALUE_INTEGER;
            break;

        case AMA_FEATURE_EQUALIZER_TREBLE:
            *Pstate = Ama_EqGetEqualizerTreble();
            *pValueCase = AMA_STATE_VALUE_INTEGER;
            break;
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */


        /* cannot get state for the features below */
        case AMA_FEATURE_DEVICE_DND_ENABLED:
        case AMA_FEATURE_DEVICE_CELLULAR_CONNECTIVITY_STATUS:
        case AMA_FEATURE_MESSAGE_NOTIFICATION:
        case AMA_FEATURE_REMOTE_NOTIFICATION:
        case AMA_FEATURE_CALL_NOTIFICATION:
            errorCode = ama_error_code_unsupported;
            break;


       default:
            errorCode = ama_error_code_invalid;
            break;

        }


    return errorCode;
}

ama_error_code_t AmaState_SetState(uint32 feature, uint32 state, ama_state_value_case_t valueCase)
{
    ama_error_code_t errorCode = ama_error_code_success;

    DEBUG_LOG("AmaState_SetState feature %x state %u", feature, state);

    if(valueCase != AMA_STATE_VALUE_BOOLEAN &&
       valueCase != AMA_STATE_VALUE_INTEGER)
    {

        return ama_error_code_unsupported;
    }

    switch(feature)
    {
        case AMA_FEATURE_BLUETOOTH_A2DP_ENABLED:
        case AMA_FEATURE_BLUETOOTH_HFP_ENABLED:
        case AMA_FEATURE_BLUETOOTH_CLASSIC_DISCOVERABLE:
        case AMA_FEATURE_DEVICE_CALIBRATION_REQUIRED:

        case AMA_FEATURE_DEVICE_THEME:
             //   *pValueCase = AMA_STATE_VALUE_INTEGER;
        break;

#ifdef INCLUDE_AMA_DEVICE_CONTROLS
       case AMA_FEATURE_EQUALIZER_BASS:
           Ama_EqSetEqualizerBass(state);
           break;
       
       case AMA_FEATURE_EQUALIZER_MID:
           Ama_EqSetEqualizerMid(state);
           break;
       
       case AMA_FEATURE_EQUALIZER_TREBLE:
           Ama_EqSetEqualizerTreble(state);
           break;
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */


       case AMA_FEATURE_AUXILIARY_CONNECTED:
       case AMA_FEATURE_BLUETOOTH_A2DP_CONNECTED:
       case AMA_FEATURE_BLUETOOTH_HFP_CONNECTED:

       errorCode = ama_error_code_unsupported;
       break;


       default:
       break;
    }

    return errorCode;
}


bool AmaState_SendIntegerStateEvent(uint32 feature ,uint16 integer, bool get)
{
    bool ret = TRUE;

    switch(feature)
    {
        case    AMA_FEATURE_DEVICE_THEME:
        case    AMA_FEATURE_DEVICE_CELLULAR_CONNECTIVITY_STATUS:
        case    AMA_FEATURE_MESSAGE_NOTIFICATION:
        case    AMA_FEATURE_CALL_NOTIFICATION:
        case    AMA_FEATURE_REMOTE_NOTIFICATION:
            if(get == TRUE)
            {
                AmaSendCommand_GetState(feature);
            }
            else
            {
                /* send sync */
                AmaSendCommand_SyncState(feature, AMA_STATE_VALUE_INTEGER, integer);
            }
        break;

        default:
            ret = FALSE;
        break;
    }

    return ret;
}


bool AmaState_SendBooleanStateEvent(uint32 feature,bool True, bool get)
{
    bool ret = TRUE;

    switch(feature)
    {
        case    AMA_FEATURE_DEVICE_DND_ENABLED:
            if(get == TRUE)
            {
                AmaSendCommand_GetState(feature);
            }
            else
            {
                AmaSendCommand_SyncState(feature, AMA_STATE_VALUE_BOOLEAN, True);
            }

        break;

        default:
            ret = FALSE;
        break;
    }

    return ret;
}

#endif /* INCLUDE_AMA */
