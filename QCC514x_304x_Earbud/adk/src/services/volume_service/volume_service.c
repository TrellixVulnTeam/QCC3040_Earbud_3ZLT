/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Volume Service.
*/

#include "volume_service.h"

#include "audio_sources.h"
#include "kymera_adaptation.h"
#include "volume_system.h"
#include "voice_sources.h"
#include "volume_messages.h"
#include "volume_mute.h"
#include "volume_utils.h"

#include <av.h>
#include <focus_audio_source.h>
#include <focus_voice_source.h>
#include <focus_generic_source.h>
#include <panic.h>
#include <task_list.h>
#include <message.h>
#include <message_broker.h>
#include <logging.h>
#include <source_param_types.h>
#include <stdio.h>
#include <ui.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(volume_service_messages)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(VOLUME_SERVICE, VOLUME_SERVICE_MESSAGE_END)

#define VOLUME_SERVICE_CLIENT_TASK_LIST_INIT_CAPACITY 1

/*! Macro for creating messages */
#define MAKE_VOLUME_SERVICE_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! \brief Internal message IDs */
enum
{
    INTERNAL_MSG_APPLY_AUDIO_VOLUME,
    INTERNAL_MSG_VOLUME_RAMP_REPEAT,
};

/*! Internal message for a volume repeat */
typedef struct
{
    generic_source_t source;    /*!< Source volume ramp is applied to */
    int16 step;                 /*!< Step to adjust volume by +ve or -ve */
} INTERNAL_MSG_VOLUME_RAMP_REPEAT_T;


/*! \brief Time between volume changes being applied for a volume ramp (in milliseconds). */
#define VOLUME_RAMP_REPEAT_TIME_MSECS               (300)

/* Ui Inputs in which volume service is interested*/
static const message_group_t ui_inputs[] =
{
    UI_INPUTS_VOLUME_MESSAGE_GROUP
};

static void volumeService_OnAudioRoutingChange(audio_source_t source, audio_routing_change_t change);
static void volumeService_InternalMessageHandler( Task task, MessageId id, Message message );
static void volumeService_RefreshAudioVolume(event_origin_t origin, audio_source_t source);

static const audio_source_observer_interface_t volume_service_audio_observer_interface =
{
    .OnVolumeChange = NULL,
    .OnAudioRoutingChange = volumeService_OnAudioRoutingChange,
    .OnMuteChange = NULL,
};

typedef struct
{
    TASK_LIST_WITH_INITIAL_CAPACITY(VOLUME_SERVICE_CLIENT_TASK_LIST_INIT_CAPACITY)  client_list;
    TaskData volume_message_handler_task;
} volume_service_data;

static volume_service_data the_volume_service;

#define VolumeServiceGetClientLIst() (task_list_flexible_t *)(&the_volume_service.client_list)

static TaskData internal_message_task = { volumeService_InternalMessageHandler };

/*! \brief Make a volume change to a Voice Source.
*/
static void volumeService_ChangeVoiceVolume(voice_source_t source, int16 step)
{
    int step_size = VolumeUtils_GetStepSize(VoiceSources_GetVolume(source).config);

    if(step == step_size)
    {
        Volume_SendVoiceSourceVolumeIncrementRequest(source, event_origin_local);
    }
    else if(step == -step_size)
    {
        Volume_SendVoiceSourceVolumeDecrementRequest(source, event_origin_local);
    }
    else
    {
        int new_volume = VolumeUtils_LimitVolumeToRange((VoiceSources_GetVolume(source).value + step),
                                                        VoiceSources_GetVolume(source).config.range);

        DEBUG_LOG("volumeService_ChangeVoiceVolume enum:voice_source_t:%d new=%d", source, new_volume);

        Volume_SendVoiceSourceVolumeUpdateRequest(source, event_origin_local, new_volume);
    }
}

