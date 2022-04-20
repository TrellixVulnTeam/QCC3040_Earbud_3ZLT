/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Earbud Tones UI Indicator configuration table
*/
#include "earbud_tones_config_table.h"
#include "earbud_tones.h"
#include "earbud_buttons.h"

#include <domain_message.h>
#include <ui_inputs.h>
#include <ui_indicator_tones.h>

#include <av.h>
#include <gaming_mode.h>
#include <pairing.h>
#include <peer_signalling.h>
#include <telephony_messages.h>
#include <handset_service_protected.h>
#include <power_manager.h>
#include <volume_service.h>
#if defined INCLUDE_AMA || defined INCLUDE_GAA
#include <voice_ui_message_ids.h>
#endif

/*! \ingroup configuration

   \brief Sytem events to tones configuration table

   This is an ordered table that associates system events with specific tones.
*/
#ifdef INCLUDE_TONES
const ui_event_indicator_table_t earbud_ui_tones_table[] =
{
    {.sys_event=TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE,  { .tone.tone = app_tone_hfp_ring,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_TRANSFERED,                          { .tone.tone = app_tone_hfp_talk_long_press,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_LINK_LOSS_OCCURRED,                  { .tone.tone = app_tone_hfp_link_loss,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifndef QCC3020_FF_ENTRY_LEVEL_AA
    {.sys_event=TELEPHONY_ERROR,                               { .tone.tone = app_tone_error,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=AV_CONNECTED_PEER,                             { .tone.tone = app_tone_av_connected,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif
    {.sys_event=AV_LINK_LOSS,                                  { .tone.tone = app_tone_av_link_loss,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=AV_ERROR,                                      { .tone.tone = app_tone_error,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifdef INCLUDE_AV
    {.sys_event=AV_A2DP_NOT_ROUTED,                            { .tone.tone = app_tone_a2dp_not_routed,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif /* INCLUDE_AV */
#ifdef INCLUDE_GAMING_MODE
    {.sys_event=GAMING_MODE_ON,                                { .tone.tone = app_tone_gaming_mode_on,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=GAMING_MODE_OFF,                               { .tone.tone = app_tone_gaming_mode_off,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif
    {.sys_event=VOLUME_SERVICE_MIN_VOLUME,                     { .tone.tone = app_tone_volume_limit,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=VOLUME_SERVICE_MAX_VOLUME,                     { .tone.tone = app_tone_volume_limit,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=LI_MFB_BUTTON_SINGLE_PRESS,                    { .tone.tone = app_tone_button,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#ifdef HAVE_RDP_UI
    {.sys_event=CAP_SENSE_DOUBLE_PRESS_HOLD,                   { .tone.tone = app_tone_button,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#else
    {.sys_event=LI_MFB_BUTTON_HELD_1SEC,                       { .tone.tone = app_tone_button,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#endif
    {.sys_event=LI_MFB_BUTTON_HELD_3SEC,                       { .tone.tone = app_tone_button_2,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=LI_MFB_BUTTON_HELD_6SEC,                       { .tone.tone = app_tone_button_3,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#ifdef HAVE_RDP_UI
    {.sys_event=LI_MFB_BUTTON_HELD_FACTORY_RESET_DS_CANCEL,    { .tone.tone = app_tone_av_disconnected,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=APP_ANC_SET_NEXT_MODE,                         { .tone.tone = app_tone_double_tap,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#else
    {.sys_event=LI_MFB_BUTTON_HELD_8SEC,                       { .tone.tone = app_tone_button_4,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#endif
#ifdef INCLUDE_DFU
    {.sys_event=LI_MFB_BUTTON_RELEASE_DFU,                     { .tone.tone = app_tone_button_dfu,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=LI_MFB_BUTTON_HELD_DFU,                        { .tone.tone = app_tone_button_dfu,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#endif
    {.sys_event=LI_MFB_BUTTON_HELD_FACTORY_RESET_DS,           { .tone.tone = app_tone_button_factory_reset,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#ifdef EXCLUDE_CONN_PROMPTS
    {.sys_event=HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND,   { .tone.tone = app_tone_hfp_connected,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=HANDSET_SERVICE_DISCONNECTED_IND,              { .tone.tone = app_tone_hfp_disconnected,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif
#ifdef INCLUDE_AMA
#ifdef HAVE_RDP_UI
    {.sys_event=VOICE_UI_AMA_UNREGISTERED,                     { .tone.tone = app_tone_ama_unregistered,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=VOICE_UI_AMA_NOT_CONNECTED,                    { .tone.tone = app_tone_ama_not_connected,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif /* HAVE_RDP_UI */
#ifdef INCLUDE_WUW
    {.sys_event=VOICE_UI_AMA_WUW_DISABLED,                     { .tone.tone = app_tone_ama_wuw_disabled,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=VOICE_UI_AMA_WUW_ENABLED,                      { .tone.tone = app_tone_ama_wuw_enabled,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif  /* INCLUDE_WUW */
#endif  /* INCLUDE_AMA */
#ifdef INCLUDE_GAA
#ifdef INCLUDE_WUW
    {.sys_event=VOICE_UI_GAA_DOFF,                             { .tone.tone = app_tone_doff,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif /* INCLUDE_WUW */
#endif /* INCLUDE_GAA */
#ifdef EXCLUDE_POWER_PROMPTS
    {.sys_event=POWER_ON,                                      { .tone.tone = app_tone_power_on,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=POWER_OFF,                                     { .tone.tone = app_tone_battery_empty,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE },
                                                                 .await_indication_completion = TRUE },
#endif
};

/*! \ingroup configuration

   \brief Repeating system events to tones configuration table

   This is an ordered table that associates repeating system events with specific tones.
*/
const ui_repeating_indication_table_t earbud_ui_repeating_tones_table[] =
{
    {.triggering_sys_event = TELEPHONY_MUTE_ACTIVE,              .reminder_period = 15,
                                                                 .cancelling_sys_event = TELEPHONY_MUTE_INACTIVE,
                                                               { .tone.tone = app_tone_hfp_mute_reminder,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }}
};

#endif

uint8 EarbudTonesConfigTable_SingleGetSize(void)
{
#ifdef INCLUDE_TONES
    return ARRAY_DIM(earbud_ui_tones_table);
#else
    return 0;
#endif
}

uint8 EarbudTonesConfigTable_RepeatingGetSize(void)
{
#ifdef INCLUDE_TONES
    return ARRAY_DIM(earbud_ui_repeating_tones_table);
#else
    return 0;
#endif
}
