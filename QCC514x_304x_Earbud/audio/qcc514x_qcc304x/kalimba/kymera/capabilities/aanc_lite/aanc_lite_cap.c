/****************************************************************************
 * Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC_LITE
 * \ingroup capabilities
 * \file  aanc_lite_cap.c
 * \ingroup AANC_LITE
 *
 * Adaptive ANC (AANC) Lite operator capability.
 *
 */

/****************************************************************************
Include Files
*/

#include "aanc_lite_cap.h"

/*****************************************************************************
Private Constant Definitions
*/
#ifdef CAPABILITY_DOWNLOAD_BUILD
#define AANC_LITE_16K_CAP_ID   CAP_ID_DOWNLOAD_AANC_LITE_16K
#else
#define AANC_LITE_16K_CAP_ID   CAP_ID_AANC_LITE_16K
#endif

/* Message handlers */
const handler_lookup_struct aanc_lite_handler_table =
{
    aanc_lite_create,             /* OPCMD_CREATE */
    base_op_destroy,              /* OPCMD_DESTROY */
    aanc_lite_start,              /* OPCMD_START */
    base_op_stop,                 /* OPCMD_STOP */
    aanc_lite_reset,              /* OPCMD_RESET */
    aanc_lite_connect,            /* OPCMD_CONNECT */
    aanc_lite_disconnect,         /* OPCMD_DISCONNECT */
    aanc_lite_buffer_details,     /* OPCMD_BUFFER_DETAILS */
    base_op_get_data_format,      /* OPCMD_DATA_FORMAT */
    aanc_lite_get_sched_info      /* OPCMD_GET_SCHED_INFO */
};

/* Null-terminated operator message handler table */
const opmsg_handler_lookup_table_entry aanc_lite_opmsg_handler_table[] =
    {{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION, base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_CONTROL,             aanc_lite_opmsg_set_control},
    {OPMSG_COMMON_ID_GET_PARAMS,              aanc_lite_opmsg_get_params},
    {OPMSG_COMMON_ID_GET_DEFAULTS,            aanc_lite_opmsg_get_defaults},
    {OPMSG_COMMON_ID_SET_PARAMS,              aanc_lite_opmsg_set_params},
    {OPMSG_COMMON_ID_GET_STATUS,              aanc_lite_opmsg_get_status},
    {OPMSG_COMMON_ID_SET_UCID,                aanc_lite_opmsg_set_ucid},
    {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,       aanc_lite_opmsg_get_ps_id},

    {OPMSG_AANC_ID_SET_AANC_STATIC_GAIN,      aanc_lite_opmsg_set_static_gain},
    {0, NULL}};

#ifdef INSTALL_OPERATOR_AANC_LITE_16K
const CAPABILITY_DATA aanc_lite_16k_cap_data =
    {
        /* Capability ID */
        AANC_LITE_16K_CAP_ID,
        /* Version information - hi and lo */
        AANC_LITE_AANC_LITE_16K_VERSION_MAJOR, AANC_LITE_CAP_VERSION_MINOR,
        /* Max number of sinks/inputs and sources/outputs */
        2, 2,
        /* Pointer to message handler function table */
        &aanc_lite_handler_table,
        /* Pointer to operator message handler function table */
        aanc_lite_opmsg_handler_table,
        /* Pointer to data processing function */
        aanc_lite_process_data,
        /* Reserved */
        0,
        /* Size of capability-specific per-instance data */
        sizeof(AANC_LITE_OP_DATA)
    };

MAP_INSTANCE_DATA(AANC_LITE_16K_CAP_ID, AANC_LITE_OP_DATA)
#endif /* INSTALL_OPERATOR_AANC_LITE_16K */

/****************************************************************************
Inline Functions
*/

/**
 * \brief  Get AANC Lite instance data.
 *
 * \param  op_data  Pointer to the operator data.
 *
 * \return  Pointer to extra operator data AANC_LITE_OP_DATA.
 */
static inline AANC_LITE_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (AANC_LITE_OP_DATA *) base_op_get_instance_data(op_data);
}

