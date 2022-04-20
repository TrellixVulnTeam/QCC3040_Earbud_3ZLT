/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_configure.c

DESCRIPTION
    Functions required to configure ANC Sinks/Sources.
*/

#include "anc_configure.h"
#include "anc.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_configure_coefficients.h"
#include <audio_anc.h>
#include <operators.h>
#include <source.h>
#include <sink.h>
#include <audio_config.h>
#include <gain_utils.h>
#include <audio_processor.h>

#define ANC_ENABLE_MASK            0x08
#define IGNORE_SAMPLE_RATE        0x00

#define ACCMD_ANC_CONTROL_DMIC_X2_A_SEL_MASK           0x0004
#define ACCMD_ANC_CONTROL_DMIC_X2_B_SEL_MASK           0x0008
#define ACCMD_ANC_CONTROL_ACCESS_SELECT_ENABLES_SHIFT  16


static void enableAudioFramework(void)
{
    OperatorsFrameworkEnable();
    AudioProcessorAddUseCase(audio_ucid_not_defined);
}

static void disableAudioFramework(void)
{
    OperatorsFrameworkDisable();
}

static uint32 getRawGain(anc_path_enable mic_path)
{
    uint32 gain = (ancConfigDataGetHardwareGainForMicPath(mic_path));
    return ((gainUtilsCalculateRawAdcGainAnalogueComponent(gain) << 16) | RAW_GAIN_DIGITAL_COMPONENT_0_GAIN);
}

static void configureMicGain(anc_path_enable mic_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(mic_path);

    if(mic_params->is_digital == FALSE)
    {
        Source mic_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
        ANC_ASSERT(SourceConfigure(mic_source, STREAM_CODEC_RAW_INPUT_GAIN, getRawGain(mic_path)));
    }
}

static void configureMicGains(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();

    if(mic_params->enabled_mics & feed_forward_left)
    {
        configureMicGain(feed_forward_left);
    }

    if(mic_params->enabled_mics & feed_forward_right)
    {
        configureMicGain(feed_forward_right);
    }

    if(mic_params->enabled_mics & feed_back_left)
    {
        configureMicGain(feed_back_left);
    }

    if(mic_params->enabled_mics & feed_back_right)
    {
        configureMicGain(feed_back_right);
    }
}

static void associateInstance(audio_anc_instance instance, Source ffa_source, Source ffb_source)
{
    if(ffa_source)
    {
        ANC_ASSERT(SourceConfigure(ffa_source, STREAM_ANC_INSTANCE, instance));
    }
    if(ffb_source)
    {
        ANC_ASSERT(SourceConfigure(ffb_source, STREAM_ANC_INSTANCE, instance));
    }
}


static void associateInputPaths(Source ffa_source, Source ffb_source)
{

    if(ffa_source)
    {
       ANC_ASSERT(SourceConfigure(ffa_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA));
    }
    
    if(ffb_source)
    {
       ANC_ASSERT(SourceConfigure(ffb_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFB));
    }
}

static void associateInputPathsForParallelAnc(Source ffa_source, Source ffb_source)
{
    if(ffa_source)
    {
        ANC_ASSERT(SourceConfigure(ffa_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA|INSTANCE_0_MASK));
        ANC_ASSERT(SourceConfigure(ffa_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA|INSTANCE_1_MASK));
    }

    if(ffb_source)
    {
        ANC_ASSERT(SourceConfigure(ffb_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFB|INSTANCE_1_MASK));
        ANC_ASSERT(SourceConfigure(ffb_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFB|INSTANCE_0_MASK));
    }
}

static void configureUpConverterA(audio_anc_instance instance, Source mic_source)
{
    anc_instance_config_t * config = getInstanceConfig(instance);
    if(mic_source)
    {
        uint32 ffa_dmic_x2_mask = (ACCMD_ANC_CONTROL_DMIC_X2_A_SEL_MASK << ACCMD_ANC_CONTROL_ACCESS_SELECT_ENABLES_SHIFT);

        if(config->feed_forward_a.upconvertor_config.dmic_x2_ff == 1)
        {
             ffa_dmic_x2_mask |= ACCMD_ANC_CONTROL_DMIC_X2_A_SEL_MASK;
        }
         ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_CONTROL, ffa_dmic_x2_mask));
    }
}

