/******************************************************************************
Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.

 
FILE NAME
    audio_output_gain.c
 
DESCRIPTION
    Contains functions related to setting/getting of audio output gains.
*/

#include "audio_output.h"
#include "audio_output_private.h"

#include "audio_i2s_common.h"
#include <print.h>
#include <stdlib.h>
#include <gain_utils.h>
#include <sink.h>
#include <panic.h>
#include <audio_config.h>

static uint16 system_gain_from_master(int16 master)
{
    /* Convert dB master volume to DAC based system volume */
    int16 system = (CODEC_STEPS + (master / DB_TO_DAC));
    
    /* DAC gain only goes down to -45dB, DSP volume control goes to -60dB */
    if(system < 0)
        system = 0;
    
    return (uint16)system;
}

static i2s_out_t get_i2s_chan(audio_output_t audio_out)
{
    audio_output_hardware_instance_t inst = config->mapping[audio_out].hardware_instance;
    audio_output_channel_t chan = config->mapping[audio_out].channel;
    
    if(inst == audio_output_hardware_instance_0)
    {
        if(chan == audio_output_channel_a)
            return i2s_out_1_left;
        if(chan == audio_output_channel_b)
            return i2s_out_1_right;
    }
    else if(inst == audio_output_hardware_instance_1)
    {
        if(chan == audio_output_channel_a)
            return i2s_out_2_left;
        if(chan == audio_output_channel_b)
            return i2s_out_2_right;
    }
    
    /* Catch invalid configuration with panic */
    Panic();
    return i2s_out_1_left;
}

int16 AudioOutputGainGetFixedHardwareLevel(void)
{
    PanicNull((void*)config);
    return config->fixed_hw_gain;
}

static void setDacOutputGain(audio_output_t channel, int16 dac_gain)
{
    Sink sink = audioOutputGetSink(channel);
    if(sink)
    {
        PanicFalse(SinkConfigure(sink, STREAM_CODEC_OUTPUT_GAIN, system_gain_from_master(dac_gain)));
    }
}

static void set_hardware_gain(audio_output_t audio_out, int16 hardware_gain, int16 trim)
{
    /* Include trim in hardware gain setting */
    hardware_gain += trim;

    switch(config->mapping[audio_out].hardware_type)
    {
        case audio_output_type_dac:
        {
            setDacOutputGain(audio_out, hardware_gain);
        }
        break;
        
        case audio_output_type_i2s:
        {
            AudioI2SDeviceSetChannelGain(get_i2s_chan(audio_out), hardware_gain);
        }
        break;
        
        default:
            /* Ignore unrecognised type */
        break;
    }
}

/* If trim_info is NULL then channel trim is 0, otherwise get it from trim_info */
#define get_trim(trim_info, group, channel) (int16)(trim_info ? trim_info->group.channel : 0)

static void set_all_hardware_gains(const audio_output_group_t group,
                                   const int16 hardware_gain,
                                   const audio_output_trims_t* trim_info)
{
    PRINT(("applied hw gain %d ", system_gain_from_master(hardware_gain)));
    PRINT(("%s\n", (trim_info ? "+ trims" : "(no trims)")));
    
    if(group == audio_output_group_main)
    {
        set_hardware_gain(audio_output_primary_left, hardware_gain, get_trim(trim_info, main, primary_left));
        set_hardware_gain(audio_output_primary_right, hardware_gain, get_trim(trim_info, main, primary_right));
        set_hardware_gain(audio_output_secondary_left, hardware_gain, get_trim(trim_info, main, secondary_left));
        set_hardware_gain(audio_output_secondary_right, hardware_gain, get_trim(trim_info, main, secondary_right));
        set_hardware_gain(audio_output_wired_sub, hardware_gain, get_trim(trim_info, main, wired_sub));
    }
    else if(group == audio_output_group_aux)
    {
        set_hardware_gain(audio_output_aux_left, hardware_gain, get_trim(trim_info, aux, left));
        set_hardware_gain(audio_output_aux_right, hardware_gain, get_trim(trim_info, aux, right));
    }
}

void AudioOutputGainApplyConfiguredLevels(const audio_output_group_t group,
                                          const int16 master_gain,
                                          const audio_output_trims_t *trims)
{
    int16 hardware_gain;
    bool include_trims = TRUE;
    
    PRINT(("AudioOutputGainSetHardware "));
    PRINT(("%s: ", (group == audio_output_group_main ? "Main" : " Aux")));
    PRINT(("master gain %d ", master_gain));
    
    if(config == NULL || group >= audio_output_group_all)
    {
        Panic();
        return;
    }
    
    switch(config->gain_type[group])
    {
        case audio_output_gain_hardware:
            /* Apply master gain at hardware level */
            hardware_gain = master_gain;

            /* When using hardware gain, channel trims should only be included
             * if not muted */
            include_trims = (master_gain != DIGITAL_VOLUME_MUTE);
        break;
        
        case audio_output_gain_digital:
            /* Master gain applied in DSP, set fixed gain at hardware level */
            hardware_gain = config->fixed_hw_gain;
        break;
        
        case audio_output_gain_none:
            /* Do not set any hardware gains */
            return;

        default:
            Panic();
            return;
    }
    
    set_all_hardware_gains(group, hardware_gain, (include_trims ? trims : NULL));
}

void AudioOutputGainSetLevel(const audio_output_group_t group,
                             const int16 master_gain)
{
    PRINT(("AudioOutputGainSetHardwareOnly "));
    PRINT(("%s: ", (group == audio_output_group_main ? "Main" : " Aux")));
    PRINT(("master gain %d ", master_gain));
    
    if(config == NULL || group >= audio_output_group_all)
    {
        Panic();
        return;
    }
    
    set_all_hardware_gains(group, master_gain, NULL);
}

audio_output_gain_type_t AudioOutputGainGetType(const audio_output_group_t group)
{
    if(config == NULL || group >= audio_output_group_all)
    {
        Panic();
        return audio_output_gain_invalid;
    }
    
    return config->gain_type[group];
}
