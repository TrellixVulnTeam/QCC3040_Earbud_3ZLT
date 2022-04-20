/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_adaptive_anc.c
\brief      Kymera Adaptive ANC code
*/

#include "kymera_adaptive_anc.h"
#include "kymera_common.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_lock.h"
#include "kymera_config.h"
#include "kymera_state.h"
#include "anc_state_manager.h"
#include "microphones.h"
#include "kymera_aec.h"
#include "kymera_va.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <vmal.h>
#include <file.h>
#include <stdlib.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>

#include "kymera_fit_test.h"

#ifdef ENABLE_ADAPTIVE_ANC

#define MAX_CHAIN (3)
#define CHAIN_MIC_REF_PATH_SPLITTER (MAX_CHAIN-1)
#define CHAIN_FBC (CHAIN_MIC_REF_PATH_SPLITTER-1)
#define CHAIN_AANC (CHAIN_FBC-1)

#define AANC_USB_AUDIO_MIC_CHANNELS     2   /* Number of mic and speaker channels in the audio data stream */
#define AANC_USB_AUDIO_SPKR_CHANNELS    1   /* Number of mic and speaker channels in the audio data stream */

#define AANC_SAMPLE_RATE 16000      /* Only 16KHz supported for anc tuning */
#define MAX_AANC_MICS (2)

#define kymeraAdaptiveAnc_IsAancActive() (kymeraAdaptiveAnc_GetChain(CHAIN_AANC) != NULL)
#define kymeraAdaptiveAnc_PanicIfNotActive()  if(!kymeraAdaptiveAnc_IsAancActive()) \
                                                                                    Panic()
#define kymeraAdaptiveAnc_IsAancFbcActive() (kymeraAdaptiveAnc_GetChain(CHAIN_FBC) != NULL)
#define kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated() (kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER) != NULL)

#define CONVERT_MSEC_TO_SEC(value) ((value)/1000)

#define IN_EAR  TRUE
#define OUT_OF_EAR !IN_EAR
#define ENABLE_ADAPTIVITY TRUE
#define DISABLE_ADAPTIVITY !ENABLE_ADAPTIVITY

#define NUM_STATUS_VAR              24
#define CUR_MODE_STATUS_OFFSET       0
#define FLAGS_STATUS_OFFSET          7
#define FF_GAIN_STATUS_OFFSET        8
#define FLAG_POS_QUIET_MODE         20

#define BIT_MASK(FLAG_POS)          (1 << FLAG_POS)

#define AANC_TUNNING_START_DELAY    (200U)

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#define AANC_TUNING_SINK_USB        0
#define AANC_TUNING_SINK_UNUSED     1 /* Unused */
#define AANC_TUNING_SINK_INT_MIC    2 /* Internal Mic In */
#define AANC_TUNING_SINK_EXT_MIC    3 /* External Mic In */

#define AANC_TUNING_SOURCE_DAC      0
#define AANC_TUNING_SOURCE_UNUSED   1 /* Unused */
#define AANC_TUNING_SOURCE_INT_MIC  2 /* Forwards Int Mic */
#define AANC_TUNING_SOURCE_EXT_MIC  3 /* Forwards Ext Mic */

#ifdef DOWNLOAD_USB_AUDIO
#define EB_CAP_ID_USB_AUDIO_RX CAP_ID_DOWNLOAD_USB_AUDIO_RX
#define EB_CAP_ID_USB_AUDIO_TX CAP_ID_DOWNLOAD_USB_AUDIO_TX
#else
#define EB_CAP_ID_USB_AUDIO_RX CAP_ID_USB_AUDIO_RX
#define EB_CAP_ID_USB_AUDIO_TX CAP_ID_USB_AUDIO_TX
#endif

#ifdef __QCC517X__
/* By default AANC IIR filter sample rate set to 32kHz (in QCC517x devices) */
#define AANC_FILTER_SAMPLE_RATE (adaptive_anc_sample_rate_32khz)
#endif
/* AANC Capability ID*/
/* Hard coded to 0x409F to fix unity. Will be modified*/
#define CAP_ID_DOWNLOAD_AANC_TUNING 0x409F

static kymera_chain_handle_t adaptive_anc_chains[MAX_CHAIN] = {0};

static void kymeraAdaptiveAnc_UpdateAdaptivity(bool enable_adaptivity);

static kymera_chain_handle_t kymeraAdaptiveAnc_GetChain(uint8 index)
{
    return ((index < MAX_CHAIN) ? adaptive_anc_chains[index] : NULL);
}

static void kymeraAdaptiveAnc_SetChain(uint8 index, kymera_chain_handle_t chain)
{
    if(index < MAX_CHAIN)
        adaptive_anc_chains[index] = chain;
}

static Source kymeraAdaptiveAnc_GetOutput(uint8 index, chain_endpoint_role_t output_role)
{
    return ChainGetOutput(kymeraAdaptiveAnc_GetChain(index), output_role);
}

static Sink kymeraAdaptiveAnc_GetInput(uint8 index, chain_endpoint_role_t output_role)
{
    return ChainGetInput(kymeraAdaptiveAnc_GetChain(index), output_role);
}


/* Mic interface callback functions */
static bool kymeraAdaptiveAnc_MicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymeraAdaptiveAnc_MicDisconnectIndication(const mic_change_info_t *info);
static void kymeraAdaptiveAnc_MicReconnectedIndication(void);
/* AANC support functions */
static void kymeraAdaptiveAnc_EnableConcurrency(bool transition);
static void kymeraAdaptiveAnc_CreateEchoCancellerChain(void);
static void kymeraAdaptiveAnc_ConnectConcurrency(void);
static void kymeraAdaptiveAnc_StartConcurrency(bool transition);
static void kymeraAdaptiveAnc_ConnectFbcChain(void);
static void kymeraAdaptiveAnc_ConnectSplitterFbcChain(void);
static void kymeraAdaptiveAnc_ActivateMicRefPathSplitterSecondOutput(void);

static void kymeraAdaptiveAnc_DisableConcurrency(bool transition);
static void kymeraAdaptiveAnc_StopConcurrency(void);
static void kymeraAdaptiveAnc_DisconnectConcurrency(void);
static void kymeraAdaptiveAnc_DestroyConcurrency(bool transition);
static void kymeraAdaptiveAnc_DeactivateMicRefPathSplitterSecondOutput(void);
static void kymeraAdaptiveAnc_DisconnectFbcOutputs(void);
static void kymeraAdaptiveAnc_DisconnectMicRefPathSplitterOutputs(void);

static void kymeraAdaptiveAnc_DisableStandalone(bool transition);
static void kymeraAdaptiveAnc_EnableStandalone(bool transition);

static void kymeraAdaptiveAnc_Stop(void);
static void kymeraAdaptiveAnc_Restart(void);
static void kymeraAdaptiveAnc_Disconnect(void);
static void kymeraAdaptiveAnc_Reconnect(void);

static void kymeraAdaptiveAnc_SetKymeraState(void);
static void kymeraAdaptiveAnc_ResetKymeraState(void);

/**************************************
 *** Kymera Mic interface callbacks ***
 **************************************/
static microphone_number_t kymera_AdaptiveAncMics[MAX_AANC_MICS] =
{
    appConfigAncFeedForwardMic(),
    appConfigAncFeedBackMic()
};

static const mic_callbacks_t kymera_AdaptiveAncCallbacks =
{
    .MicGetConnectionParameters = kymeraAdaptiveAnc_MicGetConnectionParameters,
    .MicDisconnectIndication = kymeraAdaptiveAnc_MicDisconnectIndication,
    .MicReconnectedIndication = kymeraAdaptiveAnc_MicReconnectedIndication,
};