static void configureUpConverterB(audio_anc_instance instance, Source mic_source)
{
    anc_instance_config_t * config = getInstanceConfig(instance);
    if(mic_source)
    {
        uint32 ffb_dmic_x2_mask = (ACCMD_ANC_CONTROL_DMIC_X2_B_SEL_MASK << ACCMD_ANC_CONTROL_ACCESS_SELECT_ENABLES_SHIFT);

       if(config->feed_forward_b.upconvertor_config.dmic_x2_ff == 1)
       {
           ffb_dmic_x2_mask |= ACCMD_ANC_CONTROL_DMIC_X2_B_SEL_MASK;
       }
        ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_CONTROL, ffb_dmic_x2_mask));
    }
}

static void configureControl(audio_anc_instance instance, Source ffa_source, Source ffb_source)
{
    configureUpConverterA(instance,ffa_source);
    configureUpConverterB(instance,ffb_source);
}


static void configureParallelAncControl(anc_path_enable ffa_path, anc_path_enable ffb_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(ffa_path);
    Source ffa_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    Source ffb_source = NULL;

    if(ffb_path)
    {
       mic_params = ancConfigDataGetMicForMicPath(ffb_path);
       ffb_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    }

    configureUpConverterA(AUDIO_ANC_INSTANCE_0,ffb_source);
    configureUpConverterA(AUDIO_ANC_INSTANCE_1,ffa_source);


    configureUpConverterB(AUDIO_ANC_INSTANCE_0,ffb_source);
    configureUpConverterB(AUDIO_ANC_INSTANCE_1,ffa_source);
}

static void configureParallelAncControlforFFaPath(audio_anc_instance instance, anc_path_enable ffa_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(ffa_path);
    Source ffa_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);

    configureUpConverterA(instance, ffa_source);
}

static void configureParallelAncInputPaths(anc_path_enable ffa_path, anc_path_enable ffb_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(ffa_path);
    Source ffa_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    Source ffb_source = NULL;

    if(ffb_path)
    {
       mic_params = ancConfigDataGetMicForMicPath(ffb_path);
       ffb_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    }
    associateInputPathsForParallelAnc(ffa_source,ffb_source);
}

static void configureParallelAncFFaPaths(anc_path_enable ffa_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(ffa_path);
    Source ffa_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);

    if(ffa_source)
    {
        ANC_ASSERT(SourceConfigure(ffa_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_FFA|INSTANCE_01_MASK));
    }
}

static void configureParallelAncInstance(audio_anc_instance instance,anc_path_enable ffx_path)
{
    if(ffx_path)
    {
        audio_mic_params *mic_params = ancConfigDataGetMicForMicPath(ffx_path);
        Source ffx_source = AudioPluginGetMicSource(*mic_params,mic_params->channel);
        associateInstance(instance,ffx_source,NULL);
    }
}

static void configureAncInstance(audio_anc_instance instance, anc_path_enable ffa_path, anc_path_enable ffb_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(ffa_path);
    Source ffa_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    Source ffb_source = NULL;

    if(ffb_path)
    {
       mic_params = ancConfigDataGetMicForMicPath(ffb_path);
       ffb_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    }

    associateInstance(instance, ffa_source, ffb_source);
    associateInputPaths(ffa_source, ffb_source);
    configureControl(instance, ffa_source, ffb_source);
}

static void configureFeedForwardModeMics(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();

    if(mic_params->enabled_mics & feed_forward_left)
    {
        configureAncInstance(AUDIO_ANC_INSTANCE_0, feed_forward_left, all_disabled);
    }

    if(mic_params->enabled_mics & feed_forward_right)
    {
        configureAncInstance(AUDIO_ANC_INSTANCE_1, feed_forward_right, all_disabled);
    }
}

