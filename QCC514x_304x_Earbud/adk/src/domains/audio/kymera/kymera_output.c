/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_output.c
\brief      Kymera module with audio output chain definitions

*/

#include "kymera_output_private.h"
#include "kymera_output.h"
#include "kymera_common.h"
#include "kymera_aec.h"
#include "kymera_volume.h"
#include "kymera_config.h"
#include "kymera_source_sync.h"
#include "kymera_output_common_chain_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <audio_output.h>
#include <cap_id_prim.h>
#include <vmal.h>
#include <operators.h>
#include <logging.h>

/*To identify if a splitter is needed after output chain to actiavte second DAC path, when ANC is enabled in earbud application */
#if defined(ENHANCED_ANC_USE_2ND_DAC_ENDPOINT) \
    && !defined(INCLUDE_KYMERA_AEC) \
    && !defined(ENABLE_ADAPTIVE_ANC)
#define INCLUDE_OUTPUT_SPLITTER
#endif

/*!@} */
#define VOLUME_CONTROL_SET_AUX_TTP_VERSION_MSB 0x2
#define DEFAULT_AEC_REF_TERMINAL_BUFFER_SIZE 15

#define SPLITTER_TERMINAL_IN_0       0
#define SPLITTER_TERMINAL_OUT_0      0
#define SPLITTER_TERMINAL_OUT_1      1

#define KICK_PERIOD_FAST_VOL_CNTRL_BUFFER 480U
#define KICK_PERIOD_SLOW_VOL_CNTRL_BUFFER 1440U
#define VOLUME_CONTROL_ALL_AUX_PATH 0xAAAA

/*! \brief The chain output channels */
typedef enum
{
    output_right_channel = 0,
    output_left_channel
}output_channel_t;

typedef struct
{
    Source source;
    Sink sink;
} input_t;

typedef struct
{
    Source input_1;
    Source input_2;
} connect_audio_output_t;

static struct
{
    struct
    {
        uint32 main;
        uint32 auxiliary;
    } input_rates;
    struct
    {
        Operator main_input;
        Operator aux_input;
    } resamplers;
    struct
    {
        Operator main_input;
#ifdef INCLUDE_OUTPUT_SPLITTER
        Operator output;
#endif
    } splitters;
    unsigned chain_include_aec:1;
    unsigned chain_started:1;
} state;

static bool kymeraOutput_IsStereoChain(kymera_chain_handle_t chain)
{
    return ChainGetInput(chain, EPR_SINK_STEREO_MIXER_L) && ChainGetInput(chain, EPR_SINK_STEREO_MIXER_R);
}

static Operator kymeraOutput_CreateSplitter(void)
{
    Operator op = CustomOperatorCreate(CAP_ID_SPLITTER, OPERATOR_PROCESSOR_ID_0, operator_priority_lowest, NULL);
    OperatorsSplitterSetWorkingMode(op, splitter_mode_clone_input);
    OperatorsSplitterSetDataFormat(op, operator_data_format_pcm);
    DEBUG_LOG("kymeraOutput_CreateSplitter: op_id=%d", op);
    return op;
}

static void kymeraOutput_StartSplitter(Operator splitter)
{
    if (splitter != INVALID_OPERATOR)
    {
        PanicFalse(OperatorStart(splitter));
        DEBUG_LOG("kymeraOutput_StartSplitter: op_id=%d", splitter);
    }
}

static void kymeraOutput_StopSplitter(Operator splitter)
{
    if (splitter != INVALID_OPERATOR)
    {
        PanicFalse(OperatorStop(splitter));
        DEBUG_LOG("kymeraOutput_StopSplitter: op_id=%d", splitter);
    }
}

static void kymeraOutput_DestroySplitter(Operator *splitter)
{
    if (*splitter != INVALID_OPERATOR)
    {
        if (state.chain_started)
        {
            PanicFalse(OperatorStop(*splitter));
        }
        CustomOperatorDestroy(splitter, 1);
        DEBUG_LOG("kymeraOutput_DestroySplitter: op_id=%d", *splitter);
        *splitter = INVALID_OPERATOR;
    }
}

