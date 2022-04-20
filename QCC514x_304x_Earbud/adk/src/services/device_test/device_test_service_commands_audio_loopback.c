/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of the audio loopback commands of the device test service
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_audio.h"
#include "device_test_parse.h"

#include <kymera.h>

#include <stdio.h>
#include <logging.h>

static microphone_number_t device_test_service_loopback_mic_used = microphone_none;

/* See header file for how values are packed */
bool DeviceTestService_SpeakerExists(uint16 speaker, appKymeraHardwareOutput *output)
{
    audio_hardware hardware;
    audio_instance instance;
    audio_channel  channel;

    channel = (audio_channel)(speaker & 0x3);
    instance = (audio_instance)((speaker >> 2) & 0x3);
    hardware = (audio_hardware)(speaker >> 4);

    if (hardware < AUDIO_HARDWARE_DIGITAL_MIC)
    {
        output->hardware = hardware;
        output->instance = instance;
        output->channel = channel;
        return TRUE;
    }
    DEBUG_LOG_WARN("DeviceTestService_SpeakerExists. Speaker 0x%x appears invalid. hardware:%d instance:%d channel:%d",
                    speaker, hardware, instance, channel);

    return FALSE;
}


bool DeviceTestService_MicrophoneExists(uint16 mic)
{
    return (microphone_1 <= mic) && (mic < microphone_max);
}


/*! Command handler for AT+LOOPBACKSTART = %d:micSelection, %d:speakerSelection, %d:sampleRate 

    The function decides if the command is allowed, and if so starts loopback
    assuming that the audio subsystem is/can be in the correct state.

    Otherwise errors are reported

    \param[in] task The task to be used in command responses
    \param[in] start_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleAudioLoopbackStart(Task task,
                         const struct DeviceTestServiceCommand_HandleAudioLoopbackStart *start_params)
{
    appKymeraHardwareOutput hardware = {0};

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAudioLoopbackStart");

    UNUSED(start_params);

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    if (   !DeviceTestService_SpeakerExists(start_params->speakerSelection, &hardware)
        || !DeviceTestService_MicrophoneExists(start_params->micSelection)
        || !Kymera_IsIdle()
        || (device_test_service_loopback_mic_used != microphone_none))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    device_test_service_loopback_mic_used = start_params->micSelection;
    appKymeraCreateLoopBackAudioChain(device_test_service_loopback_mic_used,
                                      start_params->sampleRate);

    DeviceTestService_CommandResponseOk(task);
}

/*! Command handler for AT+LOOPBACKSTOP

    The function decides if the command is allowed, and if so stops loopback.

    Otherwise errors are reported

    \note If loopback is not running, the response will still be OK.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleAudioLoopbackStop(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleAudioLoopbackStart");

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    appKymeraDestroyLoopbackAudioChain(device_test_service_loopback_mic_used);
    device_test_service_loopback_mic_used = microphone_none;

    DeviceTestService_CommandResponseOk(task);
}

/*! @} End of group documentation */

