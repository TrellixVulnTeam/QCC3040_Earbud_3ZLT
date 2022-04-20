/* Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd. */
/*  */

/*!
@file   anc.h
@brief  Header file for the ANC library.

        This file provides documentation for the ANC library API.
*/

#ifndef ANC_H_
#define ANC_H_

#include <audio_plugin_common.h>
#include <audio_plugin_music_variants.h>

#include <csrtypes.h>
#include <app/audio/audio_if.h>

/*! @brief The ANC operating topology
*/
typedef enum
{
    anc_single_filter_topology,
    anc_parallel_filter_topology
}anc_filter_topology_t;

/*! @brief The ANC Mode of operation when ANC functionality is enabled.

    Used by the VM application to set different ANC modes and also get the
    current mode of operation.
 */
typedef enum
{
    anc_mode_1,  /* ANC operational mode 1 and associated with UCID = 0 */
    anc_mode_2,  /* ANC operational mode 2 and associated with UCID = 1 */
    anc_mode_3,  /* ANC operational mode 3 and associated with UCID = 2 */
    anc_mode_4,  /* ANC operational mode 4 and associated with UCID = 3 */
    anc_mode_5,  /* ANC operational mode 5 and associated with UCID = 4 */
    anc_mode_6,  /* ANC operational mode 6 and associated with UCID = 5 */
    anc_mode_7,  /* ANC operational mode 7 and associated with UCID = 6 */
    anc_mode_8,  /* ANC operational mode 8 and associated with UCID = 7 */
    anc_mode_9,  /* ANC operational mode 9 and associated with UCID = 8 */
    anc_mode_10, /* ANC operational mode 10 and associated with UCID = 9 */
    anc_mode_fit_test = 63, /* ANC special mode for fit-test usecase and associated with UCID = 63 */
} anc_mode_t;

/*! @brief The ANC Path to be enabled

    Used by the VM application to set ANC paths
 */
typedef enum
{
    all_disabled                    = 0x00,
    feed_forward_left               = 0x01,
    feed_forward_right              = 0x02,
    feed_back_left                  = 0x04,
    feed_back_right                 = 0x08,

    feed_forward_mode               = (feed_forward_left | feed_forward_right),
    feed_forward_mode_left_only     = feed_forward_left,
    feed_forward_mode_right_only    = feed_forward_right,

    feed_back_mode                  = (feed_back_left | feed_back_right),
    feed_back_mode_left_only        = feed_back_left,
    feed_back_mode_right_only       = feed_back_right,

    hybrid_mode                     = (feed_forward_left | feed_forward_right | feed_back_left | feed_back_right),
    hybrid_mode_left_only           = (feed_forward_left | feed_back_left),
    hybrid_mode_right_only          = (feed_forward_right | feed_back_right)
} anc_path_enable;


/*! @brief The ANC Microphones to be used by the library.

    Used by the VM application to provide the ANC library with all the required
    information about the microphones to use.
 */
typedef struct
{
    audio_mic_params feed_forward_left;
    audio_mic_params feed_forward_right;
    audio_mic_params feed_back_left;
    audio_mic_params feed_back_right;
    anc_path_enable enabled_mics;
} anc_mic_params_t;

/*! @brief The ANC filter path gain configuration.

    Used by the VM application to provide the ANC library with all the required
    information about user gain configuration to use.
 */
typedef struct
{
    uint8_t ffa_gain;      /* FFA filter path fine gain configuration (valid values: 0-255) */
    uint8_t ffb_gain;      /* FFB filter path fine gain configuration (valid values 0-255) */
    uint8_t fb_gain;       /* FB filter path fine gain configuration (valid values 0-255) */
    uint8_t enable_ffa_gain_update:1;
    uint8_t enable_ffb_gain_update:1;
    uint8_t enable_fb_gain_update:1;
    uint8_t enable_ffa_gain_shift_update:1;
    uint8_t enable_ffb_gain_shift_update:1;
    uint8_t enable_fb_gain_shift_update:1;
    uint8_t unused:2;
    int16_t ffa_gain_shift; /* FFA filter path coarse gain configuration (valid values: (-4) to (+7) ) */
    int16_t ffb_gain_shift; /* FFB filter path coarse gain configuration (valid values: (-4) to (+7) ) */
    int16_t fb_gain_shift;  /* FB filter path coarse gain configuration (valid values: (-4) to (+7) ) */
} anc_user_gain_config_t;