/**
 * \brief  Calculate the number of samples to process
 *
 * \param  p_ext_data  Pointer to capability data
 *
 * \return  Number of samples to process
 *
 * If there is less data or space than the default frame size then only that
 * number of samples will be returned.
 *
 */
static inline int aanc_lite_calc_samples_to_process(AANC_LITE_OP_DATA *p_ext_data)
{
    int i, amt, min_data_space = AANC_LITE_DEFAULT_FRAME_SIZE;

    /* Return if int and ext mic input terminals are not connected */
    if ((p_ext_data->touched_sinks & AANC_LITE_MIN_VALID_SINKS) !=
                AANC_LITE_MIN_VALID_SINKS)
    {
        return INT_MAX;
    }

    /* Calculate the amount of data available */
    for (i = AANC_LITE_MIC_INT_TERMINAL_ID; i <= AANC_LITE_MIC_EXT_TERMINAL_ID; i++)
    {
        if (p_ext_data->inputs[i] != NULL)
        {
            amt = cbuffer_calc_amount_data_in_words(p_ext_data->inputs[i]);
            if (amt < min_data_space)
            {
                min_data_space = amt;
            }
        }
    }

    /*  Calculate the available space */
    if (p_ext_data->touched_sources != 0)
    {
        for (i = AANC_LITE_MIC_INT_TERMINAL_ID; i <= AANC_LITE_MIC_EXT_TERMINAL_ID; i++)
        {
            if (p_ext_data->outputs[i] != NULL)
            {
                amt = cbuffer_calc_amount_space_in_words(p_ext_data->outputs[i]);
                if (amt < min_data_space)
                {
                    min_data_space = amt;
                }
            }
        }
    }
    /* Samples to process determined as minimum of data and space available */
    return min_data_space;
}

#ifdef RUNNING_ON_KALSIM
/**
 * \brief  Simulate a gain update to the HW.
 *
 * \param  op_data  Address of the operator data.
 * \param  p_ext_data  Address of the AANC Lite extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Because simulator tests need to change the gain and also analyze the
 * behavior of the capability an unsolicited message is sent only in
 * simulation.
 */
static bool aanc_lite_update_gain(OPERATOR_DATA *op_data,
                                  AANC_LITE_OP_DATA *p_ext_data)
{
    unsigned msg_size = OPMSG_UNSOLICITED_AANC_INFO_WORD_SIZE;
    unsigned *trigger_message = NULL;
    OPMSG_REPLY_ID message_id = OPMSG_REPLY_ID_AANC_TRIGGER;

    trigger_message = xzpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        return FALSE;
    }

    p_ext_data->ff_fine_gain_prev = p_ext_data->ff_fine_gain;

    OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                             FF_FINE_GAIN, p_ext_data->ff_fine_gain);

    common_send_unsolicited_message(op_data, (unsigned)message_id, msg_size,
                                    trigger_message);

    pdelete(trigger_message);

    return TRUE;
}
/**
 * \brief  Update the gain in the ANC HW.
 *
 * \param  op_data  Address of the operator data.
 * \param  p_ext_data  Address of the AANC Lite extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Any changes in the gain value since the previous value was set is written
 * to the HW.
 *
 */
#else
static bool aanc_lite_update_gain(OPERATOR_DATA *op_data,
                                  AANC_LITE_OP_DATA *p_ext_data)
{
    /* Update FF gain */
    if (p_ext_data->ff_fine_gain != p_ext_data->ff_fine_gain_prev)
    {
        stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                     p_ext_data->anc_ff_path,
                                     p_ext_data->ff_fine_gain);
        p_ext_data->ff_fine_gain_prev = p_ext_data->ff_fine_gain;
    }
    return TRUE;
}
#endif /* RUNNING_ON_KALSIM */

/**
 * \brief  Update touched terminals for the capability
 *
 * \param  p_ext_data  Address of the AANC Lite extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Because this is solely dependent on the terminal connections it can be
 * calculated in connect/disconnect rather than in every process_data loop.
 */
