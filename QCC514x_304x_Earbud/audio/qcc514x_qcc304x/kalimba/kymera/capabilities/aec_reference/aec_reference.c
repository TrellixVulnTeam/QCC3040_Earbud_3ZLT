/**
 * Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.
 * \file  aec_reference.c
 * \ingroup  capabilities
 *
 *  AEC Reference
 *
 */

/****************************************************************************
Include Files
*/
#include "aec_reference_cap_c.h"

#include "mem_utils/shared_memory_ids.h"
#include "../lib/audio_proc/iir_resamplev2_util.h"
#include "aec_reference_config.h"
#include "capabilities.h"

/****************************************************************************
Public Type Declarations
*/

#ifdef CAPABILITY_DOWNLOAD_BUILD
#define AEC_REFERENCE_CAP_ID CAP_ID_DOWNLOAD_AEC_REFERENCE
#else
#define AEC_REFERENCE_CAP_ID CAP_ID_AEC_REFERENCE
#endif

/****************************************************************************
Private Constant Definitions
*/

/* Macro to enable debug logs for creating
 * graphs.
 * Note: this shouldn't be enabled permanently
 */
#define DEBUG_GRAPHS_BUILDx
#ifdef DEBUG_GRAPHS_BUILD
#define DEBUG_GRAPHS(x) L2_DBG_MSG1(x " - time = 0x%08x", time_get_time())
#else
#define DEBUG_GRAPHS(x) ((void)0)
#endif

/* Message handlers */

/** The aec_reference capability function handler table */
const handler_lookup_struct aec_reference_handler_table =
{
    aec_reference_create,          /* OPCMD_CREATE */
    aec_reference_destroy,         /* OPCMD_DESTROY */
    aec_reference_start,           /* OPCMD_START */
    aec_reference_stop_reset,      /* OPCMD_STOP */
    aec_reference_stop_reset,      /* OPCMD_RESET */
    aec_reference_connect,         /* OPCMD_CONNECT */
    aec_reference_disconnect,      /* OPCMD_DISCONNECT */
    aec_reference_buffer_details,  /* OPCMD_BUFFER_DETAILS */
    aec_reference_get_data_format, /* OPCMD_DATA_FORMAT */
    aec_reference_get_sched_info   /* OPCMD_GET_SCHED_INFO */
};

/* Null terminated operator message handler table */

const opmsg_handler_lookup_table_entry aec_reference_opmsg_handler_table[] =
{{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION,           base_op_opmsg_get_capability_version},

 {OPMSG_COMMON_ID_SET_CONTROL,                       aec_reference_opmsg_obpm_set_control},
 {OPMSG_COMMON_ID_GET_PARAMS,                        aec_reference_opmsg_obpm_get_params},
 {OPMSG_COMMON_ID_GET_DEFAULTS,                      aec_reference_opmsg_obpm_get_defaults},
 {OPMSG_COMMON_ID_SET_PARAMS,                        aec_reference_opmsg_obpm_set_params},
 {OPMSG_COMMON_ID_GET_STATUS,                        aec_reference_opmsg_obpm_get_status},

 {OPMSG_COMMON_GET_CONFIGURATION,                    aec_reference_opmsg_ep_get_config},
 {OPMSG_COMMON_CONFIGURE,                            aec_reference_opmsg_ep_configure},
 {OPMSG_COMMON_GET_CLOCK_ID,                         aec_reference_opmsg_ep_clock_id},

 {OPMSG_AEC_REFERENCE_ID_SET_SAMPLE_RATES,           aec_reference_set_rates},
 {OPMSG_AEC_REFERENCE_ID_SET_INPUT_OUTPUT_SAMPLE_RATES, aec_reference_set_input_output_rates},

 {OPMSG_COMMON_ID_SET_UCID,                          aec_reference_opmsg_set_ucid},
 {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,                 aec_reference_opmsg_get_ps_id},

 {OPMSG_AEC_REFERENCE_ID_MUTE_MIC_OUTPUT,            aec_reference_opmsg_mute_mic_output},
#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
 {OPMSG_COMMON_SET_TTP_LATENCY,                      aec_reference_opmsg_set_ttp_latency},
 {OPMSG_COMMON_SET_LATENCY_LIMITS,                   aec_reference_opmsg_set_latency_limits},
 {OPMSG_COMMON_SET_TTP_PARAMS,                       aec_reference_opmsg_set_ttp_params},
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
 {OPMSG_AEC_REFERENCE_ID_SAME_INPUT_OUTPUT_CLK_SOURCE, aec_reference_opmsg_enable_mic_sync},
 {OPMSG_COMMON_ID_SET_TERMINAL_BUFFER_SIZE, aec_reference_opmsg_set_buffer_size},
 {OPMSG_AEC_REFERENCE_ID_SET_TASK_PERIOD, aec_reference_opmsg_set_task_period},
 {OPMSG_AEC_REFERENCE_ID_SET_OUTPUT_BLOCK_SIZE, aec_reference_opmsg_set_output_block_size},

 {0, NULL}};


/* Supports up to 8 MICs, 2 SPKRS, and AEC Reference */

const CAPABILITY_DATA aec_reference_cap_data =
{
    AEC_REFERENCE_CAP_ID,              /* Capability ID */
    AEC_REFERENCE_AECREF_VERSION_MAJOR, AEC_REFERENCE_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
    AEC_REF_NUM_SINK_TERMINALS, AEC_REF_NUM_SOURCE_TERMINALS,            /* Max number of sinks/inputs and sources/outputs */
    &aec_reference_handler_table,       /* Pointer to message handler function table */
    aec_reference_opmsg_handler_table,  /* Pointer to operator message handler function table */
    base_op_process_data,               /* Pointer to data processing function */
    0,                                  /* Reserved */
    sizeof(AEC_REFERENCE_OP_DATA)       /* Size of capability-specific per-instance data */
};
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_AEC_REFERENCE, AEC_REFERENCE_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_AEC_REFERENCE, AEC_REFERENCE_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

/****************************************************************************
Private Function Declarations
*/
#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
static void map_hl_ui(AEC_REFERENCE_PARAMETERS *params_ptr, HL_LIMITER_UI *hl_ui);
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

static inline AEC_REFERENCE_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (AEC_REFERENCE_OP_DATA *) base_op_get_instance_data(op_data);
}

/****************************************************************************
Public Function Declarations
*/

#define SIDETONE_ENABLE_FLAG     0x02
#define SIDETONE_MIC_SPKR_FLAG   0x01
#define USE_SIDETONE_FLAG        0x03

/* ********************************** API functions ************************************* */

bool aec_reference_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    /* Setup Response to Creation Request.   Assume Failure*/
    if(!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data))
    {
        return(FALSE);
    }

    /* AEC REFERENCE runs on periodic timer task and doesn't
     * need to receive usual operator kicks, neither from source
     * side nor from sink side.
     */
    base_op_stop_kicks(op_data, BOTH_SIDES);

    /* Initialize extended data for operator.  Assume intialized to zero*/
    op_extra_data->cap_id = base_op_get_cap_id(op_data);
    op_extra_data->ReInitFlag = TRUE;
    op_extra_data->Cur_mode = AEC_REFERENCE_SYSMODE_FULL;
    op_extra_data->kick_id = TIMER_ID_INVALID;

    /* statically check default task period */
    COMPILE_TIME_ASSERT(((AEC_REFERENCE_DEFAULT_TASK_PERIOD <= AEC_REFERENCE_MAX_TASK_PERIOD) &&
                         (AEC_REFERENCE_DEFAULT_TASK_PERIOD >= AEC_REFERENCE_MIN_TASK_PERIOD) &&
                         ((SECOND%AEC_REFERENCE_DEFAULT_TASK_PERIOD)==0)),
                        AEC_REFERENCE_DEFAULT_TASK_PERIOD_Not_Accepted);

    /* set default task period */
    if(!aec_reference_set_task_period(op_extra_data, AEC_REFERENCE_DEFAULT_TASK_PERIOD, 1))
    {
        /* failed to set the default task period */
        goto aFailed;
    }


#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* set minimum tag length for mic output metadata tags */
    op_extra_data->mic_metadata_min_tag_len = AEC_REFERENCE_MIC_METADATA_MIN_TAG_LEN;

    /* create time to play with default params */
    op_extra_data->mic_time_to_play = ttp_init();
    if (op_extra_data->mic_time_to_play != NULL)
    {
        ttp_params params;
        ttp_get_default_params(&params, TTP_TYPE_PCM);
        ttp_configure_params(op_extra_data->mic_time_to_play, &params);
    }
    else
    {
        goto aFailed;
    }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

    /* Task Period as fraction - this field is currently a fixed constant,
       but is not declared as such because we may want it to be configurable in the future */

    /* For Atlas this must be less than for equal to the ping/pong period.
       Also set AEC_REFERENCE_TIME_PERIOD
    */
    op_extra_data->mic_rate_ability  = RATEMATCHING_SUPPORT_NONE;
    op_extra_data->spkr_rate_ability = RATEMATCHING_SUPPORT_NONE;
    op_extra_data->mic_shift  = AEC_REFERENCE_DEFAULT_EP_SHIFT;
    op_extra_data->spkr_shift = -AEC_REFERENCE_DEFAULT_EP_SHIFT;

    /* Note:  sample rate config must be sent before the operator's terminals may be connected
       input_rate and output_rate are initialized to zero and checked in the connect operation */

    /*allocate the volume control shared memory */
    op_extra_data->shared_volume_ptr = allocate_shared_volume_cntrl();
    if(!op_extra_data->shared_volume_ptr)
    {
        goto aFailed;
    }

    if(!cpsInitParameters(&op_extra_data->parms_def,(unsigned*)AEC_REFERENCE_GetDefaults(op_extra_data->cap_id),(unsigned*)&op_extra_data->params,sizeof(AEC_REFERENCE_PARAMETERS)))
    {
        goto aFailed;
    }

#if defined(IO_DEBUG)
    op_extra_data->aec_latency_ptr = &op_extra_data->sync_block;
#endif

    /* chance to fix up */
    patch_fn_shared(aec_reference);

    /* We don't have a new constant table to add - only register our interest in the IIR RESAMPLER constant tables. */
    iir_resamplerv2_add_config_to_list(NULL);

    base_op_change_response_status(response_data,STATUS_OK);
    return TRUE;
  aFailed:
    if(op_extra_data->shared_volume_ptr)
    {
        release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
        op_extra_data->shared_volume_ptr = NULL;
    }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* free it if we created time to play context for mic */
    if (op_extra_data->mic_time_to_play != NULL)
    {
        ttp_free(op_extra_data->mic_time_to_play);
        op_extra_data->mic_time_to_play = NULL;
    }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

    base_op_change_response_status(response_data, STATUS_CMD_FAILED);
    return TRUE;
}


void aec_reference_set_mic_gains(OPERATOR_DATA *op_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    unsigned i, *lpadcgains = (unsigned*)&op_extra_data->params.OFFSET_ADC_GAIN1;

    patch_fn_shared(aec_reference);

    for(i=0;i<MAX_NUMBER_MICS;i++)
    {
        if(op_extra_data->input_stream[MicrophoneTerminalByIndex(i)])
        {
            uint32 config_value = (uint32)(lpadcgains[i]);
            opmgr_override_set_ep_gain(opmgr_override_get_endpoint(op_data,
                                       MicrophoneTerminalByIndex(i) | TERMINAL_SINK_MASK),
                                       config_value);
        }
    }
}

/**
 * aec_reference_cleanup
 * \brief clean up the aec-reference operator internal states
 *
 * \param op_data Pointer to the AEC reference operator data.
 *
 * Note: This function is the same as aec_reference_cleanup_graphs
 *       except that it will reset the entire channel status so any new attempt
 *       to build the graphs will rebuild everything from scratch.
 */
void aec_reference_cleanup(OPERATOR_DATA *op_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    /* cleanup all the graphs */
    aec_reference_cleanup_graphs(op_extra_data);

    /* reset channel status, so any new trying
     * of building graphs will rebuild every thing
     * from scratch
     */
    op_extra_data->channel_status = 0;
}

bool aec_reference_destroy(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    /* Setup Response to Destroy Request.*/
    if(!base_op_destroy(op_data, message_data, response_id, response_data))
    {
        return(FALSE);
    }

    /* Make sure everything is cleared */
    aec_reference_cleanup(op_data);

    /* calling the "destroy" assembly function - this frees up all the capability-internal memory */
    /*free volume control shared memory*/
    release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
    op_extra_data->shared_volume_ptr = NULL;

    /* delete the configuration list */
    iir_resamplerv2_delete_config_list();

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* delete mic time-to-play object */
    if(op_extra_data->mic_time_to_play != NULL)
    {
        ttp_free(op_extra_data->mic_time_to_play);
        op_extra_data->mic_time_to_play = NULL;
    }
#endif

    base_op_change_response_status(response_data,STATUS_OK);
    return(TRUE);
}

#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
/**
 * aec_reference_cleanup_sidetone_graph
 * \brief clean up sidetone graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_sidetone_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
    /* mic_howling_limiter_op, no longer is valid` */
    op_extra_data->mic_howling_limiter_op = NULL;
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

    /* mic_sidetone_op, no longer is valid` */
    op_extra_data->mic_sidetone_op = NULL;

    /* Free cbops sidetone graph */
    if(op_extra_data->sidetone_graph != NULL)
    {
        destroy_graph(op_extra_data->sidetone_graph);
        op_extra_data->sidetone_graph = NULL;
    }

    /* Free Sidetone buffers */
    if(op_extra_data->sidetone_buf != NULL)
    {
        cbuffer_destroy(op_extra_data->sidetone_buf);
        op_extra_data->sidetone_buf = NULL;
    }

    /* free cbuffer structure for clone mic buff */
    if(op_extra_data->sidetone_mic_buf != NULL)
    {
        cbuffer_destroy_struct(op_extra_data->sidetone_mic_buf);
        op_extra_data->sidetone_mic_buf = NULL;
    }

    DEBUG_GRAPHS("AEC REFERENCE: SIDETONE graph's cleanup done!" );
}

/**
 * build_sidetone_graph
 * \brief updates speaker graph to include/exclude side tone mixing
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool build_sidetone_graph(AEC_REFERENCE_OP_DATA* op_extra_data)
{
    tCbuffer *mic_buf = op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1];
    unsigned* idxs;
    unsigned num_io = 2;
    cbops_graph *sidetone_graph;
    cbops_op *op_ptr;
    cbops_op *override_op_ptr;
    unsigned st_mic_idx = 0;          /* buffer index for mic input */
    unsigned st_filter_out_idx = 1;   /* buffer index for sidetone filter output */
    unsigned resampler_out_idx = 0;   /* buffer index for output of possible resampler */
    unsigned sidetone_idx = st_filter_out_idx; /* buffer index for sidetone buffer */
    unsigned sidetone_buf_size;
    unsigned spkr_threshold = frac_mult(op_extra_data->spkr_rate, op_extra_data->kick_period_frac)+1;
    unsigned safety_threshold;
    unsigned spkr_channel_status = GetSpkrChannelStatus(op_extra_data);
    unsigned num_sidetone_spkrs = 1;

    /* --------------------------------------------------------------------------------------------------
     *
     *  MIC_BUFFER -> Sidetone filter -> DC_RM -> resampler -> latency-control -> mix to SPKR_BUFFER
     *
     *  Note1: DC RM might not be necessary
     *  Note2: Rate matching done via latency control only (no sra)
     * --------------------------------------------------------------------------------------------------*/

    patch_fn_shared(aec_reference);

    /* destroy cbops graph if already running */
    if(NULL != op_extra_data->sidetone_graph)
    {
        aec_reference_cleanup_sidetone_graph(op_extra_data);
    }

    /* see if we need to setup a separate graph for sidetone mixing */
    if(op_extra_data->sidetone_method != AEC_SIDETONE_IN_SW_USING_SEPARATE_GRAPH)
    {
        return TRUE;
    }

    DEBUG_GRAPHS("AEC_REFERENCE: Building sidetone graph ...");

    /* Determine number of speakers to have sidetone */
    if((spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA) == 0)
    {
        if((spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_MIX) == 0)
        {
            /* mono to stereo, mix to both */
            num_sidetone_spkrs = 2;
        }
    }

    /* limit to available speakers */
    num_sidetone_spkrs = MIN(num_sidetone_spkrs, op_extra_data->num_spkr_channels);

    /* we don't expect Sidetone buffer already existing at this point */
    PL_ASSERT(op_extra_data->sidetone_buf == NULL);
    PL_ASSERT(op_extra_data->sidetone_mic_buf == NULL);

    /* Allocate Buffer for sidetone samples,
     * size = sidetone_task_period + 1ms for latency control
     */
    sidetone_buf_size = frac_mult(op_extra_data->spkr_rate,
                                  op_extra_data->kick_period_frac +
                                  FRACTIONAL(0.001));

    op_extra_data->sidetone_buf = cbuffer_create_with_malloc_fast(sidetone_buf_size, BUF_DESC_SW_BUFFER);
    if(!op_extra_data->sidetone_buf)
    {
        /* Not going ahead with creating sidetone graph if we
         * cannot create shared buffer for sidetone path.
         */
        return FALSE;
    }

    /* create clone cbuffers for mic buffer */
    op_extra_data->sidetone_mic_buf = cbuffer_create(mic_buf->base_addr,
                                                     cbuffer_get_size_in_words(mic_buf),
                                                     BUF_DESC_SW_BUFFER);
    if(op_extra_data->sidetone_mic_buf == NULL)
    {
        return FALSE;
    }


    /* do we need resampler for side tone generation */
    if(op_extra_data->spkr_rate != op_extra_data->mic_rate)
    {
        /* Extra buffer needed between sidetone filter and resampler,
         * as resampler can't work in-place
         */
        num_io++;
        resampler_out_idx = st_filter_out_idx + 1;
        sidetone_idx++;
    }

    /* create indexes for cbops buffers,
     * this needs to be deleted before leaving
     * this function.
     */
    idxs = create_default_indexes(num_io);
    if(idxs == NULL)
    {
        return(FALSE);
    }

    /* Allocate sidetone graph */
    sidetone_graph = cbops_alloc_graph(num_io);
    if(!sidetone_graph)
    {
        goto aFailed;
    }
    op_extra_data->sidetone_graph = sidetone_graph;


    /* set mic input buffer */
    cbops_set_input_io_buffer(sidetone_graph,
                              st_mic_idx,
                              st_mic_idx,
                              op_extra_data->sidetone_mic_buf);

    /* set sidetone output buffer, (will then be mixed
     * into speakers by override operator)
     */
    cbops_set_output_io_buffer(sidetone_graph,
                               sidetone_idx,
                               sidetone_idx,
                               op_extra_data->sidetone_buf);

    /*
      Add Sidetone operators:
      Sidetone Apply Operator
      Note:  Sidetone is before resampler.
      Better solution is to place it at lowest sample rate
    */

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER

    {
        HL_LIMITER_UI hl_ui;

        /* Map the Howling limiter user interface parameters */
        map_hl_ui(&op_extra_data->params, &hl_ui);

        op_ptr = create_howling_limiter_op(
            st_mic_idx,
            op_extra_data->mic_rate,
            &hl_ui);
        op_extra_data->mic_howling_limiter_op = op_ptr;
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(sidetone_graph, op_ptr);
    }
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

    op_ptr = create_sidetone_filter_op(st_mic_idx, st_filter_out_idx, 3,
                                       (cbops_sidetone_params*)&op_extra_data->params.OFFSET_ST_CLIP_POINT,
                                       (void*)&op_extra_data->params.OFFSET_ST_PEQ_CONFIG);
    op_extra_data->mic_sidetone_op = op_ptr;
    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(sidetone_graph,op_ptr);

    /* DC remove on sidetone */
    op_ptr = create_dc_remove_op(1, &idxs[st_filter_out_idx], &idxs[st_filter_out_idx]);
    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(sidetone_graph, op_ptr);

    /* see if we need resampler in sidetone path */
    if(resampler_out_idx != 0)
    {
        /* sidetone filter will write into scratch buff */
        cbops_set_internal_io_buffer(sidetone_graph,
                                     st_filter_out_idx,
                                     st_filter_out_idx,
                                     op_extra_data->scratch_bufs[0]);

        /* create resampler only for one in & out channel */
        op_ptr = create_iir_resamplerv2_op(1,
                                           &idxs[st_filter_out_idx],
                                           &idxs[resampler_out_idx],
                                           op_extra_data->mic_rate,
                                           op_extra_data->spkr_rate,
                                           op_extra_data->resampler_temp_buffer_size,
                                           op_extra_data->resampler_temp_buffer, 0, 0, 0);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(sidetone_graph,op_ptr);
    }

    /* Add in disgard on sidetone */
    op_ptr = create_sink_overflow_disgard_op(1,                   /* number of channels */
                                             &idxs[sidetone_idx], /* buffer indexes */
                                             /* Minimum space needed in buffer at the beginning of
                                              * process, if not enough space this op will discard some
                                              * samples to free space for new incoming mic sapmples */
                                             spkr_threshold);
#if defined(IO_DEBUG)
    op_extra_data->st_disgard_op = op_ptr;
