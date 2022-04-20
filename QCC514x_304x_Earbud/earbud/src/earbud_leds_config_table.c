/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Earbud LED UI Indicator configuration table
*/
#include "earbud_leds_config_table.h"
#include "earbud_led.h"
#include "earbud_sm.h"

#include "earbud_buttons.h"

#include <domain_message.h>
#include <ui_indicator_leds.h>

#include <av.h>
#include <charger_monitor.h>
#include <hfp_profile.h>
#include <media_player.h>
#include <pairing.h>
#include <peer_pair_le.h>
#include <power_manager.h>
#include <telephony_messages.h>
#include <audio_curation.h>
#include <voice_sources.h>

/*! \ingroup configuration

    \brief Contexts to LEDs configuration table

   This is an ordered table that associates contexts with LED patterns.
*/
const ui_provider_context_consumer_indicator_table_t earbud_ui_leds_context_indications_table[] =
{
    {.provider=ui_provider_peer_pairing,
     .context=context_peer_pairing_active,              { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_peer_pairing,
                                                          .led.priority = LED_PRI_LOW}},

#ifdef HAVE_RDP_UI
    {.provider=ui_provider_charger,
     .context=context_charger_error,                      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_charging_error,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE}},
#endif
    {.provider=ui_provider_charger,
     .context=context_charger_fast,                      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_charging_ok,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE}},
    {.provider=ui_provider_charger,
     .context=context_charger_low,                        { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_charging_low,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE}},

    {.provider=ui_provider_handset_pairing,
     .context=context_handset_pairing_active,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_pairing,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_ringing_incoming,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_call_incoming,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_ringing_outgoing,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call,                    { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call_with_incoming,      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_call_incoming,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call_with_outgoing,      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call_with_held,          { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_multiparty_call,         { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_media_player,
     .context=context_media_player_streaming,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_streaming,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_out_of_case_idle_connected,{ .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_idle_connected,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_out_of_case_idle,          { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_idle,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_in_case,                   { .led.action = LED_STOP_PATTERN,
                                                          .led.data.pattern = NULL,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_in_ear,                    { .led.action = LED_STOP_PATTERN,
                                                          .led.data.pattern = NULL,
                                                          .led.priority = LED_PRI_LOW}},
};

/*! \ingroup configuration

   \brief System events to LED configuration table

   This is an ordered table that associates system events with LED patterns.
*/
const ui_event_indicator_table_t earbud_ui_leds_table[] =
{
    {.sys_event=POWER_ON,                               { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_power_on,
                                                          .led.priority = LED_PRI_EVENT},
                                                          .await_indication_completion = TRUE },
    {.sys_event=POWER_OFF,                              { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_power_off,
                                                          .led.priority = LED_PRI_EVENT},
                                                          .await_indication_completion = TRUE },
#ifdef QCC3020_FF_ENTRY_LEVEL_AA
    {.sys_event=APP_BUTTON_HELD_FORCE_RESET,            { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_factory_reset,
                                                          .led.priority = LED_PRI_EVENT}},
#endif
    {.sys_event=TELEPHONY_CALL_CONNECTION_FAILURE,      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_error,
                                                          .led.priority = LED_PRI_EVENT}},
    {.sys_event=TELEPHONY_ERROR,                        { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_error,
                                                          .led.priority = LED_PRI_EVENT}},
    {.sys_event=AV_ERROR,                               { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_error,
                                                          .led.priority = LED_PRI_EVENT}},
    {.sys_event=CHARGER_MESSAGE_DETACHED,               { .led.action = LED_CANCEL_FILTER,
                                                          .led.data.filter = NULL,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
    {.sys_event=CHARGER_MESSAGE_DISABLED,               { .led.action = LED_CANCEL_FILTER,
                                                          .led.data.filter = NULL,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
#ifdef HAVE_RDP_UI
    {.sys_event=CHARGER_MESSAGE_COMPLETED,              { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_charging_complete,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
#else
    {.sys_event=CHARGER_MESSAGE_COMPLETED,              { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_charging_complete,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
    {.sys_event=CHARGER_MESSAGE_CHARGING_OK,            { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_charging_ok,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
    {.sys_event=CHARGER_MESSAGE_CHARGING_LOW,           { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_charging_low,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
#endif
#ifndef HAVE_RDP_UI
    {.sys_event=AUDIO_CURATION_ANC_ON,                         { .led.action = LED_START_PATTERN,
                                                              .led.data.pattern = app_led_pattern_anc_on,
                                                              .led.priority = LED_PRI_HIGH}},
    {.sys_event=AUDIO_CURATION_ANC_OFF,                         { .led.action = LED_START_PATTERN,
                                                              .led.data.pattern = app_led_pattern_anc_off,
                                                              .led.priority = LED_PRI_HIGH}},
    {.sys_event=AUDIO_CURATION_ANC_MODE_CHANGED,                         { .led.action = LED_START_PATTERN,
                                                              .led.data.pattern = app_led_pattern_anc_mode_changed,
                                                              .led.priority = LED_PRI_HIGH}},
#endif
#ifdef CORVUS_YD300
    {.sys_event=PAIRING_COMPLETE,                         { .led.action = LED_STOP_PATTERN,
                                                                .led.data.pattern = NULL,
                                                                .led.priority = LED_PRI_MEDIUM}},
#endif
#ifdef DEMO_PRIMARY_SECONDARY
    {.sys_event=EARBUD_ROLE_PRIMARY,                    { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_primary,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
    {.sys_event=EARBUD_ROLE_SECONDARY,                  { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_secondary,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
    {.sys_event=EARBUD_ROLE_NO_ROLE,                    { .led.action = LED_CANCEL_FILTER,
                                                          .led.data.filter = NULL,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
#endif
};

uint8 EarbudLedsConfigTable_EventsTableGetSize(void)
{
    return ARRAY_DIM(earbud_ui_leds_table);
}

uint8 EarbudLedsConfigTable_ContextsTableGetSize(void)
{
    return ARRAY_DIM(earbud_ui_leds_context_indications_table);
}