static void configureFeedBackModeMics(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();

    if(mic_params->enabled_mics & feed_back_left)
    {
        configureAncInstance(AUDIO_ANC_INSTANCE_0, feed_back_left, all_disabled);
    }

    if(mic_params->enabled_mics & feed_back_right)
    {
        configureAncInstance(AUDIO_ANC_INSTANCE_1, feed_back_right, all_disabled);
    }
}

static void configureHybridModeMics(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();
    if(mic_params->enabled_mics & feed_forward_left)
    {
        configureAncInstance(AUDIO_ANC_INSTANCE_0, feed_back_left, feed_forward_left);
    }
    if(mic_params->enabled_mics & feed_forward_right)
    {
        configureAncInstance(AUDIO_ANC_INSTANCE_1, feed_back_right, feed_forward_right);
    }
}

/******************************************************************************/
static void configureMics(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();

    switch(mic_params->enabled_mics)
    {
        case hybrid_mode:
        case hybrid_mode_left_only:
        case hybrid_mode_right_only:
            configureHybridModeMics();
            break;

        case feed_back_mode:
        case feed_back_mode_left_only:
        case feed_back_mode_right_only:
            configureFeedBackModeMics();
            break;

        case feed_forward_mode:
        case feed_forward_mode_left_only:
        case feed_forward_mode_right_only:
            configureFeedForwardModeMics();
            break;

        default:
            ANC_PANIC();
            break;
    }

    configureMicGains();
}

static void deconfigureMicSource(anc_path_enable mic_path)
{
    audio_mic_params * mic_params = ancConfigDataGetMicForMicPath(mic_path);    
    Source mic_source = AudioPluginGetMicSource(*mic_params, mic_params->channel);
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_INPUT, AUDIO_ANC_PATH_ID_NONE));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_INSTANCE, AUDIO_ANC_INSTANCE_NONE));
}

/******************************************************************************/
static void deconfigureMics(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();

    if(mic_params->enabled_mics & feed_forward_left)
    {
        deconfigureMicSource(feed_forward_left);
    }

    if(mic_params->enabled_mics & feed_forward_right)
    {
        deconfigureMicSource(feed_forward_right);
    }

    if(mic_params->enabled_mics & feed_back_left)
    {
        deconfigureMicSource(feed_back_left);
    }

    if(mic_params->enabled_mics & feed_back_right)
    {
        deconfigureMicSource(feed_back_right);
    }
}

static void configureDacChannel(audio_channel channel, audio_anc_instance instance)
{
    Sink dac_channel = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, channel);
    ANC_ASSERT(SinkConfigure(dac_channel, STREAM_ANC_INSTANCE, instance));
}

/******************************************************************************/
static void configureDacs(void)
{
    if(ancDataIsLeftChannelConfigurable())
    {
        configureDacChannel(AUDIO_CHANNEL_A, AUDIO_ANC_INSTANCE_0);
    }
    if(ancDataIsRightChannelConfigurable())
    {
        configureDacChannel(AUDIO_CHANNEL_B, AUDIO_ANC_INSTANCE_1);
    }
}

/******************************************************************************/
static void deconfigureDacs(void)
{
    if(ancDataIsLeftChannelConfigurable())
    {
        configureDacChannel(AUDIO_CHANNEL_A, AUDIO_ANC_INSTANCE_NONE);
    }
    if(ancDataIsRightChannelConfigurable())
    {
        configureDacChannel(AUDIO_CHANNEL_B, AUDIO_ANC_INSTANCE_NONE);
    }
}

/******************************************************************************
 * Enable/Disable ANC stream for selected ANC paths(FFa, FFb or FB).
 * In case, stream is already enabled and ANC mode change happens then it masks
 * enabled path's bit and re-enable the required ANC path according to the new ANC mode.
*/
static void ancStreamEnable(bool enable)
{
    uint16 enable_instance_0 = ((enable && ancDataIsLeftChannelConfigurable()) ? ancDataGetCurrentModeConfig()->instance[ANC_INSTANCE_0_INDEX].enable_mask : 0);
    uint16 enable_instance_1 = ((enable && ancDataIsRightChannelConfigurable()) ? ancDataGetCurrentModeConfig()->instance[ANC_INSTANCE_1_INDEX].enable_mask : 0);

    ANC_ASSERT(AudioAncStreamEnable(enable_instance_0, enable_instance_1));
}