#endif

    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(sidetone_graph,op_ptr);

    /* The sidetone graph only prepares sidetone samples (into
     * side_tone buffer), actual mixing is done by the override
     * operator where it reads sidetone samples and mixes them
     * directly into speaker MMU buffer(s).
     */

    /* safety_threshold, this is a small safety zone to make sure
     * sidetone mixing is ahead of reading by HW, (1/4 of a ms)
     */
    safety_threshold = frac_mult(op_extra_data->spkr_rate, FRACTIONAL(0.00025))+1;

    override_op_ptr = create_aec_ref_sidetone_op(
        op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1],  /* mic mmu buffer */
        st_mic_idx,                                          /* indexes for clone mic buffer */
        &op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1], /* spkr mmu buffer(s) */
        num_sidetone_spkrs,                                 /* number of speaker(s) */
        &idxs[sidetone_idx],                                /* indexes for sidetone buffer */
        spkr_threshold,                                     /* speaker threshold */
        safety_threshold                                    /* safety threshold */
                                                );
    if(!override_op_ptr)
    {
        goto aFailed;
    }
    cbops_set_override_operator(sidetone_graph, override_op_ptr);

    DEBUG_GRAPHS("AEC REFERENCE: Building sidetone graph, Done!");
    pfree(idxs);

    return TRUE;
  aFailed:
    DEBUG_GRAPHS("AEC REFERENCE: Building sidetone graph, Failed!");
    pfree(idxs);
    return(FALSE);
}
#endif /* AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING */
/**
 * build_mic_graph_is_required
 * \brief checks whether we need to form mic graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \param sidetone_enabled sidetone is enabled
 * \return TRUE if mic path is required else FALSE.
 */
static bool build_mic_graph_is_required(AEC_REFERENCE_OP_DATA *op_extra_data,
                                        bool sidetone_enabled)
{
    /* check if we have mic config at all */
    if((op_extra_data->channel_status&CHAN_STATE_MIC_MASK) == 0)
    {
        /* mic path isn't needed */
        return FALSE;
    }

    /* default: mic path required with output */
    op_extra_data->mic_graph_no_output = FALSE;

    if((op_extra_data->channel_status&AEC_REFERENCE_CONSTANT_CONN_MIKE_1_INPUT_ONLY) != 0)
    {
        /* This check is only for efficiency, knowing that 1->0 mic config
         * only needed if sidetone is enabled. if sidetone not enabled,
         * or enabled but will be done using separate graph then no need
         * to build mic graph at all.
         */
        if(!sidetone_enabled || op_extra_data->task_decim_factor > 1)
        {
            /* mic path isn't needed */
            return FALSE;
        }

        /* build mic path without output */
        op_extra_data->mic_graph_no_output = TRUE;
    }

    return TRUE;
}

/**
 * build_mic_graph_add_output_subgraph
 * \brief complete output part of mic graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \param idxs buffer indexes table, passed to avoid regeneration here.
 * \param intern_idx buffer index that between input and output parts (for first channel)
 * \param out_idx buffer ixdex for first mic output channel
 * \param sidetone_enabled whether sidetone is possibly needed.
 * \return whether output subgraph successfully appended.
 */
static bool build_mic_graph_add_output_subgraph(AEC_REFERENCE_OP_DATA *op_extra_data,
                                                unsigned *idxs,
                                                unsigned intern_idx,
                                                unsigned out_idx,
                                                bool sidetone_enabled)
{
    cbops_graph *mic_graph = op_extra_data->mic_graph;
    unsigned num_mics = op_extra_data->num_mic_channels;
    cbops_op *op_ptr = NULL;
    unsigned i,j;

    /* Setup output Buffers*/
    for(j=0,i=0;(j<MAX_NUMBER_MICS);j++)
    {
        /* MIC outputs may not be consecutive */
        tCbuffer *buffer_ptr_snk = op_extra_data->output_stream[OutputTerminalByIndex(j)];
        if(buffer_ptr_snk != NULL)
        {
            if(i < num_mics)
            {
                /* Outputs */
                cbops_set_output_io_buffer(mic_graph, out_idx + i, out_idx, buffer_ptr_snk);
            }
            i++;
        }
    }
    /* expect to have exactly num_mics outputs connected */
    PL_ASSERT(i==num_mics);

    /* Handle output */
    if(op_extra_data->mic_rate!=op_extra_data->output_rate)
    {
        /* If only operator then shift input to output */
        unsigned shift_amount = (intern_idx != 0) ? 0 :  op_extra_data->mic_shift;

        /*  Add Resampler Operators per channel (mic_rate --> output_rate) */
        op_ptr = create_iir_resamplerv2_op(num_mics,
                                           &idxs[intern_idx], &idxs[out_idx],
                                           op_extra_data->mic_rate,op_extra_data->output_rate,
                                           op_extra_data->resampler_temp_buffer_size,
                                           op_extra_data->resampler_temp_buffer, shift_amount, 0, 0);
        if(op_ptr == NULL)
        {
            return FALSE;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }
    else if (sidetone_enabled)
    {
        /* Need to copy internal buffer to output before sidetone */
        op_ptr = create_shift_op(num_mics,&idxs[intern_idx], &idxs[out_idx],0);
        if(op_ptr == NULL)
        {
            return FALSE;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }

    /* Handle sidetone */
    if(sidetone_enabled && op_ptr != NULL)
    {
        /* mic sidetone path will be inserted
         * after this operator so it can do in-place
         * processing.
         */
        op_extra_data->mic_st_point = op_ptr;
    }

    /* add mute operator, it can be controlled by
     * message to the operator
     */
    op_ptr = create_mute_op(num_mics, &idxs[out_idx]);
    if(!op_ptr)
    {
        return FALSE;
    }
    cbops_append_operator_to_graph(mic_graph,op_ptr);
    op_extra_data->mic_mute_op = op_ptr;
    /* apply user config to mute operator, we don't apply
     * ramping at the beginning, so user can mute right from
     * the beginning.
     */
    cbops_mute_enable(op_extra_data->mic_mute_op, op_extra_data->mic_mute_enable_config, TRUE);

    /* Insert latency operator */
    op_ptr = create_mic_latency_op(out_idx,&op_extra_data->sync_block);
    if(op_ptr == NULL)
    {
        return FALSE;
    }
    cbops_append_operator_to_graph(mic_graph,op_ptr);
    op_extra_data->mic_latency_index = out_idx;

    return TRUE;
}

/**
 * build_mic_graph
 * \brief builds cbops graph for microphone path based on the relevant
 *        connections and configurations. Th microphone graph is built
 *        whenever there is a change in microphone channel status.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool build_mic_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    cbops_graph *mic_graph;
    cbops_op *op_ptr;
    unsigned i, num_mics = op_extra_data->num_mic_channels,j,num_io;
    unsigned* idxs;
    unsigned  out_idx,intern_idx;
    bool sidetone_enabled = (op_extra_data->using_sidetone&SIDETONE_ENABLE_FLAG) != 0;

    patch_fn_shared(aec_reference);

    /*********************************************************************

          INPUT SUBGRAPH            OUTPUT SUBGRAPH
    MICs --> RM --> DC_REMOVE -+--> resample --> OUT
                               |
                            ST filter
                               |
                             resample  <-- SIDETONE SUBGRAPH
                               |
                             SIDETONE



    INPUT SUBGRAPH:
       All mic graphs will have input subgraph, i.e. no mic graph
       will be formed at all if we have no input. For clarity we
       currently support N->N and 1->0 mic configs.

    OUTPUT SUBGRAPH:
       This will be included only when we have mic outputs, i.e.
       currently for N->N mic config only.

    SIDETONE SUBGRAPH:
       Sidetone subgraph, if required, will be included later by
       aec_reference_mic_spkr_include_sidetone.

    **********************************************************************/

    /* see if we need a MIC path */
    if(!build_mic_graph_is_required(op_extra_data, sidetone_enabled))
    {
        /* No need to build mic graph */
        return TRUE;
    }

    /* number of io buffers
       num_mic*[IN,INTERN,OUT] plus SideTONE

       Buffer Order
       MIC Inputs
       MIC Output
       MIC Internal
       SIDETONE OUT
    */

    out_idx    = num_mics;
    intern_idx = out_idx;
    num_io     = 2*num_mics;

    /* if sidetone enabled reserve indexes for mic sidetone process,
     * the process will insert later if required.
     */
    if(sidetone_enabled)
    {
        /* Need internal buffer between input and output */
        intern_idx = num_io;
        num_io    += num_mics;

        /* sidetone path input */
        op_extra_data->mic_st_input_idx = intern_idx;

        /* sidetone output */
        op_extra_data->mic_st_idx = num_io;

        /* one extra io for sidetone, could be unused*/
        num_io++;
    }
    else if ((op_extra_data->mic_rate_ability == RATEMATCHING_SUPPORT_SW) &&
             (op_extra_data->mic_rate != op_extra_data->output_rate))
    {
        /* Need internal buffer between input and output */
        intern_idx = num_io;
        num_io    += num_mics;
    }
    else if (op_extra_data->mic_rate!=op_extra_data->output_rate)
    {
        /* No ratematching or sidetone.  Just resampling  */
        intern_idx = 0;
    }

    idxs = create_default_indexes(num_io);
    if(idxs == NULL)
    {
        return(FALSE);
    }

    /* Allocate mic graph */
    mic_graph = cbops_alloc_graph(num_io);
    if(!mic_graph)
    {
        goto aFailed;
    }
    op_extra_data->mic_graph = mic_graph;

    /* Setup MIC input Buffers*/
    for(j=0,i=0;(j<MAX_NUMBER_MICS);j++)
    {
        /* MIC inputs may not be consecutive */
        tCbuffer *buffer_ptr_src = op_extra_data->input_stream[MicrophoneTerminalByIndex(j)];

        if(buffer_ptr_src != NULL)
        {
            if(i < num_mics)
            {
                /* Inputs */
                cbops_set_input_io_buffer(mic_graph, i, 0, buffer_ptr_src);
            }
            i++;
        }
    }
    /* expect to have exactly num_mics mics connected */
    PL_ASSERT(i==num_mics);

    if(intern_idx > out_idx)
    {
        for(i=0;i<num_mics;i++)
        {
            cbops_set_internal_io_buffer(mic_graph,intern_idx+i,intern_idx,op_extra_data->scratch_bufs[i]);
        }
    }

    op_extra_data->mic_rate_adjustment = 0;

    /* create rate monitor op if required */
    op_ptr = NULL;
    if (op_extra_data->mic_rate_ability == RATEMATCHING_SUPPORT_HW)
    {
        /* With HW rate adjustmen we always need rate monitor,
         * except when we are syncing MIC to REF.
         */
        if(!op_extra_data->mic_sync_enable)
        {
            op_ptr = create_rate_monitor_operator(op_extra_data->task_frequency, 0);
            if(!op_ptr)
            {
                goto aFailed;
            }
            rate_monitor_op_initialise(op_ptr,op_extra_data->mic_rate,TRUE,3*MS_PER_SEC);
        }
    }
    else if (!opmgr_override_is_locally_clocked(op_extra_data->mic_endpoint))
    {
        /* With SW rate adjustment we need rate monitor if mic isn't locally clocked */
        op_ptr = create_rate_monitor_operator(op_extra_data->task_frequency, 0);
        if(!op_ptr)
        {
            goto aFailed;
        }
        rate_monitor_op_initialise(op_ptr,op_extra_data->mic_rate, FALSE,
                                   100*MILLISECOND/MILLISECOND);
    }
    if(NULL != op_ptr)
    {
        /* insert rate monitor op into the graph */
        op_extra_data->mic_rate_monitor_op = op_ptr;
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }

    /* Handle input */
    if (op_extra_data->mic_rate_ability == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
        && (0 == op_extra_data->mic_ext_rate_adjust_op)
#endif
        )
    {
        /* Apply Software Rate Adjustment */
        op_ptr = create_sw_rate_adj_op(num_mics, idxs, &idxs[intern_idx],
                                       CBOPS_RATEADJUST_COEFFS,
                                       &op_extra_data->mic_rate_adjustment, op_extra_data->mic_shift);
        if(!op_ptr)
        {
            goto aFailed;
        }

        op_extra_data->mic_sw_rateadj_op=op_ptr;
        cbops_rateadjust_passthrough_mode(op_ptr,(op_extra_data->mic_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);

        cbops_append_operator_to_graph(mic_graph,op_ptr);

        /* Early DC remove on mic path. Before Sidetone split so the signal split
           to the speaker doesn't have DC */
        op_ptr = create_dc_remove_op(num_mics, &idxs[intern_idx], &idxs[intern_idx]);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);

    }
    else if(intern_idx != 0)
    {
        /* Otherwise, just copy data to next section */
        op_ptr = create_shift_op(num_mics, idxs, &idxs[intern_idx], op_extra_data->mic_shift);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);

        /* Early DC remove on mic path. Before Sidetone split so the signal split
           to the speaker doesn't have DC */
        op_ptr = create_dc_remove_op(num_mics, &idxs[intern_idx], &idxs[intern_idx]);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }

    if(sidetone_enabled)
    {
        /* mic sidetone path will be inserted after last operator in inputsubgraph.*/
        op_extra_data->mic_st_point = op_ptr;
    }

    /* check if output subgraph is needed */
    if(!op_extra_data->mic_graph_no_output)
    {
        /* append mic output subgraph */
        if(!build_mic_graph_add_output_subgraph(op_extra_data,
                                                idxs,
                                                intern_idx,
                                                out_idx,
                                                sidetone_enabled))
        {
            goto aFailed;
        }
    }

    pfree(idxs);

    /* Each mic input has a corresponding output,
     * so it's safe to purge all channels now
     */
    aec_ref_purge_mics(mic_graph,num_mics);

    DEBUG_GRAPHS("AEC REFERENCE: Building mic graph, Done!" );

    return(TRUE);
  aFailed:
    DEBUG_GRAPHS("AEC REFERENCE: Building mic graph, Failed!");

    pfree(idxs);
    return(FALSE);
}

/**
 * build_spkr_graph
 * \brief builds cbops graph for speaker path based on the relevant
 *        connections and configurations. Th speaker graph is rebuilt
 *        whenever there is a change in speaker channel status.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 * Note: Speaker path can have reference sub-path and/or side-tone mix
 *       depending on the connections and microphone state, however they
 *       aren't included by this function, instead they are added to the
 *       speaker graph later.
 */
bool build_spkr_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned    i,j,k,num_spkrs,num_inputs;
    cbops_graph *spkr_graph;
    cbops_op    *op_ptr;
    cbops_op    *overrid_op_ptr;
    unsigned    num_io;
    tCbuffer    *buffer_ptr;
    unsigned    *idxs;
    unsigned    out_indx,intern_ins_idx,intern_rs_idx,intern_rm_idx;
    unsigned    spkr_channel_status = GetSpkrChannelStatus(op_extra_data);
    unsigned    usable_scratch_idx;

    patch_fn_shared(aec_reference);

    /*********************************************************************

                                           RM ---> resample --> REF
                                             |
    IN --> INSERT --> DC_REMOVE -->  MIXER --+--> resample --> Sidetone Mix --> RM ----------> SPKR
   (0)                                   (intern_ins_idx)    (intern_rs_idx)  (intern_rm_idx)  (out_indx)

   Note: Addition of "Reference sub-path" and "Sidetone Mix" aren't done in
         this function. These two are inserted into the speaker graph later
         if required. Both of these two can also be dynamically removed from the graph
         when they are no longer required.

         "Sidetone Mix" is added/removed to/from the graph by aec_reference_mic_spkr_include_sidetone function.
         "Reference sub-path" is added/removed to/from the graph by aec_reference_spkr_include_ref_path function.

    **********************************************************************/
    /* Is there a Speaker path */
    if(!(op_extra_data->channel_status&CHAN_STATE_SPKR_MASK))
    {
        return(TRUE);
    }

    /* Determine IO configuration */
    num_spkrs  = op_extra_data->num_spkr_channels;
    num_inputs = op_extra_data->num_spkr_channels;
    if(!(spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA))
    {
        if(spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_MIX)
        {
            /* Multiple inputs, mono output */
            num_spkrs=1;
        }
        else
        {
            /* Mono input, multiple outputs */
            num_inputs=1;
        }
    }

    /* outputs follow inputs */
    /* Internal buffers for output of insert follow outputs */
    out_indx       = num_inputs;

    /* Count the buffers needed */
    num_io         = num_inputs+num_spkrs;

    /* reserve one index for sidetone input buffer */
    op_extra_data->spkr_st_in_idx = num_io;
    num_io++;

    /* Assume no resampler or rate matching */
    intern_rs_idx = 0;
    intern_rm_idx = 0;
    intern_ins_idx = num_io;

    /* speaker inputs are copied to interm buffers immediately,
       reserve indexes for num_inputs buffers.
     */
    num_io += num_inputs;

    /* We will have two set of scratch buffers
     * set 1: scratch_buffers[0:num_inputs-1];
     * set 2: scratch_buffers[num_inputs:2*num_inputs-1]
     * Each stage that needs scratch buffer will use use one
     * of this set, and the usage will alternate between two,
     * this makes sure a cbops operator will be able to use
     * scratch buffer for both inputs and outputs.
     */
    usable_scratch_idx = 0;

    /* Check if resampler is required */
    if(op_extra_data->input_rate!=op_extra_data->spkr_rate)
    {
        intern_rs_idx = out_indx;

        if(op_extra_data->spkr_rate_ability==RATEMATCHING_SUPPORT_SW)
        {
            /* Need buffers following resampler */
            intern_rs_idx    = num_io;
            if(num_spkrs>num_inputs)
            {
                /* Resampler is Mono.  Feeds shift */
                num_io++;
            }
            else
            {
                /* Resampler is multi channel */
                num_io += num_inputs;
            }
        }
    }

    /* Check if sw rate matching is required */
    if(op_extra_data->spkr_rate_ability == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
       /* not required if having access to external rate adjust op */
       && (0 == op_extra_data->spkr_ext_rate_adjust_op)
#endif
       )
    {
        intern_rm_idx = out_indx;
        /* Split needs buffer following rate matching (Mono)*/
        if(num_spkrs>num_inputs)
        {
            intern_rm_idx = num_io;
            num_io++;
        }
    }

    /* reserve 2 indexes for reference sub-path,
     * one is for reference output buffer, the
     * other one for scratch buffer for possible
     * resampler in the sub-path.
     */
    op_extra_data->spkr_ref_idx  = num_io;
    num_io +=2;

    /* Allocate buffer index array for easy setup */
    if(num_inputs<num_spkrs)
    {
        idxs = create_default_indexes(num_io+num_spkrs);
    }
    else
    {
        idxs = create_default_indexes(num_io);
    }
    if(idxs == NULL)
    {
        return(FALSE);
    }

    /* Allocate num_io buffers in spkr graph */
    spkr_graph = cbops_alloc_graph(num_io);
    if(!spkr_graph)
    {
        goto aFailed;
    }
    op_extra_data->spkr_graph = spkr_graph;

    /* Setup IO Buffers
       Buffer Order
       Inputs
       SPKR Outputs
       Internal (after insert)          : scratch[0:num_inputs-1]
       Internal (after resample)
       Internal (after rate match)
       Internal (after ref resample)    : scratch[num_inputs]
       REF OUT
    */

    /* Input, Output, and insert_op buffers */
    for(j=0,i=0,k=0;j<MAX_NUMBER_SPEAKERS;j++)
    {
        /* Inputs may not be contiguous */
        buffer_ptr =  op_extra_data->input_stream[SpeakerInputTerminalByIndex(j)];
        if(buffer_ptr)
        {
            /* Inputs */
            cbops_set_input_io_buffer(spkr_graph,i,0,buffer_ptr);
            i++;
        }
        /* Outputs may not be contiguous */
        buffer_ptr =  op_extra_data->output_stream[SpeakerTerminalByIndex(j)];
        if(buffer_ptr)
        {
            cbops_set_output_io_buffer(spkr_graph,out_indx + k,out_indx,buffer_ptr);
            k++;
        }
    }


    /* Buffers for transfer from inputs to interm buffers */
    for(i=0; i < num_inputs ; i++)
    {
         cbops_set_internal_io_buffer(spkr_graph,
                                      intern_ins_idx + i,
                                      intern_ins_idx,
                                      op_extra_data->scratch_bufs[usable_scratch_idx + i]);
    }
    /* update usable_scratch_idx for next use */
    usable_scratch_idx = usable_scratch_idx==0? num_inputs:0;

    /* Building override operator, this operator will transfer speaker
     * inputs to inerim buufers as well as any silence insertion required.
     * This way AEC_REFERERNCE will not write into its input buffer.
     */
    /* Thresholds for Insertion, keep a copy in main structure */
    op_extra_data->spkr_in_threshold  = frac_mult(op_extra_data->input_rate,op_extra_data->task_period_frac) + 1;
    op_extra_data->spkr_out_threshold = frac_mult(op_extra_data->spkr_rate,op_extra_data->task_period_frac) + 1;
    /* override threshold will control speaker buffer latency, at the end of each
     * task period there will be ~(spkr_out_threshold + max_jitter) in the output buffer,
     * this is to cover a full task period plus possible scheduling uncertainties.
     * 1ms max_jitter might be enough, 0.5ms added in case sidetone mixing will run
     * in decimated task period.
     */
    unsigned max_jitter = frac_mult(op_extra_data->spkr_rate, FRACTIONAL(0.0015));

    overrid_op_ptr = create_aec_ref_spkr_op(num_inputs,idxs,&idxs[intern_ins_idx],
                                            op_extra_data->spkr_in_threshold,
                                            num_spkrs,&idxs[out_indx],
                                            op_extra_data->spkr_out_threshold,
                                            max_jitter);
#if defined(IO_DEBUG)
    op_extra_data->insert_op = overrid_op_ptr;