static mic_user_state_t kymera_AancMicState = mic_user_state_interruptible;

static const mic_registry_per_user_t kymera_AdaptiveAncRegistry =
{
    .user = mic_user_aanc,
    .callbacks = &kymera_AdaptiveAncCallbacks,
    .mandatory_mic_ids = &kymera_AdaptiveAncMics[0],
    .num_of_mandatory_mics = MAX_AANC_MICS,
    .mic_user_state = &kymera_AancMicState,
};

/*! For a reconnection the mic parameters are sent to the mic interface.
 *  return TRUE to reconnect with the given parameters
 */
static bool kymeraAdaptiveAnc_MicGetConnectionParameters(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    *sample_rate = AANC_SAMPLE_RATE;
    *num_of_mics = MAX_AANC_MICS;
    mic_ids[0] = appConfigAncFeedForwardMic();
    mic_ids[1] = appConfigAncFeedBackMic();

    DEBUG_LOG("kymeraAdaptiveAnc_MicGetConnectionParameters");

    if (kymeraAdaptiveAnc_IsAancFbcActive())
    {
        DEBUG_LOG("AANC concurrency mic sinks");
        mic_sinks[0] = kymeraAdaptiveAnc_GetInput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_IN);
        mic_sinks[1] = kymeraAdaptiveAnc_GetInput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_IN);
        aec_ref_sink[0] = kymeraAdaptiveAnc_GetInput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_IN);
    }
    else
    {
        DEBUG_LOG("AANC standalone mic sinks");
        mic_sinks[0] = kymeraAdaptiveAnc_GetInput(CHAIN_AANC, EPR_AANC_FF_MIC_IN);
        mic_sinks[1] = kymeraAdaptiveAnc_GetInput(CHAIN_AANC, EPR_AANC_ERR_MIC_IN);
        aec_ref_sink[0] = NULL;
    }

    return TRUE;
}

/*! Before the microphones are disconnected, all users get informed with a DisconnectIndication.
 * return FALSE: accept disconnection
 * return TRUE: Try to reconnect the microphones. This will trigger a kymeraAdaptiveAnc_MicGetConnectionParameters
 */
static bool kymeraAdaptiveAnc_MicDisconnectIndication(const mic_change_info_t *info)
{
    DEBUG_LOG("kymeraAdaptiveAnc_MicDisconnectIndication user %d, event %d", info->user, info->event);
    /* Stop & disconnect Adaptive ANC audio graph - due to client disconnection request */
    kymeraAdaptiveAnc_Stop();
    kymeraAdaptiveAnc_Disconnect();
    return TRUE;
}


/*! This indication is sent if the microphones have been reconnected after a DisconnectIndication.
 * Also used in cases where ANC is enabled and SCO is started, then a kymeraAdaptiveAnc_MicDisconnectIndication
 * and a kymeraAdaptiveAnc_MicReconnectedIndication is called, which will configure the new mic sinks through
 * kymeraAdaptiveAnc_MicGetConnectionParameters callback from mic framework
 */
static void kymeraAdaptiveAnc_MicReconnectedIndication(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_MicReconnectedIndication");
    /* Reconnect & Restart Adaptive ANC audio graph - after new client added */
    kymeraAdaptiveAnc_Reconnect();
    kymeraAdaptiveAnc_Restart();
}

/***************************************
 *** Kymera Output Manager callbacks ***
 ***************************************/
/*! Notifies registered user that another user is about to connect to the output chain. */
static void kymera_AdaptiveAncOutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    UNUSED(connecting_user);
    DEBUG_LOG_INFO("kymera_AdaptiveAncOutputConnectingIndication connecting user: enum:output_users_t:%d",connecting_user);

    if(kymeraAdaptiveAnc_IsAancActive())
    {
        if(KymeraAdaptiveAnc_IsConcurrencyActive())
        {
            /* do nothing */
        }
        else
        {
            kymeraAdaptiveAnc_DisableStandalone(TRUE);
            kymeraAdaptiveAnc_EnableConcurrency(TRUE);
        }
    }
}

/*! Notifies registered user that a user has disconnected from the output chain */
static void kymera_AdaptiveAncOutputDisconnectedIndication(output_users_t disconnected_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    UNUSED(disconnected_user);
    DEBUG_LOG_INFO("kymera_AdaptiveAncOutputDisconnectingIndication disconnected user: enum:output_users_t:%d",disconnected_user);
    DEBUG_LOG_INFO("kymera_AdaptiveAncOutputDisconnectingIndication Kymera_OutputIsChainInUse():%d",Kymera_OutputIsChainInUse());

    if(!Kymera_OutputIsChainInUse())
    {
        if(kymeraAdaptiveAnc_IsAancActive())
        {
            if(KymeraAdaptiveAnc_IsConcurrencyActive())
            {
                kymeraAdaptiveAnc_DisableConcurrency(TRUE);
                kymeraAdaptiveAnc_EnableStandalone(TRUE);
            }
        }
    }
}

static const output_indications_registry_entry_t aanc_user_info =
{
    .OutputConnectingIndication = kymera_AdaptiveAncOutputConnectingIndication,
    .OutputDisconnectedIndication = kymera_AdaptiveAncOutputDisconnectedIndication,
};

/*!
 *  Registers AANC callbacks in the mic interface layer and kymera output manager
 */
void KymeraAdaptiveAnc_Init(void)
{
    Kymera_OutputRegisterForIndications(&aanc_user_info);
    Kymera_MicRegisterUser(&kymera_AdaptiveAncRegistry);
}

static void kymeraAdaptiveAnc_SetKymeraState(void)
{
    if(!appKymeraIsBusy())
        appKymeraSetState(KYMERA_STATE_ADAPTIVE_ANC_STARTED);

}

static void kymeraAdaptiveAnc_ResetKymeraState(void)
{
    if((appKymeraGetState() == KYMERA_STATE_ADAPTIVE_ANC_STARTED))
        appKymeraSetState(KYMERA_STATE_IDLE);
}


/**************** Utility functions for Adaptive ANC chain ********************************/
/*Reads the static gain for current mode in library*/
static void kymeraAdaptiveAnc_SetStaticGain(Operator op, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel)
{
    uint16 coarse_gain;
    uint8 fine_gain;

    uint16 *static_gains = PanicUnlessMalloc(sizeof(uint16) * adaptive_anc_static_gain_max);
    memset(static_gains, 0, sizeof(uint16) * adaptive_anc_static_gain_max);

    audio_anc_instance inst = hw_channel ? AUDIO_ANC_INSTANCE_1 : AUDIO_ANC_INSTANCE_0;

    /*If hybrid is configured, feedforward path is AUDIO_ANC_PATH_ID_FFB and feedback path will be AUDIO_ANC_PATH_ID_FFA*/
    audio_anc_path_id feedback_anc_path = (feedforward_anc_path==AUDIO_ANC_PATH_ID_FFB)?(AUDIO_ANC_PATH_ID_FFA):(AUDIO_ANC_PATH_ID_FFB);/*TBD for Feed forward ANC mode*/

     /*Update gains*/
    AncReadCoarseGainFromInstance(inst, feedforward_anc_path, &coarse_gain);
    static_gains[adaptive_anc_static_gain_ff_coarse] = (uint16)coarse_gain;
    AncReadFineGainFromInstance(inst, feedforward_anc_path, &fine_gain);
    static_gains[adaptive_anc_static_gain_ff_fine] = (uint16)fine_gain;

    AncReadCoarseGainFromInstance(inst, feedback_anc_path, &coarse_gain);
    static_gains[adaptive_anc_static_gain_fb_coarse] = (uint16)coarse_gain;
    AncReadFineGainFromInstance(inst, feedback_anc_path, &fine_gain);
    static_gains[adaptive_anc_static_gain_fb_fine] = (uint16)fine_gain;

    AncReadCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FB, &coarse_gain);
    static_gains[adaptive_anc_static_gain_ec_coarse] = (uint16)coarse_gain;
    AncReadFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FB, &fine_gain);
    static_gains[adaptive_anc_static_gain_ec_fine] = (uint16)fine_gain;

    AncReadRxMixCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFA, &coarse_gain);
    static_gains[adaptive_anc_static_gain_rxmix_ffa_coarse] = (uint16)coarse_gain;
    AncReadRxMixFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFA, &fine_gain);
    static_gains[adaptive_anc_static_gain_rxmix_ffa_fine] = (uint16)fine_gain;

    AncReadRxMixCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFB, &coarse_gain);
    static_gains[adaptive_anc_static_gain_rxmix_ffb_coarse] = (uint16)coarse_gain;
    AncReadRxMixFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFB, &fine_gain);
    static_gains[adaptive_anc_static_gain_rxmix_ffb_fine] = (uint16)fine_gain;

    OperatorsAdaptiveAncSetStaticGainWithRxMix(op, static_gains);

    free(static_gains);
}

