/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_wired_analog.c
\brief      Kymera Wired Analog file to create, configure and destroy the wired analog chain
*/

#include "kymera_wired_analog.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "kymera_common.h"
#include "kymera_state.h"
#include "kymera_config.h"
#include "kymera_tones_prompts.h"
#include "kymera_music_processing.h"
#include "kymera_source_sync.h"
#include "kymera_output_if.h"
#include <vmal.h>
#include <logging.h>

#if defined(INCLUDE_WIRED_ANALOG_AUDIO)

static const output_registry_entry_t output_info =
{
    .user = output_user_wired_analog,
    .connection = output_connection_stereo,
};

/**************************************************************************************

                                               UTILITY FUNCTIONS
**************************************************************************************/
static void kymeraWiredAnalog_ConfigureChain(kymeraTaskData *theKymera, uint32 rate, 
                                                                                uint32 min_latency, uint32 max_latency, uint32 latency)
{
    kymera_chain_handle_t chain_handle = theKymera->chain_input_handle;
    DEBUG_LOG("kymeraWiredAnalog_ConfigureChain");

    Operator ttp_passthrough = ChainGetOperatorByRole(chain_handle, OPR_LATENCY_BUFFER);
    if(ttp_passthrough)
    {
        OperatorsStandardSetLatencyLimits(ttp_passthrough,
                                          MS_TO_US(min_latency),
                                          MS_TO_US(max_latency));
    
        OperatorsConfigureTtpPassthrough(ttp_passthrough, MS_TO_US(latency), rate, operator_data_format_pcm);
        OperatorsStandardSetBufferSizeWithFormat(ttp_passthrough, TTP_BUFFER_SIZE, operator_data_format_pcm);
    }

    if(theKymera->chain_config_callbacks && theKymera->chain_config_callbacks->ConfigureWiredInputChain)
    {
        kymera_wired_config_params_t params = {0};
        params.sample_rate = rate;
        theKymera->chain_config_callbacks->ConfigureWiredInputChain(chain_handle, &params);
    }

    ChainConnect(chain_handle);
}

static Source SourcekymeraWiredAnalog_GetSource(audio_channel channel, uint8 inst, uint32 rate)
{
#define SAMPLE_SIZE 16 /* only if 24 bit resolution is supported this can be 24 */
    Source source;
    analogue_input_params params = {
        .pre_amp = FALSE,
        .gain = 0x09, /* for line-in set to 0dB */
        .instance = 0, /* Place holder */
        .enable_24_bit_resolution = FALSE
        };

    DEBUG_LOG("SourcekymeraWiredAnalog_GetSource, Get source for Channel: %u, Instance: %u and Sample Rate: %u", channel, inst, rate);
    params.instance = inst;
    source = AudioPluginAnalogueInputSetup(channel, params, rate);
    PanicFalse(SourceConfigure(source, STREAM_AUDIO_SAMPLE_SIZE, SAMPLE_SIZE));

    return source;
}

static void kymeraWiredAnalog_StartChains(kymeraTaskData *theKymera)
{
    bool connected;

    Source line_in_l = SourcekymeraWiredAnalog_GetSource(appConfigLeftAudioChannel(), appConfigLeftAudioInstance(), KymeraOutput_GetMainSampleRate() /* for now input/output rate are same */);
    Source line_in_r = SourcekymeraWiredAnalog_GetSource(appConfigRightAudioChannel(), appConfigRightAudioInstance(), KymeraOutput_GetMainSampleRate() /* for now input/output rate are same */);
    /* if stereo, then synchronize */
    if(line_in_r)
        SourceSynchronise(line_in_l, line_in_r);

    DEBUG_LOG("kymeraWiredAnalog_StartChains");
    /* The media source may fail to connect to the input chain if the source
    disconnects between the time wired analog audio asks Kymera to start and this
    function being called. wired analog audio will subsequently ask Kymera to stop. */
    connected = ChainConnectInput(theKymera->chain_input_handle, line_in_l, EPR_WIRED_STEREO_INPUT_L);
    if(line_in_r)
        connected = ChainConnectInput(theKymera->chain_input_handle, line_in_r, EPR_WIRED_STEREO_INPUT_R);

    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();

    if (connected)
        ChainStart(theKymera->chain_input_handle);
}

static void kymeraWiredAnalog_CreateAndConfigureOutputChain(uint32 rate,
                                                   int16 volume_in_db)
{
    kymera_output_chain_config config = {0};

    DEBUG_LOG("appKymeraCreateAndConfigureOutputChain, creating output chain, completing startup");
    config.rate = rate;
    config.kick_period = KICK_PERIOD_WIRED_ANALOG;

    if (config.kick_period == KICK_PERIOD_SLOW)
    {
        config.source_sync_max_period = appKymeraGetSlowKickSourceSyncPeriod(TRUE);
        config.source_sync_min_period = appKymeraGetSlowKickSourceSyncPeriod(FALSE);
        config.set_source_sync_min_period = TRUE;
        config.set_source_sync_max_period = TRUE;
    }
    else if (config.kick_period == KICK_PERIOD_FAST)
    {
        config.source_sync_max_period = appKymeraGetFastKickSourceSyncPeriod(TRUE);
        config.source_sync_min_period = appKymeraGetFastKickSourceSyncPeriod(FALSE);
        config.set_source_sync_min_period = TRUE;
        config.set_source_sync_max_period = TRUE;
    }

    /* Output buffer is 2.5*KP */
    appKymeraSetSourceSyncConfigOutputBufferSize(&config, 5, 2);
    config.chain_type = output_chain_stereo;

    PanicFalse(Kymera_OutputPrepare(output_user_wired_analog, &config));
    KymeraOutput_SetMainVolume(volume_in_db);
}