static void kymeraOutput_ConnectChainToAudioSink(connect_audio_output_t *params)
{
    if (state.chain_include_aec)
    {
        aec_connect_audio_output_t aec_connect_params = {0};
        aec_audio_config_t config = {0};
        aec_connect_params.input_1 = params->input_1;
        aec_connect_params.input_2 = params->input_2;

        config.spk_sample_rate = KymeraGetTaskData()->output_rate;
        config.ttp_delay = AEC_REF_DEFAULT_MIC_TTP_LATENCY;

#ifdef ENABLE_AEC_LEAKTHROUGH
        config.is_source_clock_same = TRUE; //Same clock source for speaker and mic path for AEC-leakthrough.
                                            //Should have no implication on normal aec operation.
        config.buffer_size = DEFAULT_AEC_REF_TERMINAL_BUFFER_SIZE;
#endif

        Kymera_ConnectAudioOutputToAec(&aec_connect_params, &config);
    }
    else
    {
#if defined(INCLUDE_OUTPUT_SPLITTER)
        state.splitters.output = kymeraOutput_CreateSplitter();
        PanicFalse(StreamConnect(params->input_1, StreamSinkFromOperatorTerminal(state.splitters.output, SPLITTER_TERMINAL_IN_0)));
        params->input_1 = StreamSourceFromOperatorTerminal(state.splitters.output, SPLITTER_TERMINAL_OUT_0);
        params->input_2 = StreamSourceFromOperatorTerminal(state.splitters.output, SPLITTER_TERMINAL_OUT_1);
#endif
        Kymera_ConnectOutputSource(params->input_1, params->input_2, KymeraGetTaskData()->output_rate);
    }
}

static void kymeraOutput_DisconnectChainFromAudioSink(const connect_audio_output_t *params)
{
    if (state.chain_include_aec)
    {
        Kymera_DisconnectAudioOutputFromAec();
    }
    else
    {
#ifdef INCLUDE_OUTPUT_SPLITTER
        kymeraOutput_DestroySplitter(&state.splitters.output);
#endif
        Kymera_DisconnectIfValid(params->input_1, NULL);
        Kymera_DisconnectIfValid(params->input_2, NULL);
        AudioOutputDisconnect();
    }
}

static chain_endpoint_role_t kymeraOutput_GetOuputRole(output_channel_t is_left)
{
    chain_endpoint_role_t output_role;

    if(appConfigOutputIsStereo())
    {
        output_role = is_left ? EPR_SOURCE_STEREO_OUTPUT_L : EPR_SOURCE_STEREO_OUTPUT_R;
    }
    else
    {
        output_role = EPR_SOURCE_MIXER_OUT;
    }

    return output_role;
}

static void kymeraConfigureVolumeControlAuxBuffer(kymera_chain_handle_t chain, const kymera_output_chain_config *config)
{
    Operator vol_control;
    vol_control=ChainGetOperatorByRole(chain,OPR_VOLUME_CONTROL);

    switch(config->kick_period)
    {

        /*
            Source sink operator configures the auxilary path terminal buffer size as 4 * Kick period * Sampling rate.
            Keeping volume control auxilaty path terminal buffer size of same may result in distortion of prompt
            Hence it should be atleast 5 * Kick period * Sampling rate (KICK_PERIOD_FAST_VOL_CNTRL_BUFFER = 5*2ms*48Khz = 480).
            Keeping the terminal buffer size as 4 * kick period * Sampling rate with slow kick period as no distoritons were observed with 7.5ms.
            (KICK_PERIOD_SLOW_VOL_CNTRL_BUFFER= 4 * 7.5ms * 48Khz = 1440)
            Important note -> setting up terminal buffer size of volume control operator is possible only from ADK 20.3.1 releases.
        */

        case KICK_PERIOD_FAST:
             OperatorsStandardSetTerminalBufferSize(vol_control,KICK_PERIOD_FAST_VOL_CNTRL_BUFFER, VOLUME_CONTROL_ALL_AUX_PATH, 0);
        break;

        case KICK_PERIOD_SLOW:
        default:
            OperatorsStandardSetTerminalBufferSize(vol_control,KICK_PERIOD_SLOW_VOL_CNTRL_BUFFER, VOLUME_CONTROL_ALL_AUX_PATH, 0);
        break;
    }
}

