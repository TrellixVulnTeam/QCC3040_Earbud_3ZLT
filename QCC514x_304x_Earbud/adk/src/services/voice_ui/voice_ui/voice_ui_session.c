/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice Assistant Session state
*/

#include "voice_ui_session.h"
#include "voice_ui.h"
#include "voice_ui_container.h"
#include "voice_ui_va_client_if.h"
#include "av.h"
#include "logging.h"
#include "audio_router.h"
#include <panic.h>

static bool voiceUi_PopulateSourceContext(audio_source_t source, audio_source_provider_context_t *context);
const av_context_provider_if_t provider_if =
{
    .PopulateSourceContext = voiceUi_PopulateSourceContext
};

static bool va_session_in_progress = FALSE;

static const bdaddr * voiceUi_GetVaSourceBdAddress(void)
{
    const voice_ui_handle_t * va = VoiceUi_GetActiveVa();

    if (va)
    {
        return va->voice_assistant->GetBtAddress();
    }

    return NULL;
}

static const bdaddr * voiceUi_GetA2dpSourceBdAddress(audio_source_t source)
{
    const avInstanceTaskData *av = Av_GetInstanceForHandsetSource(source);

    if (av)
    {
        return &(av->bd_addr);
    }

    return NULL;
}

static bool voiceUi_IsValidBtAddress(const bdaddr *addr)
{
    return addr && !BdaddrIsZero(addr);
}

static bool voiceUi_IsSameBtAdress(const bdaddr *addr_1, const bdaddr *addr_2)
{
    if (voiceUi_IsValidBtAddress(addr_1) && voiceUi_IsValidBtAddress(addr_2))
    {
        return BdaddrIsSame(addr_1, addr_2);
    }

    return FALSE;
}

static audio_source_t voiceUi_GetVaAudioSource(void)
{
    const bdaddr *va_addr = voiceUi_GetVaSourceBdAddress();
    audio_source_t source;
    for_all_a2dp_audio_sources(source)
    {
        if (voiceUi_IsSameBtAdress(va_addr, voiceUi_GetA2dpSourceBdAddress(source)))
        {
            return source;
        }
    }

    return audio_source_none;
}

static bool voiceUi_PopulateSourceContext(audio_source_t source, audio_source_provider_context_t *context)
{
    bool status = FALSE;

    if ((source == voiceUi_GetVaAudioSource()) && va_session_in_progress)
    {
        *context = context_audio_is_va_response;
        status = TRUE;
    }

    if (status)
    {
        DEBUG_LOG_DEBUG("voiceUi_PopulateSourceContext: enum:audio_source_t:%d set context as enum:audio_source_provider_context_t:%d", source, *context);
    }

    return status;
}

void VoiceUi_VaSessionStarted(voice_ui_handle_t* va_handle)
{
    DEBUG_LOG("VoiceUi_VaSessionStarted");
    if (VoiceUi_IsActiveAssistant(va_handle) && (va_session_in_progress != TRUE))
    {
        va_session_in_progress = TRUE;
        AudioRouter_Update();
    }
}

void VoiceUi_VaSessionEnded(voice_ui_handle_t* va_handle)
{
    DEBUG_LOG("VoiceUi_VaSessionEnded");
    if (VoiceUi_IsActiveAssistant(va_handle))
    {
        VoiceUi_VaSessionReset();
    }
}

bool VoiceUi_IsSessionInProgress(void)
{
    return va_session_in_progress;
}

void VoiceUi_VaSessionReset(void)
{
    if (va_session_in_progress != FALSE)
    {
        va_session_in_progress = FALSE;
        AudioRouter_Update();
    }
}

void VoiceUi_VaSessionInit(void)
{
    PanicFalse(Av_RegisterContextProvider(&provider_if));
}
