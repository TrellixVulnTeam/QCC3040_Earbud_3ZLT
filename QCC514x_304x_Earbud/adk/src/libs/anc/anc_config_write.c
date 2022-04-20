/*******************************************************************************
Copyright (c) 2019  Qualcomm Technologies International, Ltd.


FILE NAME
    anc_config_write.c

DESCRIPTION
    Wirte the gain parameter to Audio PS key
*/

#include <stdlib.h>
#include <string.h>
#include "anc.h"
#include "anc_debug.h"
#include "anc_config_data.h"
#include "anc_data.h"
#include "anc_config_read.h"
#include "anc_tuning_data.h"
#include "anc_config_write.h"
#include <ps.h>

extern device_ps_key_config_t device_ps_key_config;

/*! Set the ANC Tuning Item
*/
static void appAncSetTuningItem16Bit(uint16 *data, uint16 gain)
{
    data[1] = gain;
}

#ifdef ANC_UPGRADE_FILTER
/*! Derive the Delta gain value (difference between the write fine gain and golden config) for the current mode and gain path specified.
    Update the Delta gain value in the device_ps_key. This gain will be used during the ANC initialisation and/or mode change.
*/
bool ancWriteFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint16 gain)
{
    /* Maximum Gain should be 255 */
    if(gain > MAXIMUM_FINE_GAIN)
    {
        ANC_DEBUG_INFO(("ancWriteFineGain: Invalid gain [%d]", gain));
        Panic();
    }
    bool status = FALSE;
    uint8 golden_gain = 0;
    uint16 fine_gain_tune[FINE_GAIN_TUNE_DATA_ENTRIES] = {0};
    status = TRUE;

    PsRetrieve(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);

    if(ancReadFineGain(mode, gain_path, &golden_gain))
    {
        if(golden_gain == 0)
        {
            golden_gain = 128;
        }
        int16 golden_gain_q_format = convertGainTo16BitQFormat((uint16)golden_gain);
        int16 fine_gain_q_format   = convertGainTo16BitQFormat(gain);
        int16 gain_difference_q_format = 0;

        gain_difference_q_format = (fine_gain_q_format - golden_gain_q_format);
        fine_gain_tune[gain_path - 1] = gain_difference_q_format;

        /* Store delta fine gain in 16-bit fixed-point format(Q6.9) in PS*/
        status = PsStore(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);
    }
    else
    {
        status = FALSE;
    }

    return status;
}
#else
/*! Derive the Delta gain value (difference between the write fine gain and golden config) for the current mode and gain path specified.
    Update the Delta gain value in the device_ps_key. This gain will be used during the ANC initialisation and/or mode change.
*/
bool ancWriteFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint16 gain)
{
    /* Maximum Gain should be 255 */
    if(gain > MAXIMUM_FINE_GAIN)
    {
        ANC_DEBUG_INFO(("ancWriteFineGain: Invalid gain [%d]", gain));
        Panic();
    }
    uint16 anc_audio_ps[ANC_TUNING_CONFIG_TOTAL_SIZE];
    bool status = FALSE;
    uint16 total_key_length = 0;

    memset(anc_audio_ps, 0, sizeof(anc_audio_ps));

    /*Since the audio keys can't be partially updated, the entire value of the key must be read and written.*/
    if  ((PsReadAudioKey(AUDIO_PS_ANC_TUNING(mode), anc_audio_ps, ANC_TUNING_CONFIG_TOTAL_SIZE,
                0, &total_key_length) == ANC_TUNING_CONFIG_TOTAL_SIZE)
            && (total_key_length == ANC_TUNING_CONFIG_TOTAL_SIZE))
    {
        uint16 fine_gain_tune[FINE_GAIN_TUNE_DATA_ENTRIES] = {0};
        uint16 *gain_index = NULL;
        status = TRUE;

        PsRetrieve(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);

        switch(gain_path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
                gain_index = &anc_audio_ps[FFA_GAIN_OFFSET+ANC_TUNING_CONFIG_HEADER_SIZE + 1];
                break;
            case AUDIO_ANC_PATH_ID_FFB:
                gain_index = &anc_audio_ps[FFB_GAIN_OFFSET+ANC_TUNING_CONFIG_HEADER_SIZE + 1];
                break;
            case AUDIO_ANC_PATH_ID_FB:
                gain_index = &anc_audio_ps[FB_GAIN_OFFSET+ANC_TUNING_CONFIG_HEADER_SIZE + 1];
                break;
            default:
                status=FALSE;
                break;
        }
        if(status && gain_index)
        {
            if(*gain_index == 0)
            {
                *gain_index = 128;
            }
            int16 golden_gain_q_format = convertGainTo16BitQFormat(*gain_index);
            int16 fine_gain_q_format   = convertGainTo16BitQFormat(gain);
            int16 gain_difference_q_format = 0;

            gain_difference_q_format = (fine_gain_q_format - golden_gain_q_format);
            fine_gain_tune[gain_path - 1] = gain_difference_q_format;

            /* Store delta fine gain in 16-bit fixed-point format(Q6.9) in PS*/
            status = PsStore(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);
        }
        else
        {
            status = FALSE;
        }
    }
    return status;
}
#endif

