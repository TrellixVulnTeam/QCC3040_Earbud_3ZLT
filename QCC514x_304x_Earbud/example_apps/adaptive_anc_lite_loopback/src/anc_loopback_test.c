/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_loopback_test.c
\brief      anc loopback test file- responsible for running aanc_lite operator,
            once the operator is configured and started it ramps feedforward fine gain
            from 0 to 255 once in the in the ramp time specified. The ramp time can be configured
            using QACT.Updating the ucid restarts the ramp.It does not monitor microphone data.
*/
#include <anc.h>
#include <file.h>
#include <vmal.h>
#include <operator.h>
#include <operators.h>
#include <cap_id_prim.h>
#include <sink.h>
#include <stream.h>
#include <panic.h>
#include "anc_loopback_test.h"
#include "microphones_config.h"
#include "logging.h"


#define FILE_NONE 0 /*!< No such file. */
#define FILE_ROOT 1 /*!< Root directory. */
#define RIGHT_CHANNEL_ENABLED FALSE
#define AANC_SAMPLE_RATE 16000
#define EXTERNAL_MIC_SINK_TERMINAL 0
#define INTERNAL_MIC_SINK_TERMINAL 1

typedef enum
{
    adaptive_anc_lite_mode_standby    = 0,
    adaptive_anc_lite_mode_full      = 1,
} adaptive_anc_lite_mode_t;


typedef struct
{
    BundleID aanc_lite_bundle_id;
    anc_mic_params_t anc_mic_params;
    Operator op_aanc_lite;
    bool anc_status;
    bool anc_adaptivity_status;
} aanc_lite_data_t;

aanc_lite_data_t aanc_lite_data;

#define getAANCLiteData()                  (&aanc_lite_data)
#define getAANCLiteOperator()              (getAANCLiteData()->op_aanc_lite)
#define getAANCLiteMicParams()             (&getAANCLiteData()->anc_mic_params)
#define getAANCFeedForwardMic()            (getAANCLiteMicParams()->feed_forward_left)
#define getAANCFeedBackMic()               (getAANCLiteMicParams()->feed_back_left)
#define getAANCFeedForwardMicChannel()     (appConfigMic1AudioChannel())
#define getAANCFeedBackMicChannel()        (appConfigMic0AudioChannel())

#define ANC_MICROPHONE_RATE                (16000U)

static void ancloopback_ConfigureFeedforwardMic(void)
{
    audio_mic_params* feed_forward_mic;
    feed_forward_mic = &getAANCFeedForwardMic();
    feed_forward_mic->bias_config = appConfigMic1Bias();
    feed_forward_mic->pio = appConfigMic1Pio();
    feed_forward_mic->gain = appConfigMic1Gain();
    feed_forward_mic->is_digital=appConfigMic1IsDigital();
    feed_forward_mic->instance = appConfigMic1AudioInstance();
    feed_forward_mic->channel = appConfigMic1AudioChannel();
}


static void ancloopback_ConfigureFeedbackMic(void)
{
    audio_mic_params* feed_back_mic;
    feed_back_mic = &getAANCFeedBackMic();
    feed_back_mic->bias_config = appConfigMic0Bias();
    feed_back_mic->pio = appConfigMic0Pio();
    feed_back_mic->gain = appConfigMic0Gain();
    feed_back_mic->is_digital=appConfigMic0IsDigital();
    feed_back_mic->instance = appConfigMic0AudioInstance();
    feed_back_mic->channel = appConfigMic0AudioChannel();
}

static anc_mode_t ancloopback_GetDefaultMode(void)
{
    return anc_mode_1;
}

static anc_path_enable ancloopback_GetDefaultMicPath(void)
{
    return hybrid_mode_left_only;
}

static bool Ancloopback_GetAncStatus(void)
{
    return getAANCLiteData()->anc_status;
}

static void ancloopback_UpdateAdaptivityStatus(bool status)
{
    getAANCLiteData()->anc_adaptivity_status = status;
}

