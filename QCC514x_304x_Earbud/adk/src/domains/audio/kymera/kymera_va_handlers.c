/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module that implements basic build block functions to handle Voice Assistant related actions
*/

#include "kymera_va_handlers.h"
#include "kymera_va_common.h"
#include "kymera_va_encode_chain.h"
#include "kymera_va_mic_chain.h"
#include "kymera_va_wuw_chain.h"
#include "kymera_common.h"
#include "kymera_tones_prompts.h"

#include <vmal.h>

static struct
{
    unsigned engine_supports_default_low_power_clock:1;
    unsigned low_power_mode_enabled:1;
} state;

static void kymera_CreateMicChain(const va_audio_mic_config_t *mic_config, bool support_wuw, uint32 pre_roll_needed_in_ms)
{
    va_mic_chain_create_params_t params = {0};
    bool mic_chain_supported = FALSE;

    params.chain_params.clear_voice_capture = TRUE;
    params.chain_params.wake_up_word_detection = support_wuw;
    params.operators_params.max_pre_roll_in_ms = pre_roll_needed_in_ms;

#ifdef KYMERA_VA_USE_1MIC
    params.chain_params.number_of_mics = 1;
    mic_chain_supported = (mic_config->min_number_of_mics <= 1) && Kymera_IsVaMicChainSupported(&params.chain_params);
#else
    for(int i = MIN(mic_config->max_number_of_mics, Microphones_MaxSupported()); i >= mic_config->min_number_of_mics; i--)
    {
        params.chain_params.number_of_mics = i;
        if (Kymera_IsVaMicChainSupported(&params.chain_params))
        {
            mic_chain_supported = TRUE;
            break;
        }
    }
#endif

    PanicFalse(mic_chain_supported);
    bool using_multi_mic_cvc = (params.chain_params.number_of_mics > 1) && params.chain_params.clear_voice_capture;
    state.low_power_mode_enabled = !using_multi_mic_cvc;

    Kymera_CreateVaMicChain(&params);
}

static void kymera_CreateEncodeChain(const va_audio_encode_config_t *encoder_config)
{
    va_encode_chain_create_params_t chain_params = {0};
    chain_params.chain_params.encoder = encoder_config->encoder;
    chain_params.operators_params.encoder_params = &encoder_config->encoder_params;
    Kymera_CreateVaEncodeChain(&chain_params);
}

static void kymera_CreateVaWuwChain(Task detection_handler, const va_audio_wuw_config_t *wuw_config)
{
    va_wuw_chain_create_params_t wuw_params = {0};
    wuw_params.chain_params.wuw_engine = wuw_config->engine;
    wuw_params.operators_params.wuw_model = wuw_config->model;
    wuw_params.operators_params.wuw_detection_handler = detection_handler;
    wuw_params.operators_params.LoadWakeUpWordModel = wuw_config->LoadWakeUpWordModel;
    wuw_params.operators_params.engine_init_preroll_ms = wuw_config->engine_init_preroll_ms;
    Kymera_CreateVaWuwChain(&wuw_params);
}

static void kymera_WuwDetectionChainSleep(void)
{
    Kymera_VaWuwChainSleep();
    Kymera_VaMicChainSleep();
    Kymera_VaEncodeChainSleep();
}

static void kymera_WuwDetectionChainWake(void)
{
    Kymera_VaMicChainWake();
    Kymera_VaWuwChainWake();
    Kymera_VaEncodeChainWake();
}

void Kymera_CreateMicChainForLiveCapture(const void *params)
{
    const va_audio_voice_capture_params_t *capture = params;
    kymera_CreateMicChain(&capture->mic_config, FALSE, 0);
}

void Kymera_CreateMicChainForWuw(const void *params)
{
    const wuw_detection_start_t *wuw_detection = params;
#if defined (__QCC516X__) || defined (__QCC517X__)
    state.engine_supports_default_low_power_clock = FALSE;
#else
    state.engine_supports_default_low_power_clock = (wuw_detection->params->wuw_config.engine == va_wuw_engine_apva) ? FALSE : TRUE;
#endif
    kymera_CreateMicChain(&wuw_detection->params->mic_config, TRUE, wuw_detection->params->max_pre_roll_in_ms);
}

void Kymera_StartMicChain(const void *params)
{
    UNUSED(params);
    Kymera_StartVaMicChain();
}

void Kymera_StopMicChain(const void *params)
{
    UNUSED(params);
    Kymera_StopVaMicChain();
}

void Kymera_DestroyMicChain(const void *params)
{
    UNUSED(params);
    Kymera_DestroyVaMicChain();
}

void Kymera_ActivateMicChainEncodeOutputForLiveCapture(const void *params)
{
    UNUSED(params);
    Kymera_ActivateVaMicChainEncodeOutput();
}

void Kymera_ActivateMicChainEncodeOutputForWuwCapture(const void *params)
{
    const va_audio_wuw_capture_params_t *capture = params;
    Kymera_ActivateVaMicChainEncodeOutputAfterTimestamp(capture->start_timestamp);
}