/*!
    @brief Initialise the ANC library. This function must be called, and return
           indicating success, before any of the other library API functions can
           be called.

    @param mic_params The microphones to use for ANC functionality.
    @param init_mode The ANC mode at initialisation
    @param init_gain The gain at initialisation

    @return TRUE indicating the library was successfully initialised otherwise FALSE.
*/
bool AncInit(anc_mic_params_t *mic_params, anc_mode_t init_mode);

#ifdef HOSTED_TEST_ENVIRONMENT
/*!
    @brief free the memory allocated for the ANC library data.

    @return TRUE indicating the memory allocated for library was successfully freed doesn't return FALSE.
*/
bool AncLibraryTestReset(void);
#endif

/*!
    @brief Enable or Disable the ANC functionality. If enabled then the ANC will
           start operating in the last set ANC mode. To ensure no audio artefacts,
           the ANC functionality should not be enabled or disabled while audio is
           being routed to the DACs.

    @param mic_params The microphones to use for ANC functionality.

    @return TRUE indicating ANC was successfully enabled or disabled otherwise FALSE.
*/
bool AncEnable(bool enable);

/*!
    @brief Enable the ANC functionality using user specific gains for filter paths: FFA, FFB and FB.
           ANC will start operating in the last set ANC mode flter coefficients.

    @param left_channel_gain Filter path fine and coarse gains to use for ANC Instance 0

    @param right_channel_gain Filter path fine and coarse gains to use for ANC Instance 1

    @return TRUE indicating ANC was successfully enabled otherwise FALSE.
*/
bool AncEnableWithUserGain(anc_user_gain_config_t *left_channel_gain, anc_user_gain_config_t *right_channel_gain);

/*!
    @brief Enable the ANC functionality with muted gains for filter paths: FFA, FFB and FB.
           ANC will start operating in the last set ANC mode flter coefficients.

    @return TRUE indicating ANC was successfully enabled otherwise FALSE.
*/
bool AncEnableWithMutePathGains(void);

/*!
    @brief Query the current state of the ANC functionality.

    @return TRUE if ANC is currently enabled otherwise FALSE.
*/
bool AncIsEnabled(void);


/*!
    @brief Set the ANC operating mode or ANC special mode. To ensure no audio artefacts, the ANC mode
           should not be changed while audio is being routed to the DACs.

    @param anc_mode_t The ANC mode to set.

    @return TRUE indicating the ANC mode was successfully changed otherwise FALSE.
*/
bool AncSetMode(anc_mode_t mode);


/*!
    @brief Set the filter coefficients for ANC operating mode or ANC special mode.

    @param anc_mode_t The ANC mode to set.

    @return TRUE indicating the ANC filter coeffients was successfully changed otherwise FALSE.
*/
bool AncSetModeFilterCoefficients(anc_mode_t mode);

/*!
    @brief Set the ANC operating mode. It reads the filter coefficients and path gains from persistent memory
           and configured them into ANC hadrware

    @param anc_mode_t The ANC mode to set.

    @param enable_coarse_gains True to set path coarse gains (FFA, FFB, FB) into ANC hardware.

    @param enable_fine_gains True to set path fine gains (FFA, FFB, FB) into ANC hardware.

    @return TRUE indicating the ANC mode was successfully changed otherwise FALSE.
*/

bool AncSetModeWithSelectedGains(anc_mode_t mode, bool enable_coarse_gains, bool enable_fine_gains);

/*!
    @brief Returns the current ANC mic config.

    @return Pointer to current anc_mic_params_t stored in the ANC library
*/
anc_mic_params_t * AncGetAncMicParams(void);

/*!
    @brief Set the ANC filter path FFA, FFB and FB gains. If the ANC functionality is enabled then
           the ANC filter path gains will be applied immediately otherwise it will be ignored.
           This function shall be used along with AncEnableWithMutePathGains() to supress pops/clicks in ANC path.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncSetCurrentFilterPathGains(void);

/*!
    @brief Set the ANC filter path FFA gain. If the ANC functionality is enabled then
           the ANC filter path FFA gain will be applied immediately otherwise it will be ignored.

    @param instance The audio ANC hardware instance number.
    @param gain The ANC filter path FFA gain to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncConfigureFFAPathGain(audio_anc_instance instance, uint8 gain);

/*!
    @brief Set the ANC filter path FFB gain. If the ANC functionality is enabled then
           the ANC filter path FFB gain will be applied immediately otherwise it will be ignored.

    @param instance The audio ANC hardware instance number.
    @param gain The ANC filter path FFB gain to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncConfigureFFBPathGain(audio_anc_instance instance, uint8 gain);

/*!
    @brief Set the ANC filter path FB gain. If the ANC functionality is enabled then
           the ANC filter path FB gain will be applied immediately otherwise it will be ignored.

    @param instance The audio ANC hardware instance number.
    @param gain The ANC filter path FB gain to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncConfigureFBPathGain(audio_anc_instance instance, uint8 gain);

/*!
    @brief Read the ANC fine gain from the Audio PS key for current mode and specified gain path

    ADVISORY : THIS API IS RECOMMENDED TO BE USED DURING PRODUCTION TUNING/CALIBRATION ONLY.

    @param gain_path    The gain path to read gain from
    @param *gain          Pointer to gain value - This supports instance0

    @return TRUE indicating the ANC gain was successfully read otherwise FALSE.
*/
bool AncReadFineGain(audio_anc_path_id gain_path, uint8 *gain);

/*!
    @brief Read the ANC fine gain from the Audio PS key for current mode and specified gain path for both ANC instances

    ADVISORY : THIS API IS RECOMMENDED TO BE USED DURING PRODUCTION TUNING/CALIBRATION ONLY.

    @param gain_path    The gain path to read gain from
    @param *gain        Pointer to gain value for ANC instance 0
    @param *gain        Pointer to gain value for ANC instance 1

    @return TRUE indicating the ANC gain was successfully read otherwise FALSE.
*/
bool AncReadFineGainParallelFilter(audio_anc_path_id gain_path, uint8 *instance_0_gain, uint8 *instance_1_gain);

/*!
    @brief Write the Delta gain value (difference between the write fine gain and golden config) to the 
    Device PS key for the current mode and gain path specified.
    The Delta gain value is applicable to both instances of ANC.

    ADVISORY : THIS API IS RECOMMENDED TO BE USED DURING PRODUCTION TUNING/CALIBRATION ONLY.
    There is a risk to the product in detuning pre-set values if used in other scenarios.

    @param gain_path    The gain path to write the gain to
    @param gain         The ANC fine gain value to be updated - This supports instance0

    @return TRUE indicating the ANC gain was successfully changed otherwise FALSE.
*/
bool AncWriteFineGain(audio_anc_path_id gain_path, uint8 gain);

/*!
    @brief Write the ANC fine gain to the Audio PS key for current mode and specified gain path for both ANC instances

    ADVISORY : THIS API IS RECOMMENDED TO BE USED DURING PRODUCTION TUNING/CALIBRATION ONLY.
    There is a risk to the product in detuning pre-set values if used in other scenarios.

    @param gain_path               The gain path to write the gain to
    @param instance_0_gain         The ANC fine gain value to be updated for instance0
    @param instance_1_gain         The ANC fine gain value to be updated for instance1

    @return TRUE indicating the ANC gain was successfully changed otherwise FALSE.
*/
bool AncWriteFineGainParallelFilter(audio_anc_path_id gain_path, uint8 instance_0_gain, uint8 instance_1_gain);

/*!
    @brief Read the ANC coarse gain for specificed path from the particular ANC HW instance

    @param inst     The ANC Hardware instance to get the gains from
    @param gain_path    The gain path to read gain from (valid options: AUDIO_ANC_PATH_ID_FFA or AUDIO_ANC_PATH_ID_FFB or AUDIO_ANC_PATH_ID_FB)
    @param *gain        Pointer to gain value

*/
void AncReadCoarseGainFromInstance(audio_anc_instance inst, audio_anc_path_id gain_path, uint16 *gain);

/*!
    @brief Read the ANC fine gain for specificed path from the particular ANC HW instance

    @param inst     The ANC Hardware instance to get the gains from
    @param gain_path    The gain path to read gain from (valid options: AUDIO_ANC_PATH_ID_FFA or AUDIO_ANC_PATH_ID_FFB or AUDIO_ANC_PATH_ID_FB)
    @param *gain        Pointer to gain value

*/
void AncReadFineGainFromInstance(audio_anc_instance inst, audio_anc_path_id gain_path, uint8 *gain);

/*!
    @brief Read the ANC RxMix coarse gain for specificed path from the particular ANC HW instance

    @param inst     The ANC Hardware instance to get the gains from
    @param gain_path    The gain path to read gain from (valid options: AUDIO_ANC_PATH_ID_FFA or AUDIO_ANC_PATH_ID_FFB)
    @param *gain        Pointer to coarse gain value

    <Note> This feature is available only from QCC517x audio firmware onwards.

*/
void AncReadRxMixCoarseGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint16 *gain);

/*!
    @brief Read the ANC RxMix fine gain for specificed path from the particular ANC HW instance

    @param inst     The ANC Hardware instance to get the gains from
    @param gain_path    The gain path to read gain from (valid options: AUDIO_ANC_PATH_ID_FFA or AUDIO_ANC_PATH_ID_FFB)
    @param *gain        Pointer to RxMix fine gain value

    <Note> This feature is available only from QCC517x audio firmware onwards.

*/
void AncReadRxMixFineGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint8 *gain);

/*!
    @brief Read the Model Coefficient(s)

    @param inst     The ANC Hardware instance to get the coefficients from
    @param path    The path to get the coefficients
    @param denominator     Output variable to hold the denominator coefficients
    @param numerator        Output variable to hold the numerator coefficients

    @return TRUE indicating the ANC gain was successfully changed otherwise FALSE.
*/
void AncReadModelCoefficients(audio_anc_instance inst, audio_anc_path_id path, uint32 *denominator, uint32* numerator);

/*!
    @brief Read number of denominator model coefficients

    @return Value based on ANC filter order.
*/
uint32 AncReadNumOfDenominatorCoefficients(void);

/*!
    @brief Read number of numerator model coefficients

    @return Value based on ANC filter order.
*/
uint32 AncReadNumOfNumeratorCoefficients(void);

/*!
    @brief Set the Parallel-ANC filter path FFA gain. If the ANC functionality is enabled then
           the ANC filter path FFA gain will be applied immediately otherwise it will be ignored.
           Note:- In parllel-ANC configuration it is required to send the gain value for both ANC instances.

    @param gain for ANC filter path FFA for AUDIO_ANC_INSTANCE_0  to set.
    @param gain for ANC filter path FFA for AUDIO_ANC_INSTANCE_1  to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncConfigureParallelFilterFFAPathGain(uint8 instance_0_gain, uint8 instance_1_gain);

/*!
    @brief Set the Parallel-ANC filter path FFB gain. If the ANC functionality is enabled then
           the ANC filter path FFB gain will be applied immediately otherwise it will be ignored.
           Note:- In parllel-ANC configuration it is required to send the gain value for both ANC instances.

    @param gain for ANC filter path FFB for AUDIO_ANC_INSTANCE_0  to set.
    @param gain for ANC filter path FFB for AUDIO_ANC_INSTANCE_1  to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncConfigureParallelFilterFFBPathGain(uint8 instance_0_gain, uint8 instance_1_gain);

/*!
    @brief Set the Parallel-ANC filter path FB gain. If the ANC functionality is enabled then
           the ANC filter path FB gain will be applied immediately otherwise it will be ignored.
           Note:- In parllel-ANC configuration it is required to send the gain value for both ANC instances.

    @param gain for ANC filter path FB for AUDIO_ANC_INSTANCE_0  to set.
    @param gain for ANC filter path FB for AUDIO_ANC_INSTANCE_1  to set.

    @return TRUE indicating the ANC filter path gain was successfully changed
            otherwise FALSE.
*/
bool AncConfigureParallelFilterFBPathGain(uint8 instance_0_gain, uint8 instance_1_gain);


/*!
   @brief sets the ANC filter Topology
   Note:- This API is just a placeholder for ANC topology. Using this API when ANC is running - will result in un-expected behaviour.
   This API needs to be used only when ANC is disabled.
   This API needs to be used only when ANC library is initialised i.e. after the AncInit() function only.
   The Default mode of operation is single_filter_topology - need not be called if using static ANC.
   The Recommended sequence for using Parallel ANC / Enhanced ANC is as follows,
   1.AncInit()
   2.AncSetTopology()
   3.AncEnable()
   @return TRUE indicating the topology for parallel ANC is changed.
*/

bool AncSetTopology(anc_filter_topology_t anc_topology);

/*!
    @brief Set the PS key to store delta gain (in DB) between ANC golden gain configuration and calibrated gain during production
    test: FFA, FFB and FB.
    Application to set the PS key during initialization.

    @param device_ps_key ps key to store delta gain values.

*/
void AncSetDevicePsKey(uint16 device_ps_key);

#endif
