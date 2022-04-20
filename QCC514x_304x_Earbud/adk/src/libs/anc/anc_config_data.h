/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_config_data.h

DESCRIPTION
    Architecture specific config data.
*/

#ifndef ANC_CONFIG_DATA_H_
#define ANC_CONFIG_DATA_H_

#include "anc.h"
#include <csrtypes.h>

#define NUMBER_OF_ANC_INSTANCES            2
#ifdef ANC_UPGRADE_FILTER
#define NUMBER_OF_IIR_COEFFICIENTS          (17U) /* 8th order filter (8-denominators and 9-numerators in S7.24 format) */
#define NUMBER_OF_IIR_COEFF_DENOMINATORS    (8U)
#define NUMBER_OF_IIR_COEFF_NUMERATORS      (9U)
#define NUMBER_OF_WORDS_IN_IIR_COEFFICIENT  (2U)  /* S7.24 format requires 2 words */
#define MSW_16BIT_SHIFT                     (16U)
#else
#define NUMBER_OF_IIR_COEFFICIENTS          (15U) /* 7th order filter (7-denominators and 8-numerators in S2.9 format) */
#define NUMBER_OF_IIR_COEFF_DENOMINATORS    (7U)
#define NUMBER_OF_IIR_COEFF_NUMERATORS      (8U)
#define NUMBER_OF_WORDS_IN_IIR_COEFFICIENT  (1U)  /* S2.9 requires 1 word */
#endif

#define NUMBER_OF_IIR_COEFFICIENT_WORDS     (NUMBER_OF_IIR_COEFFICIENTS * NUMBER_OF_WORDS_IN_IIR_COEFFICIENT)
#define LSW_16BIT_MASK                      (0xFFFFU)

#define FINE_GAIN_TUNE_DATA_ENTRIES        3
#define FINE_GAIN_TUNE_DATA_SIZE          (FINE_GAIN_TUNE_DATA_ENTRIES)

#define UPPER_LIMIT_Q_FORMAT              (int16)(3065)  /* Upper limit for 6.0db in Q6.9 format */
#define LOWER_LIMIT_Q_FORMAT              (int16)(-3082) /* Lower limit for -6.0db in Q6.9 format */
#define MAXIMUM_FINE_GAIN                 (255U)
enum
{
    ANC_INSTANCE_0_INDEX,
    ANC_INSTANCE_1_INDEX,
    ANC_INSTANCE_MAX_INDEX = ANC_INSTANCE_1_INDEX
};

enum
{
    FFA_PATH_INDEX,
    FFB_PATH_INDEX,
    FB_PATH_INDEX
};

typedef struct
{
    uint16 device_ps_key;
}device_ps_key_config_t;

/* ANC Instance config */

typedef struct
{
    uint32 coefficients[NUMBER_OF_IIR_COEFFICIENTS];
} iir_config_t;

typedef struct
{
    uint16 lpf_shift1:4; /* valid values 1-9 */
    uint16 lpf_shift2:4;
    uint16 unused:8;
} lpf_config_t;

typedef struct
{
    lpf_config_t lpf_config;
    iir_config_t iir_config;
} filter_path_config_t;


/* ANC audio path config */

typedef struct
{
    uint16 filter_shift:4;  /* valid values 0 to 11 - for QCC514x, 5x, 6x devices */
                            /* valid values 0 to 15 - for QCC517x device */
    uint16 filter_enable:1; /* valid values 0/1 */
    uint16 unused:11;
} dc_filter_config_t;

typedef dc_filter_config_t small_lpf_config_t;

typedef struct
{
    uint32 dmic_x2_ff;
} dmic_x2_config_t;

typedef struct
{
    uint16 gain:8; /* valid values 0-255 */
    uint16 unused:8;
    uint16 gain_shift; /* 4 bits, (-4) to (+7) - for QCC514x, 5x, 6x devices */
                       /* 4 bits, (-8) to (+7) - for QCC517x device */
} gain_config_t;

typedef struct
{
    iir_config_t iir_config;
    lpf_config_t lpf_config;
    dc_filter_config_t dc_filter_config;
    dmic_x2_config_t upconvertor_config;
    gain_config_t gain_config;
#ifdef ANC_UPGRADE_FILTER
    gain_config_t rxmix_gain_config;
#endif
} feed_forward_path_config_t;

#ifdef ANC_UPGRADE_FILTER
typedef struct
{
    uint32 self_mix;
    uint32 cross_mix;
} rxmix_enables_t;
#endif

typedef struct
{
    iir_config_t iir_config;
    lpf_config_t lpf_config;
    gain_config_t gain_config;
#ifdef ANC_UPGRADE_FILTER
    rxmix_enables_t rxmix_enables;
#endif

} feed_back_path_config_t;

typedef struct
{
    small_lpf_config_t small_lpf_config;
} small_lpf_path_config_t;

typedef struct
{
    feed_forward_path_config_t feed_forward_a;
    feed_forward_path_config_t feed_forward_b;
    feed_back_path_config_t feed_back;
    small_lpf_path_config_t small_lpf;

    uint16 enable_mask:4;
    uint16 unused:12;
} anc_instance_config_t;

typedef struct
{
    uint32 feed_forward_a_mic_left;
    uint32 feed_forward_a_mic_right;
    uint32 feed_forward_b_mic_left;
    uint32 feed_forward_b_mic_right;
    uint32 dac_output_left;
    uint32 dac_output_right;
} hardware_gains_t;

typedef struct
{
    anc_instance_config_t instance[NUMBER_OF_ANC_INSTANCES];
} anc_mode_config_t;

typedef struct
{
    anc_mode_config_t mode;
    hardware_gains_t hardware_gains;
} anc_config_t;


/******************************************************************************
    Function stubs
******************************************************************************/
bool ancConfigDataUpdateOnModeChange(anc_mode_t mode);

/*!
    @brief Convert fine gain value to 16-bit fixed-point format(Q6.9).

    @param gain fine gain value.
    @return 16-bit fixed-point format(Q6.9)
    Note: This API works in the range of 1-255 fine gain values.
*/
int16 convertGainTo16BitQFormat(uint16 gain);

/*!
    @brief Convert 16-bit fixed-point format(Q6.9) into gain value.

    @param q_format 16-bit fixed-point format(Q6.9).
    @return Fine gain value.
*/
uint16 convert16BitQFormatToGain(int16 q_format);

#endif