static void kymeraWiredAnalog_CreateChain(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg)
{

    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("kymeraWiredAnalog_CreateChain, creating output chain, completing startup");

    kymeraWiredAnalog_CreateAndConfigureOutputChain(msg->rate, msg->volume_in_db);
    /* Create the wired analog chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_input_wired_analog_stereo_config));
    /* configure it */
    kymeraWiredAnalog_ConfigureChain(theKymera, msg->rate, msg->min_latency, msg->max_latency, msg->target_latency);
}

static void kymeraWiredAnalog_DestroyChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicNull(theKymera->chain_input_handle);

    Sink to_ttp_l = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_L);
    Sink to_ttp_r = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_R);

    Source from_ttp_l = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    Source from_ttp_r = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT);
    

    DEBUG_LOG("kymeraWiredAnalog_DestroyChain, l-source(%p), r-source(%p)", from_ttp_l, from_ttp_r);
    DEBUG_LOG("kymeraWiredAnalog_DestroyChain, l-sink(%p), r-sink(%p)", to_ttp_l, to_ttp_r);

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);

    /* Disconnect codec source from chain */
    StreamDisconnect(NULL, to_ttp_l);
    StreamDisconnect(NULL, to_ttp_r);

    /* Disconnect the chain output */
    StreamDisconnect(from_ttp_l, NULL);
    StreamDisconnect(from_ttp_r, NULL);

    Kymera_StopMusicProcessingChain();
    Kymera_OutputDisconnect(output_user_wired_analog);
    Kymera_DestroyMusicProcessingChain();

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;
}

static void kymeraWiredAnalog_JoinChains(kymeraTaskData *theKymera)
{
    output_source_t output = {0};
    output.stereo.left = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    output.stereo.right = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT);

    if(Kymera_IsMusicProcessingPresent())
    {
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.left, EPR_MUSIC_PROCESSING_IN_L));
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.right, EPR_MUSIC_PROCESSING_IN_R));
        output.stereo.left = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_L);
        output.stereo.right = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_R);
    }

    PanicFalse(Kymera_OutputConnect(output_user_wired_analog, &output));
}

/**************************************************************************************

                                               INTERFACE FUNCTIONS
**************************************************************************************/

/*************************************************************************/
void KymeraWiredAnalog_StartPlayingAudio(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("KymeraWiredAnalog_StartPlayingAudio, state %u, rate %u, latency %u", appKymeraGetState(), msg->rate, msg->target_latency);

    /* If there is a tone still playing at this point,
      * it must be an interruptable tone, so cut it off */
    appKymeraTonePromptStop();

    /* Can only start streaming if we're currently idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);
    /* Ensure there are no audio chains already */
    PanicNotNull(theKymera->chain_input_handle);

    kymeraWiredAnalog_CreateChain(msg);
    Kymera_CreateMusicProcessingChain();
    Kymera_ConfigureMusicProcessing(msg->rate);
    kymeraWiredAnalog_JoinChains(theKymera);
    appKymeraSetState(KYMERA_STATE_WIRED_AUDIO_PLAYING);

    /* Set the DSP clock to low-power mode */
    appKymeraConfigureDspPowerMode();
    kymeraWiredAnalog_StartChains(theKymera);    
}

/*************************************************************************/
void KymeraWiredAnalog_StopPlayingAudio(void)
{
    DEBUG_LOG("KymeraWiredAnalog_StopPlayingAudio, state %u", appKymeraGetState());
    switch (appKymeraGetState())
    {
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
            /* Keep framework enabled until after DSP clock update */
            OperatorsFrameworkEnable();
        
            kymeraWiredAnalog_DestroyChain();
            appKymeraSetState(KYMERA_STATE_IDLE);
            
            /* Return to low power mode (if applicable) */
            appKymeraConfigureDspPowerMode();
            OperatorsFrameworkDisable();
            
        break;

        case KYMERA_STATE_IDLE:
        break;

        default:
            // Report, but ignore attempts to stop in invalid states
            DEBUG_LOG("KymeraWiredAnalog_StopPlayingAudio, invalid state %u", appKymeraGetState());
        break;
    }
}

void KymeraWiredAnalog_SetVolume(int16 volume_in_db)
{
    DEBUG_LOG("KymeraWiredAnalog_SetVolume, state %u", appKymeraGetState());

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
            KymeraOutput_SetMainVolume(volume_in_db);
            break;

        default:
            break;
    }
}

void KymeraWiredAnalog_Init(void)
{
    Kymera_OutputRegister(&output_info);
}

#endif /* INCLUDE_WIRED_ANALOG_AUDIO*/
