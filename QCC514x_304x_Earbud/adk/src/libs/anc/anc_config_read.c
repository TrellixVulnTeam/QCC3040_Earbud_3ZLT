/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_config_read.c

DESCRIPTION

*/

#include <stdlib.h>
#include <string.h>
#include "anc.h"
#include "anc_debug.h"
#include "anc_config_data.h"
#include "anc_data.h"
#include "anc_config_read.h"
#include "anc_tuning_data.h"
#include "anc_configure_coefficients.h"
#include <ps.h>

extern device_ps_key_config_t device_ps_key_config;

static uint16 getTuningItem16Bit(uint16 * data)
{
    return (data[1]);
}

static uint32 getTuningItem32Bit(uint16 * data)
{
    return ((data[0] << 16) | data[1]);
}

static unsigned getCoefficientAtIndex(unsigned index, uint16 * data)
{
    return getTuningItem32Bit(&data[index * 2]);
}

static void populateCoefficients(iir_config_t * iir_config, uint16 * data)
{
    unsigned index;
    for(index = 0; index < NUMBER_OF_IIR_COEFFICIENTS; index++)
    {
        iir_config->coefficients[index] = getCoefficientAtIndex(index, data);
    }
}

static void populateInstance(anc_instance_config_t * instance, uint16 * audio_ps_data)
{
    populateCoefficients(&instance->feed_forward_a.iir_config, &audio_ps_data[FFA_COEFFICIENTS_OFFSET]);

    instance->feed_forward_a.lpf_config.lpf_shift1 = getTuningItem16Bit(&audio_ps_data[FFA_LPF_SHIFT_1_OFFSET]);
    instance->feed_forward_a.lpf_config.lpf_shift2 = getTuningItem16Bit(&audio_ps_data[FFA_LPF_SHIFT_2_OFFSET]);

    instance->feed_forward_a.dc_filter_config.filter_shift = getTuningItem16Bit(&audio_ps_data[FFA_DC_FILTER_SHIFT_OFFSET]);
    instance->feed_forward_a.dc_filter_config.filter_enable = getTuningItem16Bit(&audio_ps_data[FFA_DC_FILTER_ENABLE_OFFSET]);

    instance->feed_forward_a.gain_config.gain = getTuningItem16Bit(&audio_ps_data[FFA_GAIN_OFFSET]);
    instance->feed_forward_a.gain_config.gain_shift = getTuningItem16Bit(&audio_ps_data[FFA_GAIN_SHIFT_OFFSET]);

    instance->feed_forward_a.upconvertor_config.dmic_x2_ff = getTuningItem32Bit(&audio_ps_data[FFA_DMIC_X2_ENABLE_OFFSET]);
#ifdef ANC_UPGRADE_FILTER
    instance->feed_forward_a.rxmix_gain_config.gain = getTuningItem16Bit(&audio_ps_data[FFA_RXMIX_GAIN_OFFSET]);
    instance->feed_forward_a.rxmix_gain_config.gain_shift = getTuningItem16Bit(&audio_ps_data[FFA_RXMIX_GAIN_SHIFT_OFFSET]);
#endif

    populateCoefficients(&instance->feed_forward_b.iir_config, &audio_ps_data[FFB_COEFFICIENTS_OFFSET]);

    instance->feed_forward_b.lpf_config.lpf_shift1 = getTuningItem16Bit(&audio_ps_data[FFB_LPF_SHIFT_1_OFFSET]);
    instance->feed_forward_b.lpf_config.lpf_shift2 = getTuningItem16Bit(&audio_ps_data[FFB_LPF_SHIFT_2_OFFSET]);

    instance->feed_forward_b.dc_filter_config.filter_shift = getTuningItem16Bit(&audio_ps_data[FFB_DC_FILTER_SHIFT_OFFSET]);
    instance->feed_forward_b.dc_filter_config.filter_enable = getTuningItem16Bit(&audio_ps_data[FFB_DC_FILTER_ENABLE_OFFSET]);

    instance->feed_forward_b.gain_config.gain = getTuningItem16Bit(&audio_ps_data[FFB_GAIN_OFFSET]);
    instance->feed_forward_b.gain_config.gain_shift = getTuningItem16Bit(&audio_ps_data[FFB_GAIN_SHIFT_OFFSET]);

    instance->feed_forward_b.upconvertor_config.dmic_x2_ff = getTuningItem32Bit(&audio_ps_data[FFB_DMIC_X2_ENABLE_OFFSET]);
#ifdef ANC_UPGRADE_FILTER
    instance->feed_forward_b.rxmix_gain_config.gain = getTuningItem16Bit(&audio_ps_data[FFB_RXMIX_GAIN_OFFSET]);
    instance->feed_forward_b.rxmix_gain_config.gain_shift = getTuningItem16Bit(&audio_ps_data[FFB_RXMIX_GAIN_SHIFT_OFFSET]);
#endif

    populateCoefficients(&instance->feed_back.iir_config, &audio_ps_data[FB_COEFFICIENTS_OFFSET]);

    instance->feed_back.lpf_config.lpf_shift1 = getTuningItem16Bit(&audio_ps_data[FB_LPF_SHIFT_1_OFFSET]);
    instance->feed_back.lpf_config.lpf_shift2 = getTuningItem16Bit(&audio_ps_data[FB_LPF_SHIFT_2_OFFSET]);

    instance->feed_back.gain_config.gain = getTuningItem16Bit(&audio_ps_data[FB_GAIN_OFFSET]);
    instance->feed_back.gain_config.gain_shift = getTuningItem16Bit(&audio_ps_data[FB_GAIN_SHIFT_OFFSET]);
#ifdef ANC_UPGRADE_FILTER
    instance->feed_back.rxmix_enables.self_mix = getTuningItem16Bit(&audio_ps_data[SELF_RXMIX_ENABLE_OFFSET]);
    instance->feed_back.rxmix_enables.cross_mix = getTuningItem16Bit(&audio_ps_data[CROSS_RXMIX_ENABLE_OFFSET]);
#endif

    instance->small_lpf.small_lpf_config.filter_shift = getTuningItem16Bit(&audio_ps_data[SMALL_LPF_SHIFT_OFFSET]);
    instance->small_lpf.small_lpf_config.filter_enable = getTuningItem16Bit(&audio_ps_data[SMALL_LPF_ENABLE_OFFSET]);

    instance->enable_mask = ((getTuningItem16Bit(&audio_ps_data[ENABLE_FFA_OFFSET]) << ENABLE_BIT_FFA)
                                | (getTuningItem16Bit(&audio_ps_data[ENABLE_FFB_OFFSET]) << ENABLE_BIT_FFB)
                                | (getTuningItem16Bit(&audio_ps_data[ENABLE_FB_OFFSET]) << ENABLE_BIT_FB)
                                | (getTuningItem16Bit(&audio_ps_data[ENABLE_OUT_OFFSET]) << ENABLE_BIT_OUT));
}