static bool update_touched_sink_sources(AANC_LITE_OP_DATA *p_ext_data)
{
    uint16 touched_sinks = 0;
    uint16 touched_sources = 0;

    if (p_ext_data->outputs[AANC_LITE_MIC_INT_TERMINAL_ID] != NULL)
    {
        touched_sources |= (1 << AANC_LITE_MIC_INT_TERMINAL_ID);
    }
    /* Always read internal mic input buffer when processing data */
    touched_sinks |= (1 << AANC_LITE_MIC_INT_TERMINAL_ID);

    if (p_ext_data->outputs[AANC_LITE_MIC_EXT_TERMINAL_ID] != NULL)
    {
        touched_sources |= (1 << AANC_LITE_MIC_EXT_TERMINAL_ID);
    }
    /* Always read external mic input buffer when processing data */
    touched_sinks |= (1 << AANC_LITE_MIC_EXT_TERMINAL_ID);

    p_ext_data->touched_sinks = touched_sinks;
    p_ext_data->touched_sources = touched_sources;

    return TRUE;
}

/****************************************************************************
Capability API Handlers
*/

bool aanc_lite_create(OPERATOR_DATA *op_data, void *message_data,
                      unsigned *response_id, void **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);

    /* NB: create is passed a zero-initialized structure so any fields not
     * explicitly initialized are 0.
     */

    L5_DBG_MSG1("AANC Lite Create: p_ext_data at 0x%08X", p_ext_data);

    int i;
    unsigned *p_default_params = NULL; /* Pointer to default params */
    unsigned *p_cap_params = NULL;     /* Pointer to capability params */

    if (!base_op_create(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* patch_fn_shared(aanc_capability);  TODO: patch functions */

    /* Assume the response to be command FAILED. If we reach the correct
     * termination point in create then change it to STATUS_OK.
     */
    base_op_change_response_status(resp_data, STATUS_CMD_FAILED);

    /* Initialize buffers */
    for (i = 0; i < AANC_LITE_MAX_SINKS; i++)
    {
        p_ext_data->inputs[i] = NULL;
    }
    for (i = 0; i < AANC_LITE_MAX_SOURCES; i++)
    {
        p_ext_data->outputs[i] = NULL;
    }

    p_ext_data->metadata_ip = NULL;
    p_ext_data->metadata_op = NULL;

    /* Initialize capid and sample rate fields */
    p_ext_data->cap_id = base_op_get_cap_id(op_data);

    p_ext_data->sample_rate = 16000;
    /* Initialize parameters */
    p_default_params = (unsigned*) AANC_LITE_GetDefaults(p_ext_data->cap_id);
    p_cap_params = (unsigned*) &p_ext_data->aanc_lite_cap_params;
    if(!cpsInitParameters(&p_ext_data->params_def, p_default_params,
                          p_cap_params, sizeof(AANC_LITE_PARAMETERS)))
    {
       return TRUE;
    }

    /* Initialize system mode */
    p_ext_data->cur_mode = AANC_LITE_SYSMODE_FULL;

    /* Trigger re-initialization at start */
    p_ext_data->re_init_flag = TRUE;

    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC0_ID;
    /* Default to hybrid: ff path is FFB, fb path is FFA */
    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFB_ID;

    /* Operator creation was succesful, change respone to STATUS_OK*/
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("AANC Lite: Created");
    return TRUE;
}

bool aanc_lite_start(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* patch_fn_shared(aanc_capability); TODO: patch functions */

    /* Start with the assumption that we fail and change later if we succeed */
    if (!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, resp_data))
    {
        return FALSE;
    }

    /* Check that we have a minimum number of terminals connected */
    if (p_ext_data->inputs[AANC_LITE_MIC_INT_TERMINAL_ID] == NULL ||
        p_ext_data->inputs[AANC_LITE_MIC_EXT_TERMINAL_ID] == NULL)
    {
        L4_DBG_MSG("AANC Lite start failure: mic inputs not connected");
        return TRUE;
    }

    /* TODO: check ANC HW enable status */

    /* Initialize fine gain to 0 */
    if (p_ext_data->cur_mode == AANC_LITE_SYSMODE_FULL)
    {
        p_ext_data->ff_fine_gain = 0;
        aanc_lite_update_gain(op_data, p_ext_data);
    }

    /* Set reinitialization flags to ensure first run behavior */
    p_ext_data->re_init_flag = TRUE;

    /* All good */
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("AANC Lite Started");
    return TRUE;
}