static void enableAnc(void)
{
    enableAudioFramework();
    configureMics();
    configureDacs();
    ancConfigureFilterCoefficients();
    ancConfigureFilterPathGains();
    ancOverWriteWithUserPathGains();
#ifdef ANC_UPGRADE_FILTER
    setRxMixGains(anc_single_filter_topology);
    setRxMixEnables(anc_single_filter_topology);
#endif
    ancStreamEnable(TRUE);
    ancDataResetUserGainConfig();
}

static void disableAnc(void)
{
    ancConfigureMutePathGains();
    ancStreamEnable(FALSE);
    deconfigureMics();
    deconfigureDacs();
    disableAudioFramework();
}
/******************************************************************************/
static void reassociateFFaMicWithAncInstance(audio_anc_instance primary_anc_instance, anc_path_enable ffa_path)
{
    /* Clear instance mappings for FFa mic EP*/
    configureParallelAncInstance(AUDIO_ANC_INSTANCE_NONE, ffa_path);

    /* Associate primary ANC instance to FFa mic EP*/
    configureParallelAncInstance(primary_anc_instance, ffa_path);

    /* Associate FFa mic to FFa paths*/
    configureParallelAncFFaPaths(ffa_path);
}
/******************************************************************************/
static void configureParallelAncFFaMic(anc_path_enable ffa_path)
{
    reassociateFFaMicWithAncInstance(AUDIO_ANC_INSTANCE_1, ffa_path);
    /* Configure Control messages for ANC Instance 1*/
    configureParallelAncControlforFFaPath(AUDIO_ANC_INSTANCE_1, ffa_path);

    /* Sets FFa mic primary instance as Instance0; Associates FFa mic to FFa paths*/
    reassociateFFaMicWithAncInstance(AUDIO_ANC_INSTANCE_0, ffa_path);
    /* Configure Control messages for ANC Instance 0*/
    configureParallelAncControlforFFaPath(AUDIO_ANC_INSTANCE_0, ffa_path);
}
/******************************************************************************/

/*Configuring feedforward_left as a primary path for AUDIO_ANC_INSTANCE_0
  Configuring feedback_left as a primary path for AUDIO_ANC_INSTANCE_1
*/

static void configureParallelAncHybridModeMics(void)
{

    /*Primary Paths*/
    configureParallelAncInstance(AUDIO_ANC_INSTANCE_0,feed_forward_left);
    configureParallelAncInstance(AUDIO_ANC_INSTANCE_1,feed_back_left);

    configureParallelAncControl(feed_back_left,feed_forward_left);

    configureParallelAncInputPaths(feed_back_left,feed_forward_left);

}
/******************************************************************************/
static void configureParallelAncFeedBackModeMics(void)
{
    configureParallelAncFFaMic(feed_back_left);
}
/******************************************************************************/
static void configureParallelAncFeedForwardModeMics(void)
{
    configureParallelAncFFaMic(feed_forward_left);
}
/******************************************************************************/
static void configureMicsForParallelAnc(void)
{
    anc_mic_params_t* mic_params = ancDataGetMicParams();

    switch(mic_params->enabled_mics)
    {

        case feed_back_mode_left_only:
            configureParallelAncFeedBackModeMics();
            break;

        case feed_forward_mode_left_only:
            configureParallelAncFeedForwardModeMics();
            break;

        case hybrid_mode_left_only:
            configureParallelAncHybridModeMics();
        break;

        default:
            ANC_PANIC();
            break;
    }
    configureMicGains();
}

/******************************************************************************/
static void deconfigureMicsForParallelAnc(void)
{
    deconfigureMics();
}