static void kymeraAdaptiveAnc_SetControlModelForParallelTopology(Operator op,audio_anc_path_id control_path,adaptive_anc_coefficients_t *numerator, adaptive_anc_coefficients_t *denominator)
{
    /*ANC library to read the control coefficients */
    AncReadModelCoefficients(AUDIO_ANC_INSTANCE_0, control_path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsParallelAdaptiveAncSetControlModel(op,AUDIO_ANC_INSTANCE_0,numerator,denominator);
    AncReadModelCoefficients(AUDIO_ANC_INSTANCE_1, control_path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsParallelAdaptiveAncSetControlModel(op,AUDIO_ANC_INSTANCE_1,numerator,denominator);
}

static void kymeraAdaptiveAnc_SetControlModelForSingleTopology(Operator op,audio_anc_instance inst,audio_anc_path_id control_path,adaptive_anc_coefficients_t *numerator, adaptive_anc_coefficients_t *denominator)
{
    /*ANC library to read the control coefficients */
    AncReadModelCoefficients(inst, control_path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsAdaptiveAncSetControlModel(op, numerator, denominator);
}

static void kymeraAdaptiveAnc_SetControlPlantModel(Operator op, audio_anc_path_id control_path, adaptive_anc_hw_channel_t hw_channel)
{
    /* Currently number of numerators & denominators are defaulted to QCC514XX specific value.
        However this might change for other family of chipset. So, ideally this shall be supplied from
        ANC library during the coarse of time */
    uint8 num_denominators = AncReadNumOfDenominatorCoefficients();
    uint8 num_numerators = AncReadNumOfNumeratorCoefficients();
    adaptive_anc_coefficients_t *denominator, *numerator;
    kymeraTaskData *theKymera = KymeraGetTaskData();


    audio_anc_instance inst = (hw_channel) ? AUDIO_ANC_INSTANCE_1 : AUDIO_ANC_INSTANCE_0;

    /* Regsiter a listener with the AANC*/
    MessageOperatorTask(op, &theKymera->task);

    /*If hybrid is configured, feedforward path is AUDIO_ANC_PATH_ID_FFB and feedback path will be AUDIO_ANC_PATH_ID_FFA*/
    /*audio_anc_path_id fb_path = (param->control_path==AUDIO_ANC_PATH_ID_FFB)?(AUDIO_ANC_PATH_ID_FFA):(AUDIO_ANC_PATH_ID_FFB); */

    denominator = OperatorsCreateAdaptiveAncCoefficientsData(num_denominators);
    numerator= OperatorsCreateAdaptiveAncCoefficientsData(num_numerators);

    if(appKymeraIsParallelAncFilterEnabled())
    {
        kymeraAdaptiveAnc_SetControlModelForParallelTopology(op,control_path,numerator,denominator);
    }
    else
    {
        kymeraAdaptiveAnc_SetControlModelForSingleTopology(op,inst,control_path,numerator,denominator);
    }

    /* Free it, so that it can be re-used */
    free(denominator);
    free(numerator);

    denominator = OperatorsCreateAdaptiveAncCoefficientsData(num_denominators);
    numerator= OperatorsCreateAdaptiveAncCoefficientsData(num_numerators);
    /*ANC library to read the plant coefficients */
    AncReadModelCoefficients(inst, AUDIO_ANC_PATH_ID_FB, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsAdaptiveAncSetPlantModel(op, numerator, denominator);
    /* Free it, so that it can be re-used */
    free(denominator);
    free(numerator);
}

static void kymeraAdaptiveAnc_SetParallelTopology(Operator op)
{
    OperatorsAdaptiveAncSetParallelTopology(op,adaptive_anc_filter_config_parallel_topology);
}

static void kymeraAdaptiveAnc_ConfigureAancChain(Operator op, const KYMERA_INTERNAL_AANC_ENABLE_T* param, aanc_usecase_t usecase)
{

    if(appKymeraIsParallelAncFilterEnabled())
    {
        kymeraAdaptiveAnc_SetParallelTopology(op);
    }

#ifdef __QCC517X__
    OperatorsAdaptiveAncSetSampleRate(op, AANC_FILTER_SAMPLE_RATE);
#else
/* ANC IP (in pre-QCC517x devices) only supports an option of 16kHz IIR filter sample rate */
#endif

    kymeraAdaptiveAnc_SetControlPlantModel(op, param->control_path, param->hw_channel);
    kymeraAdaptiveAnc_SetStaticGain(op, param->control_path, param->hw_channel);
    OperatorsAdaptiveAncSetHwChannelCtrl(op, param->hw_channel);
    OperatorsAdaptiveAncSetInEarCtrl(op, param->in_ear);
    OperatorsStandardSetUCID(op, param->current_mode);

    switch (usecase)
    {
        case aanc_usecase_standalone:
            if (AncConfig_IsAncModeAdaptive(param->current_mode))
            {
                OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_full);
                DEBUG_LOG("AANC comes up in Full Proc");
            }
            else
            {
                OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_static);
                DEBUG_LOG("AANC comes up in Static");
            }
            break;

        case aanc_usecase_sco_concurrency:
            OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_standby);
            DEBUG_LOG("AANC comes up in Standby");
            break;

        default:
            DEBUG_LOG("NOTE: AANC is in default mode of capability");
            break;
    }
}

static void kymeraAdaptiveAnc_UpdateInEarStatus(bool in_ear)
{
    if(!KymeraFitTest_IsTuningModeActive())
    {
        Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
        OperatorsAdaptiveAncSetInEarCtrl(op, in_ear);
    }
}

static void kymeraAdaptiveAnc_UpdateAdaptivity(bool enable_adaptivity)
{
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    if(enable_adaptivity)
        OperatorsAdaptiveAncEnableGainCalculation(op);
    else
        OperatorsAdaptiveAncDisableGainCalculation(op);
}

static void kymeraAdaptiveAnc_CreateAancChain(const KYMERA_INTERNAL_AANC_ENABLE_T* param, aanc_usecase_t usecase)
{
    DEBUG_LOG("kymeraAdaptiveAnc_CreateAancChain");
    PanicNotNull(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
    kymeraAdaptiveAnc_SetChain(CHAIN_AANC, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aanc_config)));

    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    kymeraAdaptiveAnc_ConfigureAancChain(op, param, usecase);

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureAdaptiveAncChain)
    {
        KymeraGetTaskData()->chain_config_callbacks->ConfigureAdaptiveAncChain(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
    }

    ChainConnect(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
}

static void kymeraAdaptiveAnc_DestroyAancChain(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_DestroyAancChain");
    PanicNull(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
    ChainDestroy(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
    kymeraAdaptiveAnc_SetChain(CHAIN_AANC, NULL);
}
static get_status_data_t* kymeraAdaptiveAnc_GetStatusData(void)
{
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    get_status_data_t* get_status = OperatorsCreateGetStatusData(NUM_STATUS_VAR);
    OperatorsGetStatus(op, get_status);
    return get_status;
}

static void kymeraAdaptiveAnc_GetStatusFlags(uint32 *flags)
{
    get_status_data_t* get_status = kymeraAdaptiveAnc_GetStatusData();
    *flags = (uint32)(get_status->value[FLAGS_STATUS_OFFSET]);
    free(get_status);
}

static void kymeraAdaptiveAnc_GetGain(uint8 *warm_gain)
{
    get_status_data_t* get_status = kymeraAdaptiveAnc_GetStatusData();
    *warm_gain = (uint8)(get_status->value[FF_GAIN_STATUS_OFFSET]);
    free(get_status);
}

static void kymeraAdaptiveAnc_GetCurrentMode(adaptive_anc_mode_t *aanc_mode)
{
    get_status_data_t* get_status = kymeraAdaptiveAnc_GetStatusData();
    *aanc_mode = (adaptive_anc_mode_t)(get_status->value[CUR_MODE_STATUS_OFFSET]);
    free(get_status);
}

static bool kymeraAdaptiveAnc_IsNoiseLevelBelowQuietModeThreshold(void)
{
    uint32 flags;
    kymeraAdaptiveAnc_GetStatusFlags(&flags);
    return (flags & BIT_MASK(FLAG_POS_QUIET_MODE));
}

static void kymeraAdaptiveAnc_SetGainValues(uint32 mantissa_val, uint32 exponent_val)
{
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    OperatorsAdaptiveAncSetGainParameters(op, mantissa_val, exponent_val);
}

static void kymeraAdaptiveAnc_CreateFbcChain(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_CreateFbcChain");
    if (!kymeraAdaptiveAnc_IsAancFbcActive())
    {
        PanicNotNull(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
        kymeraAdaptiveAnc_SetChain(CHAIN_FBC, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aanc_fbc_config)));
        PanicFalse(Kymera_SetOperatorUcid(kymeraAdaptiveAnc_GetChain(CHAIN_FBC), OPR_AANC_FBC_ERR_MIC_PATH, UCID_ADAPTIVE_ANC_FBC));
        PanicFalse(Kymera_SetOperatorUcid(kymeraAdaptiveAnc_GetChain(CHAIN_FBC), OPR_AANC_FBC_FF_MIC_PATH, UCID_ADAPTIVE_ANC_FBC));
        ChainConnect(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
    }
}

static void kymeraAdaptiveAnc_DestroyFbcChain(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_DestroyFbcChain");
    if (kymeraAdaptiveAnc_IsAancFbcActive())
    {
        PanicNull(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
        ChainDestroy(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
        kymeraAdaptiveAnc_SetChain(CHAIN_FBC, NULL);
    }
}

static void kymeraAdaptiveAnc_CreateSplitterChainInMicRefPath(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_CreateSplitterChainInMicRefPath");
    if (!kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated())
    {
        Operator op;
        PanicNotNull(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        kymeraAdaptiveAnc_SetChain(CHAIN_MIC_REF_PATH_SPLITTER, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aanc_splitter_mic_ref_path_config)));

        op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), OPR_AANC_SPLT_MIC_REF_PATH);
        if(op)
        {
            OperatorsSplitterSetWorkingMode(op, splitter_mode_clone_input);
            OperatorsSplitterEnableSecondOutput(op, FALSE);
            OperatorsSplitterSetDataFormat(op, operator_data_format_pcm);
        }
        ChainConnect(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
    }
}

static void kymeraAdaptiveAnc_DestroySplitterChainInMicRefPath(void)
{
    if (kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated())
    {
        DEBUG_LOG("kymeraAdaptiveAnc_DestroySplitterChainInMicRefPath");
        PanicNull(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        ChainDestroy(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        kymeraAdaptiveAnc_SetChain(CHAIN_MIC_REF_PATH_SPLITTER, NULL);
    }
}

static void kymeraAdaptiveAnc_ActivateMicRefPathSplitterSecondOutput(void)
{
    if(kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated())
    {
        Operator splt_op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), OPR_AANC_SPLT_MIC_REF_PATH);
        DEBUG_LOG("kymeraAdaptiveAnc_ActivateMicRefPathSplitterSecondOutput");
        OperatorsSplitterEnableSecondOutput(splt_op, TRUE);
    }
}

static void kymeraAdaptiveAnc_DeactivateMicRefPathSplitterSecondOutput(void)
{
    if(kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated())
    {
        Operator splt_op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), OPR_AANC_SPLT_MIC_REF_PATH);
        DEBUG_LOG("kymeraAdaptiveAnc_DeactivateMicRefPathSplitterSecondOutput");
        OperatorsSplitterEnableSecondOutput(splt_op, FALSE);
    }
}

static void kymeraAdaptiveAnc_ConnectFbcChain(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_ConnectFbcChain");
    if(kymeraAdaptiveAnc_IsAancActive() && kymeraAdaptiveAnc_IsAancFbcActive())
    {
        /* Connect FBC and AANC operators */
        PanicNull(StreamConnect(kymeraAdaptiveAnc_GetOutput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_OUT), kymeraAdaptiveAnc_GetInput(CHAIN_AANC, EPR_AANC_FF_MIC_IN)));
        PanicNull(StreamConnect(kymeraAdaptiveAnc_GetOutput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_OUT), kymeraAdaptiveAnc_GetInput(CHAIN_AANC, EPR_AANC_ERR_MIC_IN)));
    }
}