/*! Write fine gain to the Audio PS key for the current mode and gain path specified 
*/
bool ancWriteFineGainParallelFilter(anc_mode_t mode, audio_anc_path_id gain_path, uint16 instance_0_gain, uint16 instance_1_gain)
{
    uint16 anc_audio_ps[ANC_TUNING_CONFIG_TOTAL_SIZE];
    bool status = FALSE;
    uint16 total_key_length = 0;

    memset(anc_audio_ps, 0, sizeof(anc_audio_ps));

    /*Since the audio keys can't be partially updated, the entire value of the key must be read and written.*/
    if  ((PsReadAudioKey(AUDIO_PS_ANC_TUNING(mode), anc_audio_ps, ANC_TUNING_CONFIG_TOTAL_SIZE,
                0, &total_key_length) == ANC_TUNING_CONFIG_TOTAL_SIZE)
            && (total_key_length == ANC_TUNING_CONFIG_TOTAL_SIZE))
    {

        switch(gain_path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
                appAncSetTuningItem16Bit(&anc_audio_ps[FFA_GAIN_OFFSET+ANC_TUNING_CONFIG_HEADER_SIZE], instance_0_gain);
                appAncSetTuningItem16Bit(&anc_audio_ps[FFA_GAIN_OFFSET_R+ANC_TUNING_CONFIG_HEADER_SIZE], instance_1_gain);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FFB:
                appAncSetTuningItem16Bit(&anc_audio_ps[FFB_GAIN_OFFSET+ANC_TUNING_CONFIG_HEADER_SIZE], instance_0_gain);
                appAncSetTuningItem16Bit(&anc_audio_ps[FFB_GAIN_OFFSET_R+ANC_TUNING_CONFIG_HEADER_SIZE], instance_1_gain);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FB:
                appAncSetTuningItem16Bit(&anc_audio_ps[FB_GAIN_OFFSET+ANC_TUNING_CONFIG_HEADER_SIZE], instance_0_gain);
                appAncSetTuningItem16Bit(&anc_audio_ps[FB_GAIN_OFFSET_R+ANC_TUNING_CONFIG_HEADER_SIZE], instance_1_gain);
                status=TRUE;
                break;
            default:
                break;
        }        
    }

    if (status)
    {
        if (PsUpdateAudioKey(AUDIO_PS_ANC_TUNING(mode), anc_audio_ps, ANC_TUNING_CONFIG_TOTAL_SIZE, 0, ANC_TUNING_CONFIG_TOTAL_SIZE))
        {
            status = TRUE;
        }
        else
        {
            status = FALSE;
        }
    }

    return status;
}
