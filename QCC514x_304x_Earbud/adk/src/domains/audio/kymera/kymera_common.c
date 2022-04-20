/*!
\copyright  Copyright (c) 2017-2021  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Kymera common code
*/

#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_ucid.h"
#include "kymera_anc.h"
#include "kymera_aec.h"
#include "kymera_config.h"
#include "kymera_va.h"
#include "kymera_latency_manager.h"
#include "kymera_tones_prompts.h"
#include "av.h"
#include "microphones.h"
#include "microphones_config.h"
#include "pio_common.h"
#include "kymera_music_processing.h"
#include "kymera_output_if.h"
#include "fit_test.h"
#include <opmsg_prim.h>
#include <audio_clock.h>
#include <audio_power.h>
#include <pio_common.h>
#include <vmal.h>
#include <anc_state_manager.h>
#include <audio_output.h>

#define MHZ_TO_HZ (1000000)

static uint8 audio_ss_client_count = 0;

static audio_dsp_clock_type appKymeraGetNbWbScoDspClockType(void)
{
#if defined(ENABLE_ADAPTIVE_ANC)
    return AUDIO_DSP_TURBO_CLOCK;
#else
    return AUDIO_DSP_BASE_CLOCK;
#endif
}

void Kymera_ConnectIfValid(Source source, Sink sink)
{
    if (source && sink)
    {
        PanicNull(StreamConnect(source, sink));
    }
}

void Kymera_DisconnectIfValid(Source source, Sink sink)
{
    if (source || sink)
    {
        StreamDisconnect(source, sink);
    }
}

bool appKymeraSetActiveDspClock(audio_dsp_clock_type type)
{
    audio_dsp_clock_configuration cconfig =
    {
        .active_mode = type,
        .low_power_mode =  AUDIO_DSP_CLOCK_NO_CHANGE,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };
    return AudioDspClockConfigure(&cconfig);
}

void appKymeraConfigureDspClockSpeed(void)
{
    if (Kymera_IsVaActive())
    {
        PanicFalse(AudioMapCpuSpeed(AUDIO_DSP_SLOW_CLOCK, Kymera_VaGetMinLpClockSpeedMhz() * MHZ_TO_HZ));
    }
    else
    {
        PanicFalse(AudioMapCpuSpeed(AUDIO_DSP_SLOW_CLOCK, DEFAULT_LOW_POWER_CLK_SPEED_MHZ * MHZ_TO_HZ));
    }
}