static void kymeraAdaptiveAnc_ConnectSplitterFbcChain(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_ConnectSplitterFbcChain");
    if((kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated()) && kymeraAdaptiveAnc_IsAancFbcActive())
    {
        /* Connect Splitter and FBC operators */
        PanicNull(StreamConnect(kymeraAdaptiveAnc_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT1), kymeraAdaptiveAnc_GetInput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_REF_IN)));
        PanicNull(StreamConnect(kymeraAdaptiveAnc_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT2), kymeraAdaptiveAnc_GetInput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_REF_IN)));
    }
}

static void kymeraAdaptiveAnc_DisconnectFbcOutputs(void)
{
    if(kymeraAdaptiveAnc_IsAancFbcActive())
    {
        StreamDisconnect(kymeraAdaptiveAnc_GetOutput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_OUT), NULL);
        StreamDisconnect(kymeraAdaptiveAnc_GetOutput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_OUT), NULL);
    }
}

static void kymeraAdaptiveAnc_DisconnectMicRefPathSplitterOutputs(void)
{
    if(kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated())
    {
        StreamDisconnect(kymeraAdaptiveAnc_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT1), NULL);
        StreamDisconnect(kymeraAdaptiveAnc_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT2), NULL);
    }
}

bool KymeraAdaptiveAnc_IsConcurrencyActive(void)
{
    return (kymeraAdaptiveAnc_IsAancActive() && kymeraAdaptiveAnc_IsAancFbcActive());
}

bool KymeraAdaptiveAnc_IsEnabled(void)
{
    return kymeraAdaptiveAnc_IsAancActive();
}

static void kymeraAdaptiveAnc_CreateEchoCancellerChain(void)
{
    DEBUG_LOG("kymeraAdaptiveAnc_CreateEchoCancellerChain");
    kymeraAdaptiveAnc_CreateFbcChain();
    kymeraAdaptiveAnc_CreateSplitterChainInMicRefPath();
}

static void kymeraAdaptiveAnc_DestroyEchoCancellerChain(void)
{
    kymeraAdaptiveAnc_DestroySplitterChainInMicRefPath();
    kymeraAdaptiveAnc_DestroyFbcChain();
}

static void kymeraAdaptiveAnc_ConnectConcurrency(void)
{
    kymeraAdaptiveAnc_ConnectFbcChain();
    kymeraAdaptiveAnc_ConnectSplitterFbcChain();
    kymeraAdaptiveAnc_ActivateMicRefPathSplitterSecondOutput();
}

/*! \brief Enable AANC concurrency graph.
    \param transition - TRUE for concurrency graph enable during transition from standalone to concurrency
                      - FALSE for concurrency graph enable in full enable case
    \return void
*/
static void kymeraAdaptiveAnc_StartConcurrency(bool transition)
{
    if(transition)
    {
        /* Enable Adaptivity during transitional case */
        kymeraAdaptiveAnc_UpdateAdaptivity(ENABLE_ADAPTIVITY);
    }
    else
    {
        /* start AANC chain in case of full enable */
        ChainStart(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
    }
    ChainStart(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
    ChainStart(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
}

static void kymeraAdaptiveAnc_StopConcurrency(void)
{
    if (kymeraAdaptiveAnc_IsAancActive())
    {
        if(kymeraAdaptiveAnc_IsSplitterInMicRefPathCreated())
        {
            ChainStop(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        }
        if(kymeraAdaptiveAnc_IsAancFbcActive())
        {
            ChainStop(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
        }
        /* Disable adaptivity during transition from concurrency to standalone. */
        kymeraAdaptiveAnc_UpdateAdaptivity(DISABLE_ADAPTIVITY);
    }
}

static void kymeraAdaptiveAnc_DisconnectConcurrency(void)
{
    kymeraAdaptiveAnc_DeactivateMicRefPathSplitterSecondOutput();
    kymeraAdaptiveAnc_DisconnectFbcOutputs();
    kymeraAdaptiveAnc_DisconnectMicRefPathSplitterOutputs();
}

/*! \brief Destroy AANC concurrency graph.
    \param transition - TRUE for concurrency graph destroy in transitional case
                      - FALSE for concurrency graph destroy in full disable case
    \return void
*/
static void kymeraAdaptiveAnc_DestroyConcurrency(bool transition)
{
    if (kymeraAdaptiveAnc_IsAancActive())
    {
        kymeraAdaptiveAnc_DestroyEchoCancellerChain();
        if(!transition)
        {
            /* Stop and destroy AANC chain during full disable case*/
            ChainStop(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
            kymeraAdaptiveAnc_DestroyAancChain();
        }
    }
}

static void kymeraAdaptiveAnc_Stop(void)
{
    if(kymeraAdaptiveAnc_IsAancActive())
    {
        if(kymeraAdaptiveAnc_IsAancFbcActive())
        {
            ChainStop(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
            ChainStop(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
        }

        /* Disable adaptivity during transition */
        kymeraAdaptiveAnc_UpdateAdaptivity(DISABLE_ADAPTIVITY);
    }
}

static void kymeraAdaptiveAnc_Restart(void)
{
    if(kymeraAdaptiveAnc_IsAancActive())
    {
        /* Enable Adaptivity during transitional case */
        kymeraAdaptiveAnc_UpdateAdaptivity(ENABLE_ADAPTIVITY);
        if(kymeraAdaptiveAnc_IsAancFbcActive())
        {
            ChainStart(kymeraAdaptiveAnc_GetChain(CHAIN_FBC));
            ChainStart(kymeraAdaptiveAnc_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        }
    }
}

static void kymeraAdaptiveAnc_Disconnect(void)
{
    if(kymeraAdaptiveAnc_IsAancActive())
    {
        if(kymeraAdaptiveAnc_IsAancFbcActive())
        {
            kymeraAdaptiveAnc_DeactivateMicRefPathSplitterSecondOutput();
            kymeraAdaptiveAnc_DisconnectFbcOutputs();
            kymeraAdaptiveAnc_DisconnectMicRefPathSplitterOutputs();
        }
    }
}

static void kymeraAdaptiveAnc_Reconnect(void)
{
    if(kymeraAdaptiveAnc_IsAancActive())
    {
        if(kymeraAdaptiveAnc_IsAancFbcActive())
        {
            kymeraAdaptiveAnc_ConnectFbcChain();
            kymeraAdaptiveAnc_ConnectSplitterFbcChain();
            kymeraAdaptiveAnc_ActivateMicRefPathSplitterSecondOutput();
        }
    }
}

/*! \brief Enable AANC standalone graph.
    \param transition - TRUE for standalone graph enable during transition from concurrency to standalone
                      - FALSE for standalone graph enable in full enable case
    \return void
*/
static void kymeraAdaptiveAnc_EnableStandalone(bool transition)
{
    DEBUG_LOG("KymeraAdaptiveAnc_EnableStandalone");
    appKymeraConfigureDspPowerMode();
    if (!Kymera_MicConnect(mic_user_aanc))
    {
        MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_ANC,
                         NULL, MIC_CONNECT_RETRY_MS);
    }
    else
    {
        if(transition)
        {
            /* Enable Adaptivity during transitional case */
            kymeraAdaptiveAnc_UpdateAdaptivity(ENABLE_ADAPTIVITY);
        }
        else
        {
            /* start AANC chain in case of full enable */
            ChainStart(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
        }

        kymeraAdaptiveAnc_SetKymeraState();
        /* Update optimum DSP clock for AANC usecase */
        appKymeraConfigureDspPowerMode();
    }
}

/*! \brief Destroy AANC concurrency graph.
    \param transition - TRUE for concurrency graph disable in transitional case
                      - FALSE for concurrency graph disable in full disable case
    \return void
*/
static void kymeraAdaptiveAnc_DisableStandalone(bool transition)
{
    DEBUG_LOG("KymeraAdaptiveAnc_DisableStandalone");
    if (kymeraAdaptiveAnc_IsAancActive())
    {
        if(transition)
        {
            /* Disable Adaptivity until concurrency chains are created during transitional case */
            kymeraAdaptiveAnc_UpdateAdaptivity(DISABLE_ADAPTIVITY);
            Kymera_MicDisconnect(mic_user_aanc);
        }
        else
        {
            /* Stop and destroy AANC chain during full disable case*/
            ChainStop(kymeraAdaptiveAnc_GetChain(CHAIN_AANC));
            Kymera_MicDisconnect(mic_user_aanc);
            kymeraAdaptiveAnc_DestroyAancChain();
        }
        kymeraAdaptiveAnc_ResetKymeraState();
        /* Update optimum DSP clock for AANC usecase */
        appKymeraConfigureDspPowerMode();
    }
}

static void kymeraAdaptiveAnc_EnableConcurrency(bool transition)
{
    DEBUG_LOG("KymeraAdaptiveAnc_EnableConcurrency");

    PanicFalse(kymeraAdaptiveAnc_IsAancActive());
    /* Boost DSP clock to turbo */
    appKymeraSetActiveDspClock(AUDIO_DSP_TURBO_CLOCK);

    kymeraAdaptiveAnc_CreateEchoCancellerChain();

    if (!Kymera_MicConnect(mic_user_aanc))
    {
        kymeraAdaptiveAnc_DestroyEchoCancellerChain();
        MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_ANC,
                         NULL, MIC_CONNECT_RETRY_MS);
    }
    else
    {
        kymeraAdaptiveAnc_ConnectConcurrency();
        kymeraAdaptiveAnc_StartConcurrency(transition);
        /* Update optimum DSP clock for AANC usecase */
        kymeraAdaptiveAnc_SetKymeraState();
        appKymeraConfigureDspPowerMode();
    }
}

static void kymeraAdaptiveAnc_DisableConcurrency(bool transition)
{
    DEBUG_LOG("KymeraAdaptiveAnc_DisableConcurrency");
    if (kymeraAdaptiveAnc_IsAancActive())
    {
        kymeraAdaptiveAnc_StopConcurrency();

        kymeraAdaptiveAnc_DisconnectConcurrency();
        Kymera_MicDisconnect(mic_user_aanc);

        kymeraAdaptiveAnc_DestroyConcurrency(transition);
        kymeraAdaptiveAnc_ResetKymeraState();
    }
}

static unsigned kymeraAdaptiveAnc_GentleMuteTimerHelper(void)
{
    /* Converting Timer value to Q12.20 format */
    return (CONVERT_MSEC_TO_SEC(KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER << 20));
}

/************************************General Public Utility functions*****************************************************/
void KymeraAdaptiveAnc_Enable(const KYMERA_INTERNAL_AANC_ENABLE_T* msg)
{
    /* To identify if enable request is from user
       or due to transition between standalone and concurrencies*/
    bool transition = (msg == NULL) ? TRUE : FALSE;

    /* Panic if enable request not from user or transitional case */
    if((msg == NULL) && (!kymeraAdaptiveAnc_IsAancActive()))
        Panic();

    /* Create an AANC operator only when ANC has enabled by the user,
     * otherwise creation would be ignored as AANC operator already created in transitional case */
    if(msg)
    {
        kymeraAdaptiveAnc_CreateAancChain(msg, aanc_usecase_standalone);
    }

    /* Check if speaker path is active */
    if(Kymera_OutputIsChainInUse())
    {
        kymeraAdaptiveAnc_EnableConcurrency(transition);
    }
    else
    {
        kymeraAdaptiveAnc_EnableStandalone(transition);
    }
}

void KymeraAdaptiveAnc_Disable(void)
{
    /* Assuming standalone Adaptive ANC for now */
    kymeraAdaptiveAnc_PanicIfNotActive();

    /* Check if speaker path is active */
    if(Kymera_OutputIsChainInUse())
    {
        kymeraAdaptiveAnc_DisableConcurrency(FALSE);
    }
    else /* Idle or tones/prompts active */
    {
        kymeraAdaptiveAnc_DisableStandalone(FALSE);
    }
}

void KymeraAdaptiveAnc_EnableGentleMute(void)
{
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    if(op)
    {
        DEBUG_LOG("KymeraAdaptiveAnc_EnableGentleMute");
        OperatorsAdaptiveAncSetGentleMuteTimer(op, kymeraAdaptiveAnc_GentleMuteTimerHelper());
        OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_gentle_mute);
    }
}

void KymeraAdaptiveAnc_ApplyModeChange(anc_mode_t new_mode, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel)
{
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    DEBUG_LOG("KymeraAdaptiveAnc_ApplyModeChange for enum:anc_mode_t:%d", new_mode);

    if(op)
    {
        kymeraAdaptiveAnc_SetControlPlantModel(op, feedforward_anc_path, hw_channel);
        kymeraAdaptiveAnc_SetStaticGain(op, feedforward_anc_path, hw_channel);

        if (AncConfig_IsAncModeAdaptive(new_mode))
        {
            OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_full);
            DEBUG_LOG("AANC changes mode to Full Proc");
        }
        else /*Other mode go into Static for now*/
        {
            OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_static);
            DEBUG_LOG("AANC changes mode to Static");
        }
    }
}

void KymeraAdaptiveAnc_SetUcid(anc_mode_t mode)
{
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);

    if(op)
    {
        DEBUG_LOG("KymeraAdaptiveAnc_SetUcid for enum:anc_mode_t:%d", mode);
        OperatorsStandardSetUCID(op, mode); /*Mapping mode to UCID*/
    }
}

void KymeraAdaptiveAnc_UpdateInEarStatus(void)
{
    kymeraAdaptiveAnc_PanicIfNotActive();
    kymeraAdaptiveAnc_UpdateInEarStatus(IN_EAR);
}

void KymeraAdaptiveAnc_UpdateOutOfEarStatus(void)
{
    kymeraAdaptiveAnc_PanicIfNotActive();
    kymeraAdaptiveAnc_UpdateInEarStatus(OUT_OF_EAR);
}

void KymeraAdaptiveAnc_EnableAdaptivity(void)
{
    if(kymeraAdaptiveAnc_IsAancActive())
    {
        DEBUG_LOG("KymeraAdaptiveAnc_EnableAdaptivity");
        kymeraAdaptiveAnc_UpdateAdaptivity(ENABLE_ADAPTIVITY);
    }
}

void KymeraAdaptiveAnc_DisableAdaptivity(void)
{
    if(kymeraAdaptiveAnc_IsAancActive())
    {
        DEBUG_LOG("KymeraAdaptiveAnc_DisableAdaptivity");
        kymeraAdaptiveAnc_PanicIfNotActive();
        kymeraAdaptiveAnc_UpdateAdaptivity(DISABLE_ADAPTIVITY);
    }
}

void KymeraAdaptiveAnc_GetFFGain(uint8* gain)
{
    PanicNull(gain);
    kymeraAdaptiveAnc_PanicIfNotActive();
    kymeraAdaptiveAnc_GetGain(gain);
}

void KymeraAdaptiveAnc_SetGainValues(uint32 mantissa, uint32 exponent)
{
    kymeraAdaptiveAnc_PanicIfNotActive();
    kymeraAdaptiveAnc_SetGainValues(mantissa, exponent);
}

void KymeraAdaptiveAnc_EnableQuietMode(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraAdaptiveAnc_EnableQuietMode");
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    if(op)
    {
        OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_quiet);
    }
}

void KymeraAdaptiveAnc_DisableQuietMode(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraAdaptiveAnc_DisableQuietMode");
    Operator op = ChainGetOperatorByRole(kymeraAdaptiveAnc_GetChain(CHAIN_AANC), OPR_AANC);
    if(op)
    {
        OperatorsAdaptiveAncSetModeOverrideCtrl(op, adaptive_anc_mode_full);
    }
}

bool KymeraAdaptiveAnc_ObtainCurrentAancMode(adaptive_anc_mode_t *aanc_mode)
{
    PanicNull(aanc_mode);
    kymeraAdaptiveAnc_PanicIfNotActive();
    kymeraAdaptiveAnc_GetCurrentMode(aanc_mode);
    return TRUE;
}

bool KymeraAdaptiveAnc_IsNoiseLevelBelowQmThreshold(void)
{
    kymeraAdaptiveAnc_PanicIfNotActive();
    return kymeraAdaptiveAnc_IsNoiseLevelBelowQuietModeThreshold();
}

/**************** Utility functions for Adaptive ANC Tuning ********************************/
static audio_anc_path_id kymeraAdaptiveAnc_GetAncPath(void)
{
    audio_anc_path_id path=AUDIO_ANC_PATH_ID_NONE;

    /* Since Adaptive ANC is only supported on earbud application for now, checking for just 'left only' configurations*/
    if ((appConfigAncPathEnable() == feed_forward_mode_left_only) || (appConfigAncPathEnable() == feed_back_mode_left_only))
    {
        path = AUDIO_ANC_PATH_ID_FFA;
    }
    else if (appConfigAncPathEnable() == hybrid_mode_left_only)
    {
        path = AUDIO_ANC_PATH_ID_FFB;
    }
    else
    {
        DEBUG_LOG_ERROR("Adaptive ANC is supported only on left_only configurations");
    }

    return path;
}

static void kymeraAdaptiveAnc_GetMics(microphone_number_t *mic0, microphone_number_t *mic1)
{
    *mic0 = microphone_none;
    *mic1 = microphone_none;

    if (appConfigAncPathEnable() & hybrid_mode_left_only)
    {
        *mic0 = appConfigAncFeedForwardMic();
        *mic1 = appConfigAncFeedBackMic();
    }
}

static void kymeraAdaptiveAnc_TuningCreateOperators(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    const char aanc_tuning_edkcs[] = "download_aanc.edkcs";
    FILE_INDEX index = FileFind(FILE_ROOT, aanc_tuning_edkcs, strlen(aanc_tuning_edkcs));
    PanicFalse(index != FILE_NONE);
    theKymera->aanc_tuning_bundle_id = PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */

#ifdef DOWNLOAD_USB_AUDIO
    const char usb_audio_edkcs[] = "download_usb_audio.edkcs";
    index = FileFind(FILE_ROOT, usb_audio_edkcs, strlen(usb_audio_edkcs));
    PanicFalse(index != FILE_NONE);
    theKymera->usb_audio_bundle_id = PanicZero (OperatorBundleLoad(index, 0)); /* 0 is processor ID */
#endif

    /* Create usb rx operator */
    theKymera->usb_rx = (Operator)(VmalOperatorCreate(EB_CAP_ID_USB_AUDIO_RX));
    PanicZero(theKymera->usb_rx);

    /* Create AANC tuning operator */
    theKymera->aanc_tuning = (Operator)(VmalOperatorCreate(CAP_ID_DOWNLOAD_AANC_TUNING));
    PanicZero(theKymera->aanc_tuning);

    /* Create usb rx operator */
    theKymera->usb_tx = (Operator)(VmalOperatorCreate(EB_CAP_ID_USB_AUDIO_TX));
    PanicZero(theKymera->usb_tx);

}

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
static void kymeraAdaptiveAnc_TuningConfigureOperators(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *aanc_tuning)
#else
static void kymeraAdaptiveAnc_TuningConfigureOperators(uint16 usb_rate)
#endif
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    /* configurations for usb_rx operator */
    uint16 usb_rx_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,                              // data_format
        aanc_tuning->usb_rate / 25,     // sample_rate
        aanc_tuning->spkr_channels,     // number_of_channels /* Mono audio will be sent to AANC capability */
        aanc_tuning->frame_size * 8,    // subframe_size
        aanc_tuning->frame_size * 8,    // subframe_resolution
    };

    /* configurations for usb_tx operator */
    uint16 usb_tx_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,                              // data_format
        aanc_tuning->usb_rate / 25,     // sample_rate
        aanc_tuning->mic_channels,      // number_of_channels
        aanc_tuning->frame_size * 8,    // subframe_size
        aanc_tuning->frame_size * 8,    // subframe_resolution
    };
#else
    /* configurations for usb_rx operator */
    uint16 usb_rx_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,              // data_format
        usb_rate / 25,  // sample_rate
        1,              // number_of_channels /* Mono audio will be sent to AANC capability */
        16,             // subframe_size
        16,             // subframe_resolution
    };

    /* configurations for usb_tx operator */
    uint16 usb_tx_config[] =
    {
        OPMSG_USB_AUDIO_ID_SET_CONNECTION_CONFIG,
        0,              // data_format
        usb_rate / 25,  // sample_rate
        2,              // number_of_channels
        16,             // subframe_size
        16,             // subframe_resolution
    };