bool aanc_lite_reset(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!base_op_reset(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    p_ext_data->re_init_flag = TRUE;

    L4_DBG_MSG("AANC Lite: Reset");
    return TRUE;
}

bool aanc_lite_connect(OPERATOR_DATA *op_data, void *message_data,
                       unsigned *response_id, void **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    tCbuffer* pterminal_buf = OPMGR_GET_OP_CONNECT_BUFFER(message_data);
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;
    unsigned max_value;
    tCbuffer** selected_buffer;
    tCbuffer** selected_metadata;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* Determine whether sink or source terminal being connected */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC Lite connect: sink terminal %d", terminal_num);
        max_value = AANC_LITE_MAX_SINKS;
        selected_buffer = p_ext_data->inputs;
        selected_metadata = &p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC Lite connect: source terminal %d", terminal_num);
        max_value = AANC_LITE_MAX_SOURCES;
        selected_buffer = p_ext_data->outputs;
        selected_metadata = &p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC Lite connect failed: invalid terminal %d", terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't connect if already connected */
    if (selected_buffer[terminal_num] != NULL)
    {
        L4_DBG_MSG1("AANC Lite connect failed: terminal %d already connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    selected_buffer[terminal_num] = pterminal_buf;

    if (*selected_metadata == NULL &&
        buff_has_metadata(pterminal_buf))
    {
        *selected_metadata = pterminal_buf;
    }

    update_touched_sink_sources(p_ext_data);

    return TRUE;
}

bool aanc_lite_disconnect(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *response_id, void **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;

    /* Variables used for distinguishing source/sink */
    unsigned max_value;
    tCbuffer** selected_buffer;
    tCbuffer** selected_metadata;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* Determine whether sink or source terminal being disconnected */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC Lite disconnect: sink terminal %d", terminal_num);
        max_value = AANC_LITE_MAX_SINKS;
        selected_buffer = p_ext_data->inputs;
        selected_metadata = &p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC Lite disconnect: source terminal %d", terminal_num);
        max_value = AANC_LITE_MAX_SOURCES;
        selected_buffer = p_ext_data->outputs;
        selected_metadata = &p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC Lite disconnect failed: invalid terminal %d",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't disconnect if not connected */
    if (selected_buffer[terminal_num] == NULL)
    {
        L4_DBG_MSG1("AANC Lite disconnect failed: terminal %d not connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    /*  Disconnect the existing metadata channel. */
    *selected_metadata = NULL;

    selected_buffer[terminal_num] = NULL;

    update_touched_sink_sources(p_ext_data);

    return TRUE;
}

bool aanc_lite_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    /* Response pointer */
    OP_BUF_DETAILS_RSP *p_resp;

#ifndef DISABLE_IN_PLACE
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;
#endif

    /* Variables used for distinguishing source/sink */
    unsigned max_value;
    tCbuffer** opposite_buffer;
    tCbuffer* selected_metadata;

    if (!base_op_buffer_details(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    p_resp = (OP_BUF_DETAILS_RSP*) *resp_data;

#ifdef DISABLE_IN_PLACE
    p_resp->runs_in_place = FALSE;
    p_resp->b.buffer_size = AANC_LITE_DEFAULT_BUFFER_SIZE;
#else

    /* Determine whether sink or source terminal being disconnected */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC Lite buffer details: sink buffer %d", terminal_num);
        max_value = AANC_LITE_MAX_SINKS;
        opposite_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC Lite buffer details: source buffer %d", terminal_num);
        max_value = AANC_LITE_MAX_SOURCES;
        opposite_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC Lite buffer details failed: invalid terminal %d",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    if (opposite_buffer[terminal_num] != NULL)
    {
        p_resp->runs_in_place = TRUE;
        p_resp->b.in_place_buff_params.in_place_terminal = \
            terminal_id ^ TERMINAL_SINK_MASK;
        p_resp->b.in_place_buff_params.size = AANC_LITE_DEFAULT_BUFFER_SIZE;
        p_resp->b.in_place_buff_params.buffer = \
            opposite_buffer[terminal_num];
        L4_DBG_MSG1("aanc_lite buffer_details: %d", p_resp->b.buffer_size);
    }
    else
    {
        p_resp->runs_in_place = FALSE;
        p_resp->b.buffer_size = AANC_LITE_DEFAULT_BUFFER_SIZE;
    }
    p_resp->supports_metadata = TRUE;
    p_resp->metadata_buffer = selected_metadata;


#endif /* DISABLE_IN_PLACE */
    return TRUE;
}

bool aanc_lite_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data)
{
    OP_SCHED_INFO_RSP* resp;

    resp = base_op_get_sched_info_ex(op_data, message_data, response_id);
    if (resp == NULL)
    {
        return base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED,
                                             resp_data);
    }

    *resp_data = resp;
    resp->block_size = AANC_LITE_DEFAULT_BLOCK_SIZE;

    return TRUE;
}

/****************************************************************************
Opmsg handlers
*/
bool aanc_lite_opmsg_set_control(OPERATOR_DATA *op_data, void *message_data,
                                 unsigned *resp_length,
                                 OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);

    unsigned i;
    unsigned num_controls;
    unsigned ctrl_value;
    unsigned ctrl_id;

    CPS_CONTROL_SOURCE  ctrl_src;
    OPMSG_RESULT_STATES result = OPMSG_RESULT_STATES_NORMAL_STATE;

    if(!cps_control_setup(message_data, resp_length, resp_data, &num_controls))
    {
       return FALSE;
    }

    /* Iterate through the control messages looking for mode and gain override
     * messages */
    for (i=0; i<num_controls; i++)
    {
        ctrl_id = cps_control_get(message_data, i, &ctrl_value, &ctrl_src);

        /* Mode override */
        if (ctrl_id == OPMSG_CONTROL_MODE_ID)
        {
            /* Check for valid mode */
            ctrl_value &= AANC_LITE_SYSMODE_MASK;
            if (ctrl_value >= AANC_LITE_SYSMODE_MAX_MODES)
            {
                result = OPMSG_RESULT_STATES_INVALID_CONTROL_VALUE;
                break;
            }

            /* Gain update logic */
            switch (ctrl_value)
            {
                case AANC_LITE_SYSMODE_STANDBY:
                    /* Set current mode to Standby */
                    p_ext_data->cur_mode = AANC_LITE_SYSMODE_STANDBY;
                    break;
                case AANC_LITE_SYSMODE_STATIC:
                    /* Set current mode to Static */
                    p_ext_data->cur_mode = AANC_LITE_SYSMODE_STATIC;
                    break;
                case AANC_LITE_SYSMODE_FULL:
                    /* reinitialize the gain ramp init flag */
                    p_ext_data->gain_ramp_init_flag = FALSE;
                    /* Set current mode to Full */
                    p_ext_data->cur_mode = AANC_LITE_SYSMODE_FULL;
                    break;
                default:
                    /* Handled by early exit above */
                    break;
            }
        }
    }
    cps_response_set_result(resp_data, result);

    return TRUE;
}

bool aanc_lite_opmsg_get_params(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                     resp_length, resp_data);
}

bool aanc_lite_opmsg_get_defaults(OPERATOR_DATA *op_data, void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetDefaultsMsgHandler(&p_ext_data->params_def, message_data,
                                    resp_length, resp_data);
}

bool aanc_lite_opmsg_set_params(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    bool retval;
    /* patch_fn(aanc_opmsg_set_params); */

    retval = cpsSetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                       resp_length, resp_data);

    if (retval)
    {
        /* Set re-initialization flag for capability */
        p_ext_data->re_init_flag = TRUE;
    }
    else
    {
        L2_DBG_MSG("AANC Lite Set Parameters Failed");
    }

    return retval;
}

bool aanc_lite_opmsg_get_status(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* patch_fn_shared(aanc_capability);  TODO: patch functions */
    int i;

    /* Build the response */
    unsigned *resp = NULL;
    if(!common_obpm_status_helper(message_data, resp_length, resp_data,
                                  sizeof(AANC_LITE_STATISTICS), &resp))
    {
         return FALSE;
    }

    if (resp)
    {
        AANC_LITE_STATISTICS stats;
        AANC_LITE_STATISTICS *pstats = &stats;
        ParamType *pparam = (ParamType*)pstats;

        pstats->OFFSET_CUR_MODE            = p_ext_data->cur_mode;
        /* Send previous gain value as stat because this is only updated
         * when the value is actually written to HW.
         */
        pstats->OFFSET_FF_FINE_GAIN_CTRL   = p_ext_data->ff_fine_gain_prev;

        for (i=0; i<AANC_LITE_N_STAT/2; i++)
        {
            resp = cpsPack2Words(pparam[2*i], pparam[2*i+1], resp);
        }
        if (AANC_LITE_N_STAT % 2) // last one
        {
            cpsPack1Word(pparam[AANC_LITE_N_STAT-1], resp);
        }
    }

    return TRUE;
}

bool ups_params_aanc_lite(void* instance_data, PS_KEY_TYPE key,
                          PERSISTENCE_RANK rank, uint16 length,
                          unsigned* data, STATUS_KYMERA status,
                          uint16 extra_status_info)
{
    OPERATOR_DATA *op_data = (OPERATOR_DATA*) instance_data;
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);

    cpsSetParameterFromPsStore(&p_ext_data->params_def, length, data, status);

    /* Set the reinitialization flag after setting the parameters */
    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

bool aanc_lite_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *resp_length,
                              OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    PS_KEY_TYPE key;
    bool retval;

    retval = cpsSetUcidMsgHandler(&p_ext_data->params_def, message_data,
                                  resp_length, resp_data);
    L5_DBG_MSG1("AANC Lite cpsSetUcidMsgHandler Return Value %d", retval);
    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(p_ext_data->cap_id,
                                         p_ext_data->params_def.ucid,
                                         OPMSG_P_STORE_PARAMETER_SUB_ID);

    ps_entry_read((void*)op_data, key, PERSIST_ANY, ups_params_aanc_lite);

    L5_DBG_MSG1("AANC Lite UCID Set to %d", p_ext_data->params_def.ucid);

    p_ext_data->re_init_flag = TRUE;

    return retval;
}