/*! \brief Start a volume ramp for the specified Source

    Start a repeating volume change on the specified source.

    \param source  The generic source for which to start the volume ramp
    \param step    change to be applied to volume, +ve or -ve
*/
static void volumeService_StartVolumeRamp(generic_source_t source, int16 step)
{
    DEBUG_LOG_INFO("volumeService_StartVolumeRamp enum:source_type_t:%d step=%d", source.type, step);

    if (source.type == source_type_audio)
    {
        VolumeService_ChangeAudioSourceVolume(source.u.audio, step);
    }
    else if (source.type == source_type_voice)
    {
        volumeService_ChangeVoiceVolume(source.u.voice, step);
    }

    MAKE_VOLUME_SERVICE_MESSAGE(INTERNAL_MSG_VOLUME_RAMP_REPEAT);
    message->step = step;
    message->source = source;
    MessageSendLater(&internal_message_task, INTERNAL_MSG_VOLUME_RAMP_REPEAT, message,
                     VOLUME_RAMP_REPEAT_TIME_MSECS);
}

/*! \brief Stop a volume ramp

    Cancel any active repeating volume change on a source.
*/
static void volumeService_StopVolumeRamp(void)
{
    MessageCancelFirst(&internal_message_task, INTERNAL_MSG_VOLUME_RAMP_REPEAT);
}

void volumeService_OnAudioRoutingChange(audio_source_t source, audio_routing_change_t change)
{
    UNUSED(source);

    if (change == source_unrouted)
    {
        volumeService_StopVolumeRamp();
    }
}

/*! \brief Handles UI inputs passed to the Volume Service

    Invokes routines based on ui input received from ui module.

    \param[in] id - ui input

    \returns void
 */
static void volumeService_HandleUiInput(MessageId ui_input)
{
    generic_source_t source = {0};
    int step_size = 0;
    bool handle_ui_input = FALSE;

    DEBUG_LOG_FN_ENTRY("volumeService_HandleUiInput enum:ui_input_t:%d", ui_input);

    source = Focus_GetFocusedGenericSourceForAudioRouting();
    if (GenericSource_IsValid(source))
    {
        handle_ui_input = TRUE;
        if (GenericSource_IsAudio(source))
        {
            step_size = VolumeUtils_GetStepSize(AudioSources_GetVolume(source.u.audio).config);
        }
        else
        {
            step_size = VolumeUtils_GetStepSize(VoiceSources_GetVolume(source.u.voice).config);
        }
    }

    if (handle_ui_input)
    {
        switch (ui_input)
        {
        case ui_input_volume_down_start:
            step_size = -step_size;
            // Deliberate fall through
        case ui_input_volume_up_start:
            MessageCancelAll(&internal_message_task, ui_input);
            volumeService_StartVolumeRamp(source, step_size);
            break;

        case ui_input_volume_stop:
            volumeService_StopVolumeRamp();
            break;

        case ui_input_volume_down:
            step_size = -step_size;
            // Deliberate fall through
        case ui_input_volume_up:
            if (GenericSource_IsAudio(source))
            {
                VolumeService_ChangeAudioSourceVolume(source.u.audio, step_size);
            }
            else
            {
                volumeService_ChangeVoiceVolume(source.u.voice, step_size);
            }
            break;

        default:
            break;
        }
    }
}

void VolumeService_ChangeAudioSourceVolume(audio_source_t source, int16 step)
{
    int step_size = VolumeUtils_GetStepSize(AudioSources_GetVolume(source).config);

    DEBUG_LOG_FN_ENTRY("VolumeService_ChangeAudioSourceVolume");

    if(step == step_size)
    {
        Volume_SendAudioSourceVolumeIncrementRequest(source, event_origin_local);
    }
    else if(step == -step_size)
    {
        Volume_SendAudioSourceVolumeDecrementRequest(source, event_origin_local);
    }
    else
    {
        int new_volume = VolumeUtils_LimitVolumeToRange((AudioSources_GetVolume(source).value + step),
                                                         AudioSources_GetVolume(source).config.range);
        Volume_SendAudioSourceVolumeUpdateRequest(source, event_origin_local, new_volume);
    }
}

static bool volumeService_VolumeWithinAllowedRange(volume_t volume)
{
    return volume.value < volume.config.range.max && volume.value > volume.config.range.min;
}

