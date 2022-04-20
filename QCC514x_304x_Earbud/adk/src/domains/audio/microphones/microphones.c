/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Microphone component implementation, responsible for configuration and microphone user tracking
*/

#include "microphones.h"
#include "microphones_config.h"
#include <audio_plugin_common.h>
#include <logging.h>

#define MicrophoneNumberToIndex(x)  (x-1)

typedef struct
{
    audio_mic_params config;
    unsigned non_exclusive_users;
    microphone_user_type_t microphone_user;
    uint32 current_sample_rate;
} managed_microphone;

static managed_microphone microphones[MAX_SUPPORTED_MICROPHONES] =
{
    {
        {
            .bias_config = appConfigMic0Bias(),
            .pio = appConfigMic0Pio(),
            .gain = appConfigMic0Gain(),
            .is_digital = appConfigMic0IsDigital(),
            .instance = appConfigMic0AudioInstance(),
            .channel = appConfigMic0AudioChannel(),
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#if (MAX_SUPPORTED_MICROPHONES >= 2)
    {
        {
            .bias_config = appConfigMic1Bias(),
            .pio = appConfigMic1Pio(),
            .gain = appConfigMic1Gain(),
            .is_digital = appConfigMic1IsDigital(),
            .instance = appConfigMic1AudioInstance(),
            .channel = appConfigMic1AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 3)
    {
        {
            .bias_config = appConfigMic2Bias(),
            .pio = appConfigMic2Pio(),
            .gain = appConfigMic2Gain(),
            .is_digital = appConfigMic2IsDigital(),
            .instance = appConfigMic2AudioInstance(),
            .channel = appConfigMic2AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 4)
    {
        {
            .bias_config = appConfigMic3Bias(),
            .pio = appConfigMic3Pio(),
            .gain = appConfigMic3Gain(),
            .is_digital = appConfigMic3IsDigital(),
            .instance = appConfigMic3AudioInstance(),
            .channel = appConfigMic3AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 5)
    {
        {
            .bias_config = appConfigMic4Bias(),
            .pio = appConfigMic4Pio(),
            .gain = appConfigMic4Gain(),
            .is_digital = appConfigMic4IsDigital(),
            .instance = appConfigMic4AudioInstance(),
            .channel = appConfigMic4AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 6)
    {
         {
            .bias_config = appConfigMic5Bias(),
            .pio = appConfigMic5Pio(),
            .gain = appConfigMic5Gain(),
            .is_digital = appConfigMic5IsDigital(),
            .instance = appConfigMic5AudioInstance(),
            .channel = appConfigMic5AudioChannel()
         },
         .non_exclusive_users = 0,
         .microphone_user = invalid_user
    },
#endif
};

static bool microphones_IsValidMicrophoneNumber(microphone_number_t microphone_number)
{
	return microphone_number && (microphone_number <= MAX_SUPPORTED_MICROPHONES);
}

audio_mic_params * Microphones_GetMicrophoneConfig(microphone_number_t microphone_number)
{
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    return &microphones[MicrophoneNumberToIndex(microphone_number)].config;
}

void Microphones_SetMicrophoneConfig(microphone_number_t microphone_number, audio_mic_params *config)
{
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    microphones[MicrophoneNumberToIndex(microphone_number)].config = *config;
}

static microphone_user_type_t microphones_GetMicrophoneUserType(microphone_number_t microphone_number)
{
    return (microphones[MicrophoneNumberToIndex(microphone_number)].microphone_user);
}

static inline uint32 microphones_GetCurrentSampleRate(microphone_number_t microphone_number)
{
    return (microphones[MicrophoneNumberToIndex(microphone_number)].current_sample_rate);
}

static inline bool microphones_IsMicrophoneInExclusiveUse(microphone_number_t microphone_number)
{
    return (microphones_GetMicrophoneUserType(microphone_number) == normal_priority_user ||
            microphones_GetMicrophoneUserType(microphone_number) == high_priority_user);
}

static inline bool microphones_IsMicrophoneInNonExclusiveUse(microphone_number_t microphone_number)
{
    return !!(microphones[MicrophoneNumberToIndex(microphone_number)].non_exclusive_users);
}

static inline void microphones_AddUser(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type)
{
    if(microphone_user_type == non_exclusive_user)
    {
        microphones[MicrophoneNumberToIndex(microphone_number)].non_exclusive_users++;
         DEBUG_LOG("microphones_AddUser: microphone[%d] non_exclusive_users=%d", MicrophoneNumberToIndex(microphone_number),
                   microphones[MicrophoneNumberToIndex(microphone_number)].non_exclusive_users);
    }
    else
    {
        microphones[MicrophoneNumberToIndex(microphone_number)].microphone_user = microphone_user_type;
    }
}

static inline void microphones_RemoveUser(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type)
{
    if(microphone_user_type == non_exclusive_user)
    {
        microphones[MicrophoneNumberToIndex(microphone_number)].non_exclusive_users--;
        DEBUG_LOG("microphones_RemoveUser: microphone[%d] non_exclusive_users=%d", MicrophoneNumberToIndex(microphone_number),
                  microphones[MicrophoneNumberToIndex(microphone_number)].non_exclusive_users);
    }
    else
    {
        microphones[MicrophoneNumberToIndex(microphone_number)].microphone_user = invalid_user;
    }
}

static inline void microphones_SetCurrentSampleRate(microphone_number_t microphone_number, uint32 sample_rate)
{
    microphones[MicrophoneNumberToIndex(microphone_number)].current_sample_rate = sample_rate;
}

Source Microphones_TurnOnMicrophone(microphone_number_t microphone_number, uint32 sample_rate, microphone_user_type_t microphone_user_type)
{
    Source mic_source = NULL;
    DEBUG_LOG("Microphones_TurnOnMicrophone: number=%d, rate=%d, user_type=%d", microphone_number, sample_rate, microphone_user_type);
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    PanicFalse(microphone_user_type != invalid_user);

    if(((microphone_user_type > normal_priority_user) && (microphones_GetMicrophoneUserType(microphone_number) != microphone_user_type))
            || (microphones_IsMicrophoneInExclusiveUse(microphone_number) == FALSE))
    {
        const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
        uint32 current_sample_rate = microphones_GetCurrentSampleRate(microphone_number);
        if(Microphones_IsMicrophoneInUse(microphone_number) == FALSE || current_sample_rate != sample_rate)
        {
            DEBUG_LOG("Microphones_TurnOnMicrophone: setting up, current_sample_rate %d", current_sample_rate);
            mic_source =  AudioPluginMicSetup(microphone_config->channel, *microphone_config, sample_rate);
            microphones_SetCurrentSampleRate(microphone_number, sample_rate);
        }
        else
        {
            mic_source = AudioPluginGetMicSource(*microphone_config, microphone_config->channel);
        }
        microphones_AddUser(microphone_number, microphone_user_type);
    }
    DEBUG_LOG("Microphones_TurnOnMicrophone: source=%p", mic_source);
    return mic_source;
}

void Microphones_TurnOffMicrophone(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type)
{
    DEBUG_LOG("Microphones_TurnOffMicrophone: number=%d, user_type=%d", microphone_number, microphone_user_type);
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    PanicFalse(microphone_user_type != invalid_user);

    if(Microphones_IsMicrophoneInUse(microphone_number))
    {
        bool close_mic;
        microphones_RemoveUser(microphone_number, microphone_user_type);
        close_mic = (Microphones_IsMicrophoneInUse(microphone_number) == FALSE);
        DEBUG_LOG("Microphones_TurnOffMicrophone: shutting down, close_mic=%u", close_mic);
        if(close_mic)
        {
            const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
            AudioPluginMicShutdown(microphone_config->channel, microphone_config, close_mic);
        }
    }
}

uint8 Microphones_MaxSupported(void)
{
    return MAX_SUPPORTED_MICROPHONES;
}

void Microphones_Init(void)
{
    unsigned microphone_number = 0;
    while(++microphone_number <= MAX_SUPPORTED_MICROPHONES)
    {
        microphones[MicrophoneNumberToIndex(microphone_number)].non_exclusive_users = 0;
        microphones[MicrophoneNumberToIndex(microphone_number)].microphone_user = invalid_user;
        microphones[MicrophoneNumberToIndex(microphone_number)].current_sample_rate = 0;
    }
}

Source Microphones_GetMicrophoneSource(microphone_number_t microphone_number)
{
    Source mic_source = NULL;
    DEBUG_LOG("Microphones_GetMicrophoneSource: number=%d", microphone_number);
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));

    if (Microphones_IsMicrophoneInUse(microphone_number))
    {
        const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
        mic_source = AudioPluginGetMicSource(*microphone_config, microphone_config->channel);
    }

    return mic_source;
}

bool Microphones_IsMicrophoneInUse(microphone_number_t microphone_number)
{
    return (microphones_IsMicrophoneInNonExclusiveUse(microphone_number) || microphones_IsMicrophoneInExclusiveUse(microphone_number));
}