static bool ancloopback_GetAdaptivityStatus(void)
{
    return getAANCLiteData()->anc_adaptivity_status;
}

static audio_anc_path_id ancloopback_GetAncPath(void)
{
    audio_anc_path_id audio_anc_path = AUDIO_ANC_PATH_ID_NONE;
    anc_path_enable anc_path = ancloopback_GetDefaultMicPath();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_left_only: /* fallthrough */
        case feed_back_mode:
        case feed_back_mode_left_only:
            audio_anc_path = AUDIO_ANC_PATH_ID_FFA;
            break;

        case hybrid_mode:
        case hybrid_mode_left_only:
            audio_anc_path = AUDIO_ANC_PATH_ID_FFB;
            break;

        default:
            break;
    }

    return audio_anc_path;
}


static void ancloopback_SetStaticGain(audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel)
{
    uint16 ff_coarse_static_gain;
    uint16 fb_coarse_static_gain;
    uint16 ec_coarse_static_gain;
    uint8 ff_fine_static_gain;
    uint8 fb_fine_static_gain;
    uint8 ec_fine_static_gain;

    audio_anc_instance inst = hw_channel ? AUDIO_ANC_INSTANCE_1 : AUDIO_ANC_INSTANCE_0;

    /*If hybrid is configured, feedforward path is AUDIO_ANC_PATH_ID_FFB and feedback path will be AUDIO_ANC_PATH_ID_FFA*/
    audio_anc_path_id feedback_anc_path = (feedforward_anc_path==AUDIO_ANC_PATH_ID_FFB)?(AUDIO_ANC_PATH_ID_FFA):(AUDIO_ANC_PATH_ID_FFB);/*TBD for Feed forward ANC mode*/

     /*Update gains*/
    AncReadCoarseGainFromInstance(inst, feedforward_anc_path, &ff_coarse_static_gain);
    AncReadFineGainFromInstance(inst, feedforward_anc_path, &ff_fine_static_gain);

    AncReadCoarseGainFromInstance(inst, feedback_anc_path, &fb_coarse_static_gain);
    AncReadFineGainFromInstance(inst, feedback_anc_path, &fb_fine_static_gain);

    AncReadCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FB, &ec_coarse_static_gain);
    AncReadFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FB, &ec_fine_static_gain);

    OperatorsAdaptiveAncSetStaticGain(getAANCLiteOperator(), ff_coarse_static_gain, ff_fine_static_gain, fb_coarse_static_gain, fb_fine_static_gain, ec_coarse_static_gain, ec_fine_static_gain);
}


static void ancloopback_configureAANC(void)
{

    ancloopback_SetStaticGain(ancloopback_GetAncPath(),adaptive_anc_hw_channel_0);
    OperatorsAdaptiveAncSetHwChannelCtrl(getAANCLiteOperator(),adaptive_anc_hw_channel_0 );
    OperatorsAdaptiveAncSetInEarCtrl(getAANCLiteOperator(),TRUE);
    OperatorsStandardSetUCID(getAANCLiteOperator(),0);
    OperatorsAdaptiveAncSetModeOverrideCtrl(getAANCLiteOperator(), adaptive_anc_lite_mode_full);
}

unsigned Ancloopback_GetMicrophoneBiasVoltage(mic_bias_id id)
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

    DEBUG_LOG("Ancloopback_GetMicrophoneBiasVoltage, id %u, bias %u", id, bias);
    return bias;
}

void Ancloopback_Setup(void)
{
    anc_mode_t anc_mode;
    AudioPluginCommonRegisterMicBiasVoltageCallback(Ancloopback_GetMicrophoneBiasVoltage);
    ancloopback_ConfigureFeedforwardMic();
    ancloopback_ConfigureFeedbackMic();
    getAANCLiteData()->anc_mic_params.enabled_mics = ancloopback_GetDefaultMicPath();
    anc_mode = ancloopback_GetDefaultMode();
    AncInit(&getAANCLiteData()->anc_mic_params,anc_mode);

}