#ifdef ANC_UPGRADE_FILTER
static void populateAncInstanceHardwareGains(hardware_gains_t * hardware_gains, uint16 * audio_ps_data, unsigned inst_index)
{
    if(inst_index == ANC_INSTANCE_0_INDEX)
    {
        hardware_gains->feed_forward_a_mic_left = getTuningItem32Bit(&audio_ps_data[GAIN_FFA_MIC_OFFSET_L]);
        hardware_gains->feed_forward_b_mic_left = getTuningItem32Bit(&audio_ps_data[GAIN_FFB_MIC_OFFSET_L]);
        hardware_gains->dac_output_left = getTuningItem32Bit(&audio_ps_data[GAIN_DAC_OUTPUT_A_OFFSET]);
    }
    else if(inst_index == ANC_INSTANCE_1_INDEX)
    {
        hardware_gains->feed_forward_a_mic_right = getTuningItem32Bit(&audio_ps_data[GAIN_FFA_MIC_OFFSET_L]);
        hardware_gains->feed_forward_b_mic_right = getTuningItem32Bit(&audio_ps_data[GAIN_FFB_MIC_OFFSET_L]);
        hardware_gains->dac_output_right = getTuningItem32Bit(&audio_ps_data[GAIN_DAC_OUTPUT_A_OFFSET]);
    }
}

static bool readAncInstanceTuningKey(uint32 key, uint16 * read_buffer, unsigned inst_index)
{
    uint16 total_key_length = 0;

    if(inst_index == ANC_INSTANCE_0_INDEX)
    {
        return ((PsReadAudioKey(key, read_buffer, ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE, ANC_TUNING_CONFIG_HEADER_SIZE,
                    &total_key_length) == ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE) && (total_key_length == ANC_TUNING_CONFIG_TOTAL_SIZE));
    }
    else if(inst_index == ANC_INSTANCE_1_INDEX)
    {
        return ((PsReadAudioKey(key, read_buffer, ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE, (ANC_TUNING_CONFIG_HEADER_SIZE + ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE),
                    &total_key_length) == ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE) && (total_key_length == ANC_TUNING_CONFIG_TOTAL_SIZE));
    }

    return FALSE;
}