void Kymera_DeactivateMicChainEncodeOutput(const void *params)
{
    UNUSED(params);
    Kymera_DeactivateVaMicChainEncodeOutput();
}

void Kymera_BufferMicChainEncodeOutput(const void *params)
{
    UNUSED(params);
    Kymera_BufferVaMicChainEncodeOutput();
}

void Kymera_ActivateMicChainWuwOutput(const void *params)
{
    UNUSED(params);
    Kymera_ActivateVaMicChainWuwOutput();
}

void Kymera_DeactivateMicChainWuwOutput(const void *params)
{
    UNUSED(params);
    Kymera_DeactivateVaMicChainWuwOutput();
}

void Kymera_CreateEncodeChainForLiveCapture(const void *params)
{
    const va_audio_voice_capture_params_t *capture = params;
    kymera_CreateEncodeChain(&capture->encode_config);
}

void Kymera_CreateEncodeChainForWuwCapture(const void *params)
{
    const va_audio_wuw_capture_params_t *capture = params;
    kymera_CreateEncodeChain(&capture->encode_config);
}

void Kymera_StartEncodeChain(const void *params)
{
    UNUSED(params);
    Kymera_StartVaEncodeChain();
}

void Kymera_StopEncodeChain(const void *params)
{
    UNUSED(params);
    Kymera_StopVaEncodeChain();
}

void Kymera_DestroyEncodeChain(const void *params)
{
    UNUSED(params);
    Kymera_DestroyVaEncodeChain();
}

void Kymera_CreateWuwChain(const void *params)
{
    const wuw_detection_start_t *wuw_detection = params;
    kymera_CreateVaWuwChain(wuw_detection->handler, &wuw_detection->params->wuw_config);
}

void Kymera_StartWuwChain(const void *params)
{
    UNUSED(params);
    Kymera_StartVaWuwChain();
}

void Kymera_StopWuwChain(const void *params)
{
    UNUSED(params);
    Kymera_StopVaWuwChain();
}

void Kymera_DestroyWuwChain(const void *params)
{
    UNUSED(params);
    Kymera_DestroyVaWuwChain();
}

void Kymera_ConnectWuwChainToMicChain(const void *params)
{
    UNUSED(params);
    Kymera_ConnectVaWuwChainToMicChain();
}

void Kymera_StartGraphManagerDelegation(const void *params)
{
    UNUSED(params);
    Kymera_VaWuwChainStartGraphManagerDelegation();

    if (state.low_power_mode_enabled)
    {
        kymera_WuwDetectionChainSleep();
    }
}

void Kymera_StopGraphManagerDelegation(const void *params)
{
    UNUSED(params);
    if (state.low_power_mode_enabled)
    {
        kymera_WuwDetectionChainWake();
    }
    Kymera_VaWuwChainStopGraphManagerDelegation();
}

void Kymera_EnterKeepDspOn(const void *params)
{
    UNUSED(params);
    VmalOperatorFrameworkEnableMainProcessor(TRUE);
}

void Kymera_ExitKeepDspOn(const void *params)
{
    UNUSED(params);
    VmalOperatorFrameworkEnableMainProcessor(FALSE);
}

void Kymera_UpdateDspClockSpeed(const void *params)
{
    UNUSED(params);
    appKymeraConfigureDspClockSpeed();
}

void Kymera_UpdateDspClock(const void *params)
{
    UNUSED(params);
    appKymeraConfigureDspPowerMode();
}

void Kymera_UpdateDspKickPeriod(const void *params)
{
    UNUSED(params);
    OperatorsFrameworkSetKickPeriod(KICK_PERIOD_VOICE);
}

void Kymera_BoostClockForChainCreation(const void *params)
{
    UNUSED(params);
    DEBUG_LOG("Kymera_BoostClockForChainCreation");
    PanicFalse(appKymeraSetActiveDspClock(AUDIO_DSP_TURBO_CLOCK));
}

void Kymera_SetWuwSampleRate(const void *params)
{
    const wuw_detection_start_t *wuw_detection = params;
    Kymera_SetVaSampleRate(wuw_detection->params->mic_config.sample_rate);
}

void Kymera_SetLiveCaptureSampleRate(const void *params)
{
    const va_audio_voice_capture_params_t *capture = params;
    Kymera_SetVaSampleRate(capture->mic_config.sample_rate);
}

void Kymera_LoadDownloadableCapsForPrompt(const void *params)
{
    UNUSED(params);
    Kymera_PromptLoadDownloadableCaps();
}

void Kymera_UnloadDownloadableCapsForPrompt(const void *params)
{
    UNUSED(params);
    Kymera_PromptUnloadDownloadableCaps();
}

bool Kymera_VaIsLowPowerEnabled(void)
{
    return state.low_power_mode_enabled;
}

bool Kymera_WuwEngineSupportsDefaultLpClock(void)
{
    return state.engine_supports_default_low_power_clock;
}