#endif
    if(!overrid_op_ptr)
    {
        goto aFailed;
    }

    cbops_set_override_operator(spkr_graph,overrid_op_ptr);

    /* DC remove before reference tap */
    op_ptr = create_dc_remove_op(num_inputs, &idxs[intern_ins_idx], &idxs[intern_ins_idx]);
    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(spkr_graph,op_ptr);


    /* Add Mixer to section #1 if needed*/
    if(num_inputs > num_spkrs)
    {
        for(i=1;i<num_inputs;i++)
        {
            /* NOTE: left scratch input reused for output (inplace) */
            /*  - Add Mix Operator - stereo to mono (left = (left+right)/2 */
            op_ptr = create_mixer_op(intern_ins_idx, intern_ins_idx+i, intern_ins_idx, 0, FRACTIONAL(0.5));
            if(!op_ptr)
            {
                goto aFailed;
            }
            cbops_append_operator_to_graph(spkr_graph,op_ptr);
        }
        /* Input is now mono */
        num_inputs = 1;
    }

    /* This is the point where we take input for the
     * reference sub-path, store information about
     * where in the graph the reference sub-path
     * should be inserted to.
     */
    op_extra_data->spkr_ref_input_idx = intern_ins_idx;
    op_extra_data->spkr_ref_point_op = op_ptr;
    op_extra_data->spkr_ref_scratch_idx = usable_scratch_idx;

    /* Add sample rate conversion per channel [num_inputs] (input_rate --> spkr_rate) */
    if(intern_rs_idx!=0)
    {
        int shift_amount = op_extra_data->spkr_shift;

        if(intern_rs_idx!=out_indx)
        {
            /* Buffers for output of resampler. */
            for(i=0; i < num_inputs ; i++)
            {
                cbops_set_internal_io_buffer(spkr_graph,intern_rs_idx+i,intern_rs_idx,
                                             op_extra_data->scratch_bufs[usable_scratch_idx+i]);
            }
            shift_amount=0;
            /* update usable_scratch_idx for next use */
            usable_scratch_idx = usable_scratch_idx==0?num_inputs:0;
        }

        op_ptr = create_iir_resamplerv2_op(num_inputs, &idxs[intern_ins_idx], &idxs[intern_rs_idx],
                                           op_extra_data->input_rate, op_extra_data->spkr_rate,
                                           op_extra_data->resampler_temp_buffer_size,
                                           op_extra_data->resampler_temp_buffer, shift_amount, 0, 0);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(spkr_graph,op_ptr);

        /* Move next input to output of resampler */
        intern_ins_idx = intern_rs_idx;
    }

    /* This is the point where we insert possible sidetone
     * mix operator, store both the operator and the input
     * buffer index for the operator. Sidetone mixing
     * will always be done in-place.
     */
    op_extra_data->spkr_st_point_op = op_ptr;
    op_extra_data->spkr_stmix_in_idx = intern_ins_idx;

    /* Optional section for SW rate matching */
    if(intern_rm_idx != 0)
    {
        int shift_amount =  op_extra_data->spkr_shift;

        if(intern_rm_idx!=out_indx)
        {
            /* Buffers for output of ratematch if it exists */
            for(i=0; i < num_inputs; i++)
            {
                cbops_set_internal_io_buffer(spkr_graph,intern_rm_idx+i,intern_rm_idx,
                                             op_extra_data->scratch_bufs[usable_scratch_idx+i]);
            }
            shift_amount=0;
            /* update usable_scratch_idx for possible next use */
            usable_scratch_idx = usable_scratch_idx==0?num_inputs:0;
        }

        /*  SW rate adjustment per channel [num_inputs] */
        op_ptr = create_sw_rate_adj_op(num_inputs, &idxs[intern_ins_idx], &idxs[intern_rm_idx],
                                       CBOPS_RATEADJUST_COEFFS,
                                       &op_extra_data->spkr_rate_adjustment,shift_amount);
        if(!op_ptr)
        {
            goto aFailed;
        }

        op_extra_data->spkr_sw_rateadj_op=op_ptr;
        cbops_rateadjust_passthrough_mode(op_ptr,(op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);

        cbops_append_operator_to_graph(spkr_graph,op_ptr);

        /* Move next input to output of ratematch */
        intern_ins_idx = intern_rm_idx;
    }


    /* Did previous operator terminate route? */
    if(intern_ins_idx!=out_indx)
    {
        /* Check for Mono to multi-channel */
        if(num_inputs<num_spkrs)
        {
            for(i=0; i < num_spkrs; i++)
            {
                idxs[num_io+i] = intern_ins_idx;
            }
            intern_ins_idx = num_io;
        }

        op_ptr = create_shift_op(num_spkrs, &idxs[intern_ins_idx], &idxs[out_indx], op_extra_data->spkr_shift);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(spkr_graph,op_ptr);
    }

    op_extra_data->spkr_rate_adjustment=0;

    /* create rate monitor op if required */
    op_ptr = NULL;
    if(op_extra_data->spkr_rate_ability == RATEMATCHING_SUPPORT_HW)
    {
        /* With HW rate adjustmen we always need rate monitor */
        op_ptr = create_rate_monitor_operator(op_extra_data->task_frequency, out_indx);
        if(!op_ptr)
        {
            goto aFailed;
        }
        rate_monitor_op_initialise(op_ptr,op_extra_data->spkr_rate, TRUE, 3*MS_PER_SEC);
    }
    else if (!opmgr_override_is_locally_clocked(op_extra_data->spkr_endpoint))
    {
        /* With SW rate adjustment we need rate monitor if speaker isn't locally clocked */
        op_ptr = create_rate_monitor_operator(op_extra_data->task_frequency, out_indx);
        if(!op_ptr)
        {
            goto aFailed;
        }
        rate_monitor_op_initialise(op_ptr,op_extra_data->spkr_rate, FALSE,
                                    100*MILLISECOND/MILLISECOND);
    }
    if(NULL != op_ptr)
    {
#ifdef AEC_REF_CALC_SPKR_RATE_MONITOR_AMOUNT
    /* Initialise new amount calculation for speaker buffer */
    cbuffer_calc_new_amount(op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1],
                            &op_extra_data->spkr_last_address,
                            TRUE /* This is an output buffer */);

    /* tell rate monitor cbops op to directly use calculated new amount */
    set_rate_monitor_new_amount_ptr(op_ptr, &op_extra_data->spkr_new_amount);
#endif
        /* insert rate monitor op into the graph */
        op_extra_data->spkr_rate_monitor_op = op_ptr;
        cbops_append_operator_to_graph(spkr_graph, op_ptr);
    }

#ifdef AEC_REF_CALC_SPKR_RATE_MONITOR_AMOUNT
    /* Reset flag showing speaker started to consume data */
    op_extra_data->spkr_flow_started = FALSE;
#endif

    pfree(idxs);

    DEBUG_GRAPHS("AEC REFERENCE: Building speaker graph, Done!" );
    return TRUE;
  aFailed:
    DEBUG_GRAPHS("AEC REFERENCE: Building speaker graph, Failed!" );
    pfree(idxs);
    return FALSE;
}

/**
 * validate_channels_and_build
 * \brief checks all the connections for both speaker and microphone
 *        paths and rebuild the cbops graphs for each path when needed.
 *
 * \param op_data Pointer to the AEC reference operator data.
 */
bool validate_channels_and_build(OPERATOR_DATA *op_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    /* check changes in speaker, microphone and reference paths */
    bool spkr_changed = aec_reference_update_spkr_channel_status(op_extra_data);
    bool mic_changed = aec_reference_update_mic_channel_status(op_extra_data);
    bool ref_changed = aec_reference_update_ref_channel_status(op_extra_data);



    patch_fn_shared(aec_reference);

    /* update sidetone method */
    aec_reference_update_sidetone_method(op_extra_data);

    /* rebuild graphs if there is change in
     * speaker or microphone path
     */
    if(!aec_reference_build_graphs(op_extra_data, spkr_changed, mic_changed))
    {
        goto aFailed;
    }

    /* also if there is changes in reference path, then
     * apply that change to speaker graph
     */
    if(ref_changed || spkr_changed)
    {
        if(!aec_reference_spkr_include_ref_path(op_extra_data, GetRefChannelStatus(op_extra_data)))
        {
            goto aFailed;
        }
    }

    if(op_extra_data->mic_graph != NULL ||
       op_extra_data->spkr_graph!= NULL)
    {
        /* start running the graphs if not already running
         */
        if(op_extra_data->kick_id == TIMER_ID_INVALID)
        {
            op_extra_data->kick_id = timer_schedule_event_in(op_extra_data->kick_period, aec_reference_timer_task,(void*)op_data);
        }
    }
    else
    {
        timer_cancel_event_atomic(&op_extra_data->kick_id);
    }

    return TRUE;

  aFailed:
    /* it failed to complete the build for some reason,
     * clean up all the graphs.
     */
    aec_reference_cleanup(op_data);
    return FALSE;
}

/**
 * aec_reference_update_spkr_channel_status
 * \brief updates speaker channel status based on the latest connections
 *        affecting speaker path.
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in speaker channel status that requires
 *         rebuilding of the speaker graph.
 */
bool aec_reference_update_spkr_channel_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned i;
    unsigned spkr_channel_status=0;
    unsigned number_spkrs=0;
    tCbuffer *inputBuf,*outputBuf;

    patch_fn_shared(aec_reference);

    /* Speakers are connected */
    op_extra_data->sink_kicks   = 0;
    if(op_extra_data->input_stream[AEC_REF_INPUT_TERMINAL1] && op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
    {
        int in_count=1;
        int out_count=1;

        /* Master Channel is connected */
        spkr_channel_status |= AEC_REFERENCE_CONSTANT_CONN_SPKR_1;

        /* Primary sink is connected */
        op_extra_data->sink_kicks = (1<<AEC_REF_INPUT_TERMINAL1);

        for(i=1;i<MAX_NUMBER_SPEAKERS;i++)
        {
            int sink_idx = SpeakerInputTerminalByIndex(i);

            /* Old terminals kept for backwards compatibility */
            outputBuf = op_extra_data->output_stream[SpeakerTerminalByIndex(i)];
            inputBuf  = op_extra_data->input_stream[sink_idx];

            if(inputBuf || outputBuf)
            {
                spkr_channel_status |= (AEC_REFERENCE_CONSTANT_CONN_SPKR_1<<i);

                if(inputBuf)
                {
                    op_extra_data->sink_kicks |= (1<<sink_idx);
                    in_count++;
                }
                else if(in_count>1)
                {
                    /* Special case allows mono input split to multiple outputs*/
                    return(FALSE);
                }

                if(outputBuf)
                {
                    out_count++;
                }
                else if(out_count>1)
                {
                    /* Special case allows mono output mixed from multiple inputs*/
                    return(FALSE);
                }
            }
        }
        /* Verify complete channel setup */
        number_spkrs = out_count;
        if(in_count==out_count)
        {
            /* Parallel channels */
            spkr_channel_status |= AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA;
        }
        else if(out_count==1)
        {
            /* Mono Output.  Mix inputs */
            spkr_channel_status |= AEC_REFERENCE_CONSTANT_CONN_TYPE_MIX;
            number_spkrs = in_count;
        }
        else
        {
            /* Mono Input.  Split outputs */
            PL_ASSERT(in_count==1);
        }
    }

    if((GetSpkrChannelStatus(op_extra_data)) == spkr_channel_status)
    {
        /* No change in the speaker channel status */
        return FALSE;
    }

    /* update number of speaker channels */
    op_extra_data->num_spkr_channels = number_spkrs;

    /* update speaker channel status */
    SetSpkrChannelStatus(op_extra_data, spkr_channel_status);
    return TRUE;

}

/**
 * aec_reference_update_ref_channel_status
 * \brief updates ref channel status based on the reference output connection
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in ref channel status that requires
 *         insertion/deletion of the reference sub-path into/from speaker graph.
 */
bool aec_reference_update_ref_channel_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{

    unsigned ref_channel_status = 0;
    patch_fn_shared(aec_reference);

    /* Check AEC reference */
    if(op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL] && (op_extra_data->channel_status&AEC_REFERENCE_CONSTANT_CONN_MIKE_1))
    {
        op_extra_data->source_kicks |= 1<<AEC_REF_REFERENCE_TERMINAL;
        ref_channel_status = AEC_REFERENCE_CONSTANT_CONN_TYPE_REF;
    }

    if(ref_channel_status == (GetRefChannelStatus(op_extra_data)))
    {
        /* No change in reference path */
        return FALSE;
    }

    /* reference path has changed, update
     * channel status.
     */
    SetRefChannelStatus(op_extra_data, ref_channel_status);
    return TRUE;
}

/**
 * aec_reference_update_mic_channel_status
 * \brief updates mic channel status based on the latest connections
 *        affecting mic path.
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in speaker channel status that requires
 *         rebuilding of the speaker graph.
 */
bool aec_reference_update_mic_channel_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{

    unsigned i;
    unsigned mic_channel_status=0;
    unsigned number_mics=0;
    tCbuffer *inputBuf,*outputBuf;

    patch_fn_shared(aec_reference);

    /* Microphones are connected */
    op_extra_data->source_kicks = 0;
    if(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1] && op_extra_data->output_stream[AEC_REF_OUTPUT_TERMINAL1])
    {
        bool all_mic_channels = TRUE;
        for(i=0;i<MAX_NUMBER_MICS;i++)
        {
            inputBuf  = op_extra_data->input_stream[MicrophoneTerminalByIndex(i)];
            outputBuf = op_extra_data->output_stream[OutputTerminalByIndex(i)];

            if(inputBuf && outputBuf)
            {
                number_mics++;

                if(i<4)
                {
                    mic_channel_status |= (AEC_REFERENCE_CONSTANT_CONN_MIKE_1<<i);
                }
                else
                {
                    mic_channel_status |= ((AEC_REFERENCE_CONSTANT_CONN_MIKE_5>>4)<<i);
                }
                op_extra_data->source_kicks |= (1 << OutputTerminalByIndex(i) );
            }
            else if(inputBuf || outputBuf)
            {
                all_mic_channels=FALSE;
                break;
            }
        }
        if(!all_mic_channels)
        {
            number_mics=0;
            mic_channel_status = 0;
            op_extra_data->source_kicks = 0;
        }
    }

    if(mic_channel_status == 0)
    {
        /* we cannot form N->N mic path, see if we
         * can form 1->0 mic path, the latter would enable
         * supporting sidetone without microphone output.
         */
        inputBuf  = op_extra_data->input_stream[MicrophoneTerminalByIndex(0)];
        outputBuf = op_extra_data->output_stream[OutputTerminalByIndex(0)];
        if(inputBuf != NULL && outputBuf == NULL)
        {
            /* mic0 connected, out0 disconnected, we can have 1->0 config */
            mic_channel_status = AEC_REFERENCE_CONSTANT_CONN_MIKE_1_INPUT_ONLY;
            number_mics = 1;

            /* However check that all other mics/outs are disconnected */
            for(i=1;i<MAX_NUMBER_MICS;i++)
            {
                inputBuf  = op_extra_data->input_stream[MicrophoneTerminalByIndex(i)];
                outputBuf = op_extra_data->output_stream[OutputTerminalByIndex(i)];
                if(inputBuf != NULL || outputBuf != NULL)
                {
                    /* Another mic input and/or output is connected,
                     * we cannot have 1->0 mic path either.
                     */
                    mic_channel_status = 0;
                    number_mics = 0;
                    break;
                }
            }
        }
    }

    if((GetMicChannelStatus(op_extra_data)) == mic_channel_status)
    {
        /* No change in the mic path */
        return FALSE;
    }

    /* update number of mics */
    op_extra_data->num_mic_channels = number_mics;

    /* update mic channel status */
    SetMicChannelStatus(op_extra_data, mic_channel_status);

    return TRUE;
}

/**
 * aec_reference_update_sidetone_status
 * \brief resets the state of using sidetone
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_update_sidetone_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    if((op_extra_data->params.OFFSET_CONFIG & AEC_REFERENCE_CONFIG_SIDETONE_DISABLE) == 0)
    {
        /* a sidetone path will be formed whenever
         * we have both speaker and mic paths
         */
        op_extra_data->using_sidetone = SIDETONE_ENABLE_FLAG;
    }
    else
    {
        /* User doesn't want sidetone at all */
        op_extra_data->using_sidetone = 0;
    }

    /* see if we need sidetone */
    if((op_extra_data->num_mic_channels > 0) && (op_extra_data->num_spkr_channels > 0))
    {
        op_extra_data->using_sidetone |= SIDETONE_MIC_SPKR_FLAG;
    }
    else
    {
        op_extra_data->using_sidetone &= SIDETONE_ENABLE_FLAG;
    }
}

/**
 * aec_reference_update_sidetone_method
 * \brief updates sidetone mixing method
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in sidetone mixing method
 */
void aec_reference_update_sidetone_method(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    AEC_REFERENCE_SIDETONE_METHOD sidetone_method = AEC_SIDETONE_NOT_REQUIRED;
    patch_fn_shared(aec_reference);

    /* reset state of sidetone */
    aec_reference_update_sidetone_status(op_extra_data);

    /* update HW sidetone availability */
    if((op_extra_data->params.OFFSET_CONFIG & AEC_REFERENCE_CONFIG_SUPPORT_HW_SIDETONE)
       != 0)
    {
        /* See if current mic and speaker HW setup to do side tone mixing */
        op_extra_data->hw_sidetone_available =
            opmgr_override_have_sidetone_route(op_extra_data->mic_endpoint,
                                               op_extra_data->spkr_endpoint);
    }
    else
    {
        op_extra_data->hw_sidetone_available = FALSE;
    }

    /* If the config needs sidetone, decide which method it should use */
    if(op_extra_data->using_sidetone == USE_SIDETONE_FLAG)
    {
        if(op_extra_data->hw_sidetone_available)
        {
            /* if the config can have sidetone mixing in Hw then use that method */
            sidetone_method = AEC_SIDETONE_IN_HW;
        }
#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
        else if(op_extra_data->task_decim_factor > 1)
        {
            /* HW side tone isn't supported, and speaker and mic are
             * running at high task period, use separate graph for sidetone
             * mixing which will run at decimated task period.
             */
            sidetone_method = AEC_SIDETONE_IN_SW_USING_SEPARATE_GRAPH;
        }
#endif
        else
        {
            /* traditional way of sidetone mixing in AEC_REFERENCE operator,
             * sidetone will be provided by MIC graph and will be mixed by
             * SPKR graph.
             */
            sidetone_method = AEC_SIDETONE_IN_SW_BY_MIC_SPKR_GRAPH;
        }
    }

    /* update sidetone mixing method */
    op_extra_data->sidetone_method = sidetone_method;

}

/**
 * aec_reference_mic_spkr_include_sidetone
 * \brief updates speaker and mic graphs to include/exclude side tone mixing
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \param include_sidetone whether to add or remove sidetone from graphs
 */
