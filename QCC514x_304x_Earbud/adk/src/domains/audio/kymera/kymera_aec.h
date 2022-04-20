/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header to connect/manage to AEC chain
*/

#ifndef KYMERA_AEC_H_
#define KYMERA_AEC_H_

#include "kymera_ucid.h"
#include <chain.h>
#include <operators.h>

/*! Default mic TTP latency, needed for VA */
#define AEC_REF_DEFAULT_MIC_TTP_LATENCY     (40000)

#define AEC_REF_STF_GAIN_EXP_PARAM_INDEX 14
#define AEC_REF_STF_GAIN_MANTISSA_PARAM_INDEX 15

#define AEC_REF_CONFIG_PARAM_INDEX 0x0000
#define AEC_REF_CONFIG_PARAM_DEFAULT 0x2080
#define AEC_REF_CONFIG_PARAM_ENABLE_SIDETONE 0x2090

#define AEC_REF_SAME_INPUT_OUTPUT_CLK_SOURCE 0x0008

/*Bitmaps to identify valid MIC for standalone and concurrency use cases*/
#define AEC_BITMAP_MIC1 0x1
#define AEC_BITMAP_MIC2 0x2
#define AEC_BITMAP_MIC3 0x4
#define AEC_BITMAP_MIC4 0x8

#define AEC_MAX_NUM_OF_MICS (4)

typedef struct
{
    uint16 id;
    uint16 value;
} aec_ref_set_same_in_out_clk_src_msg_t;

typedef struct
{
    Source input_1;
    Source input_2;
} aec_connect_audio_output_t;

typedef struct
{
    Sink   reference_output;
    
    Source mic_input_1;
    Source mic_input_2;
    Source mic_input_3;
    Source mic_input_4;

    Sink   mic_output_1;
    Sink   mic_output_2;
    Sink   mic_output_3;
    Sink   mic_output_4;

} aec_connect_audio_input_t;

typedef struct
{   
    uint8 mic_disconnect_bitmap;    /*Disconnect AEC input and output in case of concurrency*/

} aec_disconnect_audio_input_t;

typedef struct
{
    uint32 ttp_delay;
    uint32 ttp_gate_delay;
    uint32 spk_sample_rate;
    uint32 mic_sample_rate;
    uint32 buffer_size;
    bool is_source_clock_same;
}aec_audio_config_t;

/* descibes for what use-case AEC is being used */
typedef enum
{
    /* Non-Leakthrough usecases: */
    aec_usecase_default,
    /* Leakthrough usecases start here:
     * A separation between non-LT and LT usecases is needed
     * to determine a change of UCID when LT is activated */
    aec_usecase_enable_leakthrough,
    aec_usecase_create_leakthrough_chain,
} aec_usecase_t ;

typedef struct
{
    uint8 num_of_mics;
    Sink aec_reference;
    const Sink *mics;
} aec_mic_path_output_t;

typedef struct
{
    uint8 num_of_mics;
    const Source *mics;
} aec_mic_path_input_t;

/*! \brief Connect audio output source to AEC.
    \param params All parameters required to configure/connect to the AEC chain.
    \note Handles the creation of the AEC chain.
*/
void Kymera_ConnectAudioOutputToAec(const aec_connect_audio_output_t *params, const aec_audio_config_t* config);

/*! \brief Disconnect audio output source from AEC.
    \note Handles the destruction of the AEC chain.
*/
void Kymera_DisconnectAudioOutputFromAec(void);

/*! \brief Connect audio input source to AEC.
    \param params All parameters required to configure/connect to the AEC chain.
    \note Handles the creation of the AEC chain.
*/
void Kymera_ConnectAudioInputToAec(const aec_connect_audio_input_t *params, const aec_audio_config_t* config);

/*! \brief Connect to AEC mic path input.
    \param params All parameters required to configure/connect to the AEC chain.
    \note Handles the creation of the AEC chain.
*/
void Kymera_ConnectToAecMicPathInput(const aec_mic_path_input_t *params, const aec_audio_config_t* config);

/*! \brief Connect to AEC mic path output (should be called after mic path input is connected).
    \param params All parameters required to configure/connect to the AEC chain.
    \note Handles the creation of the AEC chain.
*/
void Kymera_ConnectToAecMicPathOutput(const aec_mic_path_output_t *params);

/*! \brief Disconnect audio input source from AEC.
    \note Handles the destruction of the AEC chain.
*/
void Kymera_DisconnectAudioInputFromAec(void);

/*! \brief get UCID for AEC_REF operator */
kymera_operator_ucid_t Kymera_GetAecUcid(void);

/*! \brief Disconnect audio input source from AEC considering concurrency cases.
*/
void Kymera_DisconnectSelectedAudioInputFromAec(const aec_disconnect_audio_input_t *params);

/*! \brief Get AEC Operator
*/
Operator Kymera_GetAecOperator(void);

/*! \brief Gets the AEC reference use-case
*/
aec_usecase_t Kymera_GetAecUseCase(void);

/*! \brief Sets the AEC reference use-case
*/
void Kymera_SetAecUseCase(aec_usecase_t usecase);

/*! \brief Enable the sidetone path for AEC */
void Kymera_AecEnableSidetonePath(bool enable);

/*! \brief set Sidetone Gain for AEC */
void Kymera_AecSetSidetoneGain(uint32 exponent_value, uint32 mantissa_value);

/*! \brief Facilitate transition to low power mode for AEC
*/
void Kymera_AecSleep(void);

/*! \brief Facilitate transition to exit low power mode for AEC
*/
void Kymera_AecWake(void);

/*! \brief Check whether mic path input is connected.
           If so you can only connect to the mic paths output.
*/
bool Kymera_AecIsMicPathInputConnected(void);

/*! \brief Returns the last set task period for aec_ref. */
uint32 Kymera_GetAecTaskPeriod(void);

/*! \brief Sets the task period for aec_ref. */
void Kymera_SetAecTaskPeriod(uint32 task_period);

#endif /* KYMERA_AEC_H_ */