static bool populateAncInstanceTuningConfigData(anc_config_t * config_data, anc_mode_t set_mode, unsigned inst_index)
{
    uint16 read_buffer[ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE];
    bool value = TRUE;

    if(readAncInstanceTuningKey(ANC_MODE_CONFIG_KEY(set_mode), read_buffer, inst_index))
    {
        populateInstance(&config_data->mode.instance[inst_index], read_buffer);
        populateAncInstanceHardwareGains(&config_data->hardware_gains, read_buffer, inst_index);
    }
    else
    {
        value = FALSE;
    }

    return value;
}

static bool populateTuningConfigData(anc_config_t * config_data, anc_mode_t set_mode)
{
    bool value = TRUE;

    if(!populateAncInstanceTuningConfigData(config_data, set_mode, ANC_INSTANCE_0_INDEX))
    {
        value = FALSE;
    }

    if(!populateAncInstanceTuningConfigData(config_data, set_mode, ANC_INSTANCE_1_INDEX))
    {
        value = FALSE;
    }

    return value;
}

#else
static void populateHardwareGains(hardware_gains_t * hardware_gains, uint16 * audio_ps_data)
{
    hardware_gains->feed_forward_a_mic_left = getTuningItem32Bit(&audio_ps_data[GAIN_FFA_MIC_OFFSET_L]);
    hardware_gains->feed_forward_a_mic_right = getTuningItem32Bit(&audio_ps_data[GAIN_FFA_MIC_OFFSET_R]);
    hardware_gains->feed_forward_b_mic_left = getTuningItem32Bit(&audio_ps_data[GAIN_FFB_MIC_OFFSET_L]);
    hardware_gains->feed_forward_b_mic_right = getTuningItem32Bit(&audio_ps_data[GAIN_FFB_MIC_OFFSET_R]);
    hardware_gains->dac_output_left = getTuningItem32Bit(&audio_ps_data[GAIN_DAC_OUTPUT_A_OFFSET]);
    hardware_gains->dac_output_right = getTuningItem32Bit(&audio_ps_data[GAIN_DAC_OUTPUT_B_OFFSET]);
}

static bool readTuningKey(uint32 key, uint16 * read_buffer)
{
    uint16 total_key_length = 0;
    return ((PsReadAudioKey(key, read_buffer, ANC_TUNING_CONFIG_DATA_SIZE,
                ANC_TUNING_CONFIG_HEADER_SIZE, &total_key_length) == ANC_TUNING_CONFIG_DATA_SIZE)
            && (total_key_length == ANC_TUNING_CONFIG_TOTAL_SIZE));
}

static bool populateTuningConfigData(anc_config_t * config_data, anc_mode_t set_mode)
{
    uint16 read_buffer[ANC_TUNING_CONFIG_DATA_SIZE];
    bool value = TRUE;

    if(readTuningKey(ANC_MODE_CONFIG_KEY(set_mode), read_buffer))
    {
       populateInstance(&config_data->mode.instance[ANC_INSTANCE_0_INDEX], &read_buffer[INSTANCE_0_OFFSET]);
       populateInstance(&config_data->mode.instance[ANC_INSTANCE_1_INDEX], &read_buffer[INSTANCE_1_OFFSET]);
       populateHardwareGains(&config_data->hardware_gains, read_buffer);
    }
    else
    {
       value = FALSE;
    }
    
    return value;   
}
#endif

static bool isPsKeyValid(uint16 key, uint16 number_of_elements)
{
    return (PsRetrieve(key, NULL, 0) == number_of_elements);
}

