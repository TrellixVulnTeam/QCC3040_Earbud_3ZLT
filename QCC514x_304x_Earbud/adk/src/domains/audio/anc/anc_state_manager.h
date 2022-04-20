/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager.h
\defgroup   anc_state_manager anc 
\ingroup    audio_domain
\brief      State manager for Active Noise Cancellation (ANC).

Responsibilities:
  Handles state transitions between init, enable and disable states.
  The ANC audio domain is used by \ref audio_curation.
*/

#ifndef ANC_STATE_MANAGER_H_
#define ANC_STATE_MANAGER_H_

/*\{*/
#include <anc.h>
#include <operators.h>
#include <rtime.h>
#include <marshal_common.h>
#include "domain_message.h"

/*! \brief ANC state manager defines the various states handled in ANC. */
typedef enum
{
    anc_state_manager_uninitialised,
    anc_state_manager_power_off,
    anc_state_manager_enabled,
    anc_state_manager_disabled,
    anc_state_manager_tuning_mode_active,
    anc_state_manager_adaptive_anc_tuning_mode_active
} anc_state_manager_t;

typedef struct
{
    uint8 mode;
} ANC_UPDATE_MODE_CHANGED_IND_T;

typedef struct
{
    uint8 anc_gain;
} ANC_UPDATE_GAIN_IND_T;

typedef struct
{
    uint8 aanc_ff_gain; /* FF gain */
} AANC_FF_GAIN_UPDATE_IND_T; /* Used to Update ANC Clients when local device AANC FF Gain is read from capablity*/

/*Currently FF Gain is the only logging information. If any data is added, this double typecasting can be removed */
typedef AANC_FF_GAIN_UPDATE_IND_T AANC_LOGGING_T;

typedef struct
{
    uint8 left_aanc_ff_gain;
    uint8 right_aanc_ff_gain;
} AANC_FF_GAIN_NOTIFY_T; /* Used to notify ANC Clients with both(local & remote device) FF Gains*/


/* Supported ANC toggle configurations */
typedef enum
{
    anc_toggle_config_off = 0,
    anc_toggle_config_mode_1,
    anc_toggle_config_mode_2,
    anc_toggle_config_mode_3,
    anc_toggle_config_mode_4,
    anc_toggle_config_mode_5,
    anc_toggle_config_mode_6,
    anc_toggle_config_mode_7,
    anc_toggle_config_mode_8,
    anc_toggle_config_mode_9,
    anc_toggle_config_mode_10,
    anc_toggle_config_is_same_as_current = 0xFF,
    anc_toggle_config_not_configured = 0xFF
} anc_toggle_config_t;

/*! \brief ANC toggle configuration msg ids. */
typedef enum {
    anc_toggle_way_config_id_1 = 1,
    anc_toggle_way_config_id_2,
    anc_toggle_way_config_id_3,
} anc_toggle_way_config_id_t;

/*! \brief ANC scenario configuration msg ids. */
typedef enum {
    anc_scenario_config_id_standalone = 1,
    anc_scenario_config_id_playback,
    anc_scenario_config_id_sco,
    anc_scenario_config_id_va
} anc_scenario_config_id_t;

typedef struct
{
    anc_toggle_way_config_id_t anc_toggle_config_id;
    anc_toggle_config_t anc_config;
} ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T;

typedef struct
{
    anc_scenario_config_id_t anc_scenario_config_id;
    anc_toggle_config_t anc_config;
} ANC_SCENARIO_CONFIG_UPDATE_IND_T;

typedef struct
{
    anc_scenario_config_id_t scenario;
} ANC_CONCURRENCY_CONNECT_REQ_T;

typedef ANC_CONCURRENCY_CONNECT_REQ_T ANC_CONCURRENCY_DISCONNECT_REQ_T;

/*! \brief Events sent by Anc_Statemanager to other modules. */
typedef enum
{
    ANC_UPDATE_STATE_DISABLE_IND = ANC_MESSAGE_BASE,
    ANC_UPDATE_STATE_ENABLE_IND,
    ANC_UPDATE_MODE_CHANGED_IND,
    ANC_UPDATE_GAIN_IND,
    ANC_TOGGLE_WAY_CONFIG_UPDATE_IND,
    ANC_SCENARIO_CONFIG_UPDATE_IND,
    ANC_UPDATE_DEMO_MODE_DISABLE_IND,
    ANC_UPDATE_DEMO_MODE_ENABLE_IND,
    ANC_UPDATE_AANC_ADAPTIVITY_PAUSED_IND,
    ANC_UPDATE_AANC_ADAPTIVITY_RESUMED_IND,
    AANC_FF_GAIN_UPDATE_IND,
    AANC_FF_GAIN_NOTIFY,
    ANC_UPDATE_QUIETMODE_ON_IND,
    ANC_UPDATE_QUIETMODE_OFF_IND,
    AANC_UPDATE_QUIETMODE_IND,

    /*! This must be the final message */
    ANC_MESSAGE_END
} anc_msg_t;

