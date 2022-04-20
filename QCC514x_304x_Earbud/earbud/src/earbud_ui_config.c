/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_ui_config.c
\brief      ui configuration table

    This file contains ui configuration table which maps different logical inputs to
    corresponding ui inputs based upon ui provider contexts.
*/

#include "earbud_ui_config.h"
#include "ui.h"
#include "earbud_buttons.h"

/* Needed for UI contexts - transitional; when table is code generated these can be anonymised
 * unsigned ints and these includes can be removed. */
#include "media_player.h"
#include "hfp_profile.h"
#include "bt_device.h"
#include "earbud_sm.h"
#include "power_manager.h"
#include "voice_ui.h"
#include "audio_curation.h"
#include <focus_select.h>
#include <macros.h>

const focus_select_audio_tie_break_t audio_source_focus_tie_break_order[] =
{
    FOCUS_SELECT_AUDIO_LINE_IN,
    FOCUS_SELECT_AUDIO_USB,
    FOCUS_SELECT_AUDIO_A2DP,
    FOCUS_SELECT_AUDIO_LEA_UNICAST,
    FOCUS_SELECT_AUDIO_LEA_BROADCAST,
};

const focus_select_audio_tie_break_t voice_source_focus_tie_break_order[] =
{
    FOCUS_SELECT_VOICE_LEA_UNICAST,
    FOCUS_SELECT_VOICE_HFP,
    FOCUS_SELECT_VOICE_USB,
};

#ifdef INCLUDE_CAPSENSE
const touch_event_config_t touch_event_table [] =
{
    /* Logical Inputs corresponding to end user reconfigurable touchpad gestures */
    {
        SINGLE_PRESS,
        LI_MFB_BUTTON_SINGLE_PRESS
    },
    {
        DOUBLE_PRESS,
        CAP_SENSE_DOUBLE_PRESS
    },
    {
        SLIDE_UP,
        CAP_SENSE_SLIDE_UP
    },
    {
        SLIDE_DOWN,
        CAP_SENSE_SLIDE_DOWN
    },
    {
        TAP_SLIDE_UP,
        CAP_SENSE_TAP_SLIDE_UP
    },
    {
        TAP_SLIDE_DOWN,
        CAP_SENSE_TAP_SLIDE_DOWN
    },
    {
        TOUCH_ONE_SECOND_PRESS_RELEASE,
        LI_MFB_BUTTON_RELEASE_1SEC
    },

    /* Logical Inputs which are not currently configurable by the end user */
    {
        TRIPLE_PRESS,
        CAP_SENSE_TRIPLE_PRESS
    },

    /* The long press and hold 1s and above can be used for VA */
    {
        HAND_COVER,
        APP_VA_BUTTON_DOWN
    },
    {
        TOUCH_ONE_SECOND_PRESS,
        APP_VA_BUTTON_HELD_1SEC
    },
    {
        HAND_COVER_RELEASE,
        APP_VA_BUTTON_RELEASE
    },

    /* The double press and hold events are for accessing standard UI menu options */
    {
        TOUCH_DOUBLE_ONE_SECOND_PRESS,
        CAP_SENSE_DOUBLE_PRESS_HOLD
    },
    {
        TOUCH_DOUBLE_ONE_SECOND_PRESS_RELEASE,
        CAP_SENSE_DOUBLE_PRESS_HOLD_RELEASE
    },
    {
        TOUCH_DOUBLE_THREE_SECOND_PRESS,
        LI_MFB_BUTTON_HELD_3SEC
    },
    {
        TOUCH_DOUBLE_THREE_SECOND_PRESS_RELEASE,
        LI_MFB_BUTTON_RELEASE_3SEC
    },
    {
        TOUCH_DOUBLE_SIX_SECOND_PRESS,
        LI_MFB_BUTTON_HELD_6SEC
    },
    {
        TOUCH_DOUBLE_SIX_SECOND_PRESS_RELEASE,
        LI_MFB_BUTTON_RELEASE_6SEC_DS
    },
    {
        TOUCH_DOUBLE_NINE_SECOND_PRESS,
        LI_MFB_BUTTON_HELD_FACTORY_RESET_DS
    },
    {
        TOUCH_DOUBLE_NINE_SECOND_PRESS_RELEASE,
        LI_MFB_BUTTON_RELEASE_FACTORY_RESET_DS
    },
    {
        TOUCH_DOUBLE_TWELVE_SECOND_PRESS,
        LI_MFB_BUTTON_HELD_FACTORY_RESET_DS_CANCEL
    },
    {
        TOUCH_DOUBLE_TWELVE_SECOND_PRESS_RELEASE,
        LI_IGNORE
    }
};
#endif /* INCLUDE_CAPSENSE */