void appKymeraConfigureDspPowerMode(void)
{
#if ! defined(__CSRA68100_APP__)
    kymeraTaskData *theKymera = KymeraGetTaskData();
    bool tone_playing = appKymeraIsPlayingPrompt();

    DEBUG_LOG("appKymeraConfigureDspPowerMode, tone %u, state %u, a2dp seid %u", tone_playing, appKymeraGetState(), theKymera->a2dp_seid);
    
    /* Assume we are switching to the low power slow clock unless one of the
     * special cases below applies */
    audio_dsp_clock_configuration cconfig =
    {
        .active_mode = AUDIO_DSP_SLOW_CLOCK,
        .low_power_mode =  AUDIO_DSP_SLOW_CLOCK,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };
    
    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_A2DP_STARTING_A:
        case KYMERA_STATE_A2DP_STARTING_B:
        case KYMERA_STATE_A2DP_STARTING_C:
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
        case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
        {
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(Kymera_IsVaActive())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(tone_playing)
            {
                mode = AUDIO_POWER_SAVE_MODE_1;
                switch(theKymera->a2dp_seid)
                {
                    case AV_SEID_APTX_SNK:
                    case AV_SEID_APTXHD_SNK:
                        cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                        break;
                    default:
                        /* For most codecs there is not enough MIPs when running on a slow clock to also play a tone */
                        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                        break;
                }
            }
            else if(appKymeraInConcurrency())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else
            {
                /* Either setting up for the first time or returning from a tone, in
                * either case return to the default clock rate for the codec in use */
                switch(theKymera->a2dp_seid)
                {
                    case AV_SEID_APTX_SNK:
                    case AV_SEID_APTXHD_SNK:
                    case AV_SEID_APTX_ADAPTIVE_SNK:
                    case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
                    {
                        /* Not enough MIPs to run aptX master (TWS standard) or
                        * aptX adaptive (TWS standard and TWS+) on slow clock */
                        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    break;

                    case AV_SEID_SBC_SNK:
                    {
                        if (Kymera_OutputIsAecAlwaysUsed() || appConfigSbcNoPcmLatencyBuffer())
                        {
                            cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                            mode = AUDIO_POWER_SAVE_MODE_1;
                        }
                    }
                    break;

                    case AV_SEID_APTX_MONO_TWS_SNK:
                    {
                        if (Kymera_OutputIsAecAlwaysUsed())
                        {
                            cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                            mode = AUDIO_POWER_SAVE_MODE_1;
                        }
                    }
                    break;

                    default:
                    break;
                }
            }
            if (Kymera_BoostClockInGamingMode() && Kymera_LatencyManagerIsGamingModeEnabled())
            {
                cconfig.active_mode += 1;
                cconfig.active_mode = MIN(cconfig.active_mode, AUDIO_DSP_TURBO_CLOCK);
            }
        }
        break;

        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        {            
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                DEBUG_LOG("appKymeraConfigureDspPowerMode:Dsp Clock Boost Required");
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if (theKymera->sco_info)
            {
                DEBUG_LOG("appKymeraConfigureDspPowerMode, sco_info %u, mode %u", theKymera->sco_info, theKymera->sco_info->mode);
                switch (theKymera->sco_info->mode)
                {
                    case SCO_NB:
                    case SCO_WB:
                    {
                        /* Always jump up to normal clock (80Mhz) for NB or WB CVC in standard build */
                        cconfig.active_mode = appKymeraGetNbWbScoDspClockType();
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    break;

                    case SCO_SWB:
                    case SCO_UWB:
                    {
                        /* Always jump up to turbo clock (120Mhz) for SWB or UWB CVC */
                        cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    break;

                    default:
                        break;
                }
            }
        }
        break;

        case KYMERA_STATE_ANC_TUNING:
        {
            /* Always jump up to turbo clock (120Mhz) for ANC tuning */
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        break;

        case KYMERA_STATE_MIC_LOOPBACK:
        case KYMERA_STATE_TONE_PLAYING:
        {
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(Kymera_IsVaActive())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if ((appKymeraInConcurrency()) || (FitTest_IsRunning()))
            {
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
			else if(theKymera->output_rate == SAMPLING_RATE_96000)
            {
                DEBUG_LOG("appKymeraConfigureDspPowerMode:Dsp Clock Boost Required as output rate is 96000");
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
        }
        break;
        
        case KYMERA_STATE_LE_AUDIO_ACTIVE:
        {
            /* Audio team testing of LE-Audio graphs has been done at 120MHz */
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        break;

        case KYMERA_STATE_LE_VOICE_ACTIVE:
        {
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        break;

        /* All other states default to slow */
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
        case KYMERA_STATE_IDLE:
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(Kymera_IsVaActive())
            {
                cconfig.active_mode = Kymera_VaGetMinDspClock();
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(FitTest_IsRunning())
            {
                /* Kymera could be in Idle state when the prompt for fit test is going
                   loop iteration this will prevent dsp clock to jump to 80Mhz-32Mhz-80Mhz.*/
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            break;

        case KYMERA_STATE_ADAPTIVE_ANC_STARTED:
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(appKymeraInConcurrency()) /* VA-AANC concurrency active */
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(FitTest_IsRunning())
            {
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            break;

        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        case KYMERA_STATE_USB_SCO_VOICE_ACTIVE:
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
            break;
    }

#ifdef AUDIO_IN_SQIF
    /* Make clock faster when running from SQIF */
    cconfig.active_mode += 1;
#endif


    PanicFalse(AudioDspClockConfigure(&cconfig));
    PanicFalse(AudioPowerSaveModeSet(mode));

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOG("appKymeraConfigureDspPowerMode, kymera clocks %d %d %d, mode %d", kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode, mode);
#else
    /* No DSP clock control on CSRA68100 */
#endif
}

void appKymeraExternalAmpSetup(void)
{
    if (appConfigExternalAmpControlRequired())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        int pio_mask = PioCommonPioMask(appConfigExternalAmpControlPio());
        int pio_bank = PioCommonPioBank(appConfigExternalAmpControlPio());

        /* Reset usage count */
        theKymera->dac_amp_usage = 0;

        /* map in PIO */
        PioSetMapPins32Bank(pio_bank, pio_mask, pio_mask);
        /* set as output */
        PioSetDir32Bank(pio_bank, pio_mask, pio_mask);
        /* start disabled */
        PioSet32Bank(pio_bank, pio_mask,
                     appConfigExternalAmpControlDisableMask());
    }
}

void appKymeraExternalAmpControl(bool enable)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (appConfigExternalAmpControlRequired())
    {
        theKymera->dac_amp_usage += enable ? 1 : - 1;

        /* Drive PIO high if enabling AMP and usage has gone from 0 to 1,
         * Drive PIO low if disabling AMP and usage has gone from 1 to 0 */
        if ((enable && theKymera->dac_amp_usage == 1) ||
            (!enable && theKymera->dac_amp_usage == 0))
        {
            int pio_mask = PioCommonPioMask(appConfigExternalAmpControlPio());
            int pio_bank = PioCommonPioBank(appConfigExternalAmpControlPio());

            PioSet32Bank(pio_bank, pio_mask,
                         enable ? appConfigExternalAmpControlEnableMask() :
                                  appConfigExternalAmpControlDisableMask());
        }
    }

    if(enable)
    {
        /* If we're enabling the amp then also call OperatorFrameworkEnable() so that the audio S/S will
           remain on even if the audio chain is destroyed, this allows us to control the timing of when the audio S/S
           and DACs are powered off to mitigate audio pops and clicks.*/

        /* Cancel any pending audio s/s disable message since we're enabling.  If message was cancelled no need
           to call OperatorFrameworkEnable() as audio S/S is still powered on from previous time */
        if(MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE))
        {
            DEBUG_LOG("appKymeraExternalAmpControl, there is already a client for the audio SS");
        }
        else
        {
            DEBUG_LOG("appKymeraExternalAmpControl, adding a client to the audio SS");
            OperatorsFrameworkEnable();
        }

        audio_ss_client_count++;
    }
    else
    {
        if (audio_ss_client_count > 1)
        {
            OperatorsFrameworkDisable();
            audio_ss_client_count--;
            DEBUG_LOG("appKymeraExternalAmpControl, removed audio source, count is %d", audio_ss_client_count);
        }
        else
        {
            /* If we're disabling the amp then send a timed message that will turn off the audio s/s later rather than 
            immediately */
            DEBUG_LOG("appKymeraExternalAmpControl, sending later KYMERA_INTERNAL_AUDIO_SS_DISABLE, count is %d", audio_ss_client_count);
            MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE, NULL, appKymeraDacDisconnectionDelayMs());
            audio_ss_client_count = 0;
        }
    }
}

Source Kymera_GetMicrophoneSource(microphone_number_t microphone_number, Source source_to_synchronise_with, uint32 sample_rate, microphone_user_type_t microphone_user_type)
{
    Source mic_source = NULL;
    if(microphone_number != microphone_none)
    {
        mic_source = Microphones_TurnOnMicrophone(microphone_number, sample_rate, microphone_user_type);
    }
    if(mic_source && source_to_synchronise_with)
    {
        SourceSynchronise(source_to_synchronise_with, mic_source);
    }
    return mic_source;
}

void Kymera_CloseMicrophone(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type)
{
    if(microphone_number != microphone_none)
    {
        Microphones_TurnOffMicrophone(microphone_number, microphone_user_type);
    }
}

unsigned Kymera_GetMicrophoneBiasVoltage(mic_bias_id id)
{
    unsigned bias = 0;
    if (id == MIC_BIAS_0)
    {
        if (appConfigMic0Bias() == BIAS_CONFIG_MIC_BIAS_0)
            bias =  appConfigMic0BiasVoltage();
        else if (appConfigMic1Bias() == BIAS_CONFIG_MIC_BIAS_0)
            bias = appConfigMic1BiasVoltage();
        else
            Panic();
    }
    else if (id == MIC_BIAS_1)
    {
        if (appConfigMic0Bias() == BIAS_CONFIG_MIC_BIAS_1)
            bias = appConfigMic0BiasVoltage();
        else if (appConfigMic1Bias() == BIAS_CONFIG_MIC_BIAS_1)
            bias = appConfigMic1BiasVoltage();
        else
            Panic();
    }
    else
        Panic();

    DEBUG_LOG("Kymera_GetMicrophoneBiasVoltage, id %u, bias %u", id, bias);
    return bias;
}

uint8 Kymera_GetNumberOfMics(void)
{
    uint8 nr_used_microphones = 1;

#if defined(KYMERA_SCO_USE_2MIC) && defined(KYMERA_SCO_USE_3MIC)
    #error Defining KYMERA_SCO_USE_2MIC and defining KYMERA_SCO_USE_3MIC is not allowed
#endif

#if defined(KYMERA_SCO_USE_3MIC)
    nr_used_microphones = 3;
#elif defined (KYMERA_SCO_USE_2MIC)
    nr_used_microphones = 2;
#endif
    return(nr_used_microphones);
}

void Kymera_ConnectOutputSource(Source left, Source right, uint32 output_sample_rate)
{
    audio_output_params_t output_params;
	
    memset(&output_params, 0, sizeof(audio_output_params_t));
    output_params.sample_rate = output_sample_rate;
    output_params.transform = audio_output_tansform_connect;

    AudioOutputAddSource(left, audio_output_primary_left);

    /*In earbud application, second DAC path needs to be activated to support Parallel ANC topology*/
    if(appConfigOutputIsStereo() || appKymeraEnhancedAncRequiresSecondDAC())
    {
        AudioOutputAddSource(right, audio_output_primary_right);
    }
    /* Connect the sources to their appropriate hardware outputs. */
    AudioOutputConnect(&output_params);
	
    AudioOutputGainApplyConfiguredLevels(audio_output_group_main, 0, NULL);

}