#endif
    KYMERA_INTERNAL_AANC_ENABLE_T* param = PanicUnlessMalloc(sizeof( KYMERA_INTERNAL_AANC_ENABLE_T));

    /*Even though device needs to be incase to perform AANC tuning, in_ear param needs to be set to TRUE as AANC capability
      runs in full processing mode only when the device is in ear */
    param->in_ear = TRUE;
    param->control_path = kymeraAdaptiveAnc_GetAncPath();
    param->hw_channel = adaptive_anc_hw_channel_0;
    param->current_mode = anc_mode_1;

    /* Configure usb rx operator */
    PanicFalse(VmalOperatorMessage(theKymera->usb_rx,
                                   usb_rx_config, SIZEOF_OPERATOR_MESSAGE(usb_rx_config),
                                   NULL, 0));

    /* Configure AANC tuning operator */
    kymeraAdaptiveAnc_ConfigureAancChain(theKymera->aanc_tuning, param, aanc_usecase_default);

    /* Configure usb tx operator */
    PanicFalse(VmalOperatorMessage(theKymera->usb_tx,
                                   usb_tx_config, SIZEOF_OPERATOR_MESSAGE(usb_tx_config),
                                   NULL, 0));

    if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureAdaptiveAncTuningChain)
    {
        KymeraGetTaskData()->chain_config_callbacks->ConfigureAdaptiveAncTuningChain(NULL);
    }

    free(param);
}

static void kymeraAdaptiveAnc_ConnectMicsAndDacToTuningOperator(Source ext_mic, Source int_mic, Sink DAC)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Connect microphones */
    PanicFalse(StreamConnect(ext_mic,
                             StreamSinkFromOperatorTerminal(theKymera->aanc_tuning, AANC_TUNING_SINK_EXT_MIC)));

    PanicFalse(StreamConnect(int_mic,
                             StreamSinkFromOperatorTerminal(theKymera->aanc_tuning, AANC_TUNING_SINK_INT_MIC)));

    /* Connect DAC */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->aanc_tuning, AANC_TUNING_SOURCE_DAC),
                             DAC));
}

static void kymeraAdaptiveAnc_ConnectUsbRxAndTxOperatorsToTuningOperator(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Connect backend (USB) to AANC operator */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_rx, 0),
                             StreamSinkFromOperatorTerminal(theKymera->aanc_tuning, AANC_TUNING_SINK_USB)));


    /* Forwards external mic data to USb Tx */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->aanc_tuning, AANC_TUNING_SOURCE_EXT_MIC),
                             StreamSinkFromOperatorTerminal(theKymera->usb_tx, 0)));

    /* Forwards internal mic data to USb Tx */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->aanc_tuning, AANC_TUNING_SOURCE_INT_MIC),
                             StreamSinkFromOperatorTerminal(theKymera->usb_tx, 1)));
}

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
static void kymeraAdaptiveAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *aanc_tuning)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Connect USB ISO in endpoint to USB Rx operator */
    PanicFalse(StreamConnect(aanc_tuning->spkr_src,
                             StreamSinkFromOperatorTerminal(theKymera->usb_rx, 0)));

    /* Connect USB Tx operator to USB ISO out endpoint */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_tx, 0),
                             aanc_tuning->mic_sink));
}
#else
static void kymeraAdaptiveAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Connect USB ISO in endpoint to USB Rx operator */
    PanicFalse(StreamConnect(StreamUsbEndPointSource(end_point_iso_in),
                             StreamSinkFromOperatorTerminal(theKymera->usb_rx, 0)));

    /* Connect USB Tx operator to USB ISO out endpoint */
    PanicFalse(StreamConnect(StreamSourceFromOperatorTerminal(theKymera->usb_tx, 0),
                             StreamUsbEndPointSink(end_point_iso_out)));
}
#endif
/**************** Interface functions for Adaptive ANC Tuning ********************************/

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
void kymeraAdaptiveAnc_EnterAdaptiveAncTuning(const adaptive_anc_tuning_connect_parameters_t *param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("kymeraAdaptiveAnc_EnterAdaptiveAncTuning");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START);
    message->usb_rate = param->usb_rate;
    message->spkr_src = param->spkr_src;
    message->mic_sink = param->mic_sink;
    message->spkr_channels = param->spkr_channels;
    message->mic_channels = param->mic_channels;
    message->frame_size = param->frame_size;
    if(theKymera->busy_lock)
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START, message, &theKymera->busy_lock);
    }
    else if(theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START, message, AANC_TUNNING_START_DELAY);
    }
    else
    {
       MessageSend(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START, message);
    }
}
#else
void kymeraAdaptiveAnc_EnterAdaptiveAncTuning(const adaptive_anc_tuning_connect_parameters_t *param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("kymeraAdaptiveAnc_EnterAdaptiveAncTuning");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START);
    message->usb_rate = param->usb_rate;
    if(theKymera->busy_lock)
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START, message, &theKymera->busy_lock);
    }
    else if(theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START, message, AANC_TUNNING_START_DELAY);
    }
    else
    {
       MessageSend(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START, message);
    }
}
#endif

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
void kymeraAdaptiveAnc_ExitAdaptiveAncTuning(const adaptive_anc_tuning_disconnect_parameters_t *param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("kymeraAdaptiveAnc_ExitAdaptiveAncTuning");
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP);
    message->spkr_src = param->spkr_src;
    message->mic_sink = param->mic_sink;
    message->kymera_stopped_handler = param->kymera_stopped_handler;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP, message);
}
#else
void kymeraAdaptiveAnc_ExitAdaptiveAncTuning(const adaptive_anc_tuning_disconnect_parameters_t *param)
{
    PanicNotNull(param);
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_FN_ENTRY("kymeraAdaptiveAnc_ExitAdaptiveAncTuning");
    MessageSend(&theKymera->task, KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP, NULL);
}
#endif

void kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint32 usb_rate = msg->usb_rate;
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    PanicFalse(msg->spkr_channels == AANC_USB_AUDIO_SPKR_CHANNELS);
    PanicFalse(msg->mic_channels == AANC_USB_AUDIO_MIC_CHANNELS);
#endif
    DEBUG_LOG_FN_ENTRY("kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain usb_rate: %d", usb_rate);

    theKymera->usb_rate = usb_rate;

    PanicFalse(usb_rate == AANC_SAMPLE_RATE);

    /* Turn on audio subsystem */
    OperatorFrameworkEnable(1);

    /* Move to ANC tuning state, this prevents A2DP and HFP from using kymera */
    appKymeraSetState(KYMERA_STATE_ANC_TUNING);

    appKymeraConfigureDspPowerMode();

    microphone_number_t mic0, mic1;
    kymeraAdaptiveAnc_GetMics(&mic0, &mic1);

    Source ext_mic = Kymera_GetMicrophoneSource(mic0, NULL, theKymera->usb_rate, high_priority_user);
    Source int_mic = Kymera_GetMicrophoneSource(mic1, NULL, theKymera->usb_rate, high_priority_user);

    PanicFalse(SourceSynchronise(ext_mic,int_mic));

    /* Get the DAC output sink */
    Sink DAC = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    PanicFalse(SinkConfigure(DAC, STREAM_CODEC_OUTPUT_RATE, usb_rate));

    /* Create usb_rx, aanc_tuning, usb_tx operators */
    kymeraAdaptiveAnc_TuningCreateOperators();

    /* Configure usb_rx, aanc_tuning, usb_tx operators */
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    kymeraAdaptiveAnc_TuningConfigureOperators(msg);
#else
    kymeraAdaptiveAnc_TuningConfigureOperators(usb_rate);
#endif

    /* Connect microphones and DAC to tuning operator */
    kymeraAdaptiveAnc_ConnectMicsAndDacToTuningOperator(ext_mic, int_mic, DAC);

    kymeraAdaptiveAnc_ConnectUsbRxAndTxOperatorsToTuningOperator();

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    kymeraAdaptiveAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints(msg);
#else
    kymeraAdaptiveAnc_ConnectUsbRxAndTxOperatorsToUsbEndpoints();
#endif


    /* Start the operators */
    Operator op_list[] = {theKymera->usb_rx, theKymera->aanc_tuning, theKymera->usb_tx};
    PanicFalse(OperatorStartMultiple(3, op_list, NULL));

    /* Ensure audio amp is on */
    appKymeraExternalAmpControl(TRUE);

    /* Set kymera lock to prevent anything else using kymera */
    appKymeraSetAdaptiveAncStartingLock(theKymera);
}

void kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T *msg)
{
    DEBUG_LOG_FN_ENTRY("kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain");
#ifndef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    PanicNotNull(msg);
#endif
    if(appKymeraGetState() == KYMERA_STATE_ANC_TUNING)
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        /* Turn audio amp is off */
        appKymeraExternalAmpControl(FALSE);

        /* Stop the operators */
        Operator op_list[] = {theKymera->usb_rx, theKymera->aanc_tuning, theKymera->usb_tx};
        PanicFalse(OperatorStopMultiple(3, op_list, NULL));

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
        /* Disconnect USB ISO in endpoint */
        StreamDisconnect(msg->spkr_src, 0);

        /* Disconnect USB ISO out endpoint */
        StreamDisconnect(0, msg->mic_sink);
#else
        /* Disconnect USB ISO in endpoint */
        StreamDisconnect(StreamUsbEndPointSource(end_point_iso_in), 0);

        /* Disconnect USB ISO out endpoint */
        StreamDisconnect(0, StreamUsbEndPointSink(end_point_iso_out));
#endif
        /* Get the DAC output sink */
        Sink DAC = (Sink)PanicFalse(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));

        microphone_number_t mic0, mic1;
        kymeraAdaptiveAnc_GetMics(&mic0, &mic1);
        Source ext_mic = Microphones_GetMicrophoneSource(mic0);
        Source int_mic = Microphones_GetMicrophoneSource(mic1);

        StreamDisconnect(ext_mic, 0);
        Kymera_CloseMicrophone(mic0, high_priority_user);
        StreamDisconnect(int_mic, 0);
        Kymera_CloseMicrophone(mic1, high_priority_user);

        /* Disconnect speaker */
        StreamDisconnect(0, DAC);

        /* Distroy operators */
        OperatorsDestroy(op_list, 3);

        /* Unload bundle */
        PanicFalse(OperatorBundleUnload(theKymera->aanc_tuning_bundle_id));
    #ifdef DOWNLOAD_USB_AUDIO
        PanicFalse(OperatorBundleUnload(theKymera->usb_audio_bundle_id));
    #endif

        /* Clear kymera lock and go back to idle state to allow other uses of kymera */
        appKymeraClearAdaptiveAncStartingLock(theKymera);
        appKymeraSetState(KYMERA_STATE_IDLE);

        /* Reset DSP clock to default*/
        appKymeraConfigureDspPowerMode();

        /* Turn off audio subsystem */
        OperatorFrameworkEnable(0);
    }
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    PanicZero(msg->kymera_stopped_handler);
    msg->kymera_stopped_handler(msg->spkr_src);
#endif
}

#endif /* ENABLE_ADAPTIVE_ANC */
