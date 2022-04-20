/*****************************************************************************
Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    ama_command_handlers.h

DESCRIPTION
    Handlers for ama commands
*/

#ifndef _AMA_COMMAND_HANDLERS_H
#define _AMA_COMMAND_HANDLERS_H

#include "accessories.pb-c.h"

/***************************************************************************
DESCRIPTION
    Handles COMMAND__NOTIFY_SPEECH_STATE message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_NotifySpeechState(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__STOP_SPEECH message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_StopSpeech(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__GET_DEVICE_INFORMATION message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_GetDeviceInformation(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__GET_DEVICE_CONFIGURATION message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_GetDeviceConfiguration(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__START_SETUP message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_StartSetup(ControlEnvelope * control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__COMPLETE_SETUP message 
 
PARAMS
    control_envelope The message from the phone
 
*/
void AmaCommandHandlers_CompleteSetup(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__UPGRADE_TRANSPORT message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_UpgradeTransport(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__SWITCH_TRANSPORT message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_SwitchTransport(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__NOTIFY_SPEECH_STATE message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_SynchronizeSettings(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__GET_STATE message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_GetState(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__SET_STATE message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_SetState(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__ISSUE_MEDIA_CONTROL message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_MediaControl(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__OVERRIDE_ASSISTANT message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_OverrideAssistant(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__PROVIDE_SPEECH message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_ProvideSpeech(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__ENDPOINT_SPEECH message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_EndpointSpeech(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__FORWARD_AT_COMMAND message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_ForwardATCommand(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles when the message id is unknown
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_NotHandled(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__KEEP_ALIVE message 
 
PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_KeepAlive(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__SYNCHRONIZE_STATE message

PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_SynchronizeState(ControlEnvelope *control_envelope_in);

#ifdef INCLUDE_AMA_DEVICE_CONTROLS
/***************************************************************************
DESCRIPTION
    Handles COMMAND__GET_DEVICE_FEATURES message

PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_GetDeviceFeatures(ControlEnvelope *control_envelope_in);
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */

/***************************************************************************
DESCRIPTION
    Handles COMMAND__GET_LOCALES message

PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_GetLocales(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__SET_LOCALE message

PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_SetLocale(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Handles COMMAND__LAUNCH_APP message

PARAMS
    control_envelope The message from the phone
*/
void AmaCommandHandlers_LaunchApp(ControlEnvelope *control_envelope_in);

/***************************************************************************
DESCRIPTION
    Populates a DeviceInformation structure

PARAMS
    device_information Pointer to the DeviceInformation structure
*/
void AmaCommandHandlers_PopulateDeviceInformation(DeviceInformation *device_information);

#ifdef INCLUDE_AMA_DEVICE_CONTROLS
/***************************************************************************
DESCRIPTION
    Populates a DeviceFeatures structure

PARAMS
    device_features Pointer to the DeviceFeatures structure
*/
void AmaCommandHandlers_PopulateDeviceFeatures(DeviceFeatures *device_features);
#endif /* INCLUDE_AMA_DEVICE_CONTROLS */

#endif /* _AMA_COMMAND_HANDLERS_H */