bool aec_reference_mic_spkr_include_sidetone(AEC_REFERENCE_OP_DATA* op_extra_data, bool include_sidetone)
{
    cbops_graph *spkr_graph = op_extra_data->spkr_graph;
    cbops_graph *mic_graph = op_extra_data->mic_graph;

    /* we need to have a speaker graph */
    if(NULL == spkr_graph || NULL == mic_graph)
    {
        /* No action if we don't have a speaker graph
         * however in that case we expect the sidetone is
         * not active.
         */
        PL_ASSERT(op_extra_data->spkr_sidetone_active == FALSE);
        return TRUE;
    }

    if(!include_sidetone == !op_extra_data->spkr_sidetone_active)
    {
        /* Also no action if new request is the same
         * as current state.
         */
        return TRUE;
    }

    if(include_sidetone)
    {
        /** -------------------- MIC SIDETONE SUB PATH ---------------------- **/
        /* side tone buffer size, 2ms more than task period */
        unsigned sidetone_buf_size = frac_mult(op_extra_data->spkr_rate,
                                               op_extra_data->task_period_frac + FRACTIONAL(0.002));
        cbops_op *after = op_extra_data->mic_st_point;

        /* number of ops in mic sidetone path */
        unsigned mic_num_st_ops = 0;
        unsigned  mic_st_rs_idx;
        cbops_op    *op_ptr;

        /* we don't expect SidetoneOA buffer already existing at this point */
        PL_ASSERT(op_extra_data->sidetone_buf == NULL);

        /* Allocate Buffer between cbops Graphs */
        op_extra_data->sidetone_buf = cbuffer_create_with_malloc_fast(sidetone_buf_size, BUF_DESC_SW_BUFFER);

        if(!op_extra_data->sidetone_buf)
        {
            /* Not going ahead with creating mic sub path if we
             * cannot create shared buffer for sidetone path.
             */
            return FALSE;
        }

        /* Minimum space needed in buffer */
        unsigned threshold = frac_mult(op_extra_data->spkr_rate,op_extra_data->task_period_frac) + 1;

        if(op_extra_data->mic_rate != op_extra_data->spkr_rate)
        {
            /* Sidetone filter is inplace */
            mic_st_rs_idx = op_extra_data->mic_st_input_idx;
        }
        else
        {
            /* no resampler, sidetone filter will transfer from internal buffer to output */
            mic_st_rs_idx = op_extra_data->mic_st_idx;
        }

        /* set sidetone output buffer */
        cbops_set_output_io_buffer(mic_graph, op_extra_data->mic_st_idx, op_extra_data->mic_st_idx, op_extra_data->sidetone_buf);

        /*  Add Sidetone operators:
            Sidetone Apply Operator
            Note:  Sidetone is before resampler.
            Better solution is to place it at lowest sample rate */

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
        {
            HL_LIMITER_UI hl_ui;

            /* Map the Howling limiter user interface parameters */
            map_hl_ui(&op_extra_data->params, &hl_ui);

            op_ptr = create_howling_limiter_op(
                op_extra_data->mic_st_input_idx,
                op_extra_data->mic_rate,
                &hl_ui);
            op_extra_data->mic_howling_limiter_op = op_ptr;
            if(!op_ptr)
            {
                return FALSE;
            }

            /* insert howling limiter op into mic cbops graph */
            cbops_insert_operator_into_graph(mic_graph, op_ptr, after);
            after = op_ptr;
            mic_num_st_ops++;
        }
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

        op_ptr = create_sidetone_filter_op(op_extra_data->mic_st_input_idx, mic_st_rs_idx, 3,
                                           (cbops_sidetone_params*)&op_extra_data->params.OFFSET_ST_CLIP_POINT,
                                           (void*)&op_extra_data->params.OFFSET_ST_PEQ_CONFIG);

        op_extra_data->mic_sidetone_op = op_ptr;
        if(!op_ptr)
        {
            return FALSE;
        }

        /* insert resamler op into mic cbops graph */
        cbops_insert_operator_into_graph(mic_graph, op_ptr, after);
        after = op_ptr;
        mic_num_st_ops++;

        if(mic_st_rs_idx != op_extra_data->mic_st_idx)
        {
            // create resampler only for one in & out channel
            op_ptr = create_iir_resamplerv2_op(1,
                                               &mic_st_rs_idx,
                                               &op_extra_data->mic_st_idx,
                                               op_extra_data->mic_rate,
                                               op_extra_data->spkr_rate,
                                               op_extra_data->resampler_temp_buffer_size,
                                               op_extra_data->resampler_temp_buffer, 0, 0, 0);
            if(!op_ptr)
            {
                return FALSE;
            }
            cbops_insert_operator_into_graph(mic_graph, op_ptr, after);
            after = op_ptr;
            mic_num_st_ops++;
        }

        /* Add in disgard on sidetone */

        op_ptr = create_sink_overflow_disgard_op(1,&op_extra_data->mic_st_idx,threshold);
#if defined(IO_DEBUG)
        op_extra_data->st_disgard_op = op_ptr;
#endif
        if(!op_ptr)
        {
            return FALSE;
        }
        cbops_insert_operator_into_graph(mic_graph,op_ptr, after);
        mic_num_st_ops++;

        /* save last operator in sidetone sub-path,
         * will be needed when removing the graph
         */
        op_extra_data->mic_st_last_op = op_ptr;
        op_extra_data->mic_num_st_ops = mic_num_st_ops;

        /** -------------------- SPKR SIDETONE SUB PATH ---------------------- **/
        /* Add sidetone mixer to resampler section,  Master channel only */
        cbops_set_input_io_buffer(spkr_graph,
                                  op_extra_data->spkr_st_in_idx,
                                  op_extra_data->spkr_st_in_idx,
                                  op_extra_data->sidetone_buf);

        /* create a multi-channel sidetone mix operator,
         * number of main channels = number of speakers
         * number of sidetone channels = 1
         */
        unsigned num_st_mix_channels  = op_extra_data->num_spkr_channels;
        unsigned spkr_channel_status = GetSpkrChannelStatus(op_extra_data);
        /* sidetone adjust threshold = (one task period + max_jitter) in speaker rate,
         * note that op_extra_data->spkr_out_threshold is one task period (+1 sample)
         */
        unsigned adjust_threshold = op_extra_data->spkr_out_threshold + frac_mult(op_extra_data->spkr_rate,
                                                                                  FRACTIONAL(AEC_REF_SIDETONE_CONSUMING_JITTER_MS/1000.0));
        if((spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA) == 0)
        {
            /* speaker path isn't parallel channels,
             * sidetone mix has only one main channel
             */
            num_st_mix_channels = 1;
        }
        op_ptr = create_multichan_sidetone_mix_op(
            num_st_mix_channels,                /* number of main channels */
            op_extra_data->spkr_stmix_in_idx,   /* idx for first input channel */
            op_extra_data->spkr_stmix_in_idx,   /* idx for first output channel */
            1,                                  /* number of sidetone channels */
            op_extra_data->spkr_st_in_idx,      /* idx for first sidetone input */
            op_extra_data->spkr_out_threshold,  /* threshold for latency control */
            adjust_threshold);                  /* threshold for controling initial latency */

        if(op_ptr == NULL)
        {
            return FALSE;
        }
        /* configure op to mix the sidetone input into all main channels */
        cbops_sidetone_mix_map_one_to_all(op_ptr, 0);

        /* insert sidetone mix operator into speaker graph */
        cbops_insert_operator_into_graph(spkr_graph, op_ptr, op_extra_data->spkr_st_point_op);

        /* save the sidetone mix operator */
        op_extra_data->spkr_stmix_op = op_ptr;

        /* now speaker graph has sidetone mix operator */
        op_extra_data->spkr_sidetone_active = TRUE;

        DEBUG_GRAPHS("AEC REFERENCE: Side tone path added between mic and speaker paths!" );
    }
    else
    {
        /** -------------------- REMOVING MIC SIDETONE SUB PATH ---------------------- **/
        /* removing sub-graph starts from its last operator */
        cbops_op *op = op_extra_data->mic_st_last_op;
        unsigned i;

        /* remove all the ops in the sidetone sub-path */
        for (i=0; i < op_extra_data->mic_num_st_ops; ++i)
        {
            cbops_op *prev_op = op->prev_operator_addr;
            cbops_remove_operator_from_graph(mic_graph, op);
            op = prev_op;
        }

        /*  tell the cbops not to care about sidetone buffer any more */
        cbops_unset_buffer(mic_graph, op_extra_data->mic_st_idx);

        op_extra_data->mic_st_last_op = NULL;

        op_extra_data->mic_num_st_ops = 0;

        op_extra_data->mic_sidetone_op = NULL;

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
        op_extra_data->mic_howling_limiter_op = NULL;
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

        /** -------------------- REMOVING SPKR SIDETONE SUB PATH ---------------------- **/
        /* remove the sidetone mix operator from speaker graph */
        PL_ASSERT(NULL != op_extra_data->spkr_stmix_op);
        cbops_remove_operator_from_graph(spkr_graph, op_extra_data->spkr_stmix_op);
        op_extra_data->spkr_stmix_op = NULL;

        /* also tell the cbops not to care about sidetone buffer any more */
        cbops_unset_buffer(spkr_graph, op_extra_data->spkr_st_in_idx);

        /* speaker graph no longer has sidetone mix operator*/
        op_extra_data->spkr_sidetone_active = FALSE;

        /* Free Sidetone buffers, this must be
         * done after removing sidetone mix from
         * speaker graph.
         */
        if(op_extra_data->sidetone_buf != NULL)
        {
            cbuffer_destroy(op_extra_data->sidetone_buf);
            op_extra_data->sidetone_buf = NULL;
        }

        DEBUG_GRAPHS("AEC REFERENCE: Side tone path removed!" );
    }

    return TRUE;
}

/**
 * aec_reference_spkr_include_ref_path
 * \brief updates speaker graph to include/exclude path for reference output
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool aec_reference_spkr_include_ref_path(AEC_REFERENCE_OP_DATA* op_extra_data, bool include_ref_path)
{
    cbops_graph *spkr_graph = op_extra_data->spkr_graph;
    cbops_op    *op_ptr;
    unsigned ref_idx = op_extra_data->spkr_ref_idx;

    /* No action if no speaker graph */
    if(NULL == spkr_graph)
    {
        /* nothing to do but we expect no reference output */
        PL_ASSERT(op_extra_data->spkr_ref_active == FALSE);
        return TRUE;
    }

    if(!include_ref_path == !op_extra_data->spkr_ref_active)
    {
        /* Also no action if new request is the same
         * as current state.
         */
        return TRUE;
    }

    if(include_ref_path)
    {
        /* Adding Reference path to speaker graph */

        unsigned ref_input_idx = op_extra_data->spkr_ref_input_idx;
        unsigned ref_rm_in_idx = ref_input_idx;
        cbops_op *after = op_extra_data->spkr_ref_point_op;
        unsigned spkr_num_ref_ops = 0;

        /* set buffer index for reference output in cbops graph */
        cbops_set_output_io_buffer(spkr_graph,
                                   ref_idx,
                                   ref_idx,
                                   op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL]);

        /* see if resampler is needed in reference path */
        if(op_extra_data->input_rate != op_extra_data->output_rate)
        {
            /* resampler outputs into internal buffer */
            ref_rm_in_idx = ref_idx+1;

            /* NOTE: left scratch input is input to resampler */

            cbops_set_internal_io_buffer(spkr_graph,
                                         ref_rm_in_idx,
                                         ref_rm_in_idx,
                                         op_extra_data->scratch_bufs[op_extra_data->spkr_ref_scratch_idx]);

            /* Add reference sample rate conversion (input_rate --> output_rate) - for 1 channel only */
            op_ptr = create_iir_resamplerv2_op(1, &ref_input_idx, &ref_rm_in_idx,
                                               op_extra_data->input_rate, op_extra_data->output_rate,
                                               op_extra_data->resampler_temp_buffer_size,
                                               op_extra_data->resampler_temp_buffer, 0, 0, 0);

            if(op_ptr == NULL)
            {
                return FALSE;
            }

            /* save the resampler op and it's scratch buffer index */
            op_extra_data->spkr_ref_rs_op = op_ptr;
            op_extra_data->spkr_ref_rs_idx = ref_rm_in_idx;

            /* insert resamler op into speaker cbops graph */
            cbops_insert_operator_into_graph(spkr_graph,op_ptr, after);
            after = op_ptr;
            spkr_num_ref_ops++;
        }

        /*  SW rate adjustment for reference */
        op_ptr = create_sw_rate_adj_op(1, &ref_rm_in_idx, &ref_idx,
                                       CBOPS_RATEADJUST_COEFFS,
                                       &op_extra_data->sync_block.rm_adjustment, 0);
        if(op_ptr == NULL)
        {
            return FALSE;
        }

        /* store reference rate adjust operator */
        op_extra_data->ref_sw_rateadj_op = op_ptr;
        if(op_extra_data->mic_sync_enable)
        {
            /* if input and output are in the same clock, then mic output
             * can get sychronised to ref, and ref won't need rate adjustment.
             * The operator will be doing simple copy here.
             */
            cbops_rateadjust_passthrough_mode(op_ptr, TRUE);
        }

        /* insert rate adjust op into speaker cbops graph*/
        cbops_insert_operator_into_graph(spkr_graph, op_ptr, after);
        after = op_ptr;
        spkr_num_ref_ops++;

        /* speaker latency cbops operator */
        op_ptr = create_speaker_latency_op(ref_idx, &op_extra_data->sync_block);
        if(op_ptr == NULL)
        {
            return FALSE;
        }
        /* insert speaker latency op into speaker cbops graph*/
        cbops_insert_operator_into_graph(spkr_graph, op_ptr, after);
        op_extra_data->ref_latency_index = ref_idx;
        spkr_num_ref_ops++;

        /* save last operator in the reference sub-path and
         * also the number of operator in the sub-path,
         * these will be required when removing the
         * reference path
         */
        op_extra_data->spkr_ref_last_op = op_ptr;
        op_extra_data->spkr_num_ref_ops = spkr_num_ref_ops;

        /* Now speaker graph includes reference sub path */
        op_extra_data->spkr_ref_active = TRUE;

        /* have reasonable distance between MIC and REF at the beginning,
         * assumes MIC buffer is empty now.
         */
        cbops_refresh_buffers(spkr_graph);

        DEBUG_GRAPHS("AEC REFERENCE: Reference path added!" );
    }
    else
    {
        /* removing sub-graph starts from its last operator */
        cbops_op *op = op_extra_data->spkr_ref_last_op;
        unsigned i;

        /* remove all the ops in the reference sub-path */
        for (i=0; i < op_extra_data->spkr_num_ref_ops; ++i)
        {
            cbops_op *prev_op = op->prev_operator_addr;
            cbops_remove_operator_from_graph(spkr_graph, op);
            op = prev_op;
        }

        /*  tell the cbops not to care about reference buffer any more */
        cbops_unset_buffer(spkr_graph, ref_idx);

        if(op_extra_data->spkr_ref_rs_op != NULL)
        {
            /* if we have resampler in the reference path
             * then also the reserved index buffer should
             * get unset.
             */
            cbops_unset_buffer(spkr_graph, op_extra_data->spkr_ref_rs_idx);
            op_extra_data->spkr_ref_rs_op = NULL;
        }

        op_extra_data->spkr_num_ref_ops = 0;
        op_extra_data->spkr_ref_last_op = NULL;
        op_extra_data->ref_sw_rateadj_op = NULL;

        /* speaker graph no longer has reference sub-path */
        op_extra_data->spkr_ref_active = FALSE;
        DEBUG_GRAPHS("AEC REFERENCE: Reference path removed!" );
    }
    return TRUE;
}

/**
 * aec_reference_cleanup_graphs
 * \brief clean up all the cbops graphs
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_graphs(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned i;

    patch_fn_shared(aec_reference);

    /* Kill timer Task */
    timer_cancel_event_atomic(&op_extra_data->kick_id);

#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
    /* cleanup sidetone graph if we have one */
    if(NULL != op_extra_data->sidetone_graph)
    {
        /* clean sidetone graph */
        aec_reference_cleanup_sidetone_graph(op_extra_data);
    }
#endif
    /* clean mic graph */
    aec_reference_cleanup_mic_graph(op_extra_data);

    /* clean speaker graph */
    aec_reference_cleanup_spkr_graph(op_extra_data);

    /* Free Internal buffers */
    for(i=0;i<AEC_NUM_SCRATCH_BUFFERS;i++)
    {
        if(op_extra_data->scratch_bufs[i] != NULL)
        {
            cbuffer_destroy(op_extra_data->scratch_bufs[i]);
            op_extra_data->scratch_bufs[i] = NULL;
        }
    }

    /* clear scratch buffer used by resampler operator */
    if(op_extra_data->resampler_temp_buffer != NULL)
    {
        pfree(op_extra_data->resampler_temp_buffer);
        op_extra_data->resampler_temp_buffer = NULL;
    }

    DEBUG_GRAPHS("AEC REFERENCE: Full graphs cleanup done!" );
}

/**
 * aec_reference_cleanup_mic_graph
 * \brief clean up mic graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_mic_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);

    /* Clear links to graphs */
    op_extra_data->mic_rate_monitor_op = NULL;
    op_extra_data->mic_sw_rateadj_op = NULL;
    op_extra_data->mic_mute_op = NULL;

    /* if we have active sidetone path then remove
     * it from speaker graph
     */
    if(op_extra_data->spkr_sidetone_active)
    {
        aec_reference_mic_spkr_include_sidetone(op_extra_data, FALSE);
    }

    /* Free cbops mic graph */
    if(op_extra_data->mic_graph != NULL)
    {
        destroy_graph(op_extra_data->mic_graph);
        op_extra_data->mic_graph = NULL;
    }

    /* update the state of sidetone */
    aec_reference_update_sidetone_status(op_extra_data);

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    op_extra_data->mic_metadata_tag_left_words = 0;
#endif

    DEBUG_GRAPHS("AEC REFERENCE: MIC graph's cleanup done!" );
}

/**
 * aec_reference_cleanup_spkr_graph
 * \brief clean up speaker graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_spkr_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);

#if defined(IO_DEBUG)
    op_extra_data->insert_op = NULL;
    op_extra_data->st_disgard_op = NULL;
#endif

    /* Clear links to graphs */
    op_extra_data->spkr_rate_monitor_op = NULL;
    op_extra_data->spkr_sw_rateadj_op = NULL;
    op_extra_data->spkr_ref_point_op = NULL;
    op_extra_data->spkr_st_point_op = NULL;
    op_extra_data->spkr_ref_last_op = NULL;
    op_extra_data->spkr_ref_rs_op = NULL;
    op_extra_data->ref_sw_rateadj_op = NULL;

    /* if we have active sidetone path then remove
     * it from speaker graph
     */
    if(op_extra_data->spkr_sidetone_active)
    {
        aec_reference_mic_spkr_include_sidetone(op_extra_data, FALSE);
    }

    /* destroy speaker graph */
    if(op_extra_data->spkr_graph != NULL)
    {
        destroy_graph(op_extra_data->spkr_graph);
        op_extra_data->spkr_graph = NULL;
    }

    /* clear flag for reference path */
    op_extra_data->spkr_ref_active = FALSE;

    /* clear flag for sidetone path */
    op_extra_data->spkr_sidetone_active = FALSE;

#ifdef AEC_REFERENCE_SPKR_TTP
    /* destroy any structure allocated for ttp playback */
    aec_reference_spkr_ttp_terminate(op_extra_data);
#endif /* AEC_REFERENCE_SPKR_TTP */

#ifdef AEC_REF_CALC_SPKR_RATE_MONITOR_AMOUNT
    /* Reset flag showing speaker started to consume data */
    op_extra_data->spkr_flow_started = FALSE;
#endif

    /* TODO - Fill speakers with silence */

    DEBUG_GRAPHS("AEC REFERENCE: Speaker graph's cleanup done!" );
}

/**
 * aec_reference_cleanup_spkr_graph
 * \brief manages allocations of scratch buffers needed for cbops graphs.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool aec_reference_allocate_scratch_buffers_for_cbops_graphs(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned i;
    unsigned num_scratch_buffs;
    unsigned size;
    unsigned max_sample_rate;
    bool scratch_buff_resized = FALSE;

    /* find maximum sample rate in active paths (8000 if none is active) */
    max_sample_rate = 8000;
    if(op_extra_data->num_spkr_channels > 0)
    {
        /* speaker path rates are valid */
        max_sample_rate = pl_max(max_sample_rate, op_extra_data->spkr_rate);
        max_sample_rate = pl_max(max_sample_rate, op_extra_data->input_rate);
    }
    if(op_extra_data->num_mic_channels > 0)
    {
        /* mic path rates are valid */
        max_sample_rate = pl_max(max_sample_rate, op_extra_data->mic_rate);
        max_sample_rate = pl_max(max_sample_rate, op_extra_data->output_rate);
    }
    /* max_io_rate is just for info */
    op_extra_data->max_io_rate = max_sample_rate;

    /* 1ms more than a task period for max sample rate in all corners.
     */
    size = frac_mult(max_sample_rate, op_extra_data->task_period_frac + FRACTIONAL(0.001));

    /* Min size, was constant size of 100 words before introducing,
     * configurable task period, don't go below that tested value.
     */
    size = pl_max(size, 100);

    /* get the number of required scratch buffers based
     * on the number of speakers and mics that we have.
     * Check also at compile time that we can have enough scratch
     * buffer for worst case.
     */
    COMPILE_TIME_ASSERT(AEC_NUM_SCRATCH_BUFFERS>=(2*MAX_NUMBER_MICS), AEC_REFERENCE_NotEnoughScratchBuffersForMics);
    COMPILE_TIME_ASSERT(AEC_NUM_SCRATCH_BUFFERS>=(2*MAX_NUMBER_SPEAKERS), AEC_REFERENCE_NotEnoughScratchBuffersForSpkrs);
    num_scratch_buffs = (pl_max(op_extra_data->num_spkr_channels,
                                op_extra_data->num_mic_channels))*2;

    /* create scratch buffers, some of them might
     * already have been created */
    for(i = 0; i < num_scratch_buffs; i++)
    {
        if(NULL == op_extra_data->scratch_bufs[i])
        {
            /* scratch buf required and not allocated, allocate it now  */
            op_extra_data->scratch_bufs[i] = cbuffer_create_with_malloc_fast(size, BUF_DESC_SW_BUFFER);
            if(NULL == op_extra_data->scratch_bufs[i])
            {
                return FALSE;
            }
        }
        else if (cbuffer_get_size_in_words(op_extra_data->scratch_bufs[i]) != size)
        {
            /* scratch buffer required, it already exists but the size is not what we want,
             * so resize it. We could keep using it if current size was bigger, however that
             * might keep a big part of that unused for the whole running time of the graphs.
             */

            /* destroy old base and create and use a new one;
             * Note: This shouldn't be interrupted by the op's task. This is the case, since
             *       either task isn't running or interrupts are blocked when we are here.
             */
            int *new_base;
            pfree(op_extra_data->scratch_bufs[i]->base_addr);
            new_base = (int*) xppmalloc(sizeof(int)*size, MALLOC_PREFERENCE_FAST);
            if(new_base == NULL)
            {
                /* Failed to create new base, destroy the rest of old scratch buffer,
                 * Note that we this function returns failure the operator will not
                 * be able to continue and all graph info including scratch buffers
                 * will be cleaned up.
                 */
                cbuffer_destroy_struct(op_extra_data->scratch_bufs[i]);
                op_extra_data->scratch_bufs[i] = NULL;

                /* failed to resize. */
                return FALSE;
            }

            /* use new base */
            cbuffer_buffer_configure(op_extra_data->scratch_bufs[i], new_base, size, BUF_DESC_SW_BUFFER);
            scratch_buff_resized = TRUE;
        }
    }

    /* Any scratch buffer above num_scratch_buffs will not be used. All scratch buffers are
     * freed when operator stops however for efficiency we free any previously allocated
     * scratch buffers that no longer will be used.
     */
    for(i = num_scratch_buffs; i < AEC_NUM_SCRATCH_BUFFERS; ++i)
    {
        cbuffer_destroy(op_extra_data->scratch_bufs[i]);
        op_extra_data->scratch_bufs[i] = NULL;
    }

    if(scratch_buff_resized)
    {
        /* if any scratch buffer resized we need to refresh
         * buffers of all currently running graphs.
         */
        if(op_extra_data->spkr_graph != NULL)
        {
            cbops_refresh_buffers(op_extra_data->spkr_graph);
        }
        if(op_extra_data->mic_graph != NULL)
        {
            cbops_refresh_buffers(op_extra_data->mic_graph);
        }
#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
        if(op_extra_data->sidetone_graph != NULL)
        {
            cbops_refresh_buffers(op_extra_data->sidetone_graph);
        }
#endif
    }

    /* scratch buffers allocated successfully */
    return TRUE;
}