/*! \ingroup configurable

   \brief Button configuration table

   This is an ordered table that associates logical inputs and contexts with actions.
*/
const ui_config_table_content_t earbud_ui_config_table[] =
{
    {APP_LEAKTHROUGH_TOGGLE_ON_OFF,    ui_provider_audio_curation,  context_leakthrough_disabled,        ui_input_leakthrough_toggle_on_off            },
    {APP_LEAKTHROUGH_TOGGLE_ON_OFF,    ui_provider_audio_curation,  context_leakthrough_enabled,         ui_input_leakthrough_toggle_on_off            },
    {APP_ANC_ENABLE,                   ui_provider_audio_curation,  context_anc_disabled,                ui_input_anc_on                               },
    {APP_ANC_DISABLE,                  ui_provider_audio_curation,  context_anc_enabled,                 ui_input_anc_off                              },
    {APP_ANC_TOGGLE_ON_OFF,            ui_provider_audio_curation,  context_anc_disabled,                ui_input_anc_toggle_on_off                    },
    {APP_ANC_TOGGLE_ON_OFF,            ui_provider_audio_curation,  context_anc_enabled,                 ui_input_anc_toggle_on_off                    },
    {APP_ANC_SET_NEXT_MODE,            ui_provider_audio_curation,  context_anc_disabled,                ui_input_anc_toggle_way                       },
    {APP_ANC_SET_NEXT_MODE,            ui_provider_audio_curation,  context_anc_enabled,                 ui_input_anc_toggle_way                       },
#ifdef CORVUS_YD300 /* This is only for corvus board */
    {APP_ANC_DELETE_PDL,               ui_provider_phy_state,       context_phy_state_out_of_case,       ui_input_sm_delete_handsets                   },
#endif

#ifdef HAVE_RDP_UI
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call,                ui_input_mic_mute_toggle                      },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_reject                    },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_incoming,  ui_input_voice_call_reject                    },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_outgoing,  ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_held,      ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_call_held,              ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_multiparty_call,     ui_input_voice_transfer                       },
    
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept                    },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_incoming,  ui_input_voice_call_accept                    },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_outgoing,  ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_held,      ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_call_held,              ui_input_voice_call_cycle                     },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_multiparty_call,     ui_input_voice_call_hang_up                   },
#else
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept                    },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_incoming,  ui_input_voice_call_accept                    },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_outgoing,  ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_held,      ui_input_voice_call_hang_up                   },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_call_held,              ui_input_voice_call_cycle                     },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_multiparty_call,     ui_input_voice_call_hang_up                   },
    
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call,                ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_reject                    },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_incoming,  ui_input_voice_call_reject                    },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_outgoing,  ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_held,      ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_call_held,              ui_input_voice_transfer                       },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_multiparty_call,     ui_input_voice_transfer                       },
#if defined(INCLUDE_DEVICE_TEST_SERVICE) && defined(INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2)
    {LI_MFB_MFB_BUTTON_5_CLICKS,       ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_dts_mode_dut                         },
#endif

#endif // HAVE_RDP_UI
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_media_player,    context_media_player_streaming,       ui_input_stop_av_connection                   },
    
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause                    },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause                    },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_device,          context_handset_not_connected,        ui_input_connect_handset                      },

#ifdef PRODUCTION_TEST_MODE
    {LI_MFB_BUTTON_HELD_3SEC,         ui_provider_ptm_state,       context_ptm_state_ptm,                ui_input_production_test_mode                 },
    {LI_MFB_BUTTON_RELEASE_3SEC,      ui_provider_ptm_state,       context_ptm_state_ptm,                ui_input_production_test_mode_request         },
#else
    {LI_MFB_BUTTON_RELEASE_3SEC,      ui_provider_telephony,       context_voice_in_call,                ui_input_mic_mute_toggle                      },
#endif

    {LI_MFB_BUTTON_RELEASE_DFU,       ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_dfu_active_when_in_case_request      },

    /* Note: this is a device specific logical input. */
    {LI_MFB_BUTTON_RELEASE_FACTORY_RESET_DS,ui_provider_phy_state, context_phy_state_out_of_case,        ui_input_factory_reset_request                },

#if defined(QCC3020_FF_ENTRY_LEVEL_AA) || (defined HAVE_RDP_UI)
    /* Note: this is a device specific logical input. */
    {LI_MFB_BUTTON_RELEASE_6SEC_DS,   ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_force_reset                          },
#endif
#ifdef HAVE_RDP_UI
    {LI_MFB_BUTTON_RELEASE_3SEC,      ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_pair_handset                      },
#else
    {LI_MFB_BUTTON_RELEASE_6SEC,      ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_pair_handset                      },
#endif // HAVE_RDP_UI
    {LI_MFB_BUTTON_RELEASE_8SEC,      ui_provider_phy_state,       context_phy_state_out_of_case,        ui_input_sm_delete_handsets                   },

#if (defined INCLUDE_GAA || defined INCLUDE_AMA)
#ifdef HAVE_RDP_UI
    {APP_VA_BUTTON_DOWN,              ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_1                                 },
    {APP_VA_BUTTON_HELD_1SEC,         ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_5                                 },
    {APP_VA_BUTTON_RELEASE,           ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_6                                 },
#else
    {APP_VA_BUTTON_DOWN,              ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_1                                 },
    {APP_VA_BUTTON_SINGLE_CLICK,      ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_3                                 },
    {APP_VA_BUTTON_DOUBLE_CLICK,      ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_4                                 },
    {APP_VA_BUTTON_HELD_1SEC,         ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_5                                 },
    {APP_VA_BUTTON_RELEASE,           ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_6                                 },
