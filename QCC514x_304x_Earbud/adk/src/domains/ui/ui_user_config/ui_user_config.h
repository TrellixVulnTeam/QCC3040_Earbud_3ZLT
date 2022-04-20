/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      User-defined touchpad gesture to UI Input mapping configuration header file.
*/
#ifndef UI_GESTURE_CONFIG_H_
#define UI_GESTURE_CONFIG_H_

#include <csrtypes.h>

#include "ui_inputs.h"

/*! \brief  Type defining the touchpad gesture identifiers, exposed to the end-user via the
            mobile application. These allow the end-user to reconfigure certain gestures to
            perform their chosen action, rather than the application's default UI input for
            the specified gesture.

    \warning The values assigned to the symbolic identifiers of this type MUST NOT be changed,
             new gestures may be appended to the list, the list MUST NOT exceed 128 gestures.

    \note   The above constraints apply because the type is used by a GAIA plug-in for
            communication with the mobile application, where the command payloads are of
            fixed width, designed to hold 7 bit values.
*/
typedef enum
{
    ui_gesture_tap                          = 0,
    ui_gesture_swipe_up                     = 1,
    ui_gesture_swipe_down                   = 2,
    ui_gesture_tap_and_swipe_up             = 3,
    ui_gesture_tap_and_swipe_down           = 4,
    ui_gesture_double_tap                   = 5,
    ui_gesture_long_press                   = 6,

    ui_gesture_end_sentinel                 = 128
} ui_user_config_gesture_id_t;

/*! \brief  Type defining the originating touchpad for a gesture

    \warning This type definition MUST NOT be modified
*/
typedef enum
{
    touchpad_single                         = 0,
    touchpad_right                          = 1,
    touchpad_left                           = 2,
    touchpad_left_and_right                 = 3

} ui_user_config_touchpad_t;

/*! \brief  Type defining context identifiers, exposed to the end-user via the mobile
            application. These context IDs allow the end-user to multiplex several actions
            onto a single gesture, dependent on the device state.


    \warning The values assigned to the symbolic identifiers of this type MUST NOT be changed,
             new gestures may be appended to the list, the list MUST NOT exceed 128 gestures.

    \note   The above constraints apply because the type is used by a GAIA plug-in for
            communication with the mobile application, where the command payloads are of
            fixed width, designed to hold 7 bit values.
*/
typedef enum
{
    ui_context_passthrough                  = 0,
    ui_context_media_streaming              = 1,
    ui_context_media_idle                   = 2,
    ui_context_voice_in_call                = 3,
    ui_context_voice_incoming               = 4,
    ui_context_voice_outgoing               = 5,
    ui_context_voice_in_call_with_incoming  = 6,
    ui_context_voice_in_call_with_outgoing  = 7,
    ui_context_voice_in_call_with_held      = 8,

    ui_context_end_sentinel                 = 128
} ui_user_config_context_id_t;

/*! \brief  Type defining action identifiers, exposed to the end-user via the mobile
            application. These allow the end-user to reconfigure certain gestures to
            perform their chosen action, rather than the application's default UI input for
            the specified gesture.


    \warning The values assigned to the symbolic identifiers of this type MUST NOT be changed,
             new gestures may be appended to the list, the list MUST NOT exceed 128 gestures.

    \note   The above constraints apply because the type is used by a GAIA plug-in for
            communication with the mobile application, where the command payloads are of
            fixed width, designed to hold 7 bit values.
*/
typedef enum
{
    ui_action_media_play_pause_toggle       = 0,
    ui_action_media_stop                    = 1,
    ui_action_media_next_track              = 2,
    ui_action_media_previous_track          = 3,
    ui_action_media_seek_forward            = 4,
    ui_action_media_seek_backward           = 5,
    ui_action_voice_accept_call             = 6,
    ui_action_voice_reject_call             = 7,
    ui_action_voice_hang_up_call            = 8,
    ui_action_voice_transfer_call           = 9,
    ui_action_voice_call_cycle              = 10,
    ui_action_voice_join_calls              = 11,
    ui_action_voice_mic_mute_toggle         = 12,
    ui_action_gaming_mode_toggle            = 13,
    ui_action_anc_enable_toggle             = 14,
    ui_action_anc_next_mode                 = 15,
    ui_action_volume_up                     = 16,
    ui_action_volume_down                   = 17,
    ui_action_reconnect_mru_handset         = 18,
    ui_action_va_privacy_toggle             = 19,
    ui_action_va_fetch_query                = 20,
    ui_action_va_ptt                        = 21,
    ui_action_va_cancel                     = 22,
    ui_action_va_fetch                      = 23,
    ui_action_va_query                      = 24,
    ui_action_disconnect_lru_handset        = 25,
    ui_action_voice_join_calls_hang_up      = 26,

    ui_action_end_sentinel                  = 128
} ui_user_config_action_id_t;

/*! \brief User defined mapping for gestures to UI Inputs table row instance structure */
typedef struct
{
    ui_user_config_gesture_id_t gesture_id;
    ui_user_config_touchpad_t   originating_touchpad;
    ui_user_config_context_id_t context_id;
    ui_user_config_action_id_t  action_id;

} ui_user_gesture_table_content_t;

typedef struct
{
    ui_user_config_context_id_t context_id;
    uint8 context;
} ui_user_config_context_id_map;

/*! \brief Initialise the UI User Config component

    Called during Application start up to initialise the UI User Config component.

    \return TRUE indicating success
*/
bool UiUserConfig_Init(Task init_task);

/*! \brief Register the UI User Config component with the Device Database Serialiser

    Called early in the Application start up to register the UI User Config component with the DBS.
*/
void UiUserConfig_RegisterPddu(void);

/*! \brief Register a Context ID to context mapping for a specific UI Provider.

    This API is called by the various UI Providers in the system that may interact with
    the End-User reconfiguration of the touchpad UI feature.

    A UI Provider that provides state (the context information) for a certain Context ID
    used by the GAIA mobile application, needs to register itself so that the UI domain
    can check this state when the configured gesture is performed on the touchpad. If the
    current context of this UI provider matches that of the configuration for the gesture,
    the End-Users action shall be performed.

    \warning The Context ID to context mapping passed is used directly by this module, it
             is not copied. It is expected that it be located in a const linker section.

    \param provider - The UI Provider which is registering its get context ID mapping.
    \param map - The Context ID to context map for this provider.
    \param map_length - The length of the map
*/
void UiUserConfig_RegisterContextIdMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map * map,
        uint8 map_length);

/*! \brief Set an End-User Gesture Configuration table.
*/
void UiUserConfig_SetUserGestureConfiguration(
        ui_user_gesture_table_content_t * table,
        size_t size);

#endif /* UI_GESTURE_CONFIG_H_ */
