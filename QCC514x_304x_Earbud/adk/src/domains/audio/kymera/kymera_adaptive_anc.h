/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_adaptive_anc.h
\brief      Private header to connect/manage to Adaptive ANC chain
*/

#ifndef KYMERA_ADAPTIVE_ANC_H_
#define KYMERA_ADAPTIVE_ANC_H_

#include <operators.h>
#include <anc.h>

/*!Structure that defines inputs for SCO Tx path */
typedef struct
{
    Sink cVc_in1;
    Sink cVc_in2;
    Sink cVc_ref_in;
}adaptive_anc_sco_send_t;

/*! Enum to define AANC use case */
typedef enum
{
    aanc_usecase_default,
    aanc_usecase_standalone,
    aanc_usecase_sco_concurrency
} aanc_usecase_t ;

typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T;

/*! \brief The KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP message content. */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T;

/*!Structure that defines Adaptive ANC connection parameters */
typedef struct
{
    bool in_ear;                          /*! to provide in-ear / out-ear status to adaptive anc capability */
    audio_anc_path_id control_path;       /*! to decide if FFa path becomes control or FFb */
    adaptive_anc_hw_channel_t hw_channel; /*! Hadware instance to select */
    anc_mode_t current_mode;              /*Current ANC mode*/
} KYMERA_INTERNAL_AANC_ENABLE_T;

/*! Connect parameters for Adaptive ANC tuning  */
typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} adaptive_anc_tuning_connect_parameters_t;

/*! Disconnect parameters for Adaptive ANC tuning  */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} adaptive_anc_tuning_disconnect_parameters_t;

/*! \brief Registers AANC callbacks in the mic interface layer
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_Init(void);
#else
#define KymeraAdaptiveAnc_Init() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_EnableGentleMute(void);
#else
#define KymeraAdaptiveAnc_EnableGentleMute() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_SetUcid(anc_mode_t mode);
#else
#define KymeraAdaptiveAnc_SetUcid(mode) ((void) mode)
#endif


#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_ApplyModeChange(anc_mode_t new_mode, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel);
#else
#define KymeraAdaptiveAnc_ApplyModeChange(new_mode, feedforward_anc_path, hw_channel) ((void)new_mode, (void)feedforward_anc_path, (void)hw_channel)
#endif


/*! \brief Enable Adaptive ANC capability
    \param msg - Adaptive ANC connection param \ref KYMERA_INTERNAL_AANC_ENABLE_T
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_Enable(const KYMERA_INTERNAL_AANC_ENABLE_T* msg);
#else
#define KymeraAdaptiveAnc_Enable(msg) ((void)(0))
#endif

/*! \brief Disable Adaptive ANC
    \param void
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_Disable(void);
#else
#define KymeraAdaptiveAnc_Disable() ((void)(0))
#endif

/*! \brief Update Adaptive ANC that earbud is in-ear
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_UpdateInEarStatus(void);
#else
#define KymeraAdaptiveAnc_UpdateInEarStatus() ((void)(0))
#endif

/*! \brief Update Adaptive ANC that earbud is out-of ear
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_UpdateOutOfEarStatus(void);
#else
#define KymeraAdaptiveAnc_UpdateOutOfEarStatus() ((void)(0))
#endif

/*! \brief Enables the adaptivity sub-module in the Adaptive ANC capability
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_EnableAdaptivity(void);
#else
#define KymeraAdaptiveAnc_EnableAdaptivity() ((void)(0))
#endif

/*! \brief Disables the adaptivity sub-module in the Adaptive ANC capability
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DisableAdaptivity(void);
#else
#define KymeraAdaptiveAnc_DisableAdaptivity() ((void)(0))
#endif

/*! \brief Get the Feed Forward gain
    \param gain - pointer to get the value
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_GetFFGain(uint8* gain);
#else
#define KymeraAdaptiveAnc_GetFFGain(gain) ((void)(0))
#endif

/*! \brief Set the Mantissa & Exponent values
    \param gain - pointer to get the value
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_SetGainValues(uint32 mantissa, uint32 exponent);
#else
#define KymeraAdaptiveAnc_SetGainValues(mantissa, exponent) ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_EnableQuietMode(void);
#else
#define KymeraAdaptiveAnc_EnableQuietMode() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DisableQuietMode(void);
#else
#define KymeraAdaptiveAnc_DisableQuietMode() ((void)(0))
#endif

/*! \brief Obtain Current Adaptive ANC mode from AANC operator
    \param aanc_mode - pointer to get the mode value
    \return TRUE if current mode is stored in aanc_mode, else FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_ObtainCurrentAancMode(adaptive_anc_mode_t *aanc_mode);
#else
#define KymeraAdaptiveAnc_ObtainCurrentAancMode(aanc_mode) (FALSE)
#endif

/*! \brief Identify if noise level is below Quiet Mode threshold
    \param void
    \return TRUE if noise level is below threshold, otherwise FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_IsNoiseLevelBelowQmThreshold(void);
#else
#define KymeraAdaptiveAnc_IsNoiseLevelBelowQmThreshold() (FALSE)
#endif

/*! \brief Start Adaptive Anc tuning procedure.
           Note that Device has to be enumerated as USB audio device before calling this API.
    \param adaptive anc tuning connect parameters
    \return void
*/

#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_EnterAdaptiveAncTuning(const adaptive_anc_tuning_connect_parameters_t *param);
#else
#define kymeraAdaptiveAnc_EnterAdaptiveAncTuning(param) ((void)(param))
#endif

/*! \brief Stop Adaptive ANC tuning procedure.
    \param adaptive anc tuning disconnect parameters
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_ExitAdaptiveAncTuning(const adaptive_anc_tuning_disconnect_parameters_t *param);
#else
#define kymeraAdaptiveAnc_ExitAdaptiveAncTuning(param) ((void)(param))
#endif

/*! \brief Check if Adaptive ANC and other concurrency audio source is active.
    \param void
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_IsConcurrencyActive(void);
#else
#define KymeraAdaptiveAnc_IsConcurrencyActive() (FALSE)
#endif

/*! \brief Check if Adaptive ANC has enabled.
    \return void
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_IsEnabled(void);
#else
#define KymeraAdaptiveAnc_IsEnabled() (FALSE)
#endif

/*! \brief Creates the Kymera Adaptive ANC Tuning Chain
    \param msg internal message which has the adaptive anc tuning connect parameters
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *msg);
#else
#define kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(x) ((void)(x))
#endif

/*! \brief Destroys the Kymera Adaptive ANC Tuning Chain
    \param msg internal message which has the adaptive anc tuning disconnect parameters
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T *msg);
#else
#define kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain(x) ((void)(x))
#endif

#endif /* KYMERA_ADAPTIVE_ANC_H_ */
