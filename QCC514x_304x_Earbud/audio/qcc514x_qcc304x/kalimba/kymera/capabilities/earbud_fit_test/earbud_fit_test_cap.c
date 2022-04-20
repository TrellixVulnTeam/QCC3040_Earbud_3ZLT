/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \ingroup capabilities
 * \file  earbud_fit_test_cap.c
 * \ingroup Earbud Fit Test
 *
 * Earbud Fit Test operator capability.
 *
 */

/****************************************************************************
Include Files
*/

#include "earbud_fit_test_cap.h"

/*****************************************************************************
Private Constant Definitions
*/
#ifdef CAPABILITY_DOWNLOAD_BUILD
#define EARBUD_FIT_TEST_16K_CAP_ID   CAP_ID_DOWNLOAD_EARBUD_FIT_TEST_16K
#else
#define EARBUD_FIT_TEST_16K_CAP_ID   CAP_ID_EARBUD_FIT_TEST_16K
#endif

/* Message handlers */
const handler_lookup_struct eft_handler_table =
{
    eft_create,                   /* OPCMD_CREATE */
    eft_destroy,                  /* OPCMD_DESTROY */
    eft_start,                    /* OPCMD_START */
    base_op_stop,                 /* OPCMD_STOP */
    eft_reset,                    /* OPCMD_RESET */
    eft_connect,                  /* OPCMD_CONNECT */
    eft_disconnect,               /* OPCMD_DISCONNECT */
    eft_buffer_details,           /* OPCMD_BUFFER_DETAILS */
    base_op_get_data_format,      /* OPCMD_DATA_FORMAT */
    eft_get_sched_info            /* OPCMD_GET_SCHED_INFO */
};

/* Null-terminated operator message handler table */
const opmsg_handler_lookup_table_entry eft_opmsg_handler_table[] =
    {{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION, base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_CONTROL,             eft_opmsg_set_control},
    {OPMSG_COMMON_ID_GET_PARAMS,              eft_opmsg_get_params},
    {OPMSG_COMMON_ID_GET_DEFAULTS,            eft_opmsg_get_defaults},
    {OPMSG_COMMON_ID_SET_PARAMS,              eft_opmsg_set_params},
    {OPMSG_COMMON_ID_GET_STATUS,              eft_opmsg_get_status},
    {OPMSG_COMMON_ID_SET_UCID,                eft_opmsg_set_ucid},
    {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,       eft_opmsg_get_ps_id},

    {0, NULL}};

const CAPABILITY_DATA earbud_fit_test_16k_cap_data =
    {
        /* Capability ID */
        EARBUD_FIT_TEST_16K_CAP_ID,
        /* Version information - hi and lo */
        EARBUD_FIT_TEST_EARBUD_FIT_TEST_16K_VERSION_MAJOR, EARBUD_FIT_TEST_CAP_VERSION_MINOR,
        /* Max number of sinks/inputs and sources/outputs */
        2, 0,
        /* Pointer to message handler function table */
        &eft_handler_table,
        /* Pointer to operator message handler function table */
        eft_opmsg_handler_table,
        /* Pointer to data processing function */
        eft_process_data,
        /* Reserved */
        0,
        /* Size of capability-specific per-instance data */
        sizeof(EFT_OP_DATA)
    };

MAP_INSTANCE_DATA(EARBUD_FIT_TEST_16K_CAP_ID, EFT_OP_DATA)

/****************************************************************************
Inline Functions
*/

/**
 * \brief  Get EFT instance data.
 *
 * \param  op_data  Pointer to the operator data.
 *
 * \return  Pointer to extra operator data EFT_OP_DATA.
 */
static inline EFT_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (EFT_OP_DATA *) base_op_get_instance_data(op_data);
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
static int eft_calc_samples_to_process(EFT_OP_DATA *p_ext_data)
{
    int i, amt, min_data;

    /* Return if playback and int mic input terminals are not connected */
    if (p_ext_data->inputs[EFT_PLAYBACK_TERMINAL_ID] == NULL ||
        p_ext_data->inputs[EFT_MIC_INT_TERMINAL_ID] == NULL)
    {
        return INT_MAX;
    }

    min_data = EFT_DEFAULT_FRAME_SIZE;
    /* Calculate the amount of data available */
    for (i = EFT_PLAYBACK_TERMINAL_ID; i <= EFT_MIC_INT_TERMINAL_ID; i++)
    {
        if (p_ext_data->inputs[i] != NULL)
        {
            amt = cbuffer_calc_amount_data_in_words(p_ext_data->inputs[i]);
            if (amt < min_data)
            {
                min_data = amt;
            }
        }
    }

    /* Samples to process determined as minimum data available */
    return min_data;
}