static void volumeService_DoVolumeRampRepeat(MessageId id, INTERNAL_MSG_VOLUME_RAMP_REPEAT_T * msg)
{
    volume_t new_volume = { 0 };
    if (msg->source.type == source_type_audio)
    {
        audio_source_t source = msg->source.u.audio;
        volume_t volume = AudioSources_GetVolume(source);
        int volume_step_size = VolumeUtils_GetStepSize(volume.config);

        new_volume = volume;
        new_volume.value += msg->step;

        if (volumeService_VolumeWithinAllowedRange(volume))
        {
            if (msg->step == volume_step_size)
            {
                VolumeService_IncrementAudioSourceVolume(source, event_origin_local);
            }
            else if (msg->step == -volume_step_size)
            {
                VolumeService_DecrementAudioSourceVolume(source, event_origin_local);
            }
            else
            {
                VolumeService_SetAudioSourceVolume(source, event_origin_local, new_volume);
            }
        }
    }
    else if (msg->source.type == source_type_voice)
    {
        voice_source_t source = msg->source.u.voice;
        volume_t volume = VoiceSources_GetVolume(source);
        int volume_step_size = VolumeUtils_GetStepSize(volume.config);

        new_volume = volume;
        new_volume.value += msg->step;

        if (volumeService_VolumeWithinAllowedRange(volume))
        {
            if (msg->step == volume_step_size)
            {
                VolumeService_IncrementVoiceSourceVolume(source, event_origin_local);
            }
            else if (msg->step == -volume_step_size)
            {
                VolumeService_DecrementVoiceSourceVolume(source, event_origin_local);
            }
            else
            {
                VolumeService_SetVoiceSourceVolume(source, event_origin_local, new_volume);
            }
        }
    }
    else
    {
        Panic();
    }

    if (volumeService_VolumeWithinAllowedRange(new_volume))
    {
        MAKE_VOLUME_SERVICE_MESSAGE(INTERNAL_MSG_VOLUME_RAMP_REPEAT);
        memcpy(message, msg, sizeof(INTERNAL_MSG_VOLUME_RAMP_REPEAT_T));
        MessageSendLater(&internal_message_task, id, message, VOLUME_RAMP_REPEAT_TIME_MSECS);
    }
}

static void volumeService_InternalMessageHandler( Task task, MessageId id, Message msg )
{
    UNUSED(task);

    if (isMessageUiInput(id))
    {
        volumeService_HandleUiInput(id);
    }
    else
    {
        switch(id)
        {
        case INTERNAL_MSG_APPLY_AUDIO_VOLUME:
            {
                generic_source_t focused_source = Focus_GetFocusedGenericSourceForAudioRouting();
                if (GenericSource_IsAudio(focused_source))
                {
                    volumeService_RefreshAudioVolume(event_origin_local, focused_source.u.audio);
                }
            }
            break;

        case INTERNAL_MSG_VOLUME_RAMP_REPEAT:
            volumeService_DoVolumeRampRepeat(id, (INTERNAL_MSG_VOLUME_RAMP_REPEAT_T *) msg);
            break;

        default:
            Panic();
            break;
        }
    }
}

#define volumeService_VolumeIsMax(volume) (volume.value >= volume.config.range.max)
#define volumeService_VolumeIsMin(volume) (volume.value <= volume.config.range.min)

static void volumeService_NotifyMinOrMaxVolume(volume_t volume)
{
    if(volumeService_VolumeIsMax(volume))
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(VolumeServiceGetClientLIst()), VOLUME_SERVICE_MAX_VOLUME);
    }
    if(volumeService_VolumeIsMin(volume))
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(VolumeServiceGetClientLIst()), VOLUME_SERVICE_MIN_VOLUME);
    }
}

static void volumeService_NotifyMinOrMaxGenericVolume(generic_source_t source, volume_t volume)
{
    if(volumeService_VolumeIsMax(volume) || volumeService_VolumeIsMin(volume))
    {
        generic_source_t routed_source = Focus_GetFocusedGenericSourceForAudioRouting();
        bool is_nothing_routed = !GenericSource_IsValid(routed_source);

        DEBUG_LOG_VERBOSE("volumeService_NotifyMinOrMaxGenericVolume src(enum:source_type_t:%d,%d), routed_src(enum:source_type_t:%d,%d)",
                          source.type, source.u.audio, routed_source.type, routed_source.u.audio);

        if((GenericSource_IsValid(routed_source) && GenericSource_IsSame(source, routed_source)) || is_nothing_routed)
        {
            volumeService_NotifyMinOrMaxVolume(volume);
        }
    }
}