static void populateDeviceSpecificHardwareGains(hardware_gains_t * hardware_gains)
{
    if(isPsKeyValid(ANC_HARDWARE_TUNING_KEY, production_hardware_gain_index_max))
    {
        uint16 gains[production_hardware_gain_index_max];

        PsRetrieve(ANC_HARDWARE_TUNING_KEY, gains, production_hardware_gain_index_max);

        hardware_gains->feed_forward_a_mic_left = (gains[production_hardware_gain_index_feed_forward_mic_a_low_16]
                                                    | (gains[production_hardware_gain_index_feed_forward_mic_a_high_16] << 16));
        hardware_gains->feed_forward_a_mic_right = (gains[production_hardware_gain_index_feed_forward_mic_b_low_16]
                                                    | (gains[production_hardware_gain_index_feed_forward_mic_b_high_16] << 16));
        hardware_gains->feed_forward_b_mic_left = (gains[production_hardware_gain_index_feed_back_mic_a_low_16]
                                                 | (gains[production_hardware_gain_index_feed_back_mic_a_high_16] << 16));
        hardware_gains->feed_forward_b_mic_right = (gains[production_hardware_gain_index_feed_back_mic_b_low_16]
                                                 | (gains[production_hardware_gain_index_feed_back_mic_b_high_16] << 16));
        hardware_gains->dac_output_left = (gains[production_hardware_gain_index_dac_a_low_16]
                                              | (gains[production_hardware_gain_index_dac_a_high_16] << 16));
        hardware_gains->dac_output_right = (gains[production_hardware_gain_index_dac_b_low_16]
                                              | (gains[production_hardware_gain_index_dac_b_high_16] << 16));
    }
}

static void populateAncSpecificPathGain(gain_config_t * gain_config, int16 delta_fine_gain_q_format)
{
    if(delta_fine_gain_q_format == 0)
    {
        return;
    }
    if(gain_config->gain == 0)
    {
        gain_config->gain = 128;
    }

    int16 golden_fine_gain_q_format = convertGainTo16BitQFormat(gain_config->gain);

    golden_fine_gain_q_format += delta_fine_gain_q_format;

    if(golden_fine_gain_q_format > UPPER_LIMIT_Q_FORMAT)
    {
      golden_fine_gain_q_format -= UPPER_LIMIT_Q_FORMAT;
      gain_config->gain_shift++;
    }
    while(golden_fine_gain_q_format < LOWER_LIMIT_Q_FORMAT)
    {
        golden_fine_gain_q_format += (-LOWER_LIMIT_Q_FORMAT);
        gain_config->gain_shift--;
    }

    gain_config->gain = convert16BitQFormatToGain(golden_fine_gain_q_format);
}

static void populateDeviceSpecificAncPathGains(anc_config_t * config_data)
{
    int16 delta_fine_gain_tune[FINE_GAIN_TUNE_DATA_ENTRIES];

    uint16 ret = PsRetrieve(device_ps_key_config.device_ps_key, (uint16*)delta_fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);

    if(ret)
    {
        populateAncSpecificPathGain(&config_data->mode.instance[ANC_INSTANCE_0_INDEX].feed_forward_a.gain_config, delta_fine_gain_tune[FFA_PATH_INDEX]);
        populateAncSpecificPathGain(&config_data->mode.instance[ANC_INSTANCE_1_INDEX].feed_forward_a.gain_config, delta_fine_gain_tune[FFA_PATH_INDEX]);

        populateAncSpecificPathGain(&config_data->mode.instance[ANC_INSTANCE_0_INDEX].feed_forward_b.gain_config, delta_fine_gain_tune[FFB_PATH_INDEX]);
        populateAncSpecificPathGain(&config_data->mode.instance[ANC_INSTANCE_1_INDEX].feed_forward_b.gain_config, delta_fine_gain_tune[FFB_PATH_INDEX]);

        populateAncSpecificPathGain(&config_data->mode.instance[ANC_INSTANCE_0_INDEX].feed_back.gain_config, delta_fine_gain_tune[FB_PATH_INDEX]);
        populateAncSpecificPathGain(&config_data->mode.instance[ANC_INSTANCE_1_INDEX].feed_back.gain_config, delta_fine_gain_tune[FB_PATH_INDEX]);
    }
}

static void populateDeviceSpecificTuningConfigData(anc_config_t * config_data)
{
    populateDeviceSpecificHardwareGains(&config_data->hardware_gains);
    populateDeviceSpecificAncPathGains(config_data);
}


bool ancConfigReadPopulateAncData(anc_config_t * config_data, anc_mode_t set_mode)
{
    bool value = populateTuningConfigData(config_data, set_mode);
    populateDeviceSpecificTuningConfigData(config_data);

    return value;
}