/******************************************************************************/
static void configureDacForParallelAnc(void)
{
    configureDacChannel(AUDIO_CHANNEL_A, AUDIO_ANC_INSTANCE_0);
#ifdef ANC_UPGRADE_FILTER
    configureDacChannel(AUDIO_CHANNEL_A, AUDIO_ANC_INSTANCE_1);
#else
    configureDacChannel(AUDIO_CHANNEL_B, AUDIO_ANC_INSTANCE_1);
#endif
}

/******************************************************************************/
static void deconfigureDacForParallelAnc(void)
{
    configureDacChannel(AUDIO_CHANNEL_A,AUDIO_ANC_INSTANCE_NONE);
    configureDacChannel(AUDIO_CHANNEL_B,AUDIO_ANC_INSTANCE_NONE);
}

/******************************************************************************/
static void configureOutMixForParallelAnc(void)
{
    /* Sets primary instance of FFA mic to Instance 1 incase of FF/FB mode*/
    switch(ancDataGetMicParams()->enabled_mics)
    {
        case feed_back_mode_left_only:
            reassociateFFaMicWithAncInstance(AUDIO_ANC_INSTANCE_0, feed_back_left);
            break;

        case feed_forward_mode_left_only:
            reassociateFFaMicWithAncInstance(AUDIO_ANC_INSTANCE_0, feed_forward_left);
            break;

        default:
            break;
    }

    ancEnableOutMix();
}

/******************************************************************************/
#ifdef ANC_UPGRADE_FILTER
static void configureAnc1ToUseAnc0PcmInput(void)
{
    /* Sets primary instance of FFA mic to Instance0 incase of FF/FB mode*/
    switch(ancDataGetMicParams()->enabled_mics)
    {
        case feed_back_mode_left_only:
            reassociateFFaMicWithAncInstance(AUDIO_ANC_INSTANCE_1, feed_back_left);
            break;

        case feed_forward_mode_left_only:
            reassociateFFaMicWithAncInstance(AUDIO_ANC_INSTANCE_1, feed_forward_left);
            break;

        default:
            break;
    }

    ancEnableAnc1UsesAnc0RxPcmInput();
}
#endif

/******************************************************************************/
static void ancParallelStreamEnable(bool enable)
{
    uint16 enable_instance_0 = ((enable) ? ancDataGetCurrentModeConfig()->instance[ANC_INSTANCE_0_INDEX].enable_mask : 0);
    uint16 enable_instance_1 = ((enable) ? ancDataGetCurrentModeConfig()->instance[ANC_INSTANCE_1_INDEX].enable_mask : 0);

    ANC_ASSERT(AudioAncStreamEnable(enable_instance_0, enable_instance_1));
}

/******************************************************************************/
bool ancEnableWithPathGainsMuted(void)
{
    enableAudioFramework();
    configureMics();
    configureDacs();
    ancConfigureMutePathGains();
#ifdef ANC_UPGRADE_FILTER
    setRxMixEnables(anc_single_filter_topology);
#endif
    ancConfigureFilterCoefficients();
    ancStreamEnable(TRUE);
    return TRUE;
}

bool ancConfigure(bool enable)
{
    enable ? enableAnc() : disableAnc();
    return TRUE;
}

bool ancConfigureAfterModeChange(void)
{
    ancConfigureMutePathGains();
    configureMicGains();
    ancConfigureFilterCoefficients();
    ancStreamEnable(TRUE);
    ancConfigureFilterPathGains();
#ifdef ANC_UPGRADE_FILTER
    setRxMixGains(anc_single_filter_topology);
    setRxMixEnables(anc_single_filter_topology);
#endif

    return TRUE;
}

bool ancConfigureFilterCoefficientsAfterModeChange(void)
{
    ancConfigureFilterCoefficients();
    ancStreamEnable(TRUE);
    return TRUE;
}

bool ancConfigureFilterCoefficientsPathGainsAfterModeChange(bool enable_coarse_gains, bool enable_fine_gains)
{
    ancConfigureFilterCoefficients();
    ancStreamEnable(TRUE);
    if(enable_coarse_gains)
        ancConfigureFilterPathCoarseGains();
    if(enable_fine_gains)
        ancConfigureFilterPathFineGains();
#ifdef ANC_UPGRADE_FILTER
    setRxMixGains(anc_single_filter_topology);
    setRxMixEnables(anc_single_filter_topology);
#endif

    return TRUE;
}