static inline void kymeraOutput_SetVolume(kymera_chain_handle_t chain, int16 volume_in_db)
{
    Operator vol_op;

    if (GET_OP_FROM_CHAIN(vol_op, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainAndAuxGain(vol_op, Kymera_VolDbToGain(volume_in_db));
    }
}

static void kymeraOutput_ConfigureOperators(kymera_chain_handle_t chain, const kymera_output_chain_config *config)
{
    bool is_stereo = kymeraOutput_IsStereoChain(chain);
    Operator volume_op;
    bool input_buffer_set = FALSE;

    if (config->source_sync_input_buffer_size_samples)
    {
        /* Not all chains have a separate latency buffer operator but if present
           then set the buffer size. Source Sync version X.X allows its input
           buffer size to be set, so chains using that version of source sync
           typically do not have a seperate latency buffer and the source sync
           input buffer size is set instead in appKymeraConfigureSourceSync(). */
        Operator op;
        if (GET_OP_FROM_CHAIN(op, chain, OPR_LATENCY_BUFFER))
        {
            OperatorsStandardSetBufferSize(op, config->source_sync_input_buffer_size_samples);
            /* Mark buffer size as done */
            input_buffer_set = TRUE;
        }
    }

    appKymeraConfigureSourceSync(chain, config, !input_buffer_set, is_stereo);

    kymeraConfigureVolumeControlAuxBuffer(chain,config);
    volume_op = ChainGetOperatorByRole(chain, OPR_VOLUME_CONTROL);
    OperatorsStandardSetSampleRate(volume_op, config->rate);
    kymeraOutput_SetVolume(chain, VOLUME_MUTE_IN_DB);
    PanicFalse(Kymera_SetOperatorUcid(chain, OPR_VOLUME_CONTROL, UCID_VOLUME_CONTROL));
    PanicFalse(Kymera_SetOperatorUcid(chain, OPR_SOURCE_SYNC, UCID_SOURCE_SYNC));
#ifdef INCLUDE_KYMERA_COMPANDER
    PanicFalse(Kymera_SetOperatorUcid(chain, OPR_COMPANDER, UCID_COMPANDER_LIMITER));
#endif
    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureOutputChain)
    {
        kymera_output_config_params_t params = {0};
        params.sample_rate = config->rate;
        KymeraGetTaskData()->chain_config_callbacks->ConfigureOutputChain(chain, &params);
    }
}

static const chain_config_t * kymeraOutput_GetChainConfig(output_chain_t chain_type)
{
    const chain_config_t *config = NULL;

    switch(chain_type)
    {
        case output_chain_mono:
            config = Kymera_GetChainConfigs()->chain_output_volume_mono_config;
            break;
        case output_chain_stereo:
            config = Kymera_GetChainConfigs()->chain_output_volume_stereo_config;
            break;
        case output_chain_common:
            config = Kymera_GetChainConfigs()->chain_output_volume_common_config;
            break;
    }

    PanicFalse(config != NULL);
    return config;
}

/*! \brief Create only the audio output - e.g. the DACs */
static void KymeraOutput_ConnectToSpeakerPath(void)
{
    connect_audio_output_t connect_params = {0};
    connect_params.input_1 = ChainGetOutput(KymeraGetTaskData()->chain_output_handle, kymeraOutput_GetOuputRole(output_left_channel));

    if(appConfigOutputIsStereo())
    {
        connect_params.input_2 = ChainGetOutput(KymeraGetTaskData()->chain_output_handle, kymeraOutput_GetOuputRole(output_right_channel));
    }

    kymeraOutput_ConnectChainToAudioSink(&connect_params);
}

static Sink kymeraOutput_GetInput(unsigned input_role)
{
    return ChainGetInput(KymeraGetTaskData()->chain_output_handle, input_role);
}

static bool kymeraOutput_IsAuxTtpSupported(capablity_version_t cap_version)
{
    return cap_version.version_msb >= VOLUME_CONTROL_SET_AUX_TTP_VERSION_MSB ? TRUE : FALSE;
}

static void kymeraOutput_SetOverallSampleRate(uint32 rate)
{
    KymeraGetTaskData()->output_rate = rate;
    state.input_rates.auxiliary = rate;
    state.input_rates.main = rate;
}

static Operator kymeraOutput_CreateResampler(uint32 input_rate, uint32 output_rate)
{
    Operator op = CustomOperatorCreate(CAP_ID_IIR_RESAMPLER, OPERATOR_PROCESSOR_ID_0, operator_priority_lowest, NULL);
    OperatorsConfigureResampler(op, input_rate, output_rate);
    DEBUG_LOG("kymeraOutput_CreateResampler: op_id=%d, in_rate=%d, out_rate=%d", op, input_rate, output_rate);
    return op;
}

static void kymeraOutput_StartResampler(Operator resampler)
{
    if (resampler != INVALID_OPERATOR)
    {
        PanicFalse(OperatorStart(resampler));
        DEBUG_LOG("kymeraOutput_StartResampler: op_id=%d", resampler);
    }
}