static void eft_clear_event(EFT_EVENT *p_event)
{
       p_event->frame_counter =p_event->set_frames;
       p_event->running = EFT_EVENT_CLEAR;
}

/**
 * \brief  Sent an event trigger message.
 *
 * \param op_data  Address of the EFT operator data.
 * \param  id  ID for the event message
 * \param  payload Payload for the event message
 *
 * \return  bool indicating success
 */
static bool eft_send_event_trigger(OPERATOR_DATA *op_data,
                                    uint16 id, uint16 payload)
{
    unsigned msg_size;
    unsigned *trigger_message = NULL;

    msg_size = OPMSG_UNSOLICITED_EFT_EVENT_TRIGGER_WORD_SIZE;
    trigger_message = xpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        L2_DBG_MSG("Failed to send EFT event message");
        return FALSE;
    }

    OPMSG_CREATION_FIELD_SET(trigger_message,
                             OPMSG_UNSOLICITED_EFT_EVENT_TRIGGER,
                             ID,
                             id);
    OPMSG_CREATION_FIELD_SET(trigger_message,
                             OPMSG_UNSOLICITED_EFT_EVENT_TRIGGER,
                             PAYLOAD,
                             payload);

    L2_DBG_MSG2("EFT Event Sent: [%u, %u]", trigger_message[0],
                trigger_message[1]);
    common_send_unsolicited_message(op_data,
                                    (unsigned)OPMSG_REPLY_ID_EFT_EVENT_TRIGGER,
                                    msg_size,
                                    trigger_message);

    pdelete(trigger_message);

    return TRUE;
}

/**
 * \brief  Initialize events for messaging.
 *
 * \param  op_data  Address of the operator data
 * \param  p_ext_data  Address of the EFT extra_op_data.
 *
 * \return  void.
 */
static void eft_initialize_events(OPERATOR_DATA *op_data, EFT_OP_DATA *p_ext_data)
{
    EARBUD_FIT_TEST_PARAMETERS *p_params = &p_ext_data->eft_cap_params;
    unsigned set_frames;

    set_frames = (p_params->OFFSET_EVENT_GOOD_FIT * EFT_FRAME_RATE);
    set_frames = set_frames >> EFT_TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("EFT Fit Detect Event Initialized at %u frames", set_frames);
    p_ext_data->fit_event_detect.set_frames = set_frames;
    eft_clear_event(&p_ext_data->fit_event_detect);

}

/**
 * \brief  Calculate events for messaging.
 *
 * \param op_data  Address of the EFT operator data.
 * \param  p_ext_data  Address of the EFT extra_op_data.
 *
 * \return  boolean indicating success or failure.
 */
static bool eft_process_events(OPERATOR_DATA *op_data,
                               EFT_OP_DATA *p_ext_data)
{
    /* Current and previous fit quality */
    bool cur_fit = (p_ext_data->fit_quality == 1);
    bool prev_fit = (p_ext_data->prev_fit_quality == 1);
    EFT_EVENT* fit_event = &p_ext_data->fit_event_detect;
    uint16 payload = EFT_EVENT_PAYLOAD_UNUSED;

    if (cur_fit)
    {
        if (prev_fit) /* Steady state for fit detect event */
        {
            if (fit_event->running == EFT_EVENT_DETECTED)
            {
                fit_event->frame_counter -= 1;
                if (fit_event->frame_counter <= 0)
                {
                    /* Payload 1 indicates good fit */
                    payload = EFT_EVENT_PAYLOAD_GOOD;
                    fit_event->running = EFT_EVENT_SENT;
                }
            }
            else if (fit_event->running == EFT_EVENT_CLEAR)
            {
                fit_event->running == EFT_EVENT_DETECTED;
            }
        }
        else
        {
            fit_event->frame_counter -= 1;
            fit_event->running = EFT_EVENT_DETECTED;
        }
    }
    else
    {
        if (prev_fit) /* Check if good fit message has been sent */
        {
            if (fit_event->running == EFT_EVENT_SENT)
            {
                /* if good fit message previously sent, send bad fit message
                    Payload 0 indicates bad fit */
                payload = EFT_EVENT_PAYLOAD_BAD;
            }
            eft_clear_event(fit_event);
        }
    }

    if (payload != EFT_EVENT_PAYLOAD_UNUSED)
    {
        eft_send_event_trigger(op_data,
                               EFT_EVENT_ID_FIT,
                               payload);
    }
    return TRUE;
}

