/****************************************************************************
 * Copyright (c) 2014 - 2018 Qualcomm Technologies International, Ltd
****************************************************************************/
/**
 * \file  wwe.c
 * \ingroup  capabilities
 *
 *  A capability library wrapper for a common Wake Word Engine (WWE) class.
 *
 */

#include <string.h>
#include "capabilities.h"
#include "op_msg_helpers.h"
#include "obpm_prim.h"
#include "accmd_prim.h"
#include "mem_utils/scratch_memory.h"
#include "wwe_cap_private.h"
#include "ttp/ttp.h"
#include "ttp_utilities.h"
#include "opmgr/opmgr_for_ops.h"
#include "cap_id_prim.h"
#include "hal_time.h"

#pragma unitsuppress Unused

/****************************************************************************
Private Function Definitions
*/

/****************************************************************************
Private Constant Declarations
*/


/* Miscellaneous */
static bool wwe_ups_params(void* instance_data, PS_KEY_TYPE key,
                           PERSISTENCE_RANK rank, uint16 length, unsigned* data,
                           STATUS_KYMERA status, uint16 extra_status_info);

/* Reset and initialize statistics on the operator data */
static void wwe_initialize_stats(WWE_COMMON_DATA *p_wwe_data);
static void wwe_initialize_timestamps(WWE_COMMON_DATA *p_wwe_data);

#ifndef RUNNING_ON_KALSIM
static bool wwe_load_fn(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data,
                        DATA_FILE *trigger_file);
static bool wwe_unload_fn(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *resp_length,
                          OP_OPMSG_RSP_PAYLOAD **resp_data, uint16 file_id);
#endif

/******************** Public Function Definitions *************************** */

static inline WWE_CLASS_DATA *get_class_data(OPERATOR_DATA *op_data)
{
    return (WWE_CLASS_DATA *) base_op_get_class_ext(op_data);
}

static inline void set_class_data(OPERATOR_DATA *op_data,
                                  WWE_CLASS_DATA *class_data)
{
    base_op_set_class_ext(op_data, class_data);
}

static inline WWE_COMMON_DATA *get_wwe_data(OPERATOR_DATA *op_data)
{
    WWE_CLASS_DATA *p_class_data = base_op_get_class_ext(op_data);
    return &p_class_data->wwe;
}

bool wwe_base_class_init(OPERATOR_DATA *op_data, WWE_CLASS_DATA *p_class_data,
                         const WWE_CAP_VIRTUAL_TABLE *const vt)
{
    /* Make sure valid pointers have been passed */
    if (op_data == NULL || p_class_data == NULL || vt == NULL)
    {
        return FALSE;
    }

    p_class_data->vt = vt;
    set_class_data(op_data, p_class_data);

    return TRUE;
}

static bool wwe_ups_params(void* instance_data, PS_KEY_TYPE key,
                           PERSISTENCE_RANK rank, uint16 length, unsigned* data,
                           STATUS_KYMERA status, uint16 extra_status_info)
{
    WWE_CLASS_DATA *p_class_data = \
        get_class_data((OPERATOR_DATA*) instance_data);

    cpsSetParameterFromPsStore(&p_class_data->wwe.parms_def, length, data,
                               status);

    return TRUE;
}

static void wwe_initialize_stats(WWE_COMMON_DATA *p_wwe_data)
{
    p_wwe_data->keyword_detected = 0;
    p_wwe_data->result_confidence = 0;
    p_wwe_data->keyword_len = 0;
    p_wwe_data->trigger = 0;
}

static void wwe_initialize_timestamps(WWE_COMMON_DATA *p_wwe_data)
{
    /* TODO: what's the correct initialization for timestamps? */
    p_wwe_data->ts.trigger_start_timestamp = 0;
    p_wwe_data->ts.trigger_end_timestamp = 0;
    p_wwe_data->ts.trigger_offset = 0;
    p_wwe_data->last_timestamp = 0;
}