static void volumeService_NotifyMinOrMaxAudioVolume(audio_source_t source, volume_t volume)
{
    GENERIC_AUDIO_SOURCE_MAKE(source);
    volumeService_NotifyMinOrMaxGenericVolume(generic_source, volume);
}

static void volumeService_NotifyMinOrMaxVoiceVolume(voice_source_t source, volume_t volume)
{
    GENERIC_VOICE_SOURCE_MAKE(source);
    volumeService_NotifyMinOrMaxGenericVolume(generic_source, volume);
}

static void volumeService_RefreshVoiceVolume(voice_source_t voice_source)
{
    DEBUG_LOG_FN_ENTRY("volumeService_RefreshVoiceVolume enum:voice_source_t:%d", voice_source);

    volume_t volume = VoiceSources_CalculateOutputVolume(voice_source);
    volume_parameters_t volume_params = { .source_type = source_type_voice, .u.voice = voice_source, .volume = volume };
    KymeraAdaptation_SetVolume(&volume_params);
}

static bool isVolumeToBeSynchronised(void)
{
    /* Doesn't exist yet */
    return FALSE;
}

static uint16 getSynchronisedVolumeDelay(void)
{
    return 0;
}

static void volumeService_RefreshAudioVolume(event_origin_t origin, audio_source_t audio_source)
{
    if ((origin == event_origin_local) && isVolumeToBeSynchronised())
    {
        MessageSendLater(&internal_message_task, INTERNAL_MSG_APPLY_AUDIO_VOLUME, 0, getSynchronisedVolumeDelay());
    }
    else
    {
        volume_t volume = AudioSources_CalculateOutputVolume(audio_source);
        volume_parameters_t volume_params = { .source_type = source_type_audio, .u.audio = audio_source, .volume = volume };
        KymeraAdaptation_SetVolume(&volume_params);
    }
}

static void volumeService_RefreshCurrentVolume(event_origin_t origin)
{
    generic_source_t focused_source = Focus_GetFocusedGenericSourceForAudioRouting();

    DEBUG_LOG_INFO("volumeService_RefreshCurrentVolume src=(enum:source_type_t:%d,%d)",
                   focused_source.type,
                   focused_source.u.voice);

    if (GenericSource_IsVoice(focused_source))
    {
        volumeService_RefreshVoiceVolume(focused_source.u.voice);
    }
    else if (GenericSource_IsAudio(focused_source))
    {
        volumeService_RefreshAudioVolume(origin, focused_source.u.audio);
    }
}

static void volumeService_UpdateAudioSourceVolume(audio_source_t source, volume_t new_volume, event_origin_t origin)
{
    AudioSources_SetVolume(source, new_volume);
    AudioSources_OnVolumeChange(source, origin, new_volume);

    generic_source_t focused_source = Focus_GetFocusedGenericSourceForAudioRouting();
    if (GenericSource_IsAudio(focused_source) && focused_source.u.audio == source)
    {
        volumeService_RefreshAudioVolume(origin, source);
    }
}

static void volumeService_UpdateSystemVolume(volume_t new_volume, event_origin_t origin)
{
    Volume_SetSystemVolume(new_volume);
    volumeService_RefreshCurrentVolume(origin);
}

static void volumeService_UpdateVoiceSourceLocalVolume(voice_source_t source, volume_t new_volume, event_origin_t origin)
{
    DEBUG_LOG_FN_ENTRY("volumeService_UpdateVoiceSourceLocalVolume");
    VoiceSources_SetVolume(source, new_volume);

    generic_source_t focused_source = Focus_GetFocusedGenericSourceForAudioRouting();
    if (GenericSource_IsVoice(focused_source) && focused_source.u.voice == source)
    {
        VoiceSources_OnVolumeChange(source, origin, new_volume);
        volumeService_RefreshVoiceVolume(source);
    }
}

void VolumeService_SetAudioSourceVolume(audio_source_t source, event_origin_t origin, volume_t new_volume)
{
    volume_t source_volume = AudioSources_GetVolume(source);
    DEBUG_LOG("VolumeService_SetAudioSourceVolume, enum:audio_source_t:%d enum:event_origin_t:%d volume %u",
              source, origin, new_volume.value);
    source_volume.value = VolumeUtils_ConvertToVolumeConfig(new_volume, source_volume.config);

    if(AudioSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        AudioSources_VolumeSetAbsolute(source, source_volume);
    }
    else
    {
        volumeService_UpdateAudioSourceVolume(source, source_volume, origin);
    }
    volumeService_NotifyMinOrMaxAudioVolume(source, source_volume);
}