/**
 * \brief  Free memory allocated during processing
 *
 * \param  p_ext_data  Address of the EFT extra_op_data.
 *
 * \return  boolean indicating success or failure.
 */

static bool eft_proc_destroy(EFT_OP_DATA *p_ext_data)
{
    /* Unregister FFT twiddle */
    if (p_ext_data->twiddle_registered)
    {
        aanc_afb_twiddle_release(AANC_FILTER_BANK_WINDOW_SIZE);
        p_ext_data->twiddle_registered = FALSE;
    }
    /* De-register scratch & free AFB */
    if (p_ext_data->scratch_registered)
    {
        scratch_deregister();
        p_ext_data->scratch_registered = FALSE;
    }

    aanc_afb_destroy(p_ext_data->p_afb_ref);
    pfree(p_ext_data->p_afb_ref);
    aanc_afb_destroy(p_ext_data->p_afb_int);
    pfree(p_ext_data->p_afb_int);

    aanc_fit100_destroy(p_ext_data->p_fit);
    pfree(p_ext_data->p_fit);

    cbuffer_destroy(p_ext_data->p_tmp_ref_ip);
    cbuffer_destroy(p_ext_data->p_tmp_int_ip);

    unload_aanc_handle(p_ext_data->f_handle);

    return TRUE;
}

/****************************************************************************
Capability API Handlers
*/