bool ancConfigureFilterPathGain(audio_anc_instance instance, audio_anc_path_id path, uint8 gain)
{
    if(path == AUDIO_ANC_PATH_ID_FFA)
    {
        return(ancConfigureGainForFFApath(instance, gain));
    }
    else if(path == AUDIO_ANC_PATH_ID_FFB)
    {
        return(ancConfigureGainForFFBpath(instance, gain));
    }
    else if(path == AUDIO_ANC_PATH_ID_FB)
    {
        return(ancConfigureGainForFBpath(instance, gain));
    }
    return FALSE;
}

bool ancConfigureParallelFilterPathGain(audio_anc_path_id path, uint8 instance_0_gain,uint8 instance_1_gain)
{
    if(path == AUDIO_ANC_PATH_ID_FFA)
    {
        return(ancConfigureParallelGainForFFApath(instance_0_gain,instance_1_gain));
    }
    else if(path == AUDIO_ANC_PATH_ID_FFB)
    {
        return(ancConfigureParallelGainForFFBpath(instance_0_gain,instance_1_gain));
    }
    else if(path == AUDIO_ANC_PATH_ID_FB)
    {
        return(ancConfigureParallelGainForFBpath(instance_0_gain,instance_1_gain));
    }
    return FALSE;
}

bool ancConfigureEnableParallelAnc(void)
{
    enableAudioFramework();
    configureMicsForParallelAnc();
    configureDacForParallelAnc();
    ancConfigureParallelFilterCoefficients();
    ancConfigureParallelFilterPathGains(TRUE, TRUE);
    configureOutMixForParallelAnc();
#ifdef ANC_UPGRADE_FILTER
    setRxMixGains(anc_parallel_filter_topology);
    setRxMixEnables(anc_parallel_filter_topology);
    configureAnc1ToUseAnc0PcmInput();
#endif
    ancParallelStreamEnable(TRUE);
    return TRUE;
}

bool ancConfigureEnableParallelAncPathGainsMuted(void)
{
    enableAudioFramework();
    configureMicsForParallelAnc();
    configureDacForParallelAnc();
    ancConfigureParallelFilterMutePathGains();
    ancConfigureParallelFilterCoefficients();
    configureOutMixForParallelAnc();
#ifdef ANC_UPGRADE_FILTER
    setRxMixEnables(anc_parallel_filter_topology);
    configureAnc1ToUseAnc0PcmInput();
#endif
    ancParallelStreamEnable(TRUE);
    return TRUE;
}

bool ancConfigureDisableParallelAnc(void)
{
    ancConfigureParallelFilterMutePathGains();
    ancParallelStreamEnable(FALSE);
    deconfigureMicsForParallelAnc();
    deconfigureDacForParallelAnc();
    disableAudioFramework();
    return TRUE;
}

bool ancConfigureParallelFilterAfterModeChange(void)
{
    ancConfigureParallelFilterMutePathGains();
    ancConfigureParallelFilterCoefficients();
    ancConfigureParallelFilterPathGains(TRUE, TRUE);
#ifdef ANC_UPGRADE_FILTER
    setRxMixGains(anc_parallel_filter_topology);
    setRxMixEnables(anc_parallel_filter_topology);
#endif
    return TRUE;
}

bool ancConfigureParallelFilterCoefAfterModeChange(void)
{
    ancConfigureParallelFilterCoefficients();
    return TRUE;
}

bool ancConfigureParallelFilterCoefPathGainsAfterModeChange(bool enable_coarse_gains, bool enable_fine_gains)
{
    ancConfigureParallelFilterCoefficients();
    ancConfigureParallelFilterPathGains(enable_coarse_gains, enable_fine_gains);
#ifdef ANC_UPGRADE_FILTER
    setRxMixGains(anc_parallel_filter_topology);
    setRxMixEnables(anc_parallel_filter_topology);
#endif

    return TRUE;
}