#endif

#endif

#if defined(HAVE_4_BUTTONS) || defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_LEAKTHROUGH_ENABLE,          ui_provider_audio_curation,  context_leakthrough_disabled,         ui_input_leakthrough_on                       },
    {APP_LEAKTHROUGH_DISABLE,         ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_off                      },
    {APP_LEAKTHROUGH_SET_NEXT_MODE,   ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_set_next_mode            },

    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_in_call,                ui_input_volume_down_start                    },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_down_start                    },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_connected,              ui_input_volume_down_start                    },

    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_in_call,                ui_input_volume_up_start                      },
    {APP_BUTTON_VOLUME_UP,            ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_up_start                      },
    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_connected,              ui_input_volume_up_start                      },

    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_telephony,       context_voice_in_call,                ui_input_volume_stop                          },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_stop                          },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,  ui_provider_telephony,       context_voice_connected,              ui_input_volume_stop                          },

    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_telephony,       context_voice_in_call,                ui_input_volume_stop                          },
    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_stop                          },
    {APP_BUTTON_VOLUME_UP_RELEASE,    ui_provider_telephony,       context_voice_connected,              ui_input_volume_stop                          },
#endif /* HAVE_4_BUTTONS || HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */

#if defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_PLAY_PAUSE_TOGGLE,    ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause                    },
    {APP_BUTTON_PLAY_PAUSE_TOGGLE,    ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause                    },
    {APP_BUTTON_FORWARD,              ui_provider_media_player,    context_media_player_streaming,       ui_input_av_forward                           },
    {APP_BUTTON_FORWARD_HELD,         ui_provider_media_player,    context_media_player_streaming,       ui_input_av_fast_forward_start                },
    {APP_BUTTON_FORWARD_HELD_RELEASE, ui_provider_media_player,    context_media_player_streaming,       ui_input_fast_forward_stop                    },
    {APP_BUTTON_BACKWARD,             ui_provider_media_player,    context_media_player_streaming,       ui_input_av_backward                          },
    {APP_BUTTON_BACKWARD_HELD,        ui_provider_media_player,    context_media_player_streaming,       ui_input_av_rewind_start                      },
    {APP_BUTTON_BACKWARD_HELD_RELEASE,ui_provider_media_player,    context_media_player_streaming,       ui_input_rewind_stop                          },
#endif /* HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */
#if defined(HAVE_5_BUTTONS)
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_in_call,                ui_input_volume_down_start                    },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_down_start                    },
    {APP_BUTTON_VOLUME_DOWN,          ui_provider_telephony,       context_voice_connected,              ui_input_volume_down_start                    },

    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_in_call,                ui_input_volume_up_start                      },
    {APP_BUTTON_VOLUME_UP,            ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_up_start                      },
    {APP_BUTTON_VOLUME_UP,            ui_provider_telephony,       context_voice_connected,              ui_input_volume_up_start                      },
#endif /* HAVE_5_BUTTONS */

#ifdef INCLUDE_CAPSENSE
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_telephony,       context_voice_in_call,                ui_input_volume_down                          },
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_down                          },
    {CAP_SENSE_SLIDE_DOWN,            ui_provider_telephony,       context_voice_connected,              ui_input_volume_down                          },

    {CAP_SENSE_SLIDE_UP,              ui_provider_telephony,       context_voice_in_call,                ui_input_volume_up                            },
    {CAP_SENSE_SLIDE_UP,              ui_provider_media_player,    context_media_player_streaming,       ui_input_volume_up                            },
    {CAP_SENSE_SLIDE_UP,              ui_provider_telephony,       context_voice_connected,              ui_input_volume_up                            },
    {CAP_SENSE_TRIPLE_PRESS,          ui_provider_device,          context_handset_connected,            ui_input_gaming_mode_toggle                   },

    {CAP_SENSE_TAP_SLIDE_UP,          ui_provider_media_player,    context_media_player_streaming,       ui_input_av_forward                           },
    {CAP_SENSE_TAP_SLIDE_DOWN,        ui_provider_media_player,    context_media_player_streaming,       ui_input_av_backward                          },

    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_toggle_way                       },
    {CAP_SENSE_DOUBLE_PRESS,          ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_toggle_way                       },
#endif /* INCLUDE_CAPSENSE */
};


const ui_config_table_content_t* EarbudUi_GetConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(earbud_ui_config_table);
    return earbud_ui_config_table;
}

#ifdef INCLUDE_CAPSENSE
const touch_event_config_t* EarbudUi_GetCapsenseEventTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(touch_event_table);
    return touch_event_table;
}
#endif

void EarbudUi_ConfigureFocusSelection(void)
{
    FocusSelect_ConfigureAudioSourceTieBreakOrder(audio_source_focus_tie_break_order);
    FocusSelect_ConfigureVoiceSourceTieBreakOrder(voice_source_focus_tie_break_order);
}
