/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager_private.h
\defgroup   anc_state_manager anc 
\ingroup    audio_domain
\brief      Private header for State manager for Active Noise Cancellation (ANC).
*/


#ifndef ANC_STATE_MANAGER_PRIVATE_H_
#define ANC_STATE_MANAGER_PRIVATE_H_

#include "aanc_quiet_mode.h"
#include "state_proxy.h"
#include <task_list.h>
/* ANC State Manager Events which are triggered during state transitions */
typedef enum
{
    anc_state_manager_event_initialise,
    anc_state_manager_event_power_on,
    anc_state_manager_event_power_off,
    anc_state_manager_event_enable,
    anc_state_manager_event_disable,
    anc_state_manager_event_set_mode_1,
    anc_state_manager_event_set_mode_2,
    anc_state_manager_event_set_mode_3,
    anc_state_manager_event_set_mode_4,
    anc_state_manager_event_set_mode_5,
    anc_state_manager_event_set_mode_6,
    anc_state_manager_event_set_mode_7,
    anc_state_manager_event_set_mode_8,
    anc_state_manager_event_set_mode_9,
    anc_state_manager_event_set_mode_10,
    anc_state_manager_event_toggle_way,
    anc_state_manager_event_activate_anc_tuning_mode,
    anc_state_manager_event_deactivate_tuning_mode,
    anc_state_manager_event_activate_adaptive_anc_tuning_mode,
    anc_state_manager_event_deactivate_adaptive_anc_tuning_mode,
    anc_state_manager_event_set_anc_leakthrough_gain,
    anc_state_manager_event_config_timer_expiry, /*Handled in Enabled state and ignored in other states*/
    anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry, /*Handled in Enabled state and ignored in other states*/
    anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry, /*Handled in Enabled state and ignored in other states*/
    anc_state_manager_event_usb_enumerated_start_tuning,
    anc_state_manager_event_usb_detached_stop_tuning,
    anc_state_manager_event_read_aanc_ff_gain_timer_expiry,
    anc_state_manager_event_aanc_quiet_mode_detected,
    anc_state_manager_event_aanc_quiet_mode_not_detected,
    anc_state_manager_event_aanc_quiet_mode_enable,
    anc_state_manager_event_aanc_quiet_mode_disable,
    anc_state_manager_event_implicit_enable,
    anc_state_manager_event_implicit_disable,
    anc_state_manager_event_set_filter_path_gains,
	anc_state_manager_event_set_filter_path_gains_on_mode_change,
    anc_state_manager_event_concurrency_connect,
    anc_state_manager_event_concurrency_disconnect
} anc_state_manager_event_id_t;

/*!
    \brief Gets the list of clients registered with ANC state manager.
    \param None.
    \return
*/
#ifdef ENABLE_ANC
task_list_t *AncStateManager_GetClientTask(void);
#else
#define AncStateManager_GetClientTask() (NULL)
#endif

/*!
    \brief Event handler for AANC and quiet mode.
    \param anc_state_manager_event_id_t -event received by anc_state_manager for quiet mode.
    \return None

*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_HandleEvent(anc_state_manager_event_id_t event);
#else
#define AancQuietMode_HandleEvent(x) ((void)(0))
#endif


/*!
    \brief Function responsible for handling handling aanc data received from peer device.
    \param STATE_PROXY_AANC_DATA_T* new_aanc_data - peer data as received by state proxy.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_HandleQuietModeRx(STATE_PROXY_AANC_DATA_T* new_aanc_data);
#else
#define AancQuietMode_HandleQuietModeRx(x) ((void)(0))
#endif


/*!
    \brief Function responsible for reseting all the data used by quiet mode- used typically
           after AAANC-ANC mode change.
    \param None.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_ResetQuietModeData(void);
#else
#define AancQuietMode_ResetQuietModeData() ((void)(0))
#endif

/*!
    \brief Function responsible handling all the quiet mode operation once quiet mode is detected locally.
    \param None.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_HandleQuietModeDetected(void);
#else
#define AancQuietMode_HandleQuietModeDetected() ((void)(0))
#endif

/*!
    \brief Function responsible handling all the quiet mode operation once quiet mode is cleared locally.
    \param None.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_HandleQuietModeCleared(void);
#else
#define AancQuietMode_HandleQuietModeCleared() ((void)(0))
#endif

/*!
    \brief Function responsible handling all the quiet mode operation once quiet once the peer device decides to
           go in quiet mode.
    \param None.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_HandleQuietModeEnable(void);
#else
#define AancQuietMode_HandleQuietModeEnable() ((void)(0))
#endif

/*!
    \briefFunction responsible handling all the quiet mode operation once quiet once the peer device decides to
           go in quiet mode.
    \param None.
    \return None
*/
#ifdef ENABLE_ADAPTIVE_ANC
void AancQuietMode_HandleQuietModeDisable(void);
#else
#define AancQuietMode_HandleQuietModeDisable() ((void)(0))
#endif

#endif /* ANC_STATE_MANAGER_PRIVATE_H_ */