#ifdef ANC_UPGRADE_FILTER
/*! Read fine gain from the Audio PS key for the current mode and gain path specified for given instance
*/
static bool ancInstanceReadFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint8 *gain, unsigned inst_index)
{
    uint16 read_buffer[ANC_SINGLE_INST_TUNING_CONFIG_DATA_SIZE];
    bool status = FALSE;

    if(readAncInstanceTuningKey(ANC_MODE_CONFIG_KEY(mode), read_buffer, inst_index))
    {
        switch(gain_path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
                *gain = getTuningItem16Bit(&read_buffer[FFA_GAIN_OFFSET]);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FFB:
                *gain = getTuningItem16Bit(&read_buffer[FFB_GAIN_OFFSET]);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FB:
                *gain = getTuningItem16Bit(&read_buffer[FB_GAIN_OFFSET]);
                status=TRUE;
                break;
            default:
                break;
        }
    }

    return status;
}

bool ancReadFineGainParallelFilter(anc_mode_t mode, audio_anc_path_id gain_path, uint8 *instance_0_gain, uint8 *instance_1_gain)
{
    bool status = FALSE;

    if(ancInstanceReadFineGain(mode, gain_path, instance_0_gain, ANC_INSTANCE_0_INDEX))
    {
        if(ancInstanceReadFineGain(mode, gain_path, instance_1_gain, ANC_INSTANCE_1_INDEX))
        {
           status = TRUE;
        }
    }

    return status;
}

/*! Read fine gain from the Audio PS key for the current mode and gain path specified for ANC instance 0
*/
bool ancReadFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint8 *gain)
{
    return ancInstanceReadFineGain(mode, gain_path, gain, ANC_INSTANCE_0_INDEX);
}
#else
/*! Read fine gain from the Audio PS key for the current mode and gain path specified for both instances
*/
bool ancReadFineGainParallelFilter(anc_mode_t mode, audio_anc_path_id gain_path, uint8 *instance_0_gain, uint8 *instance_1_gain)
{
    uint16 read_buffer[ANC_TUNING_CONFIG_DATA_SIZE];
    bool status = FALSE;

    if(readTuningKey(ANC_MODE_CONFIG_KEY(mode), read_buffer))
    {
        switch(gain_path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
                *instance_0_gain = getTuningItem16Bit(&read_buffer[FFA_GAIN_OFFSET]);
                *instance_1_gain = getTuningItem16Bit(&read_buffer[FFA_GAIN_OFFSET_R]);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FFB:
                *instance_0_gain = getTuningItem16Bit(&read_buffer[FFB_GAIN_OFFSET]);                
                *instance_1_gain = getTuningItem16Bit(&read_buffer[FFB_GAIN_OFFSET_R]);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FB:
                *instance_0_gain = getTuningItem16Bit(&read_buffer[FB_GAIN_OFFSET]);
                *instance_1_gain = getTuningItem16Bit(&read_buffer[FB_GAIN_OFFSET_R]);
                status=TRUE;
                break;
            default:
                break;
        }
    }

    return status;
}

/*! Read fine gain from the Audio PS key for the current mode and gain path specified
*/
bool ancReadFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint8 *gain)
{
    uint16 read_buffer[ANC_TUNING_CONFIG_DATA_SIZE];
    bool status = FALSE;

    if(readTuningKey(ANC_MODE_CONFIG_KEY(mode), read_buffer))
    {
        switch(gain_path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
                *gain = getTuningItem16Bit(&read_buffer[FFA_GAIN_OFFSET]);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FFB:
                *gain = getTuningItem16Bit(&read_buffer[FFB_GAIN_OFFSET]);
                status=TRUE;
                break;
            case AUDIO_ANC_PATH_ID_FB:
                *gain = getTuningItem16Bit(&read_buffer[FB_GAIN_OFFSET]);
                status=TRUE;
                break;
            default:
                break;
        }
    }

    return status;
}
#endif

void ancReadCoarseGainFromInst(audio_anc_instance inst, audio_anc_path_id path, uint16 *gain)
{
    anc_instance_config_t *instance = getInstanceConfig(inst);
    if(instance)
    {
        switch(path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
            {
                gain_config_t *gain_config = &instance->feed_forward_a.gain_config;
                *gain = gain_config->gain_shift;
            }
            break;
            case AUDIO_ANC_PATH_ID_FFB:
            {
                gain_config_t *gain_config = &instance->feed_forward_b.gain_config;
                *gain = gain_config->gain_shift;
            }
            break;
            case AUDIO_ANC_PATH_ID_FB:
            {
                gain_config_t *gain_config = &instance->feed_back.gain_config;
                *gain = gain_config->gain_shift;
            }
            break;
        default:
            break;
        }
    }
}