/**
 * aec_reference_build_graphs
 * \brief main function that creates mic, speaker or sidetone graphs
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \param spkr_changed whether there has been a change in speaker path
 * \param mic_changed whether there has been a change in mic path
 */
bool aec_reference_build_graphs(AEC_REFERENCE_OP_DATA *op_extra_data, bool spkr_changed, bool mic_changed)
{

    patch_fn_shared(aec_reference);

    /* nothing to do if neither speaker nor mic has changed */
    if(!spkr_changed && !mic_changed)
    {
        return TRUE;
    }

    if(!aec_reference_allocate_scratch_buffers_for_cbops_graphs(op_extra_data))
    {
        /* was unable to allocate required scratch buffers */
        return FALSE;
    }

    /* allocate buffer needed for resampler */
    if(op_extra_data->resampler_temp_buffer == NULL)
    {
        /* resampler_temp_buffer_size must already have been calculated */
        PL_ASSERT(0 != op_extra_data->resampler_temp_buffer_size);

        op_extra_data->resampler_temp_buffer =
            (unsigned*)xzpmalloc(op_extra_data->resampler_temp_buffer_size*sizeof(unsigned));
        if(op_extra_data->resampler_temp_buffer == NULL)
        {
            return FALSE;
        }
    }

    /** Setup Latency Control */
    {
        unsigned one_task_samples = frac_mult(op_extra_data->output_rate, op_extra_data->task_period_frac);
        unsigned one_ms_samples = frac_mult(op_extra_data->output_rate, FRACTIONAL(0.001));
        op_extra_data->sync_block.jitter     =  one_task_samples + one_task_samples/2;               /* 1.5 times task period */
        op_extra_data->sync_block.ref_delay  = one_ms_samples;                                       /* 1.0 msec */
        op_extra_data->sync_block.block_sync = 0;
        op_extra_data->sync_block.rm_adjustment = 0;
        op_extra_data->sync_block.min_space = one_task_samples + one_ms_samples/2;
    }

    if(spkr_changed)
    {
        /* any change in main speaker path will cause full
         * rebuild of every thing from scratch
         */
        aec_reference_cleanup_spkr_graph(op_extra_data);
        /* rebuild speaker graph */
        if(!build_spkr_graph(op_extra_data))
        {
            return FALSE;
        }
    }

    if(mic_changed)
    {
        /* if only mic has changed, then only mic graph will be rebuilt */
        aec_reference_cleanup_mic_graph(op_extra_data);

        /* mic graph is rebuilt if there is a change
         * in either mic path or speaker path */
        if(!build_mic_graph(op_extra_data))
        {
            return FALSE;
        }
    }

    /* update mic and speaker graphs to include sidetone mix if required */
    if(!aec_reference_mic_spkr_include_sidetone(
           op_extra_data, op_extra_data->sidetone_method == AEC_SIDETONE_IN_SW_BY_MIC_SPKR_GRAPH))
    {
        return FALSE;
    }

#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
    /* build separate sidetone graph if required */
    if(!build_sidetone_graph(op_extra_data))
    {
        return FALSE;
    }
#endif /* AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING */

    /*  Re-init */
    op_extra_data->ReInitFlag = TRUE;

    return TRUE;
}

bool aec_reference_start(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    patch_fn_shared(aec_reference);

    /* Setup Response to Start Request.   Assume Failure*/
    if(!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data))
    {
        return(FALSE);
    }
    /* do something only if the current state is "connected" */
    if (opmgr_op_is_running(op_data))
    {
        base_op_change_response_status(response_data,STATUS_OK);
        return(TRUE);
    }

    /* Validate channel configuration */
    if(!validate_channels_and_build(op_data))
    {
        return(TRUE);
    }
#ifdef AEC_REFERENCE_SPKR_TTP
    {
        AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
        /* initialisation for timed playback in speaker path */
        aec_reference_spkr_ttp_init(op_extra_data);
    }
#endif /* AEC_REFERENCE_SPKR_TTP */

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
    {
        AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

        if(op_extra_data->mic_howling_limiter_op)
        {
            HL_LIMITER_UI hl_ui;

            /* Map the Howling limiter user interface parameters */
            map_hl_ui(&op_extra_data->params, &hl_ui);

            initialize_howling_limiter_op(
                    op_extra_data->mic_howling_limiter_op,
                    op_extra_data->mic_rate,
                    &hl_ui);
        }
    }
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

    base_op_change_response_status(response_data,STATUS_OK);

    return TRUE;
}

bool aec_reference_stop_reset(OPERATOR_DATA *op_data,void *message_data, unsigned *response_id, void **response_data)
{
    patch_fn_shared(aec_reference);

    if(!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data))
    {
        return(FALSE);
    }
    /* do something only if the current state is "running" */
    if (opmgr_op_is_running(op_data))
    {
        aec_reference_cleanup(op_data);
    }
    /* Mark the operator as stopped. */
    base_op_stop_operator(op_data);
    base_op_change_response_status(response_data,STATUS_OK);
    return TRUE;
}

/**
 * aec_reference_update_stream_and_validate_channels
 * \brief updates a stream buffer and re-validates graphs.
 * \param op_data Pointer to the operator instance data.
 * \param bufp address to store cbuffer pointer, if NULL no buffer changes
 * \param bufval, cbuffer buffer pointer for new connection (NULL for disconnection)
 *
 * \return whether re-validation carried out successfully
 */
bool aec_reference_update_stream_and_validate_channels(OPERATOR_DATA *op_data, tCbuffer **bufp, tCbuffer *bufval)
{
    patch_fn_shared(aec_reference);

    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    /* if operator isn't running just set the stream buffer and exit */
    if (!opmgr_op_is_running(op_data))
    {
        if(bufp != NULL)
        {
            *bufp = bufval;
        }

        return TRUE;
    }

    /* block interrupts if the op has a runnig task */
    bool interrupts_blocked = FALSE;
    if(op_extra_data->kick_id != TIMER_ID_INVALID)
    {
        LOCK_INTERRUPTS;
        interrupts_blocked = TRUE;
    }

    /* set the stream buffer */
    if(bufp != NULL)
    {
        *bufp = bufval;
    }

    /* revisit the graphs based on latest connections */
    bool retval = validate_channels_and_build(op_data);

    /* unblock interrupts if we blocked them */
    if(interrupts_blocked)
    {
        UNLOCK_INTERRUPTS;
    }

    return retval;
}

bool aec_reference_connect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);    /* extract the terminal_id */
    tCbuffer* pterminal_buf = OPMGR_GET_OP_CONNECT_BUFFER(message_data);
    tCbuffer **bufp = NULL;

    patch_fn_shared(aec_reference);

    L3_DBG_MSG1("AEC REFERENCE --- connect TID = %x", terminal_id);


    /* Setup Response to Connection Request.   Assume Failure*/
    if(!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data))
    {
        return(FALSE);
    }

    /* Only allow connection if operator has been configured */
    if((op_extra_data->input_rate==0) || (op_extra_data->output_rate==0) )
    {
        return(TRUE);
    }

    /* (i)  check if the terminal ID is valid . The number has to be less than the maximum number of sinks or sources .  */
    /* (ii) check if we are connecting to the right type . It has to be a buffer pointer and not endpoint connection */
    if( !base_op_is_terminal_valid(op_data, terminal_id) || !pterminal_buf)
    {
        base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* check if the terminal is already connected and if not , connect the terminal */
    if(terminal_id & TERMINAL_SINK_MASK)
    {
        terminal_id &= ~ TERMINAL_SINK_MASK;

        if(terminal_id==AEC_REF_MIC_TERMINAL1)
        {
            RATEMATCHING_SUPPORT ability;

            /* get info about overridden endpoints */
            op_extra_data->mic_endpoint = opmgr_override_get_endpoint(op_data,
                                                                      AEC_REF_MIC_TERMINAL1 | TERMINAL_SINK_MASK);

            if (!opmgr_override_get_ratematch_ability(op_extra_data->mic_endpoint,
                                                      &ability))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }
#if defined(ENABLE_FORCE_SW_RATEMATCH)
            /* Save rate matching ability HW or SW*/
            op_extra_data->mic_rate_ability   = RATEMATCHING_SUPPORT_SW;
            op_extra_data->mic_rate_enactment = RATEMATCHING_SUPPORT_SW;
#else
            /* Save rate matching ability HW or SW*/
            op_extra_data->mic_rate_ability   = ability;
            op_extra_data->mic_rate_enactment = RATEMATCHING_SUPPORT_NONE;
#endif
            if (op_extra_data->mic_sync_enable)
            {
                /* Mic output will be synchronised to speaker input, SW or HW depends on
                 * it's ability.
                 */
                op_extra_data->mic_rate_enactment = op_extra_data->mic_rate_ability;
                if (op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
                {
                    opmgr_override_set_ratematch_enacting(op_extra_data->mic_endpoint,
                                                          TRUE);
                }
            }

#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
            /* see if mic graph wants to use an external rate adjust operator */
            aec_reference_mic_check_external_rate_adjust_op(op_extra_data);
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

            if (!opmgr_override_get_sample_rate(op_extra_data->mic_endpoint,
                                                &op_extra_data->mic_rate))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,
                                               STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }
        }
        else if(terminal_id==AEC_REF_INPUT_TERMINAL1)
        {
            op_extra_data->spkr_in_endpoint = opmgr_override_get_endpoint(op_data,
                                                                          AEC_REF_INPUT_TERMINAL1 | TERMINAL_SINK_MASK);
        }
        bufp = &op_extra_data->input_stream[terminal_id];

#ifdef AEC_REFERENCE_SUPPORT_METADATA
        /* Metadata might be enabled for speaker graph inputs
         * in multi input cases we expect all use same metadata
         * buffer.
         */
        if(IsSpeakerInputTerminal(terminal_id) &&
           buff_has_metadata(pterminal_buf))
        {
            /* set metadata buffer if it hasn't been set already */
            if(NULL == op_extra_data->spkr_input_metadata_buffer)
            {
                op_extra_data->spkr_input_metadata_buffer = pterminal_buf;
            }
        }
#endif /* AEC_REFERENCE_SUPPORT_METADATA */
    }
    else
    {
        if(terminal_id==AEC_REF_SPKR_TERMINAL1)
        {
            RATEMATCHING_SUPPORT ability;

            op_extra_data->spkr_endpoint = opmgr_override_get_endpoint(op_data,
                                                                       AEC_REF_SPKR_TERMINAL1);

            if (!opmgr_override_get_ratematch_ability(op_extra_data->spkr_endpoint,
                                                      &ability))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }

#if defined(ENABLE_FORCE_SW_RATEMATCH)
            op_extra_data->spkr_rate_ability   = RATEMATCHING_SUPPORT_SW;
            op_extra_data->spkr_rate_enactment = RATEMATCHING_SUPPORT_SW;
#else
            op_extra_data->spkr_rate_ability   = ability;
            op_extra_data->spkr_rate_enactment = RATEMATCHING_SUPPORT_NONE;
#endif

#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
            /* see if speaker graph wants to use an external rate adjust operator */
            aec_reference_spkr_check_external_rate_adjust_op(op_extra_data);
#endif
            if (!opmgr_override_get_sample_rate(op_extra_data->spkr_endpoint,
                                                &op_extra_data->spkr_rate))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,
                                               STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }

        }
        else if (terminal_id==AEC_REF_OUTPUT_TERMINAL1)
        {
            op_extra_data->mic_out_endpoint = opmgr_override_get_endpoint(op_data,
                                                                          AEC_REF_OUTPUT_TERMINAL1);
            op_extra_data->sync_block.mic_data = 0;
        }
        else if(terminal_id==AEC_REF_REFERENCE_TERMINAL)
        {
            op_extra_data->sync_block.speaker_data = 0;
        }
        bufp = &op_extra_data->output_stream[terminal_id];

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
       /* set metadata buffer for mic outputs */
       if(IsMicrophoneOutputTerminal(terminal_id) &&
          buff_has_metadata(pterminal_buf))
       {
           if(NULL == op_extra_data->mic_metadata_buffer)
           {   /* first connected mic output buffer with metadata */
               op_extra_data->mic_metadata_buffer = pterminal_buf;
           }
       }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

    }

    /* set the stream buffer for this terminal and rebuild the graphs if needed */
    if(aec_reference_update_stream_and_validate_channels(op_data, bufp, pterminal_buf))
    {
        base_op_change_response_status(response_data,STATUS_OK);
    }
    return TRUE;
}

bool aec_reference_disconnect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_DISCONNECT_TERMINAL_ID(message_data);
    tCbuffer **bufp = NULL;

    patch_fn_shared(aec_reference);

    L3_DBG_MSG1("AEC REFERENCE --- disconnect TID = %x", terminal_id);

    /* Setup Response to Disconnection Request. Assume Failure*/
    if(!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data))
    {
        return(FALSE);
    }

    /* check if the terminal ID is valid . The number has to be less than the maximum number of sinks or sources.  */
    if(!base_op_is_terminal_valid(op_data, terminal_id))
    {
        base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* check if the terminal is connected and if so , disconnect the terminal */
    if(terminal_id & TERMINAL_SINK_MASK)
    {
        terminal_id &= ~ TERMINAL_SINK_MASK;

#ifdef AEC_REFERENCE_SUPPORT_METADATA
        if(IsSpeakerInputTerminal(terminal_id))
        {
            tCbuffer *this_buf = op_extra_data->input_stream[terminal_id];
            if(this_buf == op_extra_data->spkr_input_metadata_buffer)
            {
                /* disconnecting buffer is the metadata buffer,
                 * change the metadata buffer to another connected
                 * buffer with metadata, if there is any.
                 */
                tCbuffer *new_metadata_buf = NULL;
                int idx;
                for(idx=0; idx < MAX_NUMBER_SPEAKERS; idx++)
                {
                    tCbuffer *inp_buf = op_extra_data->input_stream[SpeakerInputTerminalByIndex(idx)];
                    if(inp_buf != NULL &&
                       inp_buf != this_buf &&
                       buff_has_metadata(inp_buf))
                    {
                        new_metadata_buf = inp_buf;
                        break;
                    }
                }
                op_extra_data->spkr_input_metadata_buffer = new_metadata_buf;
            }
        }
#endif /* AEC_REFERENCE_SUPPORT_METADATA */

        if(terminal_id==AEC_REF_MIC_TERMINAL1)
        {
            op_extra_data->mic_endpoint=NULL;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
            if(op_extra_data->mic_ext_rate_adjust_op != 0)
            {
                /* set external op to passthrough mode */
                stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->mic_ext_rate_adjust_op, TRUE);
                op_extra_data->mic_ext_rate_adjust_op = 0;
            }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

        }
        else if(terminal_id==AEC_REF_INPUT_TERMINAL1)
        {
            op_extra_data->spkr_in_endpoint=NULL;
        }
        bufp = &op_extra_data->input_stream[terminal_id];
    }
    else
    {
#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
        if(IsMicrophoneOutputTerminal(terminal_id))
        {
            tCbuffer *this_buf = op_extra_data->output_stream[terminal_id];
            if(this_buf == op_extra_data->mic_metadata_buffer)
            {
                /* disconnecting buffer is the mic metadata buffer,
                 * change the metadata buffer to another connected
                 * buffer with metadata, if there is any.
                 */
                tCbuffer *new_metadata_buf = NULL;
                int idx;
                for(idx=0; idx < MAX_NUMBER_MICS; idx++)
                {
                    tCbuffer *out_buf = op_extra_data->output_stream[OutputTerminalByIndex(idx)];
                    if(out_buf != NULL &&
                       out_buf != this_buf &&
                       buff_has_metadata(out_buf))
                    {
                        new_metadata_buf = out_buf;
                        break;
                    }
                }
                op_extra_data->mic_metadata_buffer = new_metadata_buf;
            }
        }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

        if(terminal_id==AEC_REF_SPKR_TERMINAL1)
        {
            op_extra_data->spkr_endpoint=NULL;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
            if(op_extra_data->spkr_ext_rate_adjust_op != 0)
            {
                /* set external op to passthrough mode */
                stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->spkr_ext_rate_adjust_op, TRUE);
                op_extra_data->spkr_ext_rate_adjust_op = 0;
            }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

        }
        else if(terminal_id==AEC_REF_OUTPUT_TERMINAL1)
        {
            op_extra_data->mic_out_endpoint=NULL;
        }
        bufp = &op_extra_data->output_stream[terminal_id];
    }

    /* clear the stream buffer for this terminal and rebuild the graphs if needed */
    if(aec_reference_update_stream_and_validate_channels(op_data, bufp, NULL))
    {
        base_op_change_response_status(response_data,STATUS_OK);
    }

    return TRUE;
}

bool aec_reference_buffer_details(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    unsigned term_id = OPMGR_GET_OP_BUF_DETAILS_TERMINAL_ID(message_data);
    OP_BUF_DETAILS_RSP *resp;

    patch_fn_shared(aec_reference);

    if(!base_op_buffer_details(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }
    resp = (OP_BUF_DETAILS_RSP*)*response_data;

    if ( term_id & TERMINAL_SINK_MASK)
    {
        if(IsMicrophoneTerminal(term_id))
        {
            /* override MIC endpoints */
            resp->needs_override = TRUE;
            resp->b.buffer_size  = 0;
        }
        else
        {
            if(op_extra_data->input_buffer_size != 0)
            {
                /* buffer size based on user configuration */
                resp->b.buffer_size = op_extra_data->input_buffer_size;
            }
            else
            {
                /* buffer size based on sample rate. task period + 2ms for safety,
                 * 2ms extra should be enough as assumption is that scheduling jitter
                 * assumed will not be too high.
                 */
                resp->b.buffer_size  =
                    frac_mult(op_extra_data->input_rate,FRACTIONAL(0.002)+op_extra_data->task_period_frac);
            }

#ifdef AEC_REFERENCE_SUPPORT_METADATA
            /* currently metadata is supported only for
             * speaker input channels.
             */
            L3_DBG_MSG("AEC_REFERENCE: metadata is supported for speaker inputs");
            resp->metadata_buffer = op_extra_data->spkr_input_metadata_buffer;
            resp->supports_metadata = TRUE;
#endif /* SCO_RX_OP_GENERATE_METADATA */
        }
    }
    else
    {
        if(IsSpeakerTerminal(term_id))
        {
            /* override SPKR endpoints */
            resp->needs_override = TRUE;
            resp->b.buffer_size  = 0;
        }
        else
        {
            if(op_extra_data->output_buffer_size != 0)
            {
                /* buffer size based on user configuration */
                resp->b.buffer_size = op_extra_data->output_buffer_size;

                if(AEC_REF_REFERENCE_TERMINAL == term_id)
                {
                    /* This is for REFERENCE output, for causality REFERENCE output (the reference)
                     * needs to always be ahead of MIC output (the echo), this is controlled by the
                     * latency cbops operator which makes sure REFERENCE buffer vs MIC output is
                     * within [ref_delay, ref_delay+jitter] range. For that reason we add an extra
                     * for ref buffer so mic output can use full output_buffer_size
                     */
                    unsigned ref_extra =
                        frac_mult(op_extra_data->output_rate,
                                  op_extra_data->task_period_frac + FRACTIONAL(0.002));

                    resp->b.buffer_size += ref_extra;
                }
            }
            else
            {
                unsigned two_task_period_size = frac_mult(op_extra_data->output_rate,
                                                          2*op_extra_data->task_period_frac) + 1;
                /* buffer size based on sample rate */
                resp->b.buffer_size  = frac_mult(op_extra_data->output_rate,FRACTIONAL(0.0087));

                /* given that this is for cvc-like operators and limited task periods that the
                 * operator supports, 8.7ms would be adequate for all use cases. In case of very
                 * high task period is used make sure output has space for at least two task period.
                 */
                resp->b.buffer_size = MAX(resp->b.buffer_size,
                                          two_task_period_size);
            }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
            /* Support metadta for microphone output channels (reference channel not included)
             * if enabled by the user.
             */
            if(op_extra_data->mic_metadata_enable &&
               IsMicrophoneOutputTerminal(term_id))
            {
                resp->metadata_buffer = op_extra_data->mic_metadata_buffer;
                resp->supports_metadata = TRUE;
            }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
        }
    }

    return TRUE;
}

bool aec_reference_get_sched_info(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    OP_SCHED_INFO_RSP* resp;
    unsigned terminal_id = OPMGR_GET_OP_SCHED_INFO_TERMINAL_ID(message_data);

    patch_fn_shared(aec_reference);

    resp = base_op_get_sched_info_ex(op_data, message_data, response_id);
    if (resp == NULL)
    {
        return base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data);
    }
    *response_data = resp;

    /* block_size set to 1 if ep marked as "real", else it can be any arbitrary
     * value so simplifying the code by choosing 1. */
    resp->block_size = 1;

    /* The real endpoints are locally clocked if the respective overridden
     * endpoint is locally clocked. */
    if ((terminal_id & TERMINAL_SINK_MASK) && !IsMicrophoneTerminal(terminal_id))
    {
        resp->locally_clocked = opmgr_override_is_locally_clocked(op_extra_data->spkr_endpoint);
    }
    else if (((terminal_id & TERMINAL_SINK_MASK) == 0) && !IsSpeakerTerminal(terminal_id))
    {
        resp->locally_clocked = opmgr_override_is_locally_clocked(op_extra_data->mic_endpoint);
    }
    else
    {
        resp->locally_clocked = TRUE;
    }

    return TRUE;
}