static void ancloopback_EnableANC(void)
{
    if(Ancloopback_GetAncStatus())
    {
        DEBUG_LOG_ALWAYS("ANC is already enabled");
    }
    else
    {
        OperatorsFrameworkEnable();
        AudioPluginMicSetup(getAANCFeedForwardMicChannel(),getAANCFeedForwardMic(),ANC_MICROPHONE_RATE);
        AudioPluginMicSetup(getAANCFeedBackMicChannel(),getAANCFeedBackMic(),ANC_MICROPHONE_RATE);
        AncEnable(TRUE);
        getAANCLiteData()->anc_status= TRUE;
    }
}

static void ancloopback_DisableANC(void)
{
    if(!Ancloopback_GetAncStatus())
    {
        DEBUG_LOG_ALWAYS("ANC is not enabled");
    }
    else
    {
        AncEnable(FALSE);
        getAANCLiteData()->anc_status=FALSE;
        AudioPluginMicShutdown(getAANCFeedForwardMicChannel(),&getAANCFeedForwardMic(),TRUE);
        AudioPluginMicShutdown(getAANCFeedBackMicChannel(),&getAANCFeedBackMic(),TRUE);
        OperatorsFrameworkDisable();
    }
}


void Ancloopback_EnableAdaptivity(void)
{
    const char aanc_lite_dkcs[] = "download_aanc_lite.dkcs";

    ancloopback_EnableANC();

    if(!ancloopback_GetAdaptivityStatus())
    {
        if(ancloopback_GetDefaultMicPath()!=hybrid_mode_left_only)
        {
            DEBUG_LOG_ALWAYS("AANC LITE capability only works for hybrid_mode_left_only anc path");
            Panic();
        }

        OperatorFrameworkEnable(1);
        FILE_INDEX index = FileFind(FILE_ROOT, aanc_lite_dkcs, strlen(aanc_lite_dkcs));
        PanicFalse(index != FILE_NONE);
        getAANCLiteData()->aanc_lite_bundle_id =PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */
        getAANCLiteData()->op_aanc_lite = (Operator)PanicFalse(VmalOperatorCreate(CAP_ID_DOWNLOAD_AANC_LITE_16K));
            /* CONFIGURE AANC HERE*/
        ancloopback_configureAANC();

        Source mic1 = AudioPluginGetMicSource(getAANCFeedForwardMic(),getAANCFeedForwardMicChannel());
        Source mic2 = AudioPluginGetMicSource(getAANCFeedBackMic(),getAANCFeedBackMicChannel());

        PanicFalse(StreamConnect((mic1),StreamSinkFromOperatorTerminal(getAANCLiteOperator(),EXTERNAL_MIC_SINK_TERMINAL)));
        PanicFalse(StreamConnect((mic2),StreamSinkFromOperatorTerminal(getAANCLiteOperator(),INTERNAL_MIC_SINK_TERMINAL)));

        OperatorStart(getAANCLiteOperator());

        ancloopback_UpdateAdaptivityStatus(TRUE);
    }
}

void Ancloopback_DisableAdaptivity(void)
{
    if(ancloopback_GetAdaptivityStatus())
    {
        OperatorStop(getAANCLiteOperator());
        StreamDisconnect(0,StreamSinkFromOperatorTerminal(getAANCLiteOperator(),EXTERNAL_MIC_SINK_TERMINAL));
        StreamDisconnect(0,StreamSinkFromOperatorTerminal(getAANCLiteOperator(),INTERNAL_MIC_SINK_TERMINAL));
        OperatorDestroy(getAANCLiteOperator());
        ancloopback_DisableANC();
        ancloopback_UpdateAdaptivityStatus(FALSE);
        OperatorFrameworkEnable(0);
    }
}

void Ancloopback_RestartAdaptivity(void)
{
    if(ancloopback_GetAdaptivityStatus())
    {
        OperatorsStandardSetUCID(getAANCLiteOperator(),0);
    }
    else
    {
        DEBUG_LOG_ALWAYS("Ancloopback application is not running AANC Adaptivity");
    }
}