void VolumeService_IncrementAudioSourceVolume(audio_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_IncrementAudioSourceVolume enum:audio_source_t:%d enum:event_origin_t:%d",
              source, origin);
    if (AudioSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        AudioSources_VolumeUp(source);
    }
    else
    {
        volume_t source_volume = AudioSources_GetVolume(source);
        source_volume.value = VolumeUtils_IncrementVolume(source_volume);
        volumeService_UpdateAudioSourceVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxAudioVolume(source, source_volume);
    }
}

void VolumeService_DecrementAudioSourceVolume(audio_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_DecrementAudioSourceVolume enum:audio_source_t:%d enum:event_origin_t:%d",
              source, origin);
    if (AudioSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        AudioSources_VolumeDown(source);
    }
    else
    {
        volume_t source_volume = AudioSources_GetVolume(source);
        source_volume.value = VolumeUtils_DecrementVolume(source_volume);
        volumeService_UpdateAudioSourceVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxAudioVolume(source, source_volume);
    }
}

void VolumeService_AudioSourceMute(audio_source_t source, event_origin_t origin, bool mute_state)
{
    DEBUG_LOG("VolumeService_AudioSourceMute enum:audio_source_t:%d enum:event_origin_t:%d mute_state %d",
                  source, origin, mute_state);
    AudioSources_SetMuteState(source, mute_state);
    AudioSources_OnMuteChange(source, origin, mute_state);
    volumeService_RefreshCurrentVolume(event_origin_local);
}

void VolumeService_SetSystemVolume(event_origin_t origin, volume_t new_volume)
{
    volume_t system_volume = Volume_GetSystemVolume();
    system_volume.value = VolumeUtils_ConvertToVolumeConfig(new_volume, system_volume.config);
    volumeService_UpdateSystemVolume(system_volume, origin);
}

void VolumeService_IncrementSystemVolume(event_origin_t origin)
{
    volume_t system_volume = Volume_GetSystemVolume();
    system_volume.value = VolumeUtils_IncrementVolume(system_volume);
    volumeService_UpdateSystemVolume(system_volume, origin);
}

void VolumeService_DecrementSystemVolume(event_origin_t origin)
{
    volume_t system_volume = Volume_GetSystemVolume();
    system_volume.value = VolumeUtils_DecrementVolume(system_volume);
    volumeService_UpdateSystemVolume(system_volume, origin);
}

void VolumeService_SetVoiceSourceVolume(voice_source_t source, event_origin_t origin, volume_t new_volume)
{
    volume_t source_volume = VoiceSources_GetVolume(source);
    DEBUG_LOG("VolumeService_SetVoiceSourceVolume enum:voice_source_t:%d enum:event_origin_t:%d volume %u",
              source, origin, new_volume.value);
    source_volume.value = VolumeUtils_ConvertToVolumeConfig(new_volume, source_volume.config);

    if(VoiceSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        VoiceSources_VolumeSetAbsolute(source, source_volume);
    }
    else
    {
        volumeService_UpdateVoiceSourceLocalVolume(source, source_volume, origin);
    }
    volumeService_NotifyMinOrMaxVoiceVolume(source, source_volume);
}

void VolumeService_IncrementVoiceSourceVolume(voice_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_IncrementVoiceSourceVolume enum:voice_source_t:%d enum:event_origin_t:%d",
              source, origin);
    if (VoiceSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        VoiceSources_VolumeUp(source);
    }
    else
    {
        volume_t source_volume = VoiceSources_GetVolume(source);
        source_volume.value = VolumeUtils_IncrementVolume(source_volume);
        volumeService_UpdateVoiceSourceLocalVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxVoiceVolume(source, source_volume);
    }
}

void VolumeService_DecrementVoiceSourceVolume(voice_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_DecrementVoiceSourceVolume enum:voice_source_t:%d enum:event_origin_t:%d",
              source, origin);
    if (VoiceSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        VoiceSources_VolumeDown(source);
    }
    else
    {
        volume_t source_volume = VoiceSources_GetVolume(source);
        source_volume.value = VolumeUtils_DecrementVolume(source_volume);
        volumeService_UpdateVoiceSourceLocalVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxVoiceVolume(source, source_volume);
    }
}

