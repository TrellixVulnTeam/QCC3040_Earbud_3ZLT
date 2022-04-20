/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of functions to handle capture source
*/

#include "voice_audio_manager_capture.h"
#include "voice_audio_manager_sm.h"
#include <logging.h>
#include <message.h>
#include <panic.h>

#define DEFAULT_CAPTURE_TIMEOUT 5

typedef enum
{
    CAPTURE_DATA_RECEIVED_TIMEOUT
} internal_message_ids_t;

static void voiceAudioManager_MsgHandler(Task task, MessageId id, Message message);
static const TaskData msg_handler = { voiceAudioManager_MsgHandler };

static VaAudioCaptureDataReceived mic_data_available = NULL;
static Source capture_source = NULL;

static void voiceAudioManager_MicDataReceived(Source source)
{
    MessageCancelAll((Task) &msg_handler, CAPTURE_DATA_RECEIVED_TIMEOUT);

    if (VoiceAudioManager_IsMicDataExpected())
    {
        unsigned timeout = 0;
        unsigned source_size = SourceSize(source);
        PanicFalse(mic_data_available != NULL);

        if (source_size)
        {
            timeout = mic_data_available(source);
        }

        if (timeout)
        {
            MessageSendLater((Task) &msg_handler, CAPTURE_DATA_RECEIVED_TIMEOUT, NULL, timeout);
        }
        else
        {
            MessageSendLater((Task) &msg_handler, CAPTURE_DATA_RECEIVED_TIMEOUT, NULL, DEFAULT_CAPTURE_TIMEOUT);
        }
    }
}

static void voiceAudioManager_MsgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        case MESSAGE_MORE_DATA:
        {
            DEBUG_LOG_V_VERBOSE("voiceAudioManager_MsgHandler MESSAGE_MORE_DATA");
            Source source = ((MessageMoreData *) message)->source;

            /* Ignore any message not from the current capture source */
            if (source == capture_source)
            {
                voiceAudioManager_MicDataReceived(source);
            }
        }
        break;

        case CAPTURE_DATA_RECEIVED_TIMEOUT:
            DEBUG_LOG_VERBOSE("voiceAudioManager_MsgHandler CAPTURE_DATA_RECEIVED_TIMEOUT");
            voiceAudioManager_MicDataReceived(capture_source);
        break;
    }
}

static void voiceAudioManager_RegisterSource(Source source)
{
    MessageStreamTaskFromSource(source, (Task) &msg_handler);
    PanicFalse(SourceConfigure(source, VM_SOURCE_MESSAGES, VM_MESSAGES_SOME));
}

static void voiceAudioManager_DeregisterSource(Source source)
{
    MessageStreamTaskFromSource(source, NULL);
}

void VoiceAudioManager_CaptureStopping(void)
{
    if (capture_source)
    {
        voiceAudioManager_DeregisterSource(capture_source);
        MessageFlushTask((Task) &msg_handler);
        capture_source = NULL;
    }
}

void VoiceAudioManager_CaptureStarting(VaAudioCaptureDataReceived callback)
{
    mic_data_available = callback;
}

void VoiceAudioManager_CaptureStarted(Source source)
{
    if (VoiceAudioManager_IsMicDataExpected())
    {
        capture_source = source;
        voiceAudioManager_RegisterSource(source);
        voiceAudioManager_MicDataReceived(source);
    }
}

void VoiceAudioManager_CaptureTestReset(void)
{
    mic_data_available = NULL;
    capture_source = NULL;
}