/*********************** Standard operator definitions ********************** */

bool wwe_base_create(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data,
                     unsigned block_size, unsigned buffer_size,
                     unsigned frame_size)
{

    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    /* Call base_op create initializing operator to NOT_RUNNING state,
       It also allocates and fills response message */
    if (!base_op_create(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* Initialize common structure */
    p_wwe_data->sample_rate = 16000;
    p_wwe_data->metadata_ip_buffer = NULL;
    p_wwe_data->metadata_op_buffer = NULL;
    p_wwe_data->ip_buffer = NULL;
    p_wwe_data->op_buffer = NULL;

    p_wwe_data->p_model = NULL;
    p_wwe_data->n_model_size = 0;
    p_wwe_data->file_id = 0;

    wwe_initialize_stats(p_wwe_data);

    p_wwe_data->cur_mode = WWE_SYSMODE_FULL_PROC;
    p_wwe_data->host_mode = WWE_SYSMODE_FULL_PROC;

    p_wwe_data->block_size = block_size;
    p_wwe_data->buffer_size = buffer_size;
    p_wwe_data->frame_size = frame_size;
    p_wwe_data->init_flag = TRUE;

    wwe_initialize_timestamps(p_wwe_data);

    L0_DBG_MSG("WWE: wwe_base_create\0");

    return TRUE;
}

bool wwe_start(OPERATOR_DATA *op_data, void *message_data,
               unsigned *response_id, void **response_data)
{
    /* Start with the assumption that we fail and change later if we succeed */
    base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data);

    WWE_CLASS_DATA *p_class_data = get_class_data(op_data);
    WWE_COMMON_DATA *p_wwe_data = &p_class_data->wwe;
    const WWE_CAP_VIRTUAL_TABLE *vt = p_class_data->vt;

#ifdef RUNNING_ON_KALSIM
    if (vt->static_load_fn != NULL)
    {
        L0_DBG_MSG("WWE: wwe_start - attempting static model load\0");
        if (!vt->static_load_fn(op_data, message_data, response_id,
                                response_data))
        {
            return TRUE;
        }
    }
#endif /* #ifndef RUNNING_ON_KALSIM */

    /* If model hasn't been allocated we can't start the operator */
    if (p_wwe_data->p_model == NULL || p_wwe_data->ip_buffer == NULL)
    {
        L0_DBG_MSG("WWE: wwe_start - no model loaded\0");
        return TRUE;
    }

    wwe_initialize_timestamps(p_wwe_data);

    if (vt->start_fn != NULL)
    {
        if (!vt->start_fn(op_data, message_data, response_id, response_data))
        {
            return TRUE;
        }
    }

    /* All good */
    base_op_change_response_status(response_data, STATUS_OK);

    L0_DBG_MSG("WWE: wwe_start\0");

    return TRUE;
}

bool wwe_stop(OPERATOR_DATA *op_data, void *message_data,
              unsigned *response_id, void **response_data)
{

    /* Start with the assumption that we fail and change later if we succeed */
    base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data);

    WWE_CLASS_DATA *p_class_data = get_class_data(op_data);
    const WWE_CAP_VIRTUAL_TABLE *vt = p_class_data->vt;

    if (!base_op_stop(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    if (vt->stop_fn != NULL)
    {
        if (!vt->stop_fn(op_data, message_data, response_id, response_data))
        {
            return TRUE;
        }
    }

    /* All good */
    base_op_change_response_status(response_data, STATUS_OK);

    L0_DBG_MSG("WWE: wwe_stop\0");

    return TRUE;
}

bool wwe_connect(OPERATOR_DATA *op_data, void *message_data,
                 unsigned *response_id, void **response_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;

    tCbuffer* pterminal_buf = OPMGR_GET_OP_CONNECT_BUFFER(message_data);

    if (!base_op_connect(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    /* can't connect while running */
    if (opmgr_op_is_running(op_data))
    {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (terminal_num >= 1)
    {
        /* invalid terminal id */
        base_op_change_response_status(response_data,
                                       STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* can't connect a channel which is already connected */
    if (((terminal_id & TERMINAL_SINK_MASK) &&
         (p_wwe_data->ip_buffer != NULL)) ||
         (!(terminal_id & TERMINAL_SINK_MASK) &&
         (p_wwe_data->op_buffer != NULL)))
    {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    /* set terminal cbuffer */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        p_wwe_data->ip_buffer = pterminal_buf;
        if((p_wwe_data->metadata_ip_buffer == NULL) &&
            buff_has_metadata(pterminal_buf))
        {
            p_wwe_data->metadata_ip_buffer = pterminal_buf;
        }
    }
    else
    {
        p_wwe_data->op_buffer = pterminal_buf;
        if((p_wwe_data->metadata_op_buffer == NULL) &&
            buff_has_metadata(pterminal_buf))
        {
            p_wwe_data->metadata_op_buffer = pterminal_buf;
        }
    }
    p_wwe_data->init_flag = TRUE;

    L0_DBG_MSG1("WWE: wwe_connect %d\0", terminal_num);

    return TRUE;
}

bool wwe_disconnect(OPERATOR_DATA *op_data, void *message_data,
                    unsigned *response_id, void **response_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    unsigned terminal_num, terminal_id = \
        OPMGR_GET_OP_DISCONNECT_TERMINAL_ID(message_data);
    terminal_num = terminal_id & TERMINAL_NUM_MASK;

    if (!base_op_disconnect(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    /* can't disconnect while running */
    if (opmgr_op_is_running(op_data))
    {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (terminal_num >= 1)
    {
        /* invalid terminal id */
        base_op_change_response_status(response_data,
                                       STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* can't disconnect a channel which is not connected */
    if (((terminal_id & TERMINAL_SINK_MASK) &&
         (p_wwe_data->ip_buffer == NULL)) ||
        (!(terminal_id & TERMINAL_SINK_MASK) &&
         (p_wwe_data->op_buffer == NULL)))
    {
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    /* Specific disconnect logic */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        p_wwe_data->ip_buffer = NULL;
        p_wwe_data->metadata_ip_buffer = NULL;
    }
    else
    {
        p_wwe_data->op_buffer = NULL;
        p_wwe_data->metadata_op_buffer = NULL;
    }

    L0_DBG_MSG1("WWE: wwe_disconnect %d\0", terminal_num);

    return TRUE;
}

bool wwe_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *response_id, void **response_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    unsigned terminal_id = ((unsigned *)message_data)[0];

    if (!base_op_buffer_details(op_data, message_data, response_id,
                                response_data))
    {
        return FALSE;
    }

    ((OP_BUF_DETAILS_RSP*)*response_data)->b.buffer_size = \
        p_wwe_data->buffer_size;

    ((OP_BUF_DETAILS_RSP*)*response_data)->supports_metadata = TRUE;

    if (terminal_id & TERMINAL_SINK_MASK)
    {
        ((OP_BUF_DETAILS_RSP*)*response_data)->metadata_buffer = \
            p_wwe_data->metadata_ip_buffer;
    }
    return TRUE;
}

bool wwe_sched_info(OPERATOR_DATA *op_data, void *message_data,
                    unsigned *response_id, void **response_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    OP_SCHED_INFO_RSP* resp;

    resp = base_op_get_sched_info_ex(op_data, message_data, response_id);
    if (resp == NULL)
    {
        return base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED,
                                             response_data);
    }
    *response_data = resp;

    resp->block_size = p_wwe_data->block_size;

    return TRUE;
}

/* Data processing function */
void wwe_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    WWE_CLASS_DATA *p_class_data = get_class_data(op_data);
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    const WWE_CAP_VIRTUAL_TABLE *vt = p_class_data->vt;

    p_wwe_data->trigger = FALSE;
    p_wwe_data->ts.trigger_offset = 0;

    if ((p_wwe_data->ip_buffer != NULL) &&
        (cbuffer_calc_amount_data_in_words(p_wwe_data->ip_buffer) <
            p_wwe_data->frame_size))
    {
        return;
    }

    if ((p_wwe_data->op_buffer != NULL) &&
        (cbuffer_calc_amount_space_in_words(p_wwe_data->op_buffer) <
            p_wwe_data->frame_size))
    {
        return;
    }

    if ((p_wwe_data->cur_mode == WWE_SYSMODE_PASS_THRU))
    {
        wwe_handle_data_flow(p_wwe_data, touched);
        return;
    }

    if (p_wwe_data->init_flag)
    {
        if (vt->reset_fn != NULL)
        {
            L2_DBG_MSG("WWE: Re-initialize\0");
            if (!vt->reset_fn(op_data, touched))
            {
                return;
            }
        }
        p_wwe_data->init_flag = FALSE;
    }

    if (vt->process_fn != NULL)
    {
        L4_DBG_MSG1("WWE: processing=%d\0",p_wwe_data->frame_size);
        if (!vt->process_fn(op_data, touched))
        {
            wwe_handle_data_flow(p_wwe_data, touched);
            return;
        }
    }

    wwe_handle_data_flow(p_wwe_data, touched);

    if (p_wwe_data->trigger)
    {
        wwe_send_notification(op_data, p_wwe_data);
    }
}

void wwe_handle_data_flow(WWE_COMMON_DATA *p_wwe_data,
                          TOUCHED_TERMINALS *touched)
{

    unsigned b4idx;
    unsigned afteridx;
    metadata_tag *mtag_ip;
    metadata_tag *last_ttp_mtag_ip = NULL;
    metadata_tag *mtag_ip_list;
    TIME last_timestamp;

    /* Extract metadata tag from input */
    mtag_ip_list = buff_metadata_remove(
       p_wwe_data->metadata_ip_buffer,
       p_wwe_data->frame_size * OCTETS_PER_SAMPLE,
       &b4idx,
       &afteridx);

    /* Find the first timestamped tag */
    mtag_ip = mtag_ip_list;
    while (mtag_ip != NULL)
    {
        if (IS_TIMESTAMPED_TAG(mtag_ip) || IS_TIME_OF_ARRIVAL_TAG(mtag_ip))
        {
            last_ttp_mtag_ip = mtag_ip;
        }
        mtag_ip = mtag_ip->next;
    }

    if (last_ttp_mtag_ip)
    {
        /* calculate the last sample's timestamp based on the tag.*/
        last_timestamp = ttp_get_next_timestamp(
                last_ttp_mtag_ip->timestamp, afteridx/OCTETS_PER_SAMPLE,
                p_wwe_data->sample_rate, last_ttp_mtag_ip->timestamp);
    }
    else
    {
        /* No timestamped tag.
         * Increment the last timestamp by the consumed data.
         * */
        last_timestamp = ttp_get_next_timestamp(
                p_wwe_data->last_timestamp, p_wwe_data->frame_size,
                p_wwe_data->sample_rate, last_ttp_mtag_ip->timestamp);
    }

    /* Save the last sample's timestamp in case next run will have no tag.*/
    p_wwe_data->last_timestamp = last_timestamp;
    if (p_wwe_data->trigger)
    {
        p_wwe_data->ts.trigger_end_timestamp = last_timestamp;
    }

    if (p_wwe_data->op_buffer != NULL &&
        p_wwe_data->ip_buffer != NULL)
    {
        cbuffer_copy(p_wwe_data->op_buffer, p_wwe_data->ip_buffer,
                     p_wwe_data->frame_size);
    }
    else if (p_wwe_data->ip_buffer != NULL)
    {
        cbuffer_advance_read_ptr(p_wwe_data->ip_buffer, p_wwe_data->frame_size);
    }

    if (p_wwe_data->ip_buffer != NULL)
    {
        touched->sinks |= 1;
    }
    if (p_wwe_data->op_buffer != NULL)
    {
        touched->sources |= 1;
    }

    if (p_wwe_data->metadata_op_buffer != NULL &&
        p_wwe_data->metadata_ip_buffer != NULL)
    {
        /* Append tags to the output metadata. */
        buff_metadata_append(p_wwe_data->metadata_op_buffer, mtag_ip_list,
                             b4idx, afteridx);
    }
    else
    {
        /* Free all the incoming tags */
        buff_metadata_tag_list_delete(mtag_ip_list);
    }
}

void wwe_send_notification(OPERATOR_DATA *op_data,
                           WWE_COMMON_DATA *p_wwe_data)
{
    unsigned msg_size = \
        OPMSG_UNSOLICITED_WWE_TRIGGER_CH1_TRIGGER_DETAILS_WORD_OFFSET;
    OPMSG_REPLY_ID message_id = OPMSG_REPLY_ID_VA_NEGATIVE_TRIGGER;

    unsigned *trigger_message = xzpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        L2_DBG_MSG("WWE TRIG: NO MEMORY FOR TRIGGER MESSAGE");
        fault_diatribe(FAULT_AUDIO_INSUFFICIENT_MEMORY, msg_size);
        return;
    }

    OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_WWE_TRIGGER,
                             NUMBER_OF_CHANNELS, 1);

    unsigned start_ts_upper = 0;
    unsigned start_ts_lower = 0;
    unsigned end_ts_upper = 0;
    unsigned end_ts_lower = 0;

    unsigned *submessage = OPMSG_CREATION_FIELD_POINTER_GET_FROM_OFFSET(
        trigger_message, OPMSG_UNSOLICITED_WWE_TRIGGER, CH0_TRIGGER_DETAILS, 0);

    if (p_wwe_data->trigger)
    {
        OPMSG_CREATION_FIELD_SET(submessage,
                                 OPMSG_WWE_TRIGGER_CHANNEL_INFO,
                                 TRIGGER_CONFIDENCE,
                                 p_wwe_data->result_confidence);

        message_id = OPMSG_REPLY_ID_VA_TRIGGER;
        start_ts_upper = time_sub(p_wwe_data->ts.trigger_start_timestamp,
                                  p_wwe_data->ts.trigger_offset) >> 16;
        start_ts_lower = time_sub(p_wwe_data->ts.trigger_start_timestamp,
                                  p_wwe_data->ts.trigger_offset) & 0xffff;
        end_ts_upper = time_sub(p_wwe_data->ts.trigger_end_timestamp,
                                p_wwe_data->ts.trigger_offset) >> 16;
        end_ts_lower = time_sub(p_wwe_data->ts.trigger_end_timestamp,
                                p_wwe_data->ts.trigger_offset) & 0xffff;
    }
    else
    {
        OPMSG_CREATION_FIELD_SET(submessage,
                                 OPMSG_WWE_TRIGGER_CHANNEL_INFO,
                                 TRIGGER_CONFIDENCE,
                                 0);
    }

    OPMSG_CREATION_FIELD_SET(submessage, OPMSG_WWE_TRIGGER_CHANNEL_INFO,
                             START_TIMESTAMP_UPPER, start_ts_upper);
    OPMSG_CREATION_FIELD_SET(submessage, OPMSG_WWE_TRIGGER_CHANNEL_INFO,
                             START_TIMESTAMP_LOWER, start_ts_lower);
    OPMSG_CREATION_FIELD_SET(submessage, OPMSG_WWE_TRIGGER_CHANNEL_INFO,
                             END_TIMESTAMP_UPPER, end_ts_upper);
    OPMSG_CREATION_FIELD_SET(submessage, OPMSG_WWE_TRIGGER_CHANNEL_INFO,
                             END_TIMESTAMP_LOWER, end_ts_lower);

    common_send_unsolicited_message(op_data, (unsigned)message_id, msg_size,
                                    trigger_message);
    pdelete(trigger_message);
}

/* ********************* Operator Message Handle functions ****************** */

bool wwe_opmsg_obpm_set_control(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    unsigned            i, num_controls, cntrl_value;
    CPS_CONTROL_SOURCE  cntrl_src;
    OPMSG_RESULT_STATES result = OPMSG_RESULT_STATES_NORMAL_STATE;

    if(!cps_control_setup(message_data, resp_length, resp_data, &num_controls))
    {
       return FALSE;
    }

    for(i=0; i<num_controls; i++)
    {
        unsigned cntrl_id = cps_control_get(message_data, i, &cntrl_value,
                                            &cntrl_src);

        if (cntrl_id != OPMSG_CONTROL_MODE_ID)
        {
            result = OPMSG_RESULT_STATES_UNSUPPORTED_CONTROL;
            break;
        }
        /* Only interested in lower 8-bits of value */
        cntrl_value &= 0xFF;
        if (cntrl_value >= WWE_SYSMODE_MAX_MODES)
        {
            result = OPMSG_RESULT_STATES_INVALID_CONTROL_VALUE;
            break;
        }
        /* Control is Mode */
        if (cntrl_src == CPS_SOURCE_HOST)
        {
            p_wwe_data->host_mode = cntrl_value;
        }
        else
        {
            if (cntrl_src == CPS_SOURCE_OBPM_DISABLE)
            {
                p_wwe_data->ovr_control = 0;
            }
            else
            {
                p_wwe_data->ovr_control = WWE_CONTROL_MODE_OVERRIDE;
            }
            p_wwe_data->obpm_mode = cntrl_value;
        }
    }

    if(p_wwe_data->ovr_control & WWE_CONTROL_MODE_OVERRIDE)
    {
        p_wwe_data->cur_mode = p_wwe_data->obpm_mode;
    }
    else
    {
        p_wwe_data->cur_mode = p_wwe_data->host_mode;
    }

    cps_response_set_result(resp_data, result);

    /* Set the Reinit flag after setting the parameters */
    if (result == OPMSG_RESULT_STATES_NORMAL_STATE)
    {
        p_wwe_data->init_flag = TRUE;
    }
    return TRUE;
}

bool wwe_opmsg_obpm_get_params(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *resp_length,
                               OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    return cpsGetParameterMsgHandler(&p_wwe_data->parms_def, message_data,
                                     resp_length, resp_data);
}

bool wwe_opmsg_obpm_get_defaults(OPERATOR_DATA *op_data, void *message_data,
                                 unsigned *resp_length,
                                 OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    return cpsGetDefaultsMsgHandler(&p_wwe_data->parms_def, message_data,
                                    resp_length, resp_data);
}

bool wwe_opmsg_obpm_set_params(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *resp_length,
                               OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    bool retval = cpsSetParameterMsgHandler(&p_wwe_data->parms_def,
                                            message_data, resp_length,
                                            resp_data);

    p_wwe_data->init_flag = TRUE;
    return retval;
}

bool wwe_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    PS_KEY_TYPE key;

    bool retval = cpsSetUcidMsgHandler(&p_wwe_data->parms_def, message_data,
                                       resp_length, resp_data);

    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(base_op_get_int_op_id(op_data),
                                         p_wwe_data->parms_def.ucid,
                                         OPMSG_P_STORE_PARAMETER_SUB_ID);
    ps_entry_read((void*)op_data, key, PERSIST_ANY, wwe_ups_params);

    return retval;
}

bool wwe_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *resp_length,
                         OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    return cpsGetUcidMsgHandler(&p_wwe_data->parms_def, base_op_get_int_op_id(op_data),
                                message_data, resp_length, resp_data);
}

bool wwe_reset_status(OPERATOR_DATA *op_data, void *message_data,
                      unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);


    p_wwe_data->cur_mode = WWE_SYSMODE_FULL_PROC;
    /* TODO: host_mode as well? */
    p_wwe_data->keyword_detected = 0;
    p_wwe_data->init_flag = TRUE;

    L0_DBG_MSG("WWE: wwe_reset_status\0");

    return TRUE;
}

/* TODO: determine whether to deprecate mode_change */
bool wwe_mode_change(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    unsigned new_mode = OPMSG_FIELD_GET(message_data,
                                        OPMSG_WWE_MODE_CHANGE, WORKING_MODE);

    L0_DBG_MSG2("WWE: cur=%d new=%d\0", p_wwe_data->cur_mode, new_mode);

    if ((new_mode > 0) && (new_mode < WWE_SYSMODE_MAX_MODES))
    {
        p_wwe_data->cur_mode = new_mode;
        p_wwe_data->host_mode = new_mode;
        p_wwe_data->init_flag = TRUE;
        return TRUE;
    }

    L0_DBG_MSG("WWE: Attempted to set an unknown mode configuration  \n");
    return FALSE;
}

/* ********************* Model load/unload functions ************************ */

#ifndef RUNNING_ON_KALSIM
static bool wwe_load_fn(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data,
                        DATA_FILE *trigger_file)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    if (trigger_file->type == ACCMD_TYPE_OF_FILE_EDF)
    {
        /* External buffer */
        p_wwe_data->n_model_size = \
            ext_buffer_amount_data(trigger_file->u.ext_file_data);
        p_wwe_data->p_model = \
            ppmalloc(p_wwe_data->n_model_size, MALLOC_PREFERENCE_NONE);
        memcpy(p_wwe_data->p_model, trigger_file->u.file_data->base_addr,
               p_wwe_data->n_model_size);
        L0_DBG_MSG1("WWE: model data copied from external buffer size=%d\0",
                    p_wwe_data->n_model_size);
    }
    else
    {
        /* Internal buffer */
        p_wwe_data->p_model = (uint8 *)trigger_file->u.file_data->base_addr;
        /* Model size in bytes (*4) */
        p_wwe_data->n_model_size = \
            cbuffer_calc_amount_data_in_words(trigger_file->u.file_data) * 4;
        L0_DBG_MSG1("WWE: model data in internal buffer size=%d\0",
                    p_wwe_data->n_model_size);
    }
    return TRUE;
}
#endif /* #ifndef RUNNING_ON_KALSIM */

bool wwe_trigger_phrase_load(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *resp_length,
                             OP_OPMSG_RSP_PAYLOAD **resp_data)
{

#ifndef RUNNING_ON_KALSIM
    WWE_CLASS_DATA *p_class_data = get_class_data(op_data);
    WWE_COMMON_DATA *p_wwe_data = &p_class_data->wwe;
    const WWE_CAP_VIRTUAL_TABLE *vt = p_class_data->vt;

    /* can't load model while running */
    if (opmgr_op_is_running(op_data))
    {
        L0_DBG_MSG("WWE: Cant load model while operator is running\0");
        return FALSE;
    }

    unsigned num_files = OPMSG_FIELD_GET(message_data,
                                         OPMSG_WWE_TRIGGER_PHRASE_LOAD,
                                         NUM_FILES);
    if (num_files > 1)
    {
        L0_DBG_MSG("WWE: Does not currently support the loading of more than "
                   "one model at a time\0");
        return FALSE;
    }

    int16 file_id = OPMSG_FIELD_GET(message_data,
                                     OPMSG_WWE_TRIGGER_PHRASE_LOAD,
                                     FILE_ID_LIST);
    DATA_FILE *trigger_file = file_mgr_get_file(file_id);

    /* Check for valid FILE ID*/
    if (trigger_file == NULL)
    {
        L0_DBG_MSG("WWE: Invalid file ID\0");
        return FALSE;
    }

    if (vt->load_fn != NULL)
    {
        if (!vt->load_fn(op_data, message_data, resp_length, resp_data,
                         trigger_file))
        {
            return FALSE;
        }
    }

    p_wwe_data->file_id = file_id;

    if (vt->check_load_fn != NULL)
    {
        if (!vt->check_load_fn(op_data, message_data, resp_length, resp_data,
                               trigger_file))
        {
            return FALSE;
        }
    }

    L0_DBG_MSG1("WWE: trigger_phrase_load size=%d\0", p_wwe_data->n_model_size);
#endif /* #ifndef RUNNING_ON_KALSIM */

    return TRUE;
}

#ifndef RUNNING_ON_KALSIM
static bool wwe_unload_fn(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *resp_length,
                          OP_OPMSG_RSP_PAYLOAD **resp_data, uint16 file_id)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    DATA_FILE *trigger_file = file_mgr_get_file(file_id);
    if (trigger_file->type == ACCMD_TYPE_OF_FILE_EDF)
        pfree(p_wwe_data->p_model);
    return TRUE;
}
#endif /* #ifndef RUNNING_ON_KALSIM */

bool wwe_trigger_phrase_unload(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *resp_length,
                               OP_OPMSG_RSP_PAYLOAD **resp_data)
{
#ifndef RUNNING_ON_KALSIM
    WWE_CLASS_DATA *p_class_data = get_class_data(op_data);
    WWE_COMMON_DATA *p_wwe_data = &p_class_data->wwe;
    const WWE_CAP_VIRTUAL_TABLE *vt = p_class_data->vt;

    int16 file_id;
    unsigned num_files;

    /* can't unload model while running */
    if (opmgr_op_is_running(op_data))
    {
        L0_DBG_MSG("WWE: Cant unload model while operator is running\0");
        return FALSE;
    }

    num_files = OPMSG_FIELD_GET(message_data,
                                OPMSG_QVA_TRIGGER_PHRASE_UNLOAD, NUM_FILES);
    if (num_files > 1)
    {
        L0_DBG_MSG("WWE: Does not currently support the unloading of more "
                   "than one model at a time\0");
        return FALSE;
    }

    file_id = OPMSG_FIELD_GET(message_data,
                              OPMSG_QVA_TRIGGER_PHRASE_UNLOAD, FILE_ID_LIST);
    if (file_id != p_wwe_data->file_id)
    {
        L0_DBG_MSG1("WWE: This file ID is not in use. FileID is  %d\0", file_id);
        return FALSE;
    }

    if (vt->unload_fn != NULL)
    {
        if (!vt->unload_fn(op_data, message_data, resp_length, resp_data,
                           file_id))
        {
            return FALSE;
        }
    }

    file_mgr_release_file(p_wwe_data->file_id);
    p_wwe_data->file_id = 0;
    p_wwe_data->p_model = NULL;
    p_wwe_data->n_model_size = 0;
    p_wwe_data->init_flag = TRUE;

    wwe_initialize_stats(p_wwe_data);

    L0_DBG_MSG("WWE: trigger_phrase_unload\0");
#endif /* #ifndef RUNNING_ON_KALSIM */

    return TRUE;
}

bool wwe_set_sample_rate(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *resp_length,
                         OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    p_wwe_data->sample_rate = 25 * (OPMSG_FIELD_GET(
        message_data, OPMSG_COMMON_MSG_SET_SAMPLE_RATE, SAMPLE_RATE));
    L2_DBG_MSG1("WWE: Sample rate set to %d\0", p_wwe_data->sample_rate);

    return TRUE;
}

bool wwe_reinit_algorithm(OPERATOR_DATA *op_data,
                                      void *message_data,
                                      unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    p_wwe_data->init_flag = TRUE;
    L3_DBG_MSG("WWE: Algorithm has been reset  \n");
    return TRUE;
}
