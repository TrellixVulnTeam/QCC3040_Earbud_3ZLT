/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice UI EQ APIs.
*/

#ifdef INCLUDE_MUSIC_PROCESSING
#include "voice_ui_eq.h"
#include "kymera.h"
#include "voice_ui_container.h"

/* Gain is expressed as gain_in_db*60
 * We will allow setting a range between -3dB to +3dB */
#define MAX_GAIN_IN_DB 3
#define MAX_GAIN (MAX_GAIN_IN_DB*60)

static voice_ui_eq_if_t * voice_ui_eq = NULL;

static void voiceui_EqMessageHandler(Task task, MessageId id, Message message);
static const TaskData voice_ui_eq_task = {voiceui_EqMessageHandler};

static void voiceui_EqMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("voiceUi_EqMessageHandler %d", id);

    switch(id)
    {
        case KYMERA_NOTIFCATION_USER_EQ_BANDS_UPDATED:
        {
            voice_ui_handle_t* handle = VoiceUi_GetActiveVa();
            if (handle && handle->voice_assistant && handle->voice_assistant->EqUpdate)
            {
                handle->voice_assistant->EqUpdate();
            }
            break;
        }

        default:
            break;
    }
}

static int16 voiceUi_ConvertGainToPercentage(int16 gain)
{
    return ((gain * 100) / (2 * MAX_GAIN)) + 50;
}

static int16 voiceUi_ConvertPercentageToGain(int16 percentage)
{
    return ((percentage - 50) * 2 * MAX_GAIN) / 100;
}

static bool voiceUi_IsNumberOfBandsValid(void)
{
    PanicNull(voice_ui_eq);
    return (voice_ui_eq->getNumberOfActiveBands() >= 3);
}

static uint8 voiceUi_GetHighestEqBand(void)
{
    PanicNull(voice_ui_eq);
    uint8 highest_band = voice_ui_eq->getNumberOfActiveBands()-1;
    return highest_band;
}

static uint8 voiceUi_GetNumberOfHighOrLowEqBands(void)
{
    PanicNull(voice_ui_eq);
    uint8 num_of_bands = (voice_ui_eq->getNumberOfActiveBands()+1)/3;
    return num_of_bands;
}

static uint8 voiceUi_GetNumberOfMidEqBands(void)
{
    PanicNull(voice_ui_eq);
    return voice_ui_eq->getNumberOfActiveBands() - (2*voiceUi_GetNumberOfHighOrLowEqBands());
}

static uint8 voiceUi_GetLowEqUpperBound(void)
{
    return voiceUi_GetNumberOfHighOrLowEqBands() - 1;
}

static uint8 voiceUi_GetHighEqLowerBound(void)
{
    PanicNull(voice_ui_eq);
    uint8 high_eq_lower_bound =(voice_ui_eq->getNumberOfActiveBands() - voiceUi_GetNumberOfHighOrLowEqBands());
    return high_eq_lower_bound;
}

static uint8 voiceUi_GetMidEqUpperBound(void)
{
    return voiceUi_GetHighEqLowerBound() - 1;
}

static uint8 voiceUi_GetMidEqLowerBound(void)
{
    return voiceUi_GetLowEqUpperBound() + 1;
}

static int16 voiceUi_GetEqGain(uint8 band)
{
    int16 gain = 0;

    if(voiceUi_IsNumberOfBandsValid())
    {
        kymera_eq_paramter_set_t eq_param_set;
        Kymera_GetEqBandInformation(band, &eq_param_set);
        gain = voiceUi_ConvertGainToPercentage(eq_param_set.gain);

        if(gain > 100)
        {
            /* Other modules may support setting EQ levels higher than the maximum for this module */
            gain = 100;
        }

        if(gain < 0)
        {
            /* Other modules may support setting EQ levels lower than the minumum for this module */
            gain = 0;
        }

        DEBUG_LOG("voiceUi_GetEqGain band %u gain %d", band, gain);
    }
    else
    {
        DEBUG_LOG_WARN("voiceUi_GetEqGain not enough bands");
    }

    return gain;
}

static void voiceUi_SetEqGain(int16 gain_percentage, uint8 lower_band, uint8 upper_band, uint8 num_of_bands)
{
    PanicFalse(gain_percentage <= 100);
    PanicNull(voice_ui_eq);

    voice_ui_eq->setPreset(EQ_BANK_USER);

    if(voiceUi_IsNumberOfBandsValid())
    {
        int16 gain = voiceUi_ConvertPercentageToGain(gain_percentage);
        DEBUG_LOG("voiceUi_SetEqGain %d", gain);
        int16 * gains = (int16*)PanicUnlessMalloc(sizeof(int16)*num_of_bands);

        for(unsigned i=0; i<num_of_bands; i++)
        {
            gains[i] = gain;
        }

        voice_ui_eq->setUserEqBands(lower_band, upper_band, gains);
        free(gains);
    }
    else
    {
        DEBUG_LOG_WARN("voiceUi_SetEqGain not enough bands");
    }
}

void VoiceUi_EqInit(void)
{
    DEBUG_LOG("VoiceUi_EqInit");
    Kymera_RegisterNotificationListener((Task)&voice_ui_eq_task);
}

bool VoiceUi_IsUserEqActive(void)
{
    PanicNull(voice_ui_eq);
    return voice_ui_eq->isEqActive();
}

int16 VoiceUi_GetLowEqGain(void)
{
    return voiceUi_GetEqGain(0);
}

void VoiceUi_SetLowEqGain(int16 gain_percentage)
{
    DEBUG_LOG("VoiceUi_SetLowEqGain %u", gain_percentage);
    voiceUi_SetEqGain(gain_percentage, 0, voiceUi_GetLowEqUpperBound(), voiceUi_GetNumberOfHighOrLowEqBands());
}

int16 VoiceUi_GetMidEqGain(void)
{
    return voiceUi_GetEqGain(voiceUi_GetMidEqLowerBound());
}

void VoiceUi_SetMidEqGain(int16 gain_percentage)
{
    DEBUG_LOG("VoiceUi_SetMidEqGain %u", gain_percentage);
    voiceUi_SetEqGain(gain_percentage, voiceUi_GetMidEqLowerBound(), voiceUi_GetMidEqUpperBound(), voiceUi_GetNumberOfMidEqBands());
}

int16 VoiceUi_GetHighEqGain(void)
{
    return voiceUi_GetEqGain(voiceUi_GetHighestEqBand());
}

void VoiceUi_SetHighEqGain(int16 gain_percentage)
{
    DEBUG_LOG("VoiceUi_SetHighEqGain %d", gain_percentage);
    voiceUi_SetEqGain(gain_percentage, voiceUi_GetHighEqLowerBound(), voiceUi_GetHighestEqBand(), voiceUi_GetNumberOfHighOrLowEqBands());
}

void VoiceUi_SetEqInterface(voice_ui_eq_if_t * voice_ui_eq_if)
{
    DEBUG_LOG("VoiceUi_SetEqInterface");
    voice_ui_eq = voice_ui_eq_if;
}

#endif /* INCLUDE_MUSIC_PROCESSING */