static void kymeraOutput_DestroyResampler(Operator *resampler)
{
    if (*resampler != INVALID_OPERATOR)
    {
        if (state.chain_started)
        {
            PanicFalse(OperatorStop(*resampler));
        }
        CustomOperatorDestroy(resampler, 1);
        DEBUG_LOG("kymeraOutput_DestroyResampler: op_id=%d", *resampler);
        *resampler = INVALID_OPERATOR;
    }
}

static void kymeraOutput_StartInputResamplers(void)
{
    kymeraOutput_StartResampler(state.resamplers.main_input);
    kymeraOutput_StartResampler(state.resamplers.aux_input);
}

static void kymeraOutput_ConnectViaResampler(input_t *connections, unsigned connections_length, Operator *resampler, uint32 in_rate)
{
    PanicFalse(*resampler == INVALID_OPERATOR);
    if (in_rate != KymeraGetTaskData()->output_rate)
    {
        *resampler = kymeraOutput_CreateResampler(in_rate, KymeraGetTaskData()->output_rate);
        for(unsigned i = 0; i < connections_length; i++)
        {
            PanicNull(StreamConnect(connections[i].source, StreamSinkFromOperatorTerminal(*resampler, i)));
            connections[i].source = StreamSourceFromOperatorTerminal(*resampler, i);
        }
    }
    for(unsigned i = 0; i < connections_length; i++)
    {
        PanicNull(StreamConnect(connections[i].source, connections[i].sink));
    }
    if (state.chain_started)
    {
        kymeraOutput_StartResampler(*resampler);
    }
}

static void kymeraOutput_DisconnectViaResampler(chain_endpoint_role_t *roles, unsigned roles_length, Operator *resampler)
{
    kymeraOutput_DestroyResampler(resampler);
    for(unsigned i = 0; i < roles_length; i++)
    {
        StreamDisconnect(NULL, kymeraOutput_GetInput(roles[i]));
    }
}

bool KymeraOutput_MustAlwaysIncludeAec(void)
{
#if defined(INCLUDE_KYMERA_AEC) || defined(ENABLE_ADAPTIVE_ANC)
    return TRUE;
#else
    return FALSE;
#endif
}

void KymeraOutput_SetDefaultOutputChainConfig(kymera_output_chain_config *config,
                                              uint32 rate, unsigned kick_period,
                                              unsigned buffer_size)
{
    memset(config, 0, sizeof(*config));
    config->rate = rate;
    config->kick_period = kick_period;
    config->source_sync_input_buffer_size_samples = buffer_size;
    /* By default or for source sync version <=3.3 the output buffer needs to
       be able to hold at least SS_MAX_PERIOD worth  of audio (default = 2 *
       Kp), but be less than SS_MAX_LATENCY (5 * Kp). The recommendation is 2 Kp
       more than SS_MAX_PERIOD, so 4 * Kp. */
    appKymeraSetSourceSyncConfigOutputBufferSize(config, 4, 0);
    config->chain_type = appConfigOutputIsStereo() ? output_chain_stereo : output_chain_mono;
}

void KymeraOutput_CreateOperators(const kymera_output_chain_config *config)
{
    if (KymeraOutput_MustAlwaysIncludeAec())
    {
        state.chain_include_aec = TRUE;
    }
    else
    {
        state.chain_include_aec = config->chain_include_aec;
    }

    kymeraOutput_SetOverallSampleRate(config->rate);
    KymeraGetTaskData()->chain_output_handle = ChainCreate(kymeraOutput_GetChainConfig(config->chain_type));
    kymeraOutput_ConfigureOperators(KymeraGetTaskData()->chain_output_handle, config);
    PanicFalse(OperatorsFrameworkSetKickPeriod(config->kick_period));
    DEBUG_LOG_FN_ENTRY("KymeraOutput_CreateOperators: include_aec=%d, is_stereo=%d",
                       state.chain_include_aec,
                       kymeraOutput_IsStereoChain(KymeraGetTaskData()->chain_output_handle));
}

void KymeraOutput_ConnectChain(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraOutput_ConnectChain");
    ChainConnect(KymeraGetTaskData()->chain_output_handle);
    KymeraOutput_ConnectToSpeakerPath();
}

