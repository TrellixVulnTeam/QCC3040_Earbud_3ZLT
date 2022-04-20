/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the Earbud Application user interface tone indications.
*/
#ifndef EARBUD_TONES_H
#define EARBUD_TONES_H

#include "domain_message.h"
#include "kymera.h"

extern const ringtone_note app_tone_button[];
extern const ringtone_note app_tone_button_2[];
extern const ringtone_note app_tone_button_3[];
extern const ringtone_note app_tone_button_4[];
#ifdef INCLUDE_CAPSENSE
extern const ringtone_note app_tone_double_tap[];
#endif

extern const ringtone_note app_tone_button_dfu[];
extern const ringtone_note app_tone_button_factory_reset[];

extern const ringtone_note app_tone_hfp_connect[];
extern const ringtone_note app_tone_hfp_connected[];
extern const ringtone_note app_tone_hfp_disconnected[];
extern const ringtone_note app_tone_hfp_link_loss[];
extern const ringtone_note app_tone_hfp_sco_connected[];
extern const ringtone_note app_tone_hfp_sco_disconnected[];
extern const ringtone_note app_tone_hfp_mute_reminder[];
extern const ringtone_note app_tone_hfp_sco_unencrypted_reminder[];
extern const ringtone_note app_tone_hfp_ring[];
extern const ringtone_note app_tone_hfp_ring_caller_id[];
extern const ringtone_note app_tone_hfp_voice_dial[];
extern const ringtone_note app_tone_hfp_voice_dial_disable[];
extern const ringtone_note app_tone_hfp_answer[];
extern const ringtone_note app_tone_hfp_hangup[];
extern const ringtone_note app_tone_hfp_mute_active[];
extern const ringtone_note app_tone_hfp_mute_inactive[];
extern const ringtone_note app_tone_hfp_talk_long_press[];
extern const ringtone_note app_tone_pairing[];
extern const ringtone_note app_tone_paired[];
extern const ringtone_note app_tone_pairing_deleted[];
extern const ringtone_note app_tone_volume[];
extern const ringtone_note app_tone_volume_limit[];
extern const ringtone_note app_tone_error[];
extern const ringtone_note app_tone_battery_empty[];
extern const ringtone_note app_tone_power_on[];
extern const ringtone_note app_tone_power_off[];
extern const ringtone_note app_tone_paging_reminder[];
extern const ringtone_note app_tone_peer_pairing[];
extern const ringtone_note app_tone_peer_pairing_error[];
extern const ringtone_note app_tone_factory_reset[];

#if defined INCLUDE_GAA && defined INCLUDE_WUW
extern const ringtone_note app_tone_doff[];
#endif

#ifdef INCLUDE_DFU
extern const ringtone_note app_tone_dfu[];
#endif

#ifdef PRODUCTION_TEST_MODE
extern const ringtone_note dut_mode_tone[];
#endif

#ifdef INCLUDE_AV
extern const ringtone_note app_tone_av_connect[];
extern const ringtone_note app_tone_av_disconnect[];
extern const ringtone_note app_tone_av_remote_control[];
extern const ringtone_note app_tone_av_connected[];
extern const ringtone_note app_tone_av_disconnected[];
extern const ringtone_note app_tone_av_link_loss[];
extern const ringtone_note app_tone_gaming_mode_on[];
extern const ringtone_note app_tone_gaming_mode_off[];
extern const ringtone_note app_tone_a2dp_not_routed[];
#endif

#ifdef INCLUDE_TEST_TONES
extern const ringtone_note app_tone_test_continuous[];
extern const ringtone_note app_tone_test_indexed[];
#endif

#ifdef INCLUDE_AMA
#ifdef HAVE_RDP_UI
extern const ringtone_note app_tone_ama_unregistered[];
extern const ringtone_note app_tone_ama_not_connected[];
#endif  /* HAVE_RDP_UI */
#ifdef INCLUDE_WUW
extern const ringtone_note app_tone_ama_wuw_enabled[];
extern const ringtone_note app_tone_ama_wuw_disabled[];
#endif  /* INCLUDE_WUW */
#endif  /* INCLUDE_AMA */

//!@}

#endif // EARBUD_TONE_H