bool aec_reference_get_data_format(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    /* Set up the a default success response information */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, response_data))
    {
        return FALSE;
    }

    ((OP_STD_RSP*)*response_data)->resp_data.data = AUDIO_DATA_FORMAT_FIXP;
    return TRUE;
}

/* ************************************* Data processing-related functions and wrappers **********************************/


/* **************************** Operator message handlers ******************************** */

bool aec_reference_opmsg_obpm_set_control(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    patch_fn_shared(aec_reference);

    /* In the case of this capability, nothing is done for control message. Just follow protocol and ignore any content. */
    return cps_control_setup(message_data, resp_length, resp_data,NULL);
}

bool aec_reference_opmsg_obpm_get_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    return cpsGetParameterMsgHandler(&op_extra_data->parms_def ,message_data, resp_length,resp_data);
}

bool aec_reference_opmsg_obpm_get_defaults(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    return cpsGetDefaultsMsgHandler(&op_extra_data->parms_def ,message_data, resp_length,resp_data);
}

bool aec_reference_opmsg_obpm_set_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    bool retval;

    patch_fn_shared(aec_reference);

    retval = cpsSetParameterMsgHandler(&op_extra_data->parms_def ,message_data, resp_length,resp_data);

    /* Set the Reinit flag after setting the parameters */
    op_extra_data->ReInitFlag = TRUE;

    return retval;
}

bool aec_reference_opmsg_obpm_get_status(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    unsigned  *resp;

#if defined(IO_DEBUG)
    unsigned num_inserts_total  = 0;
    unsigned num_inserts_insert = 0;
    unsigned num_inserts_wrap   = 0;

    unsigned last_acc_mic = 0;
    unsigned last_acc_spkr = 0;
#endif

    patch_fn_shared(aec_reference);

    if(!common_obpm_status_helper(message_data,resp_length,resp_data,sizeof(AEC_REFERENCE_STATISTICS) ,&resp))
    {
        return FALSE;
    }

    if(resp)
    {
        unsigned config_flag = 0;
        unsigned volume=op_extra_data->shared_volume_ptr->current_volume_level;

        if(op_extra_data->using_sidetone==USE_SIDETONE_FLAG)
        {
            config_flag = flag_uses_SIDETONE;
        }
        if(op_extra_data->channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_REF)
        {
            config_flag += flag_uses_AEC_REFERENCE;
        }

        resp = cpsPack2Words(op_extra_data->Cur_mode, op_extra_data->Ovr_Control, resp);
        resp = cpsPack2Words(config_flag, volume, resp);
        resp = cpsPack2Words(op_extra_data->channel_status, op_extra_data->mic_rate, resp);
        resp = cpsPack2Words(op_extra_data->output_rate, op_extra_data->input_rate, resp);
        resp = cpsPack1Word(op_extra_data->spkr_rate, resp);

        /* Rate Matching statistics */
        /* TODO: Make these on demand instead of always ON */

#if defined(IO_DEBUG)
        if (op_extra_data->st_disgard_op != NULL)
        {
            op_extra_data->ref_st_drop = get_sink_overflow_disgard_drops(op_extra_data->st_disgard_op);
        }

        op_extra_data->ref_spkr_refdrop = op_extra_data->sync_block.speaker_drops + op_extra_data->sync_block.speaker_inserts;
        op_extra_data->ref_micref_delay = op_extra_data->sync_block.speaker_delay;

        if (op_extra_data->insert_op != NULL)
        {
            num_inserts_total  = get_aec_ref_cbops_inserts_total(op_extra_data->insert_op);
            num_inserts_insert = get_aec_ref_cbops_insert_op_insert_total(op_extra_data->insert_op);
            num_inserts_wrap   = get_aec_ref_cbops_wrap_op_insert_total(op_extra_data->insert_op);

            if (num_inserts_total != op_extra_data->ref_last_inserts_total)
            {
                op_extra_data->ref_last_inserts_total = num_inserts_total;
                op_extra_data->ref_last_inserts_insert = num_inserts_insert;
                op_extra_data->ref_last_inserts_wrap = num_inserts_wrap;
                op_extra_data->ref_inserts++;
            }
        }

        /* get last acc for mic and speaker,
         * Note: it's fine if rate_monitor_op not existing, it will return 0 */
        last_acc_mic = get_rate_monitor_last_acc(op_extra_data->mic_rate_monitor_op);
        last_acc_spkr = get_rate_monitor_last_acc(op_extra_data->spkr_rate_monitor_op);

        resp = cpsPack2Words(op_extra_data->mic_rate_enactment, op_extra_data->spkr_rate_enactment, resp);
        resp = cpsPack2Words(last_acc_mic, last_acc_spkr, resp);
        resp = cpsPack2Words(op_extra_data->mic_rate_adjustment, op_extra_data->spkr_rate_adjustment, resp);
        resp = cpsPack2Words(op_extra_data->sync_block.rm_adjustment, op_extra_data->ref_last_inserts_total, resp);
        resp = cpsPack2Words(op_extra_data->ref_spkr_refdrop, op_extra_data->ref_st_drop, resp);
        resp = cpsPack1Word(op_extra_data->ref_micref_delay, resp);
#endif

        {
            unsigned hl_detect_cnt = 0;

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
            if (op_extra_data->mic_howling_limiter_op != NULL)
            {
                cbops_howling_limiter *hl_data = CBOPS_PARAM_PTR(op_extra_data->mic_howling_limiter_op, cbops_howling_limiter);

                hl_detect_cnt = hl_data->hl_detect_cnt;
            }
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */

            /* Always assign a detection count statistic (even when not installed or not enabled) */
            resp = cpsPack1Word(hl_detect_cnt, resp);
        }
    }

    return TRUE;
}

/* Callback function for getting parameters from persistent store */
bool ups_params_aec(void* instance_data,PS_KEY_TYPE key,PERSISTENCE_RANK rank,
                    uint16 length, unsigned* data, STATUS_KYMERA status,uint16 extra_status_info)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data((OPERATOR_DATA*)instance_data);

    patch_fn_shared(aec_reference);

    cpsSetParameterFromPsStore(&op_extra_data->parms_def,length,data,status);

    /* Set the Reinit flag after setting the parameters */
    op_extra_data->ReInitFlag = TRUE;

    return(TRUE);
}

bool aec_reference_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    PS_KEY_TYPE key;
    bool retval;

    patch_fn_shared(aec_reference);

    retval = cpsSetUcidMsgHandler(&op_extra_data->parms_def,message_data,resp_length,resp_data);

    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(op_extra_data->cap_id,op_extra_data->parms_def.ucid,OPMSG_P_STORE_PARAMETER_SUB_ID);
    ps_entry_read((void*)op_data,key,PERSIST_ANY,ups_params_aec);

    return retval;
}

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
/**
 * aec_reference_opmsg_set_ttp_latency
 * \brief message handler to set ttp target latency for mic output buffers
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_ttp_latency(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    /* We cant change this setting while running */
    if (opmgr_op_is_running(op_data))
    {
        return FALSE;
    }

    /* get the latency from payload */
    op_extra_data->mic_target_latency = ttp_get_msg_latency(message_data);

    /* configure latency */
    ttp_configure_latency(op_extra_data->mic_time_to_play, op_extra_data->mic_target_latency);

    /* receiving this message (with latenct != 0) will enable metadata generation */
    op_extra_data->mic_metadata_enable = op_extra_data->mic_target_latency != 0;

    return TRUE;
}

/**
 * aec_reference_opmsg_set_latency_limits
 * \brief message handler to set ttp latency limits for mic output channels
 */
bool aec_reference_opmsg_set_latency_limits(OPERATOR_DATA *op_data, void *message_data,
                                        unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    TIME_INTERVAL min_latency, max_latency;

    /* get the limits from the message payload and configure the time to play limits */
    ttp_get_msg_latency_limits(message_data, &min_latency, &max_latency);
    ttp_configure_latency_limits(op_extra_data->mic_time_to_play, min_latency, max_latency);

    return TRUE;
}

/**
 * aec_reference_opmsg_set_ttp_params
 * \brief message handler to set ttp parameters for mic output channels
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_ttp_params(OPERATOR_DATA *op_data, void *message_data,
                                    unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    ttp_params params;

    /* We cant change this setting while running */
    if (opmgr_op_is_running(op_data))
    {
        return FALSE;
    }

    /* configure ttp params */
    ttp_get_msg_params(&params, message_data);
    ttp_configure_params(op_extra_data->mic_time_to_play, &params);

    return TRUE;
}

/**
 * aec_reference_mic_generate_metadata_with_ttp
 * \brief generates metadata for microphone output channels
 * \param op_extra_data Pointer to AEC_REFERENCE operator specific data
 * \param samples number of samples in the to-be-copied chunk
 */
void aec_reference_mic_generate_metadata_with_ttp(AEC_REFERENCE_OP_DATA *op_extra_data, unsigned samples)
{
    metadata_tag *mtag;
    unsigned b4idx, afteridx;
    tCbuffer *met_buf = op_extra_data->mic_metadata_buffer;

    patch_fn_shared(aec_reference);
    /* no update needed if no new samples arrived */
    if(samples == 0)
    {
        return;
    }

    /* if previous tag was incomplete, we need first to complete the tag */
    if(op_extra_data->mic_metadata_tag_left_words > 0)
    {
        /* last written tag was incomplete, we keep adding
         * Null tag until full length of incomplete tag is
         * covered.
         */
        unsigned null_tag_len = op_extra_data->mic_metadata_tag_left_words;
        if(null_tag_len > samples)
        {
            null_tag_len = samples;
        }

        /* append Null tag, with length = null_tag_len */
        b4idx = 0;
        afteridx = null_tag_len*OCTETS_PER_SAMPLE;
        buff_metadata_append(met_buf, NULL, b4idx, afteridx);

        /* update amount left */
        op_extra_data->mic_metadata_tag_left_words -= null_tag_len;
        samples -= null_tag_len;
        if(samples == 0)
        {
            /* all new words used for completing old tag */
            return;
        }
    }

    /* create a new tag to append */
    b4idx = 0;
    afteridx = samples*OCTETS_PER_SAMPLE;
    mtag = buff_metadata_new_tag();
    if (mtag != NULL)
    {
        /* calculating time of arrival for first sample of the tag,
         * we do that by looking how many samples are in the mic buffer,
         * this could be short by up to 1 sample as our reading isn't aligned.
         */
        TIME current_time = time_get_time();

        /* amount of data in the mic buffer */
        unsigned amount_in_buffer =
            cbuffer_calc_amount_data_in_words(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1]);

        /* convert samples to time,
         * Note: some inaccuracies because of rate adjustment, but jitter will be filtered by ttp filter */
        INTERVAL time_passed = (INTERVAL) (((uint48)(amount_in_buffer)*SECOND)/op_extra_data->mic_rate) +
            (INTERVAL) (((uint48)(samples)*SECOND)/op_extra_data->output_rate);

        TIME time_of_arrival = time_sub(current_time, time_passed);

        /* see if we have minimum amount for tag */
        if(samples >= op_extra_data->mic_metadata_min_tag_len)
        {
            /* we have enough new samples to append a complete tag */
            mtag->length = samples*OCTETS_PER_SAMPLE;
        }
        else
        {
            /* new samples aren't enough to form a
             * new complete tag, we append a new tag with
             * minimum length, this tag is incomplete and
             * will be completed in next calls when we receive
             * new samples by appending Null tags.
             */
            mtag->length = op_extra_data->mic_metadata_min_tag_len*OCTETS_PER_SAMPLE;
            op_extra_data->mic_metadata_tag_left_words = op_extra_data->mic_metadata_min_tag_len - samples;
        }

        ttp_status status;
        ttp_update_ttp(op_extra_data->mic_time_to_play, time_of_arrival,
                       mtag->length/OCTETS_PER_SAMPLE, &status);
        /* Populate the metadata tag from the TTP status */
        ttp_utils_populate_tag(mtag, &status);
    }
    /* append generated metadata to the output buffer */
    buff_metadata_append(met_buf, mtag, b4idx, afteridx);
}
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

/**
 * aec_reference_opmsg_enable_mic_sync
 * \brief handler for SAME_INPUT_OUTPUT_CLK_SOURCE message
 * payload[0]: any non-zero value will tell the operator that
 *             backend input and output are from same clock
 * Note: the msaage shall not be sent when any MIC/OUTPUT/REF
 *       terminal is connected.
 */
bool aec_reference_opmsg_enable_mic_sync(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    patch_fn_shared(aec_reference);

    if((NULL != op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1]) ||
       (NULL != op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL]) ||
       (NULL != op_extra_data->input_stream[AEC_REF_OUTPUT_TERMINAL1]))
    {
        /* This message can be handled only if MIC and REF are disconnected */
        return FALSE;
    }

    /* read the enable field */
    op_extra_data->mic_sync_enable = (OPMSG_FIELD_GET(message_data, OPMSG_AEC_SAME_INPUT_OUTPUT_CLOCK_SOURCE, SAME_CLK_SRC)) != 0;
    L2_DBG_MSG1("ACE REFERENCE input-output use same clock: ", op_extra_data->mic_sync_enable);

    return TRUE;
}

/**
 * aec_reference_set_task_period
 * \brief sets the operators's task period
 * \param op_extra_data Pointer to AEC_REFERENCE operator specific data.
 * \param task_period task period in microseconds
 * \param decim_factor decimation factor for sidetone mixing task
 *
 * \return whether the task period update successfully
 */
bool aec_reference_set_task_period(AEC_REFERENCE_OP_DATA *op_extra_data, unsigned task_period, unsigned decim_factor)
{

    patch_fn_shared(aec_reference);
    /* check the limits */
    if(task_period > AEC_REFERENCE_MAX_TASK_PERIOD ||
       task_period < AEC_REFERENCE_MIN_TASK_PERIOD)
    {
        return FALSE;
    }

    /* see if task period is an integer factor of a second */
    if(0 != (SECOND % task_period))
    {
        return FALSE;
    }

    /* Also we don't expect AEC_REFERENCE task period becoms
     * larger than system kick period
     */
    if(task_period > (unsigned)stream_if_get_system_kick_period())
    {
        return FALSE;
    }

#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
    /* if a decimation factor supplied it should be a factor of
     * task period
     */
    if(decim_factor > 1)
    {
        if((task_period % decim_factor) != 0)
        {
            return FALSE;
        }
    }
    else
    {
        decim_factor = 1;
    }
    op_extra_data->task_decim_factor = decim_factor;
    op_extra_data->task_decim_counter = 0;
    op_extra_data->kick_period = task_period / decim_factor;
    op_extra_data->kick_period_frac = frac_div(op_extra_data->kick_period, SECOND);
#else
    /* No decimation */
    op_extra_data->kick_period = task_period;
    decim_factor = 1;
#endif /* AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING */

    /* All are fine, set the task period*/
    op_extra_data->task_period = task_period;
    op_extra_data->task_frequency = (unsigned)SECOND/task_period;
    op_extra_data->task_period_frac = frac_div(task_period, SECOND);

    /* set resampler_temp_buffer_size
     *  Worse case when resampling from 48k to 44.1k the first
     * stage does an upsample by a factor of 2.1.
     * 48000.0 (fs in : 21/10) --> 100800.0 (fs internal : 7/16) --> 44100.0 (fs out)
     *
     * (2*task period) @100800
     */
    op_extra_data->resampler_temp_buffer_size = frac_mult(100800, 2*op_extra_data->task_period_frac) + 4;
    op_extra_data->resampler_temp_buffer_size = MAX(op_extra_data->resampler_temp_buffer_size,
                                                        AEC_REF_RESAMPLER_TEMP_MIN_BUF_SIZE);

    L2_DBG_MSG2("AEC REFERENCE: task period set @%dus, decimation factor=%d", task_period, decim_factor);

    return TRUE;
}

/**
 * aec_reference_set_output_block_size
 * \brief sets the output block size of AEC REFERENCE
 *
 * \pram op_extra_data Pointer to the AEC reference operator specific data.
 * \param block_size output block size
 */
void aec_reference_set_output_block_size(AEC_REFERENCE_OP_DATA *op_extra_data, unsigned block_size)
{
    patch_fn_shared(aec_reference);

    /* if we have user config, then that has precedence */
    if(op_extra_data->cfg_output_block_size != 0)
    {
        block_size = op_extra_data->cfg_output_block_size;
    }

    /* set the operator's output block size */
    op_extra_data->sync_block.block_size = block_size;

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* update minimum len metadata tags for mic output */
    op_extra_data->mic_metadata_min_tag_len =
        MAX(AEC_REFERENCE_MIC_METADATA_MIN_TAG_LEN,
            op_extra_data->sync_block.block_size);
#endif

}

/**
 * aec_reference_opmsg_set_output_block_size
 * \brief message handler for OPMSG_COMMON_ID_SET_OUTPUT_BLOCK_SIZE message,
 *        can be used to directly configure output block size
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_output_block_size(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    /* We could fail this setting if op is running for any reason,
     * however we don't need that, limit it to when mic graph is
     * running with output. The output block size is only effective
     * when mic outputs are running.
     */
    if ((NULL != op_extra_data->mic_graph) &&
        !op_extra_data->mic_graph_no_output)
    {
        return FALSE;
    }

    /* get block size */
    unsigned output_block_size = OPMSG_FIELD_GET(message_data, OPMSG_AEC_SET_OUTPUT_BLOCK_SIZE, BLOCK_SIZE);

    /* update user config value, if non-zero this value will be used
     * instead of the block size that is given by a connected operator.
     */
    op_extra_data->cfg_output_block_size = output_block_size;

    /* update operator's output block size */
    aec_reference_set_output_block_size(op_extra_data, output_block_size);

    L2_DBG_MSG1("AEC REFERENCE output block size set by user: %d", output_block_size);

    return TRUE;
}

/**
 * aec_reference_opmsg_set_task_period
 * \brief message handler for OPMSG_COMMON_ID_SET_TASK_PERIOD message,
 *        can be used to configure to set operator's task period.
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_task_period(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    /* We cant change this setting while running */
    if (opmgr_op_is_running(op_data))
    {
        return FALSE;
    }

    /* get the task period */
    unsigned task_period = OPMSG_FIELD_GET(message_data, OPMSG_AEC_SET_TASK_PERIOD, TASK_PERIOD);

    /* get decimation factor */
    unsigned decim_factor = OPMSG_FIELD_GET(message_data, OPMSG_AEC_SET_TASK_PERIOD, DECIM_FACTOR);

    return aec_reference_set_task_period(op_extra_data, task_period, decim_factor);
}

/**
 * aec_reference_opmsg_set_buffer_size
 * \brief message handler for OPMSG_COMMON_ID_SET_TERMINAL_BUFFER_SIZE message,
 *        can be used to configure required buffer size for input and output terminals.
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_buffer_size(OPERATOR_DATA *op_data, void *message_data,
                                         unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{

    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    /* get the required buffer size */
    unsigned buffer_size = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_TERMINAL_BUFFER_SIZE, BUFFER_SIZE);
    /* get the sink terminals that need configuration */
    unsigned sinks = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_TERMINAL_BUFFER_SIZE,
                                     SINKS);
    /* get the source terminals that need configuration */
    unsigned sources = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_TERMINAL_BUFFER_SIZE,
                                       SOURCES);

    /* All input terminals will report same requied
     * buffer size, so only look at the first input.
     */
    sinks &= (1<<AEC_REF_INPUT_TERMINAL1);

    /* All output terminals will report same requied
     * buffer size, so only look at the first output.
     * This includes REFERENCE buffer.
     */
    sources &= (1<<AEC_REF_OUTPUT_TERMINAL1);

    /* Output buffer size is allowd to change if none of outputs are connected,
     * Note: We allow buffer size change while the operator is running, only the
     * relevant path must be not running.
     */
    if(sources != 0)
    {
        unsigned idx;

        /* No output must be connected */
        for(idx = 0; idx<MAX_NUMBER_MICS; idx++)
        {
            if(NULL != op_extra_data->output_stream[OutputTerminalByIndex(idx)])
            {
                return FALSE;
            }
        }
        /* Also REFERENCE must also be disconnected */
        if(NULL != op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL])
        {
            return FALSE;
        }

        /* Output can change */
    }

    /* Input buffer size is allowd to change if none of inputs are connected,
     * Note: We allow buffer size change while the operator is running, only the
     * relevant path must be not running.
     */
    if(sinks != 0)
    {
        unsigned idx;

        /* No input must be connected */
        for(idx = 0; idx < MAX_NUMBER_SPEAKERS; idx++)
        {
            if(NULL != op_extra_data->input_stream[SpeakerInputTerminalByIndex(idx)])
            {
                return FALSE;
            }
        }
    }

    if(sources != 0)
    {
        /* set the output buffer size */
        op_extra_data->output_buffer_size = buffer_size;
        L2_DBG_MSG1("AEC_REFERENCE: minimum output buffer size set to %d words ", buffer_size);
    }

    if(sinks != 0)
    {
        op_extra_data->input_buffer_size = buffer_size;
        L2_DBG_MSG1("AEC_REFERENCE: minimum input buffer size set to %d words ", buffer_size);
    }

    return TRUE;
}