void ancReadFineGainFromInst(audio_anc_instance inst, audio_anc_path_id path, uint8 *gain)
{
    anc_instance_config_t *instance = getInstanceConfig(inst);
    if(instance)
    {
        switch(path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
            {
                gain_config_t *gain_config = &instance->feed_forward_a.gain_config;
                *gain = (gain_config->gain & 0xFF);
            }
            break;
            case AUDIO_ANC_PATH_ID_FFB:
            {
                gain_config_t *gain_config = &instance->feed_forward_b.gain_config;
                *gain = (gain_config->gain & 0xFF);
            }
            break;
            case AUDIO_ANC_PATH_ID_FB:
            {
                gain_config_t *gain_config = &instance->feed_back.gain_config;
                *gain = (gain_config->gain & 0xFF);
            }
            break;
        default:
            break;
        }
    }
}

#ifdef ANC_UPGRADE_FILTER
void ancReadRxMixCoarseGainFromInst(audio_anc_instance inst, audio_anc_path_id path, uint16 *gain)
{
    anc_instance_config_t *instance = getInstanceConfig(inst);
    if(instance)
    {
        switch(path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
            {
                gain_config_t *gain_config = &instance->feed_forward_a.rxmix_gain_config;
                *gain = gain_config->gain_shift;
            }
            break;
            case AUDIO_ANC_PATH_ID_FFB:
            {
                gain_config_t *gain_config = &instance->feed_forward_b.rxmix_gain_config;
                *gain = gain_config->gain_shift;
            }
            break;
        default:
            ANC_DEBUG_INFO(("ancReadRxMixCoarseGainFromInst: Invalid ANC Path[%d]", path));
            ANC_PANIC();
            break;
        }
    }
}

void ancReadRxMixFineGainFromInst(audio_anc_instance inst, audio_anc_path_id path, uint8 *gain)
{
    anc_instance_config_t *instance = getInstanceConfig(inst);
    if(instance)
    {
        switch(path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
            {
                gain_config_t *gain_config = &instance->feed_forward_a.rxmix_gain_config;
                *gain = (gain_config->gain & 0xFF);
            }
            break;
            case AUDIO_ANC_PATH_ID_FFB:
            {
                gain_config_t *gain_config = &instance->feed_forward_b.rxmix_gain_config;
                *gain = (gain_config->gain & 0xFF);
            }
            break;
        default:
            ANC_DEBUG_INFO(("ancReadRxMixFineGainFromInst: Invalid ANC Path[%d]", path));
            ANC_PANIC();
            break;
        }
    }
}
#endif

void ancReadModelCoefficients(audio_anc_instance inst, audio_anc_path_id path, uint32 *denominator, uint32* numerator)
{
    anc_instance_config_t *instance = getInstanceConfig(inst);
    if(instance)
    {
        switch(path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
            {
                iir_config_t *coefficients = &instance->feed_forward_a.iir_config;
                memcpy(denominator, &coefficients->coefficients[0], sizeof(uint32) * NUMBER_OF_IIR_COEFF_DENOMINATORS);
                memcpy(numerator, &coefficients->coefficients[NUMBER_OF_IIR_COEFF_DENOMINATORS], sizeof(uint32) * NUMBER_OF_IIR_COEFF_NUMERATORS);
            }
            break;
            case AUDIO_ANC_PATH_ID_FFB:
            {
                iir_config_t *coefficients = &instance->feed_forward_b.iir_config;
                memcpy(denominator, &coefficients->coefficients[0], sizeof(uint32) * NUMBER_OF_IIR_COEFF_DENOMINATORS);
                memcpy(numerator, &coefficients->coefficients[NUMBER_OF_IIR_COEFF_DENOMINATORS], sizeof(uint32) * NUMBER_OF_IIR_COEFF_NUMERATORS);
            }
            break;
            case AUDIO_ANC_PATH_ID_FB:
            {
                iir_config_t *coefficients = &instance->feed_back.iir_config;
                memcpy(denominator, &coefficients->coefficients[0], sizeof(uint32) * NUMBER_OF_IIR_COEFF_DENOMINATORS);
                memcpy(numerator, &coefficients->coefficients[NUMBER_OF_IIR_COEFF_DENOMINATORS], sizeof(uint32) * NUMBER_OF_IIR_COEFF_NUMERATORS);
            }
            break;
        default:
            break;
        }
    }
}
