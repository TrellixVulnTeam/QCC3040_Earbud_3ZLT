/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Earbud Prompts UI Indicator configuration table
*/
#include "earbud_prompts_config_table.h"

#include <domain_message.h>
#include <ui_indicator_prompts.h>

#include <av.h>
#include <pairing.h>
#include <handset_service_protected.h>
#include <power_manager.h>
#include <voice_ui.h>

/*! \ingroup configuration

   \brief System events to voice prompts configuration table

   This is an ordered table that associates system events with voice prompt files.
*/
#ifdef INCLUDE_PROMPTS
const ui_event_indicator_table_t earbud_ui_prompts_table[] =
{
    {.sys_event=PAIRING_ACTIVE,         { .prompt.filename = "pairing.sbc",
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }},
    {.sys_event=PAIRING_COMPLETE,       { .prompt.filename = "pairing_successful.sbc",
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }},
    {.sys_event=PAIRING_FAILED,         { .prompt.filename = "pairing_failed.sbc",
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }},
#ifndef EXCLUDE_POWER_PROMPTS
    {.sys_event=POWER_ON,                {.prompt.filename = "power_on.sbc",
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE,
                                          .prompt.local_feedback = TRUE }},
    {.sys_event=POWER_OFF,              { .prompt.filename = "power_off.sbc",
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE,
                                          .prompt.local_feedback = TRUE },
                                          .await_indication_completion = TRUE },
#endif
#ifndef EXCLUDE_CONN_PROMPTS
    {.sys_event=HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND,    { .prompt.filename = "connected.sbc",
                                                                  .prompt.rate = 48000,
                                                                  .prompt.format = PROMPT_FORMAT_SBC,
                                                                  .prompt.interruptible = FALSE,
                                                                  .prompt.queueable = TRUE,
                                                                  .prompt.requires_repeat_delay = FALSE }},
    {.sys_event=HANDSET_SERVICE_DISCONNECTED_IND, { .prompt.filename = "disconnected.sbc",
                                                    .prompt.rate = 48000,
                                                    .prompt.format = PROMPT_FORMAT_SBC,
                                                    .prompt.interruptible = FALSE,
                                                    .prompt.queueable = TRUE,
                                                    .prompt.requires_repeat_delay = FALSE }},
#endif
    {.sys_event=VOICE_UI_MIC_OPEN,      { .prompt.filename = "mic_open.sbc",
                                          .prompt.rate = 16000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = TRUE,
                                          .prompt.queueable = FALSE,
                                          .prompt.requires_repeat_delay = FALSE }},
    {.sys_event=VOICE_UI_MIC_CLOSE,     { .prompt.filename = "mic_close.sbc",
                                          .prompt.rate = 16000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = TRUE,
                                          .prompt.queueable = FALSE,
                                          .prompt.requires_repeat_delay = FALSE }},
    {.sys_event=VOICE_UI_DISCONNECTED,  { .prompt.filename = "bt_va_not_connected.sbc",
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT_SBC,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }}
};
#endif

uint8 EarbudPromptsConfigTable_GetSize(void)
{
#ifdef INCLUDE_PROMPTS
    return ARRAY_DIM(earbud_ui_prompts_table);
#else
    return 0;
#endif
}