void VolumeService_VoiceSourceMute(voice_source_t source, event_origin_t origin, bool mute_state)
{
    DEBUG_LOG("VolumeService_VoiceSourceMute enum:voice_source_t:%d enum:event_origin_t:%d mute_state %d",
                      source, origin, mute_state);
    VoiceSources_SetMuteState(source, mute_state);
    VoiceSources_OnMuteChange(source, origin, mute_state);
    volumeService_RefreshCurrentVolume(event_origin_local);
}

static void volumeMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case VOICE_SOURCE_VOLUME_UPDATE_REQUEST:
            {
                voice_source_volume_update_request_message_t *msg = (voice_source_volume_update_request_message_t *)message;
                VolumeService_SetVoiceSourceVolume(msg->voice_source, msg->origin, msg->volume);
            }
            break;
        case VOICE_SOURCE_VOLUME_INCREMENT_REQUEST:
            {
                voice_source_volume_increment_request_message_t *msg = (voice_source_volume_increment_request_message_t *)message;
                VolumeService_IncrementVoiceSourceVolume(msg->voice_source, msg->origin);
            }
            break;
        case VOICE_SOURCE_VOLUME_DECREMENT_REQUEST:
            {
                voice_source_volume_decrement_request_message_t *msg = (voice_source_volume_decrement_request_message_t *)message;
                VolumeService_DecrementVoiceSourceVolume(msg->voice_source, msg->origin);
            }
            break;
        case VOICE_SOURCE_MUTE_VOLUME_REQUEST:
            {
                voice_source_mute_volume_request_message_t *msg = (voice_source_mute_volume_request_message_t *)message;
                VolumeService_VoiceSourceMute(msg->voice_source, msg->origin, msg->mute_state);
            }
            break;
        case AUDIO_SOURCE_VOLUME_UPDATE_REQUEST:
            {
                audio_source_volume_update_request_message_t *msg = (audio_source_volume_update_request_message_t *)message;
                VolumeService_SetAudioSourceVolume(msg->audio_source, msg->origin, msg->volume);
            }
            break;
        case AUDIO_SOURCE_VOLUME_INCREMENT_REQUEST:
            {
                audio_source_volume_increment_request_message_t *msg = (audio_source_volume_increment_request_message_t *)message;
                VolumeService_IncrementAudioSourceVolume(msg->audio_source, msg->origin);
            }
            break;
        case AUDIO_SOURCE_VOLUME_DECREMENT_REQUEST:
            {
                audio_source_volume_decrement_request_message_t *msg = (audio_source_volume_decrement_request_message_t *)message;
                VolumeService_DecrementAudioSourceVolume(msg->audio_source, msg->origin);
            }
            break;
        case AUDIO_SOURCE_MUTE_VOLUME_REQUEST:
            {
                audio_source_mute_volume_request_message_t *msg = (audio_source_mute_volume_request_message_t *)message;
                VolumeService_AudioSourceMute(msg->audio_source, msg->origin, msg->mute_state);
            }
            break;
        default:
            break;
    }
}

bool VolumeService_Init(Task init_task)
{
    UNUSED(init_task);
    TaskList_InitialiseWithCapacity(VolumeServiceGetClientLIst(), VOLUME_SERVICE_CLIENT_TASK_LIST_INIT_CAPACITY);

    the_volume_service.volume_message_handler_task.handler = volumeMessageHandler;
    Volume_RegisterForMessages(&the_volume_service.volume_message_handler_task);

    Ui_RegisterUiInputConsumer(&internal_message_task, ui_inputs, ARRAY_DIM(ui_inputs));

    /* Register for all audio sources, to obtain indications when an audio source becomes unrouted,
       in order to cancel any currently active volume ramp operations. */
    audio_source_t source = audio_source_none;
    while(++source < max_audio_sources)
    {
        AudioSources_RegisterObserver(source, &volume_service_audio_observer_interface);
    }

    return TRUE;
}

static void volumeService_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == VOLUME_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(VolumeServiceGetClientLIst()), task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(VOLUME_SERVICE, volumeService_RegisterMessageGroup, NULL);