bool aanc_lite_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *resp_length,
                               OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetUcidMsgHandler(&p_ext_data->params_def, p_ext_data->cap_id,
                                message_data, resp_length, resp_data);
}

/****************************************************************************
Custom opmsg handlers
*/
bool aanc_lite_opmsg_set_static_gain(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *resp_length,
                                     OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);

    p_ext_data->ff_static_fine_gain = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FF_FINE_STATIC_GAIN);
    L4_DBG_MSG1("AANC Lite Set FF Static Gain: Fine = %d",
                p_ext_data->ff_static_fine_gain);
    return TRUE;
}

/****************************************************************************
Data processing function
*/
void aanc_lite_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    AANC_LITE_OP_DATA *p_ext_data = get_instance_data(op_data);

    int i = 0;

    /* Certain conditions require an "early exit" that will just discard any
     * data in the input buffers and not do any other processing
     */
    bool exit_early = FALSE;

    /* track the number of samples process */
    int sample_count = 0;

    /* Reference the capability parameters */
    AANC_LITE_PARAMETERS *p_params = &p_ext_data->aanc_lite_cap_params;

    int samples_to_process = INT_MAX;

    /*********************
     * Early exit testing
     *********************/

    /* Without adequate data or space we can just return */

    /* Determine whether to copy any input data to output terminals */
    samples_to_process = aanc_lite_calc_samples_to_process(p_ext_data);

    /* Return early if int and ext mic input terminals are not connected */
    if (samples_to_process == INT_MAX)
    {
        L5_DBG_MSG("Minimum number of ports (int and ext mic) not connected");
        return;
    }

     /* Return early if no data or not enough space to process */
    if (samples_to_process < AANC_LITE_DEFAULT_FRAME_SIZE)
    {
        L5_DBG_MSG1("Not enough data/space to process (%d)", samples_to_process);
        return;
    }

    /* Other conditions that are invalid for running AANC need to discard
     * input data if it exists.
     */

    /* Don't do any processing in standby */
    if (p_ext_data->cur_mode == AANC_LITE_SYSMODE_STANDBY)
    {
        exit_early = TRUE;
    }

    sample_count = 0;
    if (exit_early)
    {
        bool discard_data = TRUE;

        /* There is at least 1 frame to process */
        do {
            sample_count += AANC_LITE_DEFAULT_FRAME_SIZE;
            /* Iterate through all sinks */
            for (i = 0; i < AANC_LITE_MAX_SINKS; i++)
            {
                if (p_ext_data->inputs[i] != NULL)
                {
                    /* Discard a frame of data */
                    cbuffer_discard_data(p_ext_data->inputs[i],
                                         AANC_LITE_DEFAULT_FRAME_SIZE);

                    /* If there isn't a frame worth of data left then don't
                     * iterate through the input terminals again.
                     */
                    samples_to_process = cbuffer_calc_amount_data_in_words(
                        p_ext_data->inputs[i]);

                    if (samples_to_process < AANC_LITE_DEFAULT_FRAME_SIZE)
                    {
                        discard_data = FALSE;
                    }
                }
            }
        } while (discard_data);
        /* Input is discarded, so consume and delete incoming metadata tags
            This can be done by transporting input metadata to NULL ptr */
        metadata_strict_transport(p_ext_data->metadata_ip,
                                  NULL,
                                  sample_count * OCTETS_PER_SAMPLE);

        /* Return on early exit */
        return;
    }

    if (p_ext_data->re_init_flag == TRUE)
    {
        p_ext_data->re_init_flag = FALSE;
        /* Initialize gain ramp duration frame count */
        p_ext_data->gain_ramp_duration = (uint16) (
            (p_params->OFFSET_GAIN_RAMP_TIMER * AANC_LITE_FRAME_RATE)
                >> AANC_LITE_TIMER_PARAM_SHIFT);
        /* Check if gain ramp duration is 0,
        if it is 0, set to 1 to allow 1 frame for gain to ramp up to max value */

        if (p_ext_data->gain_ramp_duration == 0)
        {
            p_ext_data->gain_ramp_duration = 1;
        }

        p_ext_data->gain_ramp_init_flag = FALSE;
        p_ext_data->gain_ramp_rate = 0;
        p_ext_data->gain_full_precision = 0;
        p_ext_data->ff_fine_gain = 0;
    }

    sample_count = 0;
    while (samples_to_process >= AANC_LITE_DEFAULT_FRAME_SIZE)
    {
        if (p_ext_data->outputs[AANC_LITE_MIC_INT_TERMINAL_ID] != NULL &&
            p_ext_data->outputs[AANC_LITE_MIC_EXT_TERMINAL_ID] != NULL)
        {
            cbuffer_copy(p_ext_data->outputs[AANC_LITE_MIC_INT_TERMINAL_ID],
                         p_ext_data->inputs[AANC_LITE_MIC_INT_TERMINAL_ID],
                         AANC_LITE_DEFAULT_FRAME_SIZE);
            cbuffer_copy(p_ext_data->outputs[AANC_LITE_MIC_EXT_TERMINAL_ID],
                         p_ext_data->inputs[AANC_LITE_MIC_EXT_TERMINAL_ID],
                         AANC_LITE_DEFAULT_FRAME_SIZE);
        }
        else
        {
            cbuffer_discard_data(p_ext_data->inputs[AANC_LITE_MIC_INT_TERMINAL_ID],
                                 AANC_LITE_DEFAULT_FRAME_SIZE);
            cbuffer_discard_data(p_ext_data->inputs[AANC_LITE_MIC_EXT_TERMINAL_ID],
                                 AANC_LITE_DEFAULT_FRAME_SIZE);
        }

        samples_to_process = aanc_lite_calc_samples_to_process(p_ext_data);
        sample_count += AANC_LITE_DEFAULT_FRAME_SIZE;
    }

    /**************
     * Update gain
     **************/
    /* Check SYSMODE state as this is the primary control */
    bool disable_gain_ramp = (p_params->OFFSET_AANC_LITE_CONFIG &
                              AANC_LITE_CONFIG_AANC_LITE_CONFIG_DISABLE_GAIN_RAMP);
    switch (p_ext_data->cur_mode)
    {
        case AANC_LITE_SYSMODE_STANDBY:
            L4_DBG_MSG("AANC Lite STANDBY mode");
            break; /* Shouldn't ever get here */
        case AANC_LITE_SYSMODE_FULL:
            /* Gain ramps up to 255 in duration specified by parameter */
            if (!(disable_gain_ramp) && (p_ext_data->ff_fine_gain < AANC_LITE_MAX_GAIN))
            {
                if (!(p_ext_data->gain_ramp_init_flag))
                {
                    /* Initialize gain value,
                    calculate gain ramp rate */
                    p_ext_data->gain_ramp_init_flag = TRUE;
                    p_ext_data->gain_full_precision =
                                (unsigned) (p_ext_data->ff_fine_gain);
                    p_ext_data->gain_full_precision =
                        p_ext_data->gain_full_precision << AANC_LITE_GAIN_SHIFT;
                    p_ext_data->gain_ramp_rate =
                       ((AANC_LITE_MAX_GAIN << AANC_LITE_GAIN_SHIFT) -
                        p_ext_data->gain_full_precision)/p_ext_data->gain_ramp_duration;
                }
                /* ramp gain at the determined rate */
                p_ext_data->gain_full_precision =
                     p_ext_data->gain_full_precision + p_ext_data->gain_ramp_rate;

                uint16 new_gain = (uint16) ((p_ext_data->gain_full_precision +
                         (1 << (AANC_LITE_GAIN_SHIFT - 1))) >> AANC_LITE_GAIN_SHIFT);

                if (new_gain < AANC_LITE_MAX_GAIN)
                {
                    p_ext_data->ff_fine_gain = new_gain;
                }
                else
                {
                    p_ext_data->ff_fine_gain = AANC_LITE_MAX_GAIN;
                }
                L4_DBG_MSG1("AANC Lite FULL mode: updating gain to %d",
                            p_ext_data->ff_fine_gain);
            }
            break;
        case AANC_LITE_SYSMODE_STATIC:
            /* Set fine gain to static */
            p_ext_data->ff_fine_gain = p_ext_data->ff_static_fine_gain;
            L4_DBG_MSG1("AANC Lite STATIC mode: gain set to static value: %d",
                        p_ext_data->ff_fine_gain);
            break;
        default:
            L2_DBG_MSG1("AANC Lite SYSMODE invalid: %d", p_ext_data->cur_mode);
            break;
    }

    aanc_lite_update_gain(op_data, p_ext_data);

    /****************
     * Pass Metadata
     ****************/
    metadata_strict_transport(p_ext_data->metadata_ip,
                              p_ext_data->metadata_op,
                              sample_count * OCTETS_PER_SAMPLE);

    /***************************
     * Update touched terminals
     ***************************/
    touched->sinks = (unsigned) p_ext_data->touched_sinks;
    touched->sources = (unsigned) p_ext_data->touched_sources;

    L5_DBG_MSG("AANC Lite process channel data completed");

    return;
}