bool aec_reference_opmsg_mute_mic_output(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    bool mute_enable;

    patch_fn_shared(aec_reference);

    /* read the enable field */
    mute_enable  = (OPMSG_FIELD_GET(message_data, OPMSG_AEC_MUTE_MIC_OUTPUT, ENABLE)) != 0;
    L2_DBG_MSG1("ACE REFERENCE muting mic, mute=%d", mute_enable);

    if(NULL != op_extra_data->mic_mute_op)
    {
        /* configure already running mute operator with simple ramping */
        cbops_mute_enable(op_extra_data->mic_mute_op, mute_enable, FALSE);
    }

    /* store the last mute config */
    op_extra_data->mic_mute_enable_config = mute_enable;

    return TRUE;
}

bool aec_reference_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);

    patch_fn_shared(aec_reference);

    return cpsGetUcidMsgHandler(&op_extra_data->parms_def,op_extra_data->cap_id,message_data,resp_length,resp_data);
}
/**
 * aec_reference_update_input_output_rates
 * \brief updates input and output sample rates
 * \param op_data operator data structure
 * \irate input rate in Hz
 * \orate output rate in Hz
 * \return TRUE if it can accept the new rates else FALSE.
 */
static bool aec_reference_update_input_output_rates(OPERATOR_DATA *op_data, unsigned irate, unsigned orate)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    bool rebuild = FALSE;

    patch_fn_shared(aec_reference);

    L2_DBG_MSG2("AEC REFERENCE: set rates = %d %d", irate, orate);

    if(op_extra_data->input_rate!=irate)
    {
        op_extra_data->input_rate = irate;
        if (opmgr_op_is_running(op_data))
        {
            /* if the operator is running, any change in input
             * rate should trigger a rebuild of speaker graph
             */
            SetSpkrChannelStatus(op_extra_data, 0);
            rebuild = TRUE;
        }
    }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* configure sample rate for MIC ttp */
    ttp_configure_rate(op_extra_data->mic_time_to_play, orate);
#endif

    if(op_extra_data->output_rate!=orate)
    {
        op_extra_data->output_rate = orate;
        if (opmgr_op_is_running(op_data))
        {
            /* if the operator is running, any change in output
             * rate should trigger a rebuild of microphone graph
             */
            SetMicChannelStatus(op_extra_data, 0);
            rebuild = TRUE;
        }
    }

    if(rebuild)
    {
        /* At least one of input or output rate has changed while the operator
         * is running. This will require fresh rebuild of the affected cbops graphs.
         * No need to do this when operator isn't running since this will be done
         * at start point.
         */
        if(!aec_reference_update_stream_and_validate_channels(op_data, NULL, NULL))
        {
            /* Not the best way to indicate failure, but best we can do */
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * aec_reference_set_input_output_rates
 * \brief Message handler for SET_INPUT_OUTPUT_SAMPLE_RATES message ID
 *  Note: Payload contains input and output rates in 25Hz units.
 */
bool aec_reference_set_input_output_rates(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    /* get the input rate from payload */
    unsigned irate = 25 * (OPMSG_FIELD_GET(message_data,
                                           OPMSG_AEC_SET_INPUT_OUTPUT_SAMPLE_RATES,
                                           INPUT_RATE));

    /* get the output rate from payload */
    unsigned orate = 25 * (OPMSG_FIELD_GET(message_data,
                                           OPMSG_AEC_SET_INPUT_OUTPUT_SAMPLE_RATES,
                                           OUTPUT_RATE));

    /* Rates needed for creating cbops and for "aec_reference_buffer_details" */
    return aec_reference_update_input_output_rates(op_data, irate, orate);
}

/**
 * aec_reference_set_input_output_rates
 * \brief Message handler for SET_SAMPLE_RATES message ID
 * Note: Payload contains input and output rates in 1Hz units.
 */
bool aec_reference_set_rates(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    /* get the input rate from payload */
    unsigned irate  = OPMSG_FIELD_GET(message_data,
                                      OPMSG_AEC_SET_SAMPLE_RATES,
                                      INPUT_RATE);

    /* get the output rate from payload */
    unsigned orate  = OPMSG_FIELD_GET(message_data,
                                      OPMSG_AEC_SET_SAMPLE_RATES,
                                      OUTPUT_RATE);

    /* Rates needed for creating cbops and for "aec_reference_buffer_details" */
    return aec_reference_update_input_output_rates(op_data, irate, orate);
}

/**
 * aec_reference_update_mic_reference_sync
 * \brief keeps mic and ref syncronised by updatin mic or reference path warp value
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_update_mic_reference_sync( AEC_REFERENCE_OP_DATA * op_extra_data)
{
    int mic_ra = 0;
    int spkr_ra = 0;
    unsigned mic_rt;
    unsigned spkr_rt;

    patch_fn_shared(aec_reference_run);
    /* This is run in main loop, decimate update as it won't be
     * needed to get updated that often
     */
    op_extra_data->ref_update_counter++;
    if(op_extra_data->ref_update_counter >= AEC_REFERENCE_REF_RATE_UPDATE_PERIOD)
    {
        op_extra_data->ref_update_counter = 0;
        if(op_extra_data->spkr_rate_enactment == RATEMATCHING_SUPPORT_HW)
        {
            /* speaker is using HW rate adjustment, read the latest HW warp rate,
             * when using HW rate adjust pretend it is applied in SW and speaker rate
             * itself is perfect.
             * TODO: might need a constant offset for 44.1kHz-like speaker rates
             */
            if (!opmgr_override_get_hw_warp(op_extra_data->spkr_endpoint, &spkr_ra))
            {
                spkr_ra = 0;
            }
            spkr_rt = (1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT);
        }
        else
        {
            /* get the latest speaker rate measurement */
            spkr_rt = rate_monitor_op_get_rate(op_extra_data->spkr_rate_monitor_op,0);
            if(op_extra_data->spkr_rate_enactment == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
               /* if using standalone operator, no built-in adjust rate is applied */
               &&(0 == op_extra_data->spkr_ext_rate_adjust_op)
#endif
               )
            {
                /* if SW rate adjustment is used, get the current value */
                spkr_ra = cbops_sra_get_current_rate_adjust(op_extra_data->spkr_sw_rateadj_op);
            }
        }
        if (op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
        {
            /* MIC is using HW rate adjustment, read the latest HW warp rate,
             * when using HW rate adjust pretend it is applied in SW and mic rate
             * itself is perfect.
             * TODO: might need a constant offset for 44.1khz-like mic rates
             */
            if (opmgr_override_get_hw_warp(op_extra_data->mic_endpoint, &mic_ra))
            {
                mic_ra = -mic_ra;
            }
            else
            {
                mic_ra = 0;
            }
            mic_rt = (1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT);
        }
        else
        {
            /* get the latest speaker rate measurement */
            mic_rt = rate_monitor_op_get_rate(op_extra_data->mic_rate_monitor_op,0);
            if (op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
               /* if using standalone operator, no built-in adjust rate is applied */
               &&(0 == op_extra_data->mic_ext_rate_adjust_op)
#endif
               )
            {
                mic_ra = cbops_sra_get_current_rate_adjust(op_extra_data->mic_sw_rateadj_op);
            }
        }

        /*
         * Requirements for REF-OUT latency:
         *
         *   1- No or very smooth variation, echo cancellers can cope with slight
         *      latency variation but not with sudden change.

         *   2- Must always be in this range: [ref_delay, ref_delay+jitter]
         *
         *   (1) is accomodated by this function where we continuously apply the overal
         *       INPUT->OUTPUT rate to REFERENCE path (see aecref_calc_ref_rate function).
         *   (2) is guaranteed by cbops latency operator, if latency ever reaches beyond the
         *       limits it will force it back to the withing range by discarding/inserting
         *       samples from/into the REFERENCE path.
         *
         *   However to avoid any need to discard/insert in long runs (will cause echo canceller
         *   re-adaptation) we make sure that latency is always tending towards the centre of the
         *   desired range.
         */

        if(op_extra_data->spkr_ref_active)
        {
            int ref_mic_delay_to_centre =
                (int) op_extra_data->sync_block.ref_delay +
                (int) (op_extra_data->sync_block.jitter/2) -
                (int) op_extra_data->sync_block.speaker_delay;
            op_extra_data->ref_mic_adj_fix += ref_mic_delay_to_centre * AEC_REFERENCE_REF_MIC_ADJ_COEFF;
            op_extra_data->ref_mic_adj_fix = pl_min(op_extra_data->ref_mic_adj_fix, AEC_REFERENCE_REF_MIC_ADJ_MAX);
            op_extra_data->ref_mic_adj_fix = pl_max(op_extra_data->ref_mic_adj_fix, -AEC_REFERENCE_REF_MIC_ADJ_MAX);
        }
        else
        {
            /* Note: No REF-MIC latency control when no reference produced by speaker graph */
            op_extra_data->ref_mic_adj_fix = 0;
        }

        if(op_extra_data->mic_sync_enable)
        {
            /* if we are syncronising MIC to REF then calculate the rate needs
             * to be applied to the mic path, so it will be syncronised to
             * REFERENCE output(i.e. speaker input)*/
            int new_mic_ra = (int) aecref_calc_sync_mic_rate(spkr_ra,spkr_rt,mic_rt) - op_extra_data->ref_mic_adj_fix;
            int diff = new_mic_ra - mic_ra;
            if(diff != 0)
            {
                if (op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
                {
                    /* HW rate adjustment, apply the change only */
                    opmgr_override_set_ratematch_adjustment(op_extra_data->mic_endpoint, diff);
                }
                else
                {
                    /* apply new SW rate adjustment */
                    cbops_sra_set_rate_adjust(op_extra_data->mic_sw_rateadj_op,
                                              new_mic_ra);
                }
            }
            op_extra_data->mic_rate_adjustment = new_mic_ra;
        }
        else
        {

            /* We are synchronising REFERENCE to MIC output,
             * Update reference SW rate adjustment.
             */
            op_extra_data->sync_block.rm_adjustment = (int) aecref_calc_ref_rate(mic_rt,mic_ra,spkr_rt,spkr_ra) + op_extra_data->ref_mic_adj_fix;

            /* update rate adjust for reference path */
            cbops_sra_set_rate_adjust(op_extra_data->ref_sw_rateadj_op,
                                      op_extra_data->sync_block.rm_adjustment);

        }
    }
}

/**
 * aec_reference_update_hw_sidetone
 * \brief does necessary updates for HW Sidetone
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_update_hw_sidetone(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference_run);
}

/**
 * aec_reference_init_hw_sidetone
 * \brief does necessary changes to init HW sidetone
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_init_hw_sidetone(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);
}

/**
 * aec_reference_mic_ref_latency_limit_control
 * \brief keeps distance between reference output and mic output at a suitable range
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_mic_ref_latency_limit_control(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    aec_latency_common *sync_block = &op_extra_data->sync_block;
    tCbuffer *ref_buf = op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL];
    unsigned mic_new_transfer;
    unsigned speaker_new_transfer;
    unsigned block_size;
    patch_fn_shared(aec_reference_run);
    /*
     * MIC-OUT: mic graph is running with mic outputs connected
     * REF-OUT: reference output is connected
     * SPKR-REF: reference sub-path in speaker graph is active
     * ----------------------------------------+------------+--------------------------------------------
     * MIC-OUT  REF-OUT  SPKR_REF   | Latency limit control | Kick forward
     * -----------------------------------------------------+--------------------------------------------
     * N        x           x       | N/A                   | N/A
     * Y        N           x       | N/A                   | every block or every run if non-block based
     * Y        Y           N       | Y, insertion only*    | every run
     * Y        Y           Y       | Y, insert/discard     | every block or every run if non-block based
     *
     * (*) SPKR_REF is inactive so reference path will have all silence.
     */

    if(op_extra_data->mic_graph == NULL
       || op_extra_data->mic_graph_no_output)
    {
        /* no microphone output */
        return;
    }

    /* get new amount written to mic outputs */
    mic_new_transfer = cbops_get_amount(op_extra_data->mic_graph, op_extra_data->mic_latency_index);
    sync_block->mic_data += mic_new_transfer;

    /* gent new amount written to reference output */
    speaker_new_transfer = 0;
    if(op_extra_data->spkr_ref_active)
    {
        speaker_new_transfer = cbops_get_amount(op_extra_data->spkr_graph, op_extra_data->ref_latency_index);
    }
    sync_block->speaker_data += speaker_new_transfer;

    /* get the block size */
    block_size = sync_block->block_size;
    if(block_size <= 1 ||                /* We don't have a valid block size */
       !op_extra_data->spkr_ref_active)  /* We don't need a block size */
    {
        /* this will take us to non-block base mode,
         * where we will control the limits as well as kick
         * forward every run */
        block_size = sync_block->mic_data;
    }

    /* no action until one block written to mic outputs */
    if(sync_block->mic_data < block_size)
    {
        if(NULL != ref_buf)
        {
            /* just update the current speaker delay */
            sync_block->speaker_delay = sync_block->speaker_data - sync_block->mic_data;
        }
        return;
    }

    /* kick outputs */
    sync_block->block_sync = 1;
    sync_block->mic_data -= block_size;
    if(NULL != ref_buf)
    {
         int  speaker_data = (int)sync_block->speaker_data - (int)block_size;
         unsigned space_available = cbuffer_calc_amount_space_in_words(ref_buf);
         unsigned speaker_data_low = sync_block->mic_data + sync_block->ref_delay;
         unsigned speaker_data_high = speaker_data_low + sync_block->jitter;
         if(speaker_data < (int)speaker_data_low)
         {
             /* reached lower limit, insert silence into reference path */
             unsigned amount_to_insert = (speaker_data_low + speaker_data_high)/2 - speaker_data;
             /* limit to space available */
             amount_to_insert = pl_min(amount_to_insert, space_available);
             cbuffer_block_fill(ref_buf, amount_to_insert, 0);
             sync_block->speaker_inserts += amount_to_insert;
             speaker_data += amount_to_insert;
             if(op_extra_data->spkr_ref_active)
             {
                 /* Any update outside cbops needs refreshing cbops graph */
                 cbops_refresh_buffers(op_extra_data->spkr_graph);
             }
         }
         else if (speaker_data > (int) speaker_data_high)
         {
             /* reached higher limit, drop some samples */
             unsigned amount_to_drop = speaker_data - (speaker_data_high + speaker_data_low)/2;

             /* only newly written samples can be dropped */
             amount_to_drop = pl_min(amount_to_drop, speaker_new_transfer);

             /* drop by moving back the write pointer */
             cbuffer_advance_write_ptr(ref_buf, cbuffer_get_size_in_words(ref_buf)-amount_to_drop);
             sync_block->speaker_drops += amount_to_drop;
             speaker_data -= amount_to_drop;
             if(op_extra_data->spkr_ref_active)
             {
                 /* Any update outside cbops needs refreshing cbops graph */
                 cbops_refresh_buffers(op_extra_data->spkr_graph);
             }
         }
         sync_block->speaker_data = speaker_data;

         /* update current speaker-mic delay */
         sync_block->speaker_delay = sync_block->speaker_data - sync_block->mic_data;
    }
}

void aec_reference_timer_task(void *kick_object)
{
    OPERATOR_DATA         *op_data = (OPERATOR_DATA*) kick_object;
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    TIME next_fire_time;
    unsigned                 sink_kicks=0,source_kicks=0;

    base_op_profiler_start(op_data);

    patch_fn_shared(aec_reference_run);

    if(op_extra_data->ReInitFlag==TRUE)
    {
        op_extra_data->ReInitFlag=FALSE;

        /* Handle Reinitialize */
        if(op_extra_data->mic_sidetone_op)
        {
            initialize_sidetone_filter_op(op_extra_data->mic_sidetone_op);
        }
        else if(op_extra_data->sidetone_method == AEC_SIDETONE_IN_HW)
        {
            aec_reference_init_hw_sidetone(op_extra_data);
        }

        aec_reference_set_mic_gains(op_data);

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
        if(op_extra_data->mic_howling_limiter_op)
        {
            HL_LIMITER_UI hl_ui;

            /* Map the Howling limiter user interface parameters */
            map_hl_ui(&op_extra_data->params, &hl_ui);

            configure_howling_limiter_op(
                op_extra_data->mic_howling_limiter_op,
                op_extra_data->mic_rate,
                &hl_ui);
        }
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */
    }

    if(op_extra_data->mic_sidetone_op)
    {
        update_sidetone_filter_op(op_extra_data->mic_sidetone_op,
                                  op_extra_data->params.OFFSET_CONFIG & AEC_REFERENCE_CONFIG_SIDETONEENA,
                                  op_extra_data->shared_volume_ptr->ndvc_filter_sum_lpdnz);
    }
    else if(op_extra_data->sidetone_method == AEC_SIDETONE_IN_HW)
    {
       aec_reference_update_hw_sidetone(op_extra_data);
    }
#ifdef AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING
    /* if we have a separate sidetone graph
     * run the graph at short kick periods,
     * the rest will run at longer task period.
     */
    if(op_extra_data->sidetone_graph != NULL
       /* this check isn't needed as the sidetone filter won't process anything if
        * the apply flag is disabled, however for more MIPS saving we don't run cbops
        * graph at all if sidetone apply flag is disabled by user.
        * This may cause a very small glitch if the flag is changed in real time.
        */
       && (0 != (op_extra_data->params.OFFSET_CONFIG & AEC_REFERENCE_CONFIG_SIDETONEENA))
       /* Also check that both speaker and mic still connected */
       && (NULL != op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
       && (NULL != op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1])
       )
    {
        /* Run sidetone graph */
        cbops_process_data(op_extra_data->sidetone_graph, CBOPS_MAX_COPY_SIZE-1);
    }

    op_extra_data->task_decim_counter++;
    if(op_extra_data->task_decim_counter >= op_extra_data->task_decim_factor)
    {
        op_extra_data->task_decim_counter = 0;
    }
    if(op_extra_data->task_decim_counter == 0)
#endif /* AEC_REFERENCE_CAN_PERFORM_INPLACE_SIDETONE_MIXING */
    {
        /* Process the speaker path if we have speaker graph and
         * speaker is still connected
         */
        bool spkr_graph_active = (NULL != op_extra_data->spkr_graph) &&
            (NULL != op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1]);

        /* Process the mic path if we have mic graph and
         * mic is still connected
         */
        bool mic_graph_active = (NULL != op_extra_data->mic_graph) &&
            (NULL != op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1]);

#ifdef AEC_REF_CALC_SPKR_RATE_MONITOR_AMOUNT
        /* calculate the amount of data consumed by speaker in two cases:
         * 1- we have a rate monitor op for speaker, and/or
         * 2- speaker hasn't started consuming (to detect start of flow)
         */
        if(spkr_graph_active &&
           (op_extra_data->spkr_rate_monitor_op != NULL ||
            !op_extra_data->spkr_flow_started))
        {
            /* we calculate the amount of samples that speaker buffer moved
             * since previous run and pass it to the rate monitor cbops op. The
             * op can use its transfer amount, however since speaker graph runs after mic
             * graph, the new amount will have bigger jitter which might affect
             * reference synchronisation.
             */
                op_extra_data->spkr_new_amount =
                     cbuffer_calc_new_amount(
                         op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1],
                         &op_extra_data->spkr_last_address,
                         TRUE /* This is an output buffer */);

            /* Set a flag showing that speaker output has started consuming samples. */
            if(!op_extra_data->spkr_flow_started)
            {
                op_extra_data->spkr_flow_started = op_extra_data->spkr_new_amount != 0;
            }
        }
#endif /* AEC_REF_CALC_SPKR_RATE_MONITOR_AMOUNT */

        /*  - Run MIC cbops */
        if(mic_graph_active)
        {
            if(op_extra_data->spkr_ref_active
               || op_extra_data->mic_sync_enable)
            {
                /* keep REFERENCE path and MIC output synchronised */
                aec_reference_update_mic_reference_sync(op_extra_data);
            }

            if(!op_extra_data->mic_graph_no_output)
            {
                /* Mic graph with output */
                unsigned b4_space = 0;
                unsigned after_space;
                tCbuffer *mic_buf = op_extra_data->output_stream[AEC_REF_OUTPUT_TERMINAL1];
                b4_space = cbuffer_calc_amount_space_in_words(mic_buf);
                cbops_process_data(op_extra_data->mic_graph, CBOPS_MAX_COPY_SIZE-1);
                after_space = cbuffer_calc_amount_space_in_words(mic_buf);

                /* Don't tolerate cbops writing more than available space */
                PL_ASSERT(b4_space >= after_space);

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
                if(op_extra_data->mic_metadata_buffer!= NULL
                   && buff_has_metadata(op_extra_data->mic_metadata_buffer))
                {
                    aec_reference_mic_generate_metadata_with_ttp(op_extra_data, b4_space - after_space);
                }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
            }
            else /* if(!op_extra_data->mic_graph_no_output) */
            {
                /* run cbops process for mic graph without output */
                cbops_process_data(op_extra_data->mic_graph, CBOPS_MAX_COPY_SIZE-1);
            }
        }

        /*  - Run SPKR cbops */
        if(spkr_graph_active)
        {
#ifdef AEC_REFERENCE_SUPPORT_METADATA
            unsigned max_to_process = CBOPS_MAX_COPY_SIZE-1;
            tCbuffer *met_buf = op_extra_data->spkr_input_metadata_buffer;
            unsigned before_amount = 0;
            if(met_buf!= NULL && buff_has_metadata(met_buf))
            {
                /* amount of metadata available */
                unsigned meta_data_available = buff_metadata_available_octets(met_buf)/OCTETS_PER_SAMPLE;

                /* get amount in the buffer before running cbops */
                before_amount = cbuffer_calc_amount_data_in_words(met_buf);

                /* if we have metadata enabled then limit the amount to
                 * process to the amount of available metadata
                 */
                max_to_process = MIN(max_to_process, meta_data_available);
#ifdef AEC_REFERENCE_SPKR_TTP
                /* Run TTP error control for speaker graph */
                aec_reference_spkr_ttp_run(op_extra_data, &max_to_process);
#endif
            }

            /* run cbops process */
            cbops_process_data(op_extra_data->spkr_graph, max_to_process);

            if(met_buf!= NULL && buff_has_metadata(met_buf))
            {
                /* calculate how much input has been consumed,
                 * The assumption is that the write pointer of
                 * input buffer isn't changed during cbops
                 * process.
                 */
                unsigned amount_processed;
                unsigned after_amount = cbuffer_calc_amount_data_in_words(met_buf);
#ifdef TODO_AEC_REFERENCE_TTP
                /* for the moment don't tolerate cbops doing anything wrong */
                PL_ASSERT(after_amount <= before_amount);
                amount_processed = before_amount - after_amount;
#else /* TODO_AEC_REFERENCE_TTP */
                if(after_amount <= before_amount)
                {
                    amount_processed = before_amount - after_amount;
                }
                else
                {
                    /* This shall never happen, cbops will never
                     * consume more than available data in input buffer.
                     */
                    unsigned buff_size = cbuffer_get_size_in_words(met_buf);
                    amount_processed = buff_size + before_amount - after_amount;
                }
#endif /* TODO_AEC_REFERENCE_TTP */
                if(amount_processed > 0)
                {
                    /* delete metadata tags for consumed input */
                    unsigned b4idx, afteridx;
                    buff_metadata_tag_list_delete(
                        buff_metadata_remove(met_buf, OCTETS_PER_SAMPLE * amount_processed, &b4idx, &afteridx));
                    /* update timestamp */
                    aec_reference_spkr_ttp_update_last_timestamp(op_extra_data, amount_processed);
                }
            }
#else /* AEC_REFERENCE_SUPPORT_METADATA */
            cbops_process_data(op_extra_data->spkr_graph, CBOPS_MAX_COPY_SIZE-1);
#endif /* AEC_REFERENCE_SUPPORT_METADATA*/
            base_op_profiler_add_kick(op_data);
        }

        /* limit the latency between mic and reference output if required */
        aec_reference_mic_ref_latency_limit_control(op_extra_data);

        /* Check for Kicks (outputs).   Use Output 1 available data*/
        if(op_extra_data->sync_block.block_sync)
        {
            source_kicks = op_extra_data->source_kicks;
            op_extra_data->sync_block.block_sync = 0;
        }

        /* Check for Kick (inputs).   Use Input 1 available space */
        if(op_extra_data->input_stream[AEC_REF_INPUT_TERMINAL1])
        {
            int available_space = cbuffer_calc_amount_space_in_words(op_extra_data->input_stream[AEC_REF_INPUT_TERMINAL1]);

            if(available_space >= op_extra_data->spkr_kick_size)
            {
                sink_kicks = op_extra_data->sink_kicks;
            }
        }


        if(sink_kicks || source_kicks)
        {
            opmgr_kick_from_operator(op_data,source_kicks,sink_kicks);
        }
    }

    /* Next Timer Event */
    next_fire_time = time_add(get_last_fire_time(), op_extra_data->kick_period);
    op_extra_data->kick_id = timer_schedule_event_at(next_fire_time,
                                                     aec_reference_timer_task, (void*)op_data);

    base_op_profiler_stop(op_data);
}