bool eft_create(OPERATOR_DATA *op_data, void *message_data,
                      unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    int i;
    unsigned *p_default_params; /* Pointer to default params */
    unsigned *p_cap_params;     /* Pointer to capability params */

    /* NB: create is passed a zero-initialized structure so any fields not
     * explicitly initialized are 0.
     */

    L5_DBG_MSG1("EFT Create: p_ext_data at %p", p_ext_data);

    if (!base_op_create(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* TODO: patch functions */

    /* Assume the response to be command FAILED. If we reach the correct
     * termination point in create then change it to STATUS_OK.
     */
    base_op_change_response_status(resp_data, STATUS_CMD_FAILED);

    /* Initialize buffers */
    for (i = 0; i < EFT_MAX_SINKS; i++)
    {
        p_ext_data->inputs[i] = NULL;
    }

    for (i = 0; i < EFT_NUM_METADATA_CHANNELS; i++)
    {
        p_ext_data->metadata_ip[i] = NULL;
    }

    /* Initialize capid and sample rate fields */
    p_ext_data->cap_id = EARBUD_FIT_TEST_16K_CAP_ID;

    p_ext_data->sample_rate = 16000;
    /* Initialize parameters */
    p_default_params = (unsigned*) EARBUD_FIT_TEST_GetDefaults(p_ext_data->cap_id);
    p_cap_params = (unsigned*) &p_ext_data->eft_cap_params;
    if(!cpsInitParameters(&p_ext_data->params_def,
                          p_default_params,
                          p_cap_params,
                          sizeof(EARBUD_FIT_TEST_PARAMETERS)))
    {
       return TRUE;
    }

    /* Initialize system mode */
    p_ext_data->cur_mode = EARBUD_FIT_TEST_SYSMODE_FULL;
    p_ext_data->host_mode = EARBUD_FIT_TEST_SYSMODE_FULL;
    p_ext_data->qact_mode = EARBUD_FIT_TEST_SYSMODE_FULL;

    /* Trigger re-initialization at start */
    p_ext_data->re_init_flag = TRUE;

    p_ext_data->p_tmp_ref_ip = cbuffer_create_with_malloc(
                                EFT_INTERNAL_BUFFER_SIZE, BUF_DESC_SW_BUFFER);
    if (p_ext_data->p_tmp_ref_ip == NULL)
    {
        eft_proc_destroy(p_ext_data);
        L2_DBG_MSG("EFT failed to allocate reference input buffer");
        return FALSE;
    }

    p_ext_data->p_tmp_int_ip = cbuffer_create_with_malloc(
                                EFT_INTERNAL_BUFFER_SIZE, BUF_DESC_SW_BUFFER);
    if (p_ext_data->p_tmp_int_ip == NULL)
    {
        eft_proc_destroy(p_ext_data);
        L2_DBG_MSG("EFT failed to allocate int mic input buffer");
        return FALSE;
    }

    /* Allocate twiddle factor for AFB */
    if (!aanc_afb_twiddle_alloc(AANC_FILTER_BANK_WINDOW_SIZE))
    {
        eft_proc_destroy(p_ext_data);
        L2_DBG_MSG("EFT failed to allocate twiddle factors");
        return FALSE;
    }
    p_ext_data->twiddle_registered = TRUE;

    /* Register scratch memory for AFB & allocate object */
    if (!scratch_register())
    {
        eft_proc_destroy(p_ext_data);
        L2_DBG_MSG("EFT failed to register scratch memory");
        return FALSE;
    }

    p_ext_data->scratch_registered = TRUE;

    if (!scratch_reserve(AANC_AFB_SCRATCH_MEMORY, MALLOC_PREFERENCE_DM1) ||
        !scratch_reserve(AANC_AFB_SCRATCH_MEMORY, MALLOC_PREFERENCE_DM2) ||
        !scratch_reserve(AANC_AFB_SCRATCH_MEMORY, MALLOC_PREFERENCE_DM2))
    {
        eft_proc_destroy(p_ext_data);
        L2_DBG_MSG("EFT failed to reserve scratch memory");
        return FALSE;
    }

    p_ext_data->p_afb_ref = xzpmalloc(aanc_afb_bytes());
    if (p_ext_data->p_afb_ref == NULL)
    {
        L2_DBG_MSG("EFT failed to allocate AFB ref");
        eft_proc_destroy(p_ext_data);
    }
    aanc_afb_create(p_ext_data->p_afb_ref);

    p_ext_data->p_afb_int = xzpmalloc(aanc_afb_bytes());
    if (p_ext_data->p_afb_int == NULL)
    {
        L2_DBG_MSG("EFT failed to allocate AFB int");
        eft_proc_destroy(p_ext_data);
    }
    aanc_afb_create(p_ext_data->p_afb_int);

    p_ext_data->p_fit = xzpmalloc(aanc_fit100_bytes());
    if (p_ext_data->p_fit == NULL)
    {
        L2_DBG_MSG("EFT failed to allocate fit100");
        eft_proc_destroy(p_ext_data);
    }
    aanc_fit100_create(p_ext_data->p_fit);

    if (!load_aanc_handle(&p_ext_data->f_handle))
    {
        eft_proc_destroy(p_ext_data);
        L2_DBG_MSG("EFT failed to load feature handle");
        return FALSE;
    }

    /* Operator creation was succesful, change respone to STATUS_OK*/
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("EFT: Created");
    return TRUE;
}

bool eft_destroy(OPERATOR_DATA *op_data, void *message_data,
                  unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);

    /* call base_op destroy that creates and fills response message, too */
    if (!base_op_destroy(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* TODO: patch functions */

    if (p_ext_data != NULL)
    {
        eft_proc_destroy(p_ext_data);
        L4_DBG_MSG("EFT: Cleanup complete.");
    }

    L4_DBG_MSG("EFT: Destroyed");
    return TRUE;
}

bool eft_start(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* TODO: patch functions */

    /* Start with the assumption that we fail and change later if we succeed */
    if (!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, resp_data))
    {
        return FALSE;
    }

    /* Check that we have a minimum number of terminals connected */
    if (p_ext_data->inputs[EFT_PLAYBACK_TERMINAL_ID] == NULL ||
        p_ext_data->inputs[EFT_MIC_INT_TERMINAL_ID] == NULL)
    {
        L4_DBG_MSG("EFT start failure: inputs not connected");
        return TRUE;
    }

    /* Set reinitialization flags to ensure first run behavior */
    p_ext_data->re_init_flag = TRUE;

    /* All good */
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("EFT Started");
    return TRUE;
}

bool eft_reset(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!base_op_reset(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    p_ext_data->re_init_flag = TRUE;

    L4_DBG_MSG("EFT: Reset");
    return TRUE;
}

bool eft_connect(OPERATOR_DATA *op_data, void *message_data,
                       unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id, terminal_num;
    tCbuffer* pterminal_buf;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* Only sink terminal can be connected */
    terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    terminal_num = terminal_id & TERMINAL_NUM_MASK;
    L4_DBG_MSG1("EFT connect: sink terminal %u", terminal_num);

    /* Can't use invalid ID */
    if (terminal_num >= EFT_MAX_SINKS)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("EFT connect failed: invalid terminal %u", terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't connect if already connected */
    if (p_ext_data->inputs[terminal_num] != NULL)
    {
        L4_DBG_MSG1("EFT connect failed: terminal %u already connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    pterminal_buf = OPMGR_GET_OP_CONNECT_BUFFER(message_data);
    p_ext_data->inputs[terminal_num] = pterminal_buf;

    if (p_ext_data->metadata_ip[terminal_num] == NULL &&
        buff_has_metadata(pterminal_buf))
    {
        p_ext_data->metadata_ip[terminal_num] = pterminal_buf;
    }

    return TRUE;
}

bool eft_disconnect(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id, terminal_num;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* Only sink terminal can be disconnected */
    terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    terminal_num = terminal_id & TERMINAL_NUM_MASK;
    L4_DBG_MSG1("EFT disconnect: sink terminal %u", terminal_num);

    /* Can't use invalid ID */
    if (terminal_num >= EFT_MAX_SINKS)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("EFT disconnect failed: invalid terminal %u",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't disconnect if not connected */
    if (p_ext_data->inputs[terminal_num] == NULL)
    {
        L4_DBG_MSG1("EFT disconnect failed: terminal %u not connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    /*  Disconnect the existing metadata and input channel. */
    p_ext_data->metadata_ip[terminal_num] = NULL;
    p_ext_data->inputs[terminal_num] = NULL;

    return TRUE;
}

bool eft_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    /* Response pointer */
    OP_BUF_DETAILS_RSP *p_resp;

#ifndef DISABLE_IN_PLACE
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;
#endif

    if (!base_op_buffer_details(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* Response pointer */
    p_resp = (OP_BUF_DETAILS_RSP*) *resp_data;

#ifdef DISABLE_IN_PLACE
    p_resp->runs_in_place = FALSE;
    p_resp->b.buffer_size = EFT_DEFAULT_BUFFER_SIZE;
#else

    /* Can't use invalid ID */
    if (terminal_num >= EFT_MAX_SINKS)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("EFT buffer details failed: invalid terminal %d",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }
    /* Operator does not run in place */
    p_resp->runs_in_place = FALSE;
    p_resp->b.buffer_size = EFT_DEFAULT_BUFFER_SIZE;
    p_resp->supports_metadata = TRUE;

    if (terminal_num == EFT_PLAYBACK_TERMINAL_ID)
    {
        p_resp->metadata_buffer = p_ext_data->metadata_ip[EFT_METADATA_PLAYBACK_ID];
    }
    else
    {
        p_resp->metadata_buffer = p_ext_data->metadata_ip[EFT_METADATA_INT_ID];
    }
#endif /* DISABLE_IN_PLACE */
    return TRUE;
}

bool eft_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
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
    resp->block_size = EFT_DEFAULT_BLOCK_SIZE;

    return TRUE;
}

/****************************************************************************
Opmsg handlers
*/
bool eft_opmsg_set_control(OPERATOR_DATA *op_data, void *message_data,
                                 unsigned *resp_length,
                                 OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);

    unsigned i;
    unsigned num_controls;

    OPMSG_RESULT_STATES result = OPMSG_RESULT_STATES_NORMAL_STATE;

    if(!cps_control_setup(message_data, resp_length, resp_data, &num_controls))
    {
       return FALSE;
    }

    /* Iterate through the control messages looking for mode and gain override
     * messages */
    for (i=0; i<num_controls; i++)
    {
        unsigned ctrl_value, ctrl_id;
        CPS_CONTROL_SOURCE  ctrl_src;

        ctrl_id = cps_control_get(message_data, i, &ctrl_value, &ctrl_src);

        /* Mode override */
        if (ctrl_id == OPMSG_CONTROL_MODE_ID)
        {
            /* Check for valid mode */
            ctrl_value &= EFT_SYSMODE_MASK;
            if (ctrl_value >= EARBUD_FIT_TEST_SYSMODE_MAX_MODES)
            {
                result = OPMSG_RESULT_STATES_INVALID_CONTROL_VALUE;
                break;
            }

            eft_initialize_events(op_data, p_ext_data);

            /* Gain update logic */
            switch (ctrl_value)
            {
                case EARBUD_FIT_TEST_SYSMODE_STANDBY:
                    /* Set current mode to Standby */
                    p_ext_data->cur_mode = EARBUD_FIT_TEST_SYSMODE_STANDBY;
                    break;
                case EARBUD_FIT_TEST_SYSMODE_FULL:
                    /* Set current mode to Full */
                    p_ext_data->cur_mode = EARBUD_FIT_TEST_SYSMODE_FULL;
                    break;
                default:
                    /* Handled by early exit above */
                    break;
            }

            /* Determine control mode source and set override flags for mode */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                p_ext_data->host_mode = ctrl_value;
            }
            else
            {
                p_ext_data->qact_mode = ctrl_value;
                /* Set or clear the QACT override flag.
                * &= is used to preserve the state of the
                * override word.
                */
                if (ctrl_src == CPS_SOURCE_OBPM_ENABLE)
                {
                    p_ext_data->ovr_control |= EARBUD_FIT_TEST_CONTROL_MODE_OVERRIDE;
                }
                else
                {
                    p_ext_data->ovr_control &= EFT_OVERRIDE_MODE_MASK;
                }
            }

        }
        /* In/Out of Ear control */
        else if (ctrl_id == EARBUD_FIT_TEST_CONSTANT_IN_OUT_EAR_CTRL)
        {
            ctrl_value &= 0x01;
            p_ext_data->in_out_status = ctrl_value;

        }
        else
        {
            result = OPMSG_RESULT_STATES_UNSUPPORTED_CONTROL;
            break;
        }
    }

    /* Set current operating mode based on override */
    if ((p_ext_data->ovr_control & EARBUD_FIT_TEST_CONTROL_MODE_OVERRIDE) != 0)
    {
        p_ext_data->cur_mode = p_ext_data->qact_mode;
    }
    else
    {
        p_ext_data->cur_mode = p_ext_data->host_mode;
    }

    cps_response_set_result(resp_data, result);

    return TRUE;
}

bool eft_opmsg_get_params(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                     resp_length, resp_data);
}

bool eft_opmsg_get_defaults(OPERATOR_DATA *op_data, void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetDefaultsMsgHandler(&p_ext_data->params_def, message_data,
                                    resp_length, resp_data);
}

bool eft_opmsg_set_params(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    bool success;
    /* patch_fn TODO */

    success = cpsSetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                       resp_length, resp_data);

    if (success)
    {
        /* Set re-initialization flag for capability */
        p_ext_data->re_init_flag = TRUE;
    }
    else
    {
        L2_DBG_MSG("EFT Set Parameters Failed");
    }

    return success;
}

bool eft_opmsg_get_status(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* TODO: patch functions */
    int i;

    /* Build the response */
    unsigned *resp = NULL;
    if(!common_obpm_status_helper(message_data, resp_length, resp_data,
                                  sizeof(EARBUD_FIT_TEST_STATISTICS), &resp))
    {
         return FALSE;
    }

    if (resp)
    {
        EARBUD_FIT_TEST_STATISTICS stats;
        EARBUD_FIT_TEST_STATISTICS *pstats = &stats;
        ParamType *pparam = (ParamType*)pstats;

        pstats->OFFSET_CUR_MODE         = p_ext_data->cur_mode;
        pstats->OFFSET_OVR_CONTROL      = p_ext_data->ovr_control;
        pstats->OFFSET_IN_OUT_EAR_CTRL  = p_ext_data->in_out_status;

        pstats->OFFSET_FIT_QUALITY_FLAG = p_ext_data->fit_quality;

        pstats->OFFSET_FIT_EVENT        = p_ext_data->fit_event_detect.running;
        pstats->OFFSET_FIT_TIMER        = (p_ext_data->fit_event_detect.frame_counter
                                           << EFT_TIMER_PARAM_SHIFT)/EFT_FRAME_RATE;
        pstats->OFFSET_POWER_REF        = p_ext_data->p_fit->pwr_reference;
        pstats->OFFSET_POWER_INT_MIC    = p_ext_data->p_fit->pwr_internal;
        pstats->OFFSET_POWER_RATIO      = p_ext_data->p_fit->pwr_ratio;

        for (i=0; i<EFT_N_STAT/2; i++)
        {
            resp = cpsPack2Words(pparam[2*i], pparam[2*i+1], resp);
        }
        if ((EFT_N_STAT % 2) == 1) // last one
        {
            cpsPack1Word(pparam[EFT_N_STAT-1], resp);
        }
    }

    return TRUE;
}

bool ups_params_eft(void* instance_data, PS_KEY_TYPE key,
                          PERSISTENCE_RANK rank, uint16 length,
                          unsigned* data, STATUS_KYMERA status,
                          uint16 extra_status_info)
{
    OPERATOR_DATA *op_data = (OPERATOR_DATA*) instance_data;
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);

    cpsSetParameterFromPsStore(&p_ext_data->params_def, length, data, status);

    /* Set the reinitialization flag after setting the parameters */
    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

bool eft_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *resp_length,
                              OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    PS_KEY_TYPE key;
    bool retval;

    retval = cpsSetUcidMsgHandler(&p_ext_data->params_def, message_data,
                                  resp_length, resp_data);
    L5_DBG_MSG1("EFT cpsSetUcidMsgHandler Return Value %u", retval);
    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(p_ext_data->cap_id,
                                         p_ext_data->params_def.ucid,
                                         OPMSG_P_STORE_PARAMETER_SUB_ID);

    ps_entry_read((void*)op_data, key, PERSIST_ANY, ups_params_eft);

    L5_DBG_MSG1("EFT UCID Set to %u", p_ext_data->params_def.ucid);

    p_ext_data->re_init_flag = TRUE;

    return retval;
}

bool eft_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *resp_length,
                               OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetUcidMsgHandler(&p_ext_data->params_def,
                                p_ext_data->cap_id,
                                message_data,
                                resp_length,
                                resp_data);
}

/****************************************************************************
Data processing function
*/
void eft_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    EFT_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* Reference the capability parameters */
    EARBUD_FIT_TEST_PARAMETERS *p_params;
    int i, sample_count, samples_to_process;

    /* Certain conditions require an "early exit" that will just discard any
     * data in the input buffers and not do any other processing
     */
    bool exit_early, discard_data;
    unsigned b4idx, afteridx;
    metadata_tag *mtag_ip_list;

    /*********************
     * Early exit testing
     *********************/

    /* Without adequate data we can just return */
    samples_to_process = INT_MAX;
    samples_to_process = eft_calc_samples_to_process(p_ext_data);

    /* Return early if playback and int mic input terminals are not connected */
    if (samples_to_process == INT_MAX)
    {
        L5_DBG_MSG("Minimum number of ports (ref and int mic) not connected");
        return;
    }

     /* Return early if not enough data to process */
    if (samples_to_process < EFT_DEFAULT_FRAME_SIZE)
    {
        L5_DBG_MSG1("Not enough data to process (%d)", samples_to_process);
        return;
    }

    /* Other conditions that are invalid for running EFT need to discard
     * input data if it exists.
     */

    exit_early = FALSE;
    /* Don't do any processing in standby */
    if (p_ext_data->cur_mode == EARBUD_FIT_TEST_SYSMODE_STANDBY)
    {
        exit_early = TRUE;
    }

    if (p_ext_data->in_out_status != EFT_IN_EAR)
    {
        exit_early = TRUE;
    }

    sample_count = 0;
    if (exit_early)
    {
        discard_data = TRUE;

        /* There is at least 1 frame to process */
        do {
            sample_count += EFT_DEFAULT_FRAME_SIZE;
            /* Iterate through all sinks */
            for (i = 0; i < EFT_MAX_SINKS; i++)
            {
                if (p_ext_data->inputs[i] != NULL)
                {
                    /* Discard a frame of data */
                    cbuffer_discard_data(p_ext_data->inputs[i],
                                         EFT_DEFAULT_FRAME_SIZE);

                    /* If there isn't a frame worth of data left then don't
                     * iterate through the input terminals again.
                     */
                    samples_to_process = cbuffer_calc_amount_data_in_words(
                        p_ext_data->inputs[i]);

                    if (samples_to_process < EFT_DEFAULT_FRAME_SIZE)
                    {
                        discard_data = FALSE;
                    }
                }
            }
        } while (discard_data);

        for (i=0; i < EFT_NUM_METADATA_CHANNELS; i++)
        {
            /* Extract metadata tag from input */
            mtag_ip_list = buff_metadata_remove(p_ext_data->metadata_ip[i],
                            sample_count * OCTETS_PER_SAMPLE, &b4idx, &afteridx);

            /* Free all the incoming tags */
            buff_metadata_tag_list_delete(mtag_ip_list);
        }

        /* Exit early */
        return;
    }

    if (p_ext_data->re_init_flag == TRUE)
    {
        p_ext_data->re_init_flag = FALSE;

        /* Initialize events*/
        eft_initialize_events(op_data, p_ext_data);

        /* Initialize afb and fit100 */
        aanc_afb_initialize(p_ext_data->f_handle,
                            p_ext_data->p_afb_ref);
        aanc_afb_initialize(p_ext_data->f_handle,
                            p_ext_data->p_afb_int);

        aanc_fit100_initialize(p_ext_data->f_handle,
                               p_ext_data->p_fit,
                               p_ext_data->p_afb_int,
                               p_ext_data->p_afb_ref);

        p_params = &p_ext_data->eft_cap_params;
        p_ext_data->p_fit->time_constant = p_params->OFFSET_POWER_SMOOTH_FACTOR;
        p_ext_data->p_fit->threshold = p_params->OFFSET_FIT_THRESHOLD;
        p_ext_data->p_fit->bexp_offset = 0;

        p_ext_data->fit_quality = 0;
        p_ext_data->prev_fit_quality = 0;
    }

    sample_count = 0;
    while (samples_to_process >= EFT_DEFAULT_FRAME_SIZE)
    {

        /* Copy input data to internal data buffers */
        cbuffer_copy(p_ext_data->p_tmp_ref_ip,
                     p_ext_data->inputs[EFT_PLAYBACK_TERMINAL_ID],
                     EFT_DEFAULT_FRAME_SIZE);
        cbuffer_copy(p_ext_data->p_tmp_int_ip,
                     p_ext_data->inputs[EFT_MIC_INT_TERMINAL_ID],
                     EFT_DEFAULT_FRAME_SIZE);

        t_fft_object *p_fft_ref = p_ext_data->p_afb_ref->afb.fft_object_ptr;
        p_fft_ref->real_scratch_ptr = scratch_commit(
            AANC_FILTER_BANK_NUM_BINS*sizeof(int), MALLOC_PREFERENCE_DM1);
        p_fft_ref->imag_scratch_ptr = scratch_commit(
            AANC_FILTER_BANK_NUM_BINS*sizeof(int), MALLOC_PREFERENCE_DM2);
        p_fft_ref->fft_scratch_ptr = scratch_commit(
            AANC_FILTER_BANK_NUM_BINS*sizeof(int), MALLOC_PREFERENCE_DM2);

        /* AFB process on reference */
        aanc_afb_process_data(p_ext_data->f_handle, p_ext_data->p_afb_ref,
                              p_ext_data->p_tmp_ref_ip);

        /* Second AFB call re-uses scratch memory from the first */
        t_fft_object *p_fft_int = p_ext_data->p_afb_int->afb.fft_object_ptr;
        p_fft_int->real_scratch_ptr = p_fft_ref->real_scratch_ptr;
        p_fft_int->imag_scratch_ptr = p_fft_ref->imag_scratch_ptr;
        p_fft_int->fft_scratch_ptr = p_fft_ref->fft_scratch_ptr;

        /* AFB process on int mic */
        aanc_afb_process_data(p_ext_data->f_handle, p_ext_data->p_afb_int,
                              p_ext_data->p_tmp_int_ip);

        /* Set scratch pointers to NULL before freeing scratch */
        p_fft_ref->real_scratch_ptr = NULL;
        p_fft_ref->imag_scratch_ptr = NULL;
        p_fft_ref->fft_scratch_ptr = NULL;
        p_fft_int->real_scratch_ptr = NULL;
        p_fft_int->imag_scratch_ptr = NULL;
        p_fft_int->fft_scratch_ptr = NULL;

        scratch_free();

        /* FIT100 processing */
        aanc_fit100_process_data(p_ext_data->f_handle, p_ext_data->p_fit);

        p_ext_data->fit_quality = p_ext_data->p_fit->fit_flag;

        /* Process and send significant event, if any */
        eft_process_events(op_data, p_ext_data);

        /* Update prev fit flag after event processing */
        p_ext_data->prev_fit_quality = p_ext_data->fit_quality;

        cbuffer_discard_data(p_ext_data->p_tmp_ref_ip,
                                EFT_DEFAULT_FRAME_SIZE);
        cbuffer_discard_data(p_ext_data->p_tmp_int_ip,
                                EFT_DEFAULT_FRAME_SIZE);

        samples_to_process = eft_calc_samples_to_process(p_ext_data);
        sample_count += EFT_DEFAULT_FRAME_SIZE;
    }

    for (i=0; i < EFT_NUM_METADATA_CHANNELS; i++)
    {
        /* Extract metadata tag from input */
        mtag_ip_list = buff_metadata_remove(p_ext_data->metadata_ip[i],
                        sample_count * OCTETS_PER_SAMPLE, &b4idx, &afteridx);

        /* Free all the incoming tags */
        buff_metadata_tag_list_delete(mtag_ip_list);
    }
    /***************************
     * Update touched terminals
     ***************************/
    touched->sinks = (unsigned) EFT_MIN_VALID_SINKS;

    L5_DBG_MSG("EFT process channel data completed");

    return;
}
