/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions for generating volume update request messages

*/

#include "audio_sources.h"
#include "voice_sources.h"
#include "volume_messages.h"

#include <message.h>
#include <task_list.h>
#include <panic.h>
#include <logging.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(volume_domain_messages)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(VOLUME, VOLUME_MESSAGE_END)

#endif

static task_list_t * client_list;

static task_list_t * volume_GetMessageClients(void)
{
    return client_list;
}

void Volume_SendVoiceSourceVolumeUpdateRequest(voice_source_t source, event_origin_t origin, int volume)
{
    MESSAGE_MAKE(message, voice_source_volume_update_request_message_t);
    message->voice_source = source;
    message->origin = origin;
    message->volume = VoiceSources_GetVolume(source);
    message->volume.value = volume;
    DEBUG_LOG("Volume_SendVoiceSourceVolumeUpdateRequest, enum:voice_source_t:%u, enum:event_origin_t:%u, volume %u", source, origin, volume);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), VOICE_SOURCE_VOLUME_UPDATE_REQUEST, message,
            sizeof(voice_source_volume_update_request_message_t));
}

void Volume_SendVoiceSourceVolumeIncrementRequest(voice_source_t source, event_origin_t origin)
{
    MESSAGE_MAKE(message, voice_source_volume_increment_request_message_t);
    message->voice_source = source;
    message->origin = origin;
    DEBUG_LOG("Volume_SendVoiceSourceVolumeIncrementRequest, enum:voice_source_t:%u, enum:event_origin_t:%u", source, origin);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), VOICE_SOURCE_VOLUME_INCREMENT_REQUEST, message,
            sizeof(voice_source_volume_increment_request_message_t));
}

void Volume_SendVoiceSourceVolumeDecrementRequest(voice_source_t source, event_origin_t origin)
{
    MESSAGE_MAKE(message, voice_source_volume_decrement_request_message_t);
    message->voice_source = source;
    message->origin = origin;
    DEBUG_LOG("Volume_SendVoiceSourceVolumeDecrementRequest, enum:voice_source_t:%u, enum:event_origin_t:%u", source, origin);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), VOICE_SOURCE_VOLUME_DECREMENT_REQUEST, message,
            sizeof(voice_source_volume_decrement_request_message_t));
}

void Volume_SendVoiceSourceMuteRequest(voice_source_t source, event_origin_t origin, bool mute_state)
{
    MESSAGE_MAKE(message, voice_source_mute_volume_request_message_t);
    message->voice_source = source;
    message->origin = origin;
    message->mute_state = mute_state;
    DEBUG_LOG("Volume_SendVoiceSourceMuteRequest, enum:voice_source_t:%u, enum:event_origin_t:%u, mute_state %u", source, origin, mute_state);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), VOICE_SOURCE_MUTE_VOLUME_REQUEST, message,
                    sizeof(voice_source_mute_volume_request_message_t));
}

void Volume_SendAudioSourceVolumeUpdateRequest(audio_source_t source, event_origin_t origin, int volume)
{
    MESSAGE_MAKE(message, audio_source_volume_update_request_message_t);
    message->audio_source = source;
    message->origin = origin;
    message->volume = AudioSources_GetVolume(source);
    message->volume.value = volume;
    DEBUG_LOG("Volume_SendAudioSourceVolumeUpdateRequest, enum:audio_source_t:%u, enum:event_origin_t:%u, volume %u", source, origin, volume);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), AUDIO_SOURCE_VOLUME_UPDATE_REQUEST, message,
            sizeof(audio_source_volume_update_request_message_t));
}

void Volume_SendAudioSourceVolumeIncrementRequest(audio_source_t source, event_origin_t origin)
{
    MESSAGE_MAKE(message, audio_source_volume_increment_request_message_t);
    message->audio_source = source;
    message->origin = origin;
    DEBUG_LOG("Volume_SendAudioSourceVolumeIncrementRequest, enum:audio_source_t:%u, enum:event_origin_t:%u", source, origin);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), AUDIO_SOURCE_VOLUME_INCREMENT_REQUEST, message,
            sizeof(audio_source_volume_increment_request_message_t));
}

void Volume_SendAudioSourceVolumeDecrementRequest(audio_source_t source, event_origin_t origin)
{
    MESSAGE_MAKE(message, audio_source_volume_decrement_request_message_t);
    message->audio_source = source;
    message->origin = origin;
    DEBUG_LOG("Volume_SendAudioSourceVolumeDecrementRequest, enum:audio_source_t:%u, enum:event_origin_t:%u", source, origin);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), AUDIO_SOURCE_VOLUME_DECREMENT_REQUEST, message,
            sizeof(audio_source_volume_decrement_request_message_t));
}

void Volume_SendAudioSourceMuteRequest(audio_source_t source, event_origin_t origin, bool mute_state)
{
    MESSAGE_MAKE(message, audio_source_mute_volume_request_message_t);
    message->audio_source = source;
    message->origin = origin;
    message->mute_state = mute_state;
    DEBUG_LOG("Volume_SendAudioSourceMuteRequest, enum:audio_source_t:%u, enum:event_origin_t:%u, mute_state %u", source, origin, mute_state);
    TaskList_MessageSendWithSize(volume_GetMessageClients(), AUDIO_SOURCE_MUTE_VOLUME_REQUEST, message,
                sizeof(audio_source_mute_volume_request_message_t));
}

bool Volume_InitMessages(Task init_task)
{
    UNUSED(init_task);
    client_list = TaskList_Create();
    return TRUE;
}

void Volume_RegisterForMessages(Task task_to_register)
{
    TaskList_AddTask(volume_GetMessageClients(), task_to_register);
}