void KymeraOutput_DestroyChain(void)
{
    kymera_chain_handle_t chain = KymeraGetTaskData()->chain_output_handle;
    kymeraOutput_SetOverallSampleRate(0);
    DEBUG_LOG_FN_ENTRY("KymeraOutput_DestroyChain");
    PanicNull(chain);

#ifdef INCLUDE_MIRRORING
    /* Destroying the output chain powers-off the DSP, if another
       prompt or activity is pending, the DSP has to start all over again
       which takes a long time. Therefore prospectively power on the DSP
       before destroying the output chain, which will avoid an unneccesary
       power-off/on
     */
    appKymeraProspectiveDspPowerOn();
#endif

    connect_audio_output_t disconnect_params = {0};
    disconnect_params.input_1 = ChainGetOutput(chain, kymeraOutput_GetOuputRole(output_left_channel));
    disconnect_params.input_2 = ChainGetOutput(chain, kymeraOutput_GetOuputRole(output_right_channel));
    ChainStop(chain);
    kymeraOutput_DisconnectChainFromAudioSink(&disconnect_params);
    state.chain_started = FALSE;

    ChainDestroy(chain);
    KymeraGetTaskData()->chain_output_handle = NULL;
    state.chain_include_aec = FALSE;
}

void KymeraOutput_ChainStart(void)
{
    if (state.chain_started == FALSE)
    {
        DEBUG_LOG_FN_ENTRY("KymeraOutput_ChainStart");
#ifdef INCLUDE_OUTPUT_SPLITTER
        if (state.splitters.output != INVALID_OPERATOR)
            kymeraOutput_StartSplitter(state.splitters.output);
#endif
        ChainStart(KymeraGetTaskData()->chain_output_handle);
        kymeraOutput_StartInputResamplers();
        kymeraOutput_StartSplitter(state.splitters.main_input);
        state.chain_started = TRUE;
    }
}

kymera_chain_handle_t KymeraOutput_GetOutputHandle(void)
{
    return KymeraGetTaskData()->chain_output_handle;
}

void KymeraOutput_SetMainVolume(int16 volume_in_db)
{
    DEBUG_LOG_FN_ENTRY("KymeraOutput_SetMainVolume: db=%d", volume_in_db);
    Operator vol_op;

    if (GET_OP_FROM_CHAIN(vol_op, KymeraGetTaskData()->chain_output_handle, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainGain(vol_op, Kymera_VolDbToGain(volume_in_db));
    }
}

void KymeraOutput_SetAuxVolume(int16 volume_in_db)
{
    DEBUG_LOG_FN_ENTRY("KymeraOutput_SetAuxVolume: db=%d", volume_in_db);
    Operator vol_op;

    if (GET_OP_FROM_CHAIN(vol_op, KymeraGetTaskData()->chain_output_handle, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetAuxGain(vol_op, Kymera_VolDbToGain(volume_in_db));
    }
}

bool KymeraOutput_SetAuxTtp(uint32 time_to_play)
{
    Operator vol_op;
    if (GET_OP_FROM_CHAIN(vol_op, KymeraGetTaskData()->chain_output_handle, OPR_VOLUME_CONTROL))
    {
        if (kymeraOutput_IsAuxTtpSupported(OperatorGetCapabilityVersion(vol_op)))
        {
            OperatorsVolumeSetAuxTimeToPlay(vol_op, time_to_play,  0);
            return TRUE;
        }
    }
    return FALSE;
}

uint32 KymeraOutput_GetMainSampleRate(void)
{
    // Currently other modules create output chains as well (Voice Chains), in those cases the API should return the commonly used output rate
    return (state.input_rates.main) ? state.input_rates.main : KymeraGetTaskData()->output_rate;
}

uint32 KymeraOutput_GetAuxSampleRate(void)
{
    // Currently other modules create output chains as well (Voice Chains), in those cases the API should return the commonly used output rate
    return (state.input_rates.auxiliary) ? state.input_rates.auxiliary : KymeraGetTaskData()->output_rate;
}

void KymeraOutput_SetMainSampleRate(uint32 rate)
{
    // Can only set after chain creation
    PanicFalse(KymeraGetTaskData()->chain_output_handle != NULL);
    state.input_rates.main = rate;
}

void KymeraOutput_SetAuxSampleRate(uint32 rate)
{
    // Can only set after chain creation
    PanicFalse(KymeraGetTaskData()->chain_output_handle != NULL);
    state.input_rates.auxiliary = rate;
}

void KymeraOutput_ConnectToStereoMainInput(Source left, Source right)
{
    input_t connections[] =
    {
        {left, ChainGetInput(KymeraGetTaskData()->chain_output_handle, EPR_SINK_STEREO_MIXER_L)},
        {right, ChainGetInput(KymeraGetTaskData()->chain_output_handle, EPR_SINK_STEREO_MIXER_R)},
    };

    kymeraOutput_ConnectViaResampler(connections, ARRAY_DIM(connections), &state.resamplers.main_input, KymeraOutput_GetMainSampleRate());
}

void KymeraOutput_ConnectToMonoMainInput(Source mono)
{
    bool is_stereo = kymeraOutput_IsStereoChain(KymeraGetTaskData()->chain_output_handle);
    input_t connections[] =
    {
        {mono, ChainGetInput(KymeraGetTaskData()->chain_output_handle, EPR_SINK_MIXER_MAIN_IN)},
    };

    if (is_stereo)
    {
        PanicFalse(state.splitters.main_input == INVALID_OPERATOR);
        state.splitters.main_input = kymeraOutput_CreateSplitter();
        connections[0].sink = StreamSinkFromOperatorTerminal(state.splitters.main_input, SPLITTER_TERMINAL_IN_0);
    }

    kymeraOutput_ConnectViaResampler(connections, ARRAY_DIM(connections), &state.resamplers.main_input, KymeraOutput_GetMainSampleRate());

    if (is_stereo)
    {
        PanicNull(StreamConnect(StreamSourceFromOperatorTerminal(state.splitters.main_input, SPLITTER_TERMINAL_OUT_0),
                                ChainGetInput(KymeraGetTaskData()->chain_output_handle, EPR_SINK_STEREO_MIXER_L)));
        PanicNull(StreamConnect(StreamSourceFromOperatorTerminal(state.splitters.main_input, SPLITTER_TERMINAL_OUT_1),
                                ChainGetInput(KymeraGetTaskData()->chain_output_handle, EPR_SINK_STEREO_MIXER_R)));
    }

    if (state.chain_started)
    {
        kymeraOutput_StartSplitter(state.splitters.main_input);
    }
}

void KymeraOutput_ConnectToAuxInput(Source aux)
{
    input_t connections[] =
    {
        {aux, ChainGetInput(KymeraGetTaskData()->chain_output_handle, EPR_VOLUME_AUX)},
    };

    kymeraOutput_ConnectViaResampler(connections, ARRAY_DIM(connections), &state.resamplers.aux_input, KymeraOutput_GetAuxSampleRate());
}

void KymeraOutput_DisconnectStereoMainInput(void)
{
    chain_endpoint_role_t in_roles[] =
    {
        EPR_SINK_STEREO_MIXER_L,
        EPR_SINK_STEREO_MIXER_R
    };

    kymeraOutput_DisconnectViaResampler(in_roles, ARRAY_DIM(in_roles), &state.resamplers.main_input);
}

void KymeraOutput_DisconnectMonoMainInput(void)
{
    chain_endpoint_role_t in_roles[] =
    {
        EPR_SINK_MIXER_MAIN_IN
    };

    if (state.chain_started)
    {
        kymeraOutput_StopSplitter(state.splitters.main_input);
    }

    kymeraOutput_DisconnectViaResampler(in_roles, ARRAY_DIM(in_roles), &state.resamplers.main_input);
    kymeraOutput_DestroySplitter(&state.splitters.main_input);
}

void KymeraOutput_DisconnectAuxInput(void)
{
    chain_endpoint_role_t in_roles[] =
    {
        EPR_VOLUME_AUX
    };

    kymeraOutput_DisconnectViaResampler(in_roles, ARRAY_DIM(in_roles), &state.resamplers.aux_input);
}

void KymeraOutput_LoadDownloadableCaps(output_chain_t chain_type)
{
    ChainLoadDownloadableCapsFromChainConfig(kymeraOutput_GetChainConfig(chain_type));
}

void KymeraOutput_UnloadDownloadableCaps(output_chain_t chain_type)
{
    ChainUnloadDownloadableCapsFromChainConfig(kymeraOutput_GetChainConfig(chain_type));
}

void KymeraOutput_MuteMainChannel(bool mute_enable)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    kymera_chain_handle_t output = KymeraOutput_GetOutputHandle();
    int16 gain_in_db = mute_enable ? VOLUME_MUTE_IN_DB : 0;
    appKymeraSourceSyncSetMonoRouteGain(output, theKymera->output_rate,
                                        appConfigSyncUnmuteTransitionSamples(), gain_in_db);
}