#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
/**
 * aec_reference_spkr_check_external_rate_adjust_op
 *
 * \brief Checks whether speaker path has access to a standalone RATE_ADJUST operator.
 *        If there is one then speaker graph will use that instead on built-in rate
 *        adjustment. Note that reference sub-path still will use it's own built-in
 *        rate adjust operator.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 */
void aec_reference_spkr_check_external_rate_adjust_op(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    EXT_OP_ID op_id = 0;

    patch_fn_shared(aec_reference);
    if (opmgr_override_get_rate_adjust_op(op_extra_data->spkr_endpoint, &op_id) && op_id != 0)
    {
        /* Speaker graph will use an external RATE_ADJUST operator for
         * performing rate adjustment.
         */
        op_extra_data->spkr_ext_rate_adjust_op = op_id;

        /* if we have standalone rate adjust, we will use it, even
         * if spkr is able to use HW warping */
        op_extra_data->spkr_rate_ability = RATEMATCHING_SUPPORT_SW;

        /* Speaker path will use this, so not in pass-through mode */
        stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->spkr_ext_rate_adjust_op, FALSE);

        L2_DBG_MSG1("AEC_REFERENCE: Speaker path will use standalone rate adjust: opid=0x%x", op_id);

        return;
    }

    /* No external operator found or needed */
    op_extra_data->spkr_ext_rate_adjust_op = 0;

    return;
}

/**
 * aec_reference_mic_check_external_rate_adjust_op
 *
 * \brief Checks whether microphone path has access to a standalone RATE_ADJUST operator.
 *        If there is one then speaker graph will use that instead on built-in rate
 *        adjustment.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 * \return TRUE if there is a standalone rate adjust operator linked to the microphne path
 */
void aec_reference_mic_check_external_rate_adjust_op(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    EXT_OP_ID op_id = 0;

    patch_fn_shared(aec_reference);
    if (opmgr_override_get_rate_adjust_op(op_extra_data->mic_endpoint, &op_id) && op_id != 0)
    {
        /* If we have been told to synchronise mic to speaker input then
         * we shouldn't have been told to use standalone rate adjust.
         */
        PL_ASSERT(!op_extra_data->mic_sync_enable);

        /* Microphone graph will use an external RATE_ADJUST operator for
         * performing rate adjustment.
         */
        op_extra_data->mic_ext_rate_adjust_op = op_id;

        /* if we have standalone rate adjust, we will use it, even
         * if mic is able to use HW warping */
        op_extra_data->mic_rate_ability = RATEMATCHING_SUPPORT_SW;

        /* Microphone path will use this, so not in pass-through mode */
        stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->mic_ext_rate_adjust_op, FALSE);

        L2_DBG_MSG1("AEC_REFERENCE: Microphone path will use standalone rate adjust: opid=0x%x", op_id);

        return;
    }

    /* No external operator found or needed */
    op_extra_data->mic_ext_rate_adjust_op = 0;

    return;
}
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

bool aec_reference_opmsg_ep_get_config(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    OPMSG_GET_CONFIG *msg = message_data;
    OPMSG_GET_CONFIG_RESULT *result = msg->result;
    unsigned term_idx = msg->header.cmd_header.client_id;

    patch_fn_shared(aec_reference);

    /* msg->value - Pointer which will be populated with the asked configuration value
       msg->cmd_header.client_id - Terminal ID (includes TERMINAL_SINK_MASK for sinks)
       msg->key - Parameter Key to return value for */

    switch(msg->key)
    {
        case OPMSG_OP_TERMINAL_DETAILS:
            /* Return a uint32 - Is Terminal emulating a real endpoint. Called at operator endpoint creation.
             */
            if(term_idx & TERMINAL_SINK_MASK)
            {
                result->value = (uint32)(IsMicrophoneTerminal(term_idx)?
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_NONE:
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_REAL);
            }
            else
            {
                result->value = (uint32)(IsSpeakerTerminal(term_idx)?
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_NONE:
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_REAL);
            }
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_ABILITY: /* uint32 */
            if(term_idx == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                result->value = (uint32)op_extra_data->spkr_rate_ability;
#ifdef ENABLE_FORCE_ENACTING_BY_AEC_REFERENCE
                if(RATEMATCHING_SUPPORT_SW == result->value)
                {
                    /* Advertise HW despite planning to do it in SW
                     * this will make sure that enacting will be granted
                     * to this end.
                     */
                    result->value = RATEMATCHING_SUPPORT_HW;
                }
#endif
            }
            else if (term_idx == AEC_REF_OUTPUT_TERMINAL1)
            {

                if(op_extra_data->mic_sync_enable)
                {
                    /* if we are syncing mic to speaker input, then
                     * report AUTO so no rate match pair is created for
                     * mic path.
                     */
                    result->value = (uint32)RATEMATCHING_SUPPORT_AUTO;
                }
                else
                {

                    result->value = (uint32) op_extra_data->mic_rate_ability;
#ifdef ENABLE_FORCE_ENACTING_BY_AEC_REFERENCE
                    if(RATEMATCHING_SUPPORT_SW == result->value)
                    {
                        /* Advertise HW despite planning to do it in SW
                         * this will make sure that enacting will be granted
                         * to this end.
                         */
                        result->value = RATEMATCHING_SUPPORT_HW;
                    }
#endif
                }

            }
            else
            {
                result->value = (uint32)RATEMATCHING_SUPPORT_AUTO;
            }
            break;
        case OPMSG_OP_TERMINAL_KICK_PERIOD:       /* uint32 */
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_RATE:    /* uint32 */

            if(term_idx == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                result->value = rate_monitor_op_get_rate(op_extra_data->spkr_rate_monitor_op,0);
                patch_fn_shared(aec_reference);

                op_extra_data->spkr_rate_meas = (unsigned)(result->value);

            }
            else if (term_idx == AEC_REF_OUTPUT_TERMINAL1)
            {
                result->value = rate_monitor_op_get_rate(op_extra_data->mic_rate_monitor_op,0);
                patch_fn_shared(aec_reference);

                op_extra_data->mic_rate_meas = (unsigned)(result->value);
            }
            else
            {
                /* 1.0 in Qx.22 independent of word width */
                result->value = 1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT;
            }

            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_MEASUREMENT:
            /* TODO */
            result->rm_measurement.sp_deviation = 0;
            result->rm_measurement.measurement.valid = FALSE;
            break;
        case OPMSG_OP_TERMINAL_BLOCK_SIZE:        /* uint32 */
        case OPMSG_OP_TERMINAL_PROC_TIME:         /* uint32 */
        default:
            return FALSE;
    }

    return TRUE;
}


bool aec_reference_opmsg_ep_configure(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
    OPMSG_CONFIGURE  *msg = message_data;
    unsigned terminal_id = msg->header.cmd_header.client_id;
    uint32      value = msg->value;

    patch_fn_shared(aec_reference);

    /* msg->value - Pointer or Value for Key
       msg->cmd_header.client_id - Terminal ID (includes TERMINAL_SINK_MASK for sinks)
       msg->key - Parameter Key to return value for */

    switch(msg->key)
    {
        case OPMSG_OP_TERMINAL_DATA_FORMAT:
            /* value is data type */
            if( ((AUDIO_DATA_FORMAT)msg->value)!=AUDIO_DATA_FORMAT_FIXP )
            {
                return(FALSE);
            }
            break;
        case OPMSG_OP_TERMINAL_KICK_PERIOD:
            /* uint32 polling period in usec - ignore */
            break;
        case OPMSG_OP_TERMINAL_PROC_TIME:
            /* uint32 - N/A an operator will never receive this (has_deadline always FALSE for operators)   */
            break;

        case OPMSG_OP_TERMINAL_SHIFT:
        {
            /* TODO - Really need to know type of endpoint: ADC/DAC, I2S, Digital MIC

               CBOPS_DC_REMOVE | CBOPS_SHIFT (set data format --> AUDIO_DATA_FORMAT_FIXP)
               CBOPS_RATEADJUST (EP_RATEMATCH_ENACTING) */

            if (terminal_id == (AEC_REF_MIC_TERMINAL1|TERMINAL_SINK_MASK))
            {
                op_extra_data->mic_shift = (int)value;
            }
            else if (terminal_id == AEC_REF_SPKR_TERMINAL1)
            {
                op_extra_data->spkr_shift = (int)value;
            }
            break;
        }
        case OPMSG_OP_TERMINAL_BLOCK_SIZE:
            /* uint32 expected block size per period

               endpoint->state.audio.block_size = (unsigned int)value;
               endpoint->state.audio.kick_period =  (unsigned int)(value * (unsigned long)STREAM_KICK_PERIOD_FROM_USECS(1000000UL) /
               endpoint->state.audio.sample_rate); */
            if(terminal_id == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                op_extra_data->spkr_kick_size = (unsigned)value;
            }
            else if (terminal_id == AEC_REF_OUTPUT_TERMINAL1)
            {
                unsigned block_size     = (unsigned)value;

                /* mic-ref block_size latency control only works for larger block sizes, it will fail
                 * if requested block size is small, so we apply a minimum block size, below
                 * that mic-ref latency control logic will be turned off.
                 */
                unsigned min_block_size = frac_mult(op_extra_data->output_rate, FRACTIONAL(0.006));

                /* Validate block size for sync logic.   Output buffers size will be at least 8.7 msec of data */
                if(block_size && (block_size < min_block_size) )
                {
                    /* Disable sync logic if block_size is too small */
                    block_size = 0;
                }

                /* update operator's output block size */
                aec_reference_set_output_block_size(op_extra_data, block_size);
            }
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_ENACTING:
            /* uint32 TRUE or FALSE.   Operator should perform rate matching if TRUE  */

            if(terminal_id == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                opmgr_override_set_ratematch_enacting(op_extra_data->spkr_endpoint,
                                                      (bool) value);
                if(value==(uint32)FALSE)
                {
                    op_extra_data->spkr_rate_enactment = RATEMATCHING_SUPPORT_NONE;
                }
                else
                {
                    op_extra_data->spkr_rate_enactment = op_extra_data->spkr_rate_ability;
                }

                if((op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_HW) && (op_extra_data->spkr_rate_monitor_op))
                {
                    rate_monitor_op_initialise(op_extra_data->spkr_rate_monitor_op,op_extra_data->spkr_rate,TRUE,3*MS_PER_SEC);
                }

                if(op_extra_data->spkr_sw_rateadj_op)
                {
                    cbops_rateadjust_passthrough_mode(op_extra_data->spkr_sw_rateadj_op,(op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);

                }
            }
            else if (terminal_id == AEC_REF_OUTPUT_TERMINAL1
                     && !op_extra_data->mic_sync_enable)
            {
                opmgr_override_set_ratematch_enacting(op_extra_data->mic_endpoint,
                                                      (bool) value);
                if(value==(uint32)FALSE)
                {
                    op_extra_data->mic_rate_enactment = RATEMATCHING_SUPPORT_NONE;
                }
                else
                {
                    op_extra_data->mic_rate_enactment = op_extra_data->mic_rate_ability;
                }

                if ((op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW) && (op_extra_data->mic_rate_monitor_op))
                {
                    rate_monitor_op_initialise(op_extra_data->mic_rate_monitor_op,op_extra_data->mic_rate,TRUE,3*MS_PER_SEC);
                }

                if(op_extra_data->mic_sw_rateadj_op)
                {
                    cbops_rateadjust_passthrough_mode(op_extra_data->mic_sw_rateadj_op,(op_extra_data->mic_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);
                }
            }
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_ADJUSTMENT:
        {
            int svalue = (int)value;
            /*  See BlueCore audio real endpoint function "adjust_audio_rate" for details */
            if(terminal_id == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
#ifdef AEC_REFERENCE_SPKR_TTP
                if(op_extra_data->spkr_timed_playback_mode)
                {
                    /* ignore this message when in timed playback mode,
                     * rate adjustment is managed by speaker ttp */
                    break;
                }
#endif /* AEC_REFERENCE_SPKR_TTP */
                /* Send Rate Adjustment to hardware */
                if(op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_HW)
                {
                    if(op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
                    {
                        if((op_extra_data->spkr_rate_monitor_op) && (rate_monitor_op_is_complete(op_extra_data->spkr_rate_monitor_op)))
                        {
                            op_extra_data->spkr_rate_adjustment = svalue;
                            value = op_extra_data->spkr_rate_adjustment;
                            opmgr_override_set_ratematch_adjustment(op_extra_data->spkr_endpoint,
                                                                    (int) value);
                            rate_monitor_op_restart(op_extra_data->spkr_rate_monitor_op);

                        }
                    }
                }
                else
                {
                    op_extra_data->spkr_rate_adjustment = svalue;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
                    if(0 != op_extra_data->spkr_ext_rate_adjust_op)
                    {

                        /* set the target rate value, this will be ignored if speaker path is doing TTP. */
                        stream_delegate_rate_adjust_set_target_rate(op_extra_data->spkr_ext_rate_adjust_op,
                                                                    op_extra_data->spkr_rate_adjustment);
                    }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */
                }

            }
            else if (terminal_id == AEC_REF_OUTPUT_TERMINAL1
                     /* if we are syncing mic to speaker input we shouldn't
                      * receive this message, but ignore it if we received.
                      */
                     && !op_extra_data->mic_sync_enable)
            {
                /* Send Rate Adjustment to hardware */
                if(op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
                {
                    if(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1])
                    {
                        if((op_extra_data->mic_rate_monitor_op) && (rate_monitor_op_is_complete(op_extra_data->mic_rate_monitor_op)))
                        {
                            op_extra_data->mic_rate_adjustment = svalue;
                            value = op_extra_data->mic_rate_adjustment;
                            opmgr_override_set_ratematch_adjustment(op_extra_data->mic_endpoint,
                                                                    (int) value);
                            rate_monitor_op_restart(op_extra_data->mic_rate_monitor_op);
                        }
                    }
                }
                else
                {
                    op_extra_data->mic_rate_adjustment = svalue;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
                    if(0 != op_extra_data->mic_ext_rate_adjust_op)
                    {

                        /* set the target rate */
                        stream_delegate_rate_adjust_set_target_rate(op_extra_data->mic_ext_rate_adjust_op,
                                                                    op_extra_data->mic_rate_adjustment);
                    }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */
                }
            }
        }
        break;
        case OPMSG_OP_TERMINAL_RATEMATCH_REFERENCE:
            /* TODO */
            /* break; */
        default:
            return(FALSE);
    }

    return(TRUE);
}

/* With the change of setting rate match ability for all terminals but
   AEC_REF_INPUT_TERMINAL1 and AEC_REF_OUTPUT_TERMINAL1 to AUTO
   this function is never called.

   TODO.  Remove OPMSG_COMMON_GET_CLOCK_ID from table and delete this function
*/

bool aec_reference_opmsg_ep_clock_id(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    unsigned *resp;
    OP_MSG_REQ *msg = message_data;
    unsigned terminal_id = msg->header.cmd_header.client_id;
    INT_OP_ID int_id = base_op_get_int_op_id(op_data);

    patch_fn_shared(aec_reference);

    /* There is a maximum of 2 possible rates at real terminals. All real sources
     * have a rate tied to the Mic rate. All real sinks have a rate tied to the
     * Spkr rate.
     *
     * If the Mic and Spkr share the same clock source then all real terminals
     * share the same rate.
     */

    /* Payload is a single word containing the clock ID */
    resp = xpmalloc(sizeof(unsigned));
    if (!resp)
    {
        return FALSE;
    }
    *resp_data = (OP_OPMSG_RSP_PAYLOAD*)resp;
    *resp_length = 1;

    /*
      This function is only called for terminals marked as real;AEC_REF_INPUT_TERMINAL[1:8] (sink),
      AEC_REF_OUTPUT_TERMINAL[1:8] (source), or AEC_REF_REFERENCE_TERMINAL (source).  Otherwise,
      clock ID will be reported as zero before this operation is called.

      If MIC and SPKR real endpoints are not connected then we report the same clock source.
    */

    if (terminal_id & TERMINAL_SINK_MASK)
    {
        AEC_REFERENCE_OP_DATA *op_extra_data = get_instance_data(op_data);
        /* This is only relevant for the input sinks. If the speaker has the
         * same clock source as the mic then report the same clock source of
         * the op id. If they differ then report op_id and 1 << 7 as the op id
         * is 7 bits long.
         */
        if (!op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1] ||
            !op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
        {
            *resp = int_id;
        }
        else if (opmgr_override_have_same_clock_source(op_extra_data->spkr_endpoint,
                                                       op_extra_data->mic_endpoint))
        {
            *resp = int_id;
        }
        else
        {
            *resp = int_id | (1 << 7);
        }
    }
    else
    {
        /* The sources that this is relevant to is Outputs and Reference. These
         * all have the same clock source. Report default clock source as the
         * op id.
         */
        *resp = int_id;
    }

    return TRUE;
}

/*  LocalWords:  AEC
 */

#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER
/* Map the Howling limiter user interface parameters */
static void map_hl_ui(AEC_REFERENCE_PARAMETERS *params_ptr, HL_LIMITER_UI *hl_ui)
{
    hl_ui->adc_gain1 = params_ptr->OFFSET_ADC_GAIN1;
    hl_ui->hl_switch = params_ptr->OFFSET_HL_SWITCH;
    hl_ui->hl_limit_level = params_ptr->OFFSET_HL_LIMIT_LEVEL;
    hl_ui->hl_limit_threshold = params_ptr->OFFSET_HL_LIMIT_THRESHOLD;
    hl_ui->hl_limit_hold_ms = params_ptr->OFFSET_HL_LIMIT_HOLD_MS;
    hl_ui->hl_limit_detect_fc = params_ptr->OFFSET_HL_LIMIT_DETECT_FC;
    hl_ui->hl_limit_tc = params_ptr->OFFSET_HL_LIMIT_TC;
}
#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */
