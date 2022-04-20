/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of a source prediction.
*/

#include "source_prediction.h"
#include "kymera_adaptation.h"
#include "kymera_adaptation_audio_protected.h"

#include <panic.h>
#include <logging.h>
#include <focus_audio_source.h>

static audio_source_t sourcePrediction_GetA2dpSourceInFocus(void)
{
    audio_source_t check_audio_source = audio_source_none;
    for_all_a2dp_audio_sources(check_audio_source)
    {
        if (Focus_GetFocusForAudioSource(check_audio_source) == focus_foreground)
        {
            return check_audio_source;
        }
    }
    /* If no a2dp source is in focus we provide a best guess */
    return audio_source_a2dp_1;
}

bool SourcePrediction_GetA2dpParametersPrediction(uint32 *rate, uint8 *seid)
{
    audio_source_t audio_source_in_focus = audio_source_none;
    source_defined_params_t source_params = {0, NULL};
    bool a2dp_params_are_valid = FALSE;
    *rate = 0;
    *seid = 0;

    audio_source_in_focus = sourcePrediction_GetA2dpSourceInFocus();
    if(AudioSources_GetConnectParameters(audio_source_in_focus, &source_params))
    {
        a2dp_connect_parameters_t *audio_params;
        audio_params = source_params.data;
        if ((audio_params->rate != 0) && (audio_params->seid != 0))
        {
            *rate = audio_params->rate;
            *seid = audio_params->seid;
            DEBUG_LOG("SourcePrediction_GetA2dpParametersPrediction: rate %u seid %u",
                      *rate, *seid);
            a2dp_params_are_valid = TRUE;
        }
        AudioSources_ReleaseConnectParameters(audio_source_in_focus, &source_params);
    }
    return a2dp_params_are_valid;
}