#define ANC_MAX_TOGGLE_CONFIG (3U)

/*! \brief ANC toggle way configurations. */
typedef struct {
    uint16 anc_toggle_way_config[ANC_MAX_TOGGLE_CONFIG];
} anc_toggle_way_config_t;

/*! \brief ANC toggle configuration during scenarios e.g., standalone, playback, SCO, VA. */
typedef struct {
    uint16 anc_config;
    uint16 is_same_as_current;
} anc_toggle_config_during_scenario_t;

/* Register with state proxy after initialization */
#ifdef ENABLE_ANC
void AncStateManager_PostInitSetup(void);
#else
#define AncStateManager_PostInitSetup() ((void)(0))
#endif

/*!
    \brief Initialisation of ANC feature, reads microphone configuration  
           and default mode.
    \param init_task Unused
    \return TRUE always.
*/
#ifdef ENABLE_ANC
bool AncStateManager_Init(Task init_task);
#else
#define AncStateManager_Init(x) (FALSE)
#endif

#ifdef ENABLE_ANC
TaskData *AncStateManager_GetTask(void);
#else
#define AncStateManager_GetTask() (NULL)
#endif


#ifdef ENABLE_ANC
bool AncStateManager_CheckIfDspClockBoostUpRequired(void);
#else
#define AncStateManager_CheckIfDspClockBoostUpRequired() (FALSE)
#endif
/*!
    \brief ANC specific handling due to the device Powering On.
*/
#ifdef ENABLE_ANC
void AncStateManager_PowerOn(void);
#else
#define AncStateManager_PowerOn() ((void)(0))
#endif

/*!
    \brief ANC specific handling due to the device Powering Off.
*/  
#ifdef ENABLE_ANC
void AncStateManager_PowerOff(void);
#else
#define AncStateManager_PowerOff() ((void)(0))
#endif

/*!
    \brief Enable ANC functionality.  
*/   
#ifdef ENABLE_ANC
void AncStateManager_Enable(void);
#else
#define AncStateManager_Enable() ((void)(0))
#endif

/*! 
    \brief Disable ANC functionality.
 */   
#ifdef ENABLE_ANC
void AncStateManager_Disable(void);
#else
#define AncStateManager_Disable() ((void)(0))
#endif

/*!
    \brief Is ANC supported in this build ?

    This just checks if ANC may be supported in the build.
    Separate checks are needed to determine if ANC is permitted
    (licenced) or enabled.

    \return TRUE if ANC is enabled in the build, FALSE otherwise.
 */
#ifdef ENABLE_ANC
#define AncStateManager_IsSupported() TRUE
#else
#define AncStateManager_IsSupported() FALSE
#endif


/*!
    \brief Set the operating mode of ANC to configured mode_n. 
    \param mode To be set from existing avalaible modes 0 to 9.
*/
#ifdef ENABLE_ANC
void AncStateManager_SetMode(anc_mode_t mode);
#else
#define AncStateManager_SetMode(x) ((void)(0 * (x)))
#endif

/*!
    \brief Handles the toggle way event from the user to switch to configured ANC mode
     This config comes from the GAIA app
    \param mode None
*/
#ifdef ENABLE_ANC
void AncStateManager_HandleToggleWay(void);
#else
#define AncStateManager_HandleToggleWay() ((void)(0))
#endif


/*!
    \brief Get the AANC params to implicitly enable ANC on a SCO call
    \param KYMERA_INTERNAL_AANC_ENABLE_T parameters to configure AANC capability
*/
#ifdef ENABLE_ANC
void AncStateManager_GetAdaptiveAncEnableParams(bool *in_ear, audio_anc_path_id *control_path, adaptive_anc_hw_channel_t *hw_channel, anc_mode_t *current_mode);
#else
#define AncStateManager_GetAdaptiveAncEnableParams(in_ear, control_path, hw_channel, current_mode) ((void)in_ear, (void)control_path, (void)hw_channel, (void)current_mode)
#endif

/*! 
    \brief Get the Anc mode configured.
    \return mode which is set (from available mode 0 to 9).
 */
#ifdef ENABLE_ANC
anc_mode_t AncStateManager_GetMode(void);
#else
#define AncStateManager_GetMode() (0)
#endif

/*! 
    \brief Checks if ANC is due to be enabled.
    \return TRUE if it is enabled else FALSE.
 */
#ifdef ENABLE_ANC
bool AncStateManager_IsEnabled (void);
#else
#define AncStateManager_IsEnabled() (FALSE)
#endif

/*! 
    \brief Get the Anc mode configured.
    \return mode which is set (from available mode 0 to 9).
 */
#ifdef ENABLE_ANC
anc_mode_t AncStateManager_GetCurrentMode(void);
#else
#define AncStateManager_GetCurrentMode() (0)
#endif

/*! 
    \brief The function returns the number of modes configured.
    \return total modes in anc_modes_t.
 */
#ifdef ENABLE_ANC
uint8 AncStateManager_GetNumberOfModes(void);
#else
#define AncStateManager_GetNumberOfModes() (0)
#endif

/*!
    \brief Checks whether tuning mode is currently active.
    \return TRUE if it is active, else FALSE.
 */
#ifdef ENABLE_ANC
bool AncStateManager_IsTuningModeActive(void);
#else
#define AncStateManager_IsTuningModeActive() (FALSE)
#endif

/*! 
    \brief Cycles through next mode and sets it.
 */
#ifdef ENABLE_ANC
void AncStateManager_SetNextMode(void);
#else
#define AncStateManager_SetNextMode() ((void)(0))
#endif

/*! 
    \brief Enters ANC tuning mode.
 */
#ifdef ENABLE_ANC
void AncStateManager_EnterAncTuningMode(void);
#else
#define AncStateManager_EnterAncTuningMode() ((void)(0))
#endif

/*! 
    \brief Exits the ANC tuning mode.
 */
#ifdef ENABLE_ANC
void AncStateManager_ExitAncTuningMode(void);
#else
#define AncStateManager_ExitAncTuningMode() ((void)(0))
#endif

/*! 
    \brief Enters Adaptive ANC tuning mode.
 */
#if defined(HOSTED_TEST_ENVIRONMENT) || (defined(ENABLE_ANC) && defined(ENABLE_ADAPTIVE_ANC))
void AncStateManager_EnterAdaptiveAncTuningMode(void);
#else
#define AncStateManager_EnterAdaptiveAncTuningMode() ((void)(0))
#endif

/*! 
    \brief Exits Adaptive ANC tuning mode.
 */
#if defined(HOSTED_TEST_ENVIRONMENT) || (defined(ENABLE_ANC) && defined(ENABLE_ADAPTIVE_ANC))
void AncStateManager_ExitAdaptiveAncTuningMode(void);
#else
#define AncStateManager_ExitAdaptiveAncTuningMode() ((void)(0))
#endif

/*!
    \brief Checks whether Adaptive ANC tuning mode is currently active.
    \return TRUE if it is active, else FALSE.
 */
#if defined(HOSTED_TEST_ENVIRONMENT) || (defined(ENABLE_ANC) && defined(ENABLE_ADAPTIVE_ANC))
bool AncStateManager_IsAdaptiveAncTuningModeActive(void);
#else
#define AncStateManager_IsAdaptiveAncTuningModeActive() (FALSE)
#endif

/*! 
    \brief Updates ANC feedforward fine gain from ANC Data structure to ANC H/W. This is not applicable when in 'Mode 1'.
		   AncStateManager_StoreAncLeakthroughGain(uint8 leakthrough_gain) has to be called BEFORE calling AncStateManager_UpdateAncLeakthroughGain()
		   
		   This function shall be called for "World Volume Leakthrough".
		   
 */
#ifdef ENABLE_ANC
void AncStateManager_UpdateAncLeakthroughGain(void);
#else
#define AncStateManager_UpdateAncLeakthroughGain() ((void)(0))
#endif

/*! \brief Register a Task to receive notifications from Anc_StateManager.

    Once registered, #client_task will receive #shadow_profile_msg_t messages.

    \param client_task Task to register to receive shadow_profile notifications.
*/
#ifdef ENABLE_ANC
void AncStateManager_ClientRegister(Task client_task);
#else
#define AncStateManager_ClientRegister(x) ((void)(0))
#endif

/*! \brief Un-register a Task that is receiving notifications from Anc_StateManager.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from shadow_profile notifications.
*/
#ifdef ENABLE_ANC
void AncStateManager_ClientUnregister(Task client_task);
#else
#define AncStateManager_ClientUnregister(x) ((void)(0))
#endif

/*! \brief To obtain gain for current mode stored in ANC data structure

    \returns gain of ANC H/w
*/
#ifdef ENABLE_ANC
uint8 AncStateManager_GetAncGain(void);
#else
#define AncStateManager_GetAncGain() (0)
#endif

/*! \brief To store Leakthrough gain in ANC data structure

    \param leakthrough_gain Leakthrough gain to be stored
*/
#ifdef ENABLE_ANC
void AncStateManager_StoreAncLeakthroughGain(uint8 leakthrough_gain);
#else
#define AncStateManager_StoreAncLeakthroughGain(x) ((void)(0))
#endif

/*!
    \brief Interface to get ANC toggle configuration
 */
#ifdef ENABLE_ANC
anc_toggle_config_t AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_t config_id);
#else
#define AncStateManager_GetAncToggleConfiguration(x) ((0 * (x)))
#endif

/*!
    \brief Interface to set ANC toggle configuration
 */
#ifdef ENABLE_ANC
void AncStateManager_SetAncToggleConfiguration(anc_toggle_way_config_id_t config_id, anc_toggle_config_t config);
#else
#define AncStateManager_SetAncToggleConfiguration(x, y) ((void)((0 * x) * (0 * y)))
#endif

/*!
    \brief Interface to get ANC scenario configuration
 */
#ifdef ENABLE_ANC
anc_toggle_config_t AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_t config_id);
#else
#define AncStateManager_GetAncScenarioConfiguration(x) ((0 * (x)))
#endif

/*!
    \brief Interface to set ANC scenario configuration
 */
#ifdef ENABLE_ANC
void AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_t config_id, anc_toggle_config_t config);
#else
#define AncStateManager_SetAncScenarioConfiguration(x, y) ((void)((0 * x) * (0 * y)))
#endif

/*!
    \brief Interface to enable Adaptive ANC Adaptivity
 */
#ifdef ENABLE_ANC
void AncStateManager_EnableAdaptiveAncAdaptivity(void);
#else
#define AncStateManager_EnableAdaptiveAncAdaptivity() ((void)(0))
#endif

/*!
    \brief Interface to disable Adaptive ANC Adaptivity
 */
#ifdef ENABLE_ANC
void AncStateManager_DisableAdaptiveAncAdaptivity(void);
#else
#define AncStateManager_DisableAdaptiveAncAdaptivity() ((void)(0))
#endif

/*!
    \brief Interface to get Adaptive ANC Adaptivity
 */
#ifdef ENABLE_ANC
bool AncStateManager_GetAdaptiveAncAdaptivity(void);
#else
#define AncStateManager_GetAdaptiveAncAdaptivity() (FALSE)
#endif

/*!
    \brief Return if Device supports Demo mode
    \param None
    \returns TRUE if supported, FALSE otherwise
*/
#ifdef ENABLE_ANC
bool AncStateManager_IsDemoSupported(void);
#else
#define AncStateManager_IsDemoSupported() FALSE
#endif

/*!
    \brief Return if Device is in Demo State
    \param None
    \returns TRUE if Active, FALSE otherwise
*/
#ifdef ENABLE_ANC
bool AncStateManager_IsDemoStateActive(void);
#else
#define AncStateManager_IsDemoStateActive() FALSE
#endif

/*!
    \brief Set the Demo State
    \param To put in Demo State or exit
*/
#ifdef ENABLE_ANC
void AncStateManager_SetDemoState(bool demo_active);
#else
#define AncStateManager_SetDemoState(x) ((void)(0 * (x)))
#endif

/*!
    \brief Check if a particular ANC mode is Adaptive or not
    \param mode Existing ANC modes anc_mode_1 to anc_mode_10
*/
#ifdef ENABLE_ANC
bool AncConfig_IsAncModeAdaptive(anc_mode_t anc_mode);
#else
#define AncConfig_IsAncModeAdaptive(x) (FALSE)
#endif

/*!
    \brief Check if a particular ANC mode is Leakthrough or not
    \param mode Existing ANC modes anc_mode_1 to anc_mode_10
*/
#ifdef ENABLE_ANC
bool AncConfig_IsAncModeLeakThrough(anc_mode_t anc_mode);
#else
#define AncConfig_IsAncModeLeakThrough(x) (FALSE)
#endif

/*!
    \brief Check if a particular ANC mode is Static or not
    \param mode Existing ANC modes anc_mode_1 to anc_mode_10
*/
#ifdef ENABLE_ANC
bool AncConfig_IsAncModeStatic(anc_mode_t anc_mode);
#else
#define AncConfig_IsAncModeStatic(x) (FALSE)
#endif


/*! 
    \brief Test hook for unit tests to reset the ANC state.
    \param state  Reset the particular state
 */
#ifdef ANC_TEST_BUILD

#ifdef ENABLE_ANC
void AncStateManager_ResetStateMachine(anc_state_manager_t state);
#else
#define AncStateManager_ResetStateMachine(x) ((void)(0))
#endif

#endif /* ANC_TEST_BUILD*/

/*\}*/
#endif /* ANC_STATE_MANAGER_H_ */
