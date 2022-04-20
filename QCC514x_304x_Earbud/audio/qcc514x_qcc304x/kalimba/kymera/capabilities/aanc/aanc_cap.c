/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \ingroup capabilities
 * \file  aanc_cap.c
 * \ingroup AANC
 *
 * Adaptive ANC (AANC) operator capability.
 *
 */

/****************************************************************************
Include Files
*/

#include "aanc_cap.h"

/*****************************************************************************
Private Constant Definitions
*/
#ifdef CAPABILITY_DOWNLOAD_BUILD
#define AANC_MONO_16K_CAP_ID   CAP_ID_DOWNLOAD_AANC_MONO_16K
#else
#define AANC_MONO_16K_CAP_ID   CAP_ID_AANC_MONO_16K
#endif

/* Message handlers */
const handler_lookup_struct aanc_handler_table =
{
    aanc_create,             /* OPCMD_CREATE */
    aanc_destroy,            /* OPCMD_DESTROY */
    aanc_start,              /* OPCMD_START */
    base_op_stop,            /* OPCMD_STOP */
    aanc_reset,              /* OPCMD_RESET */
    aanc_connect,            /* OPCMD_CONNECT */
    aanc_disconnect,         /* OPCMD_DISCONNECT */
    aanc_buffer_details,     /* OPCMD_BUFFER_DETAILS */
    base_op_get_data_format, /* OPCMD_DATA_FORMAT */
    aanc_get_sched_info      /* OPCMD_GET_SCHED_INFO */
};

/* Null-terminated operator message handler table */
const opmsg_handler_lookup_table_entry aanc_opmsg_handler_table[] =
    {{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION, base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_CONTROL,             aanc_opmsg_set_control},
    {OPMSG_COMMON_ID_GET_PARAMS,              aanc_opmsg_get_params},
    {OPMSG_COMMON_ID_GET_DEFAULTS,            aanc_opmsg_get_defaults},
    {OPMSG_COMMON_ID_SET_PARAMS,              aanc_opmsg_set_params},
    {OPMSG_COMMON_ID_GET_STATUS,              aanc_opmsg_get_status},
    {OPMSG_COMMON_ID_SET_UCID,                aanc_opmsg_set_ucid},
    {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,       aanc_opmsg_get_ps_id},

    {OPMSG_AANC_ID_SET_AANC_STATIC_GAIN,      aanc_opmsg_set_static_gain},
    {OPMSG_AANC_ID_SET_AANC_PLANT_COEFFS,     aanc_opmsg_set_plant_model},
    {OPMSG_AANC_ID_SET_AANC_CONTROL_COEFFS,   aanc_opmsg_set_control_model},
    {0, NULL}};

/* Provide a lookup table for gain overrides in SET_CONTROL.
 * Note: offsets into this table align with the SET_CONTROL IDs, e.g.
 * ff_fine_gain override is ID 4.
 */
const AANC_GAIN_OVERRIDE gain_override_table[] = {
    {0, FALSE},
    {0, FALSE},
    {0, FALSE},
    {0, FALSE},
    {offsetof(AANC_OP_DATA, ff_gain), FALSE},
    {0, FALSE},
    {0, FALSE},
    {offsetof(AANC_OP_DATA, ff_gain), TRUE},
    {offsetof(AANC_OP_DATA, fb_gain), FALSE},
    {offsetof(AANC_OP_DATA, fb_gain), TRUE},
    {offsetof(AANC_OP_DATA, ec_gain), FALSE},
    {offsetof(AANC_OP_DATA, ec_gain), TRUE}};

const CAPABILITY_DATA aanc_mono_16k_cap_data =
    {
        /* Capability ID */
        AANC_MONO_16K_CAP_ID,
        /* Version information - hi and lo */
        AANC_AANC_MONO_16K_VERSION_MAJOR, AANC_CAP_VERSION_MINOR,
        /* Max number of sinks/inputs and sources/outputs */
        8, 4,
        /* Pointer to message handler function table */
        &aanc_handler_table,
        /* Pointer to operator message handler function table */
        aanc_opmsg_handler_table,
        /* Pointer to data processing function */
        aanc_process_data,
        /* Reserved */
        0,
        /* Size of capability-specific per-instance data */
        sizeof(AANC_OP_DATA)
    };

MAP_INSTANCE_DATA(AANC_MONO_16K_CAP_ID, AANC_OP_DATA)

/****************************************************************************
Inline Functions
*/

/**
 * \brief  Get AANC instance data.
 *
 * \param  op_data  Pointer to the operator data.
 *
 * \return  Pointer to extra operator data AANC_OP_DATA.
 */
static inline AANC_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (AANC_OP_DATA *) base_op_get_instance_data(op_data);
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
static inline int aanc_calc_samples_to_process(AANC_OP_DATA *p_ext_data)
{
    int i, amt, min_data_space;

    /* Return if int and ext mic input terminals are not connected */
    if ((p_ext_data->touched_sinks & AANC_MIN_VALID_SINKS) != AANC_MIN_VALID_SINKS)
    {
        return INT_MAX;
    }

    min_data_space = AANC_DEFAULT_FRAME_SIZE;
    /* Calculate the amount of data available */
    for (i = AANC_PLAYBACK_TERMINAL_ID; i <= AANC_MIC_EXT_TERMINAL_ID; i++)
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
        for (i = AANC_PLAYBACK_TERMINAL_ID; i <= AANC_MIC_EXT_TERMINAL_ID; i++)
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
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Because simulator tests need to change the gain and also analyze the
 * behavior of the capability an unsolicited message is sent only in
 * simulation.
 */
static bool aanc_update_gain(OPERATOR_DATA *op_data, AANC_OP_DATA *p_ext_data)
{
    unsigned msg_size = OPMSG_UNSOLICITED_AANC_INFO_WORD_SIZE;
    unsigned *trigger_message = NULL;
    OPMSG_REPLY_ID message_id;

    trigger_message = xzpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        return FALSE;
    }

    OPMSG_CREATION_FIELD_SET32(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                FLAGS, p_ext_data->flags);
    OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                ANC_INSTANCE, p_ext_data->anc_channel);
    OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                FILTER_CONFIG, p_ext_data->filter_config);

    p_ext_data->ff_gain_prev.coarse = p_ext_data->ff_gain.coarse;
    OPMSG_CREATION_FIELD_SET(trigger_message,
                                OPMSG_UNSOLICITED_AANC_INFO,
                                FF_COARSE_GAIN,
                                p_ext_data->ff_gain.coarse);
    p_ext_data->ff_gain_prev.fine = p_ext_data->ff_gain.fine;
    OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                FF_FINE_GAIN, p_ext_data->ff_gain.fine);

    /* Only update EC and FB gains if in hybrid mode */
    if (p_ext_data->anc_fb_path > 0)
    {
        p_ext_data->fb_gain_prev.coarse = p_ext_data->fb_gain.coarse;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    FB_COARSE_GAIN, p_ext_data->fb_gain.coarse);
        p_ext_data->fb_gain_prev.fine = p_ext_data->fb_gain.fine;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    FB_FINE_GAIN, p_ext_data->fb_gain.fine);

        p_ext_data->ec_gain_prev.coarse = p_ext_data->ec_gain.coarse;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    EC_COARSE_GAIN, p_ext_data->ec_gain.coarse);
        p_ext_data->ec_gain_prev.fine = p_ext_data->ec_gain.fine;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    EC_FINE_GAIN, p_ext_data->ec_gain.fine);
    }

    message_id = OPMSG_REPLY_ID_AANC_TRIGGER;
    common_send_unsolicited_message(op_data, (unsigned)message_id, msg_size,
                                    trigger_message);

    pdelete(trigger_message);

    return TRUE;
}
/**
 * \brief  Update the gain in the ANC HW.
 *
 * \param  op_data  Address of the operator data.
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Any changes in the gain value since the previous value was set is written
 * to the HW.
 *
 */
#else
static bool aanc_update_gain(OPERATOR_DATA *op_data, AANC_OP_DATA *p_ext_data)
{
    /* Only update EC and FB gains if in hybrid mode */
    if (p_ext_data->anc_fb_path > 0)
    {
        /* Update EC gain */
        if (p_ext_data->ec_gain.fine != p_ext_data->ec_gain_prev.fine)
        {
            if (p_ext_data->filter_config == AANC_FILTER_CONFIG_PARALLEL)
            {
                stream_anc_set_anc_fine_gain(AANC_ANC_INSTANCE_ANC0_ID,
                                            AANC_ANC_PATH_FB_ID,
                                            p_ext_data->ec_gain.fine);
                stream_anc_set_anc_fine_gain(AANC_ANC_INSTANCE_ANC1_ID,
                                            AANC_ANC_PATH_FB_ID,
                                            p_ext_data->ec_gain.fine);
            }
            else
            {
                stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                            AANC_ANC_PATH_FB_ID,
                                            p_ext_data->ec_gain.fine);
            }
            p_ext_data->ec_gain_prev.fine = p_ext_data->ec_gain.fine;
        }
        if (p_ext_data->ec_gain.coarse != p_ext_data->ec_gain_prev.coarse)
        {
            if (p_ext_data->filter_config == AANC_FILTER_CONFIG_PARALLEL)
            {
                stream_anc_set_anc_coarse_gain(AANC_ANC_INSTANCE_ANC0_ID,
                                            AANC_ANC_PATH_FB_ID,
                                            p_ext_data->ec_gain.coarse);
                stream_anc_set_anc_coarse_gain(AANC_ANC_INSTANCE_ANC1_ID,
                                            AANC_ANC_PATH_FB_ID,
                                            p_ext_data->ec_gain.coarse);
            }
            else
            {
                stream_anc_set_anc_coarse_gain(p_ext_data->anc_channel,
                                            AANC_ANC_PATH_FB_ID,
                                            p_ext_data->ec_gain.coarse);
            }
            p_ext_data->ec_gain_prev.coarse = p_ext_data->ec_gain.coarse;
        }

        /* Update FB gain */
        if (p_ext_data->fb_gain.fine != p_ext_data->fb_gain_prev.fine)
        {
            if (p_ext_data->filter_config == AANC_FILTER_CONFIG_PARALLEL)
            {
                stream_anc_set_anc_fine_gain(AANC_ANC_INSTANCE_ANC0_ID,
                                            p_ext_data->anc_fb_path,
                                            p_ext_data->fb_gain.fine);
                stream_anc_set_anc_fine_gain(AANC_ANC_INSTANCE_ANC1_ID,
                                            p_ext_data->anc_fb_path,
                                            p_ext_data->fb_gain.fine);
            }
            else
            {
                stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                            p_ext_data->anc_fb_path,
                                            p_ext_data->fb_gain.fine);
            }
            p_ext_data->fb_gain_prev.fine = p_ext_data->fb_gain.fine;
        }
        if (p_ext_data->fb_gain.coarse != p_ext_data->fb_gain_prev.coarse)
        {
            if (p_ext_data->filter_config == AANC_FILTER_CONFIG_PARALLEL)
            {
                stream_anc_set_anc_coarse_gain(AANC_ANC_INSTANCE_ANC0_ID,
                                            p_ext_data->anc_fb_path,
                                            p_ext_data->fb_gain.coarse);
                stream_anc_set_anc_coarse_gain(AANC_ANC_INSTANCE_ANC1_ID,
                                            p_ext_data->anc_fb_path,
                                            p_ext_data->fb_gain.coarse);
            }
            else
            {
                stream_anc_set_anc_coarse_gain(p_ext_data->anc_channel,
                                            p_ext_data->anc_fb_path,
                                            p_ext_data->fb_gain.coarse);
            }
            p_ext_data->fb_gain_prev.coarse = p_ext_data->fb_gain.coarse;
        }
    }

    /* Update FF gain */
    if (p_ext_data->ff_gain.fine != p_ext_data->ff_gain_prev.fine)
    {
            if (p_ext_data->filter_config == AANC_FILTER_CONFIG_PARALLEL)
            {
                stream_anc_set_anc_fine_gain(AANC_ANC_INSTANCE_ANC0_ID,
                                            p_ext_data->anc_ff_path,
                                            p_ext_data->ff_gain.fine);
                stream_anc_set_anc_fine_gain(AANC_ANC_INSTANCE_ANC1_ID,
                                            p_ext_data->anc_ff_path,
                                            p_ext_data->ff_gain.fine);
            }
            else
            {
                stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                            p_ext_data->anc_ff_path,
                                            p_ext_data->ff_gain.fine);
            }
        p_ext_data->ff_gain_prev.fine = p_ext_data->ff_gain.fine;
    }
    if (p_ext_data->ff_gain.coarse != p_ext_data->ff_gain_prev.coarse)
    {
            if (p_ext_data->filter_config == AANC_FILTER_CONFIG_PARALLEL)
            {
                stream_anc_set_anc_coarse_gain(AANC_ANC_INSTANCE_ANC0_ID,
                                            p_ext_data->anc_ff_path,
                                            p_ext_data->ff_gain.coarse);
                stream_anc_set_anc_coarse_gain(AANC_ANC_INSTANCE_ANC1_ID,
                                            p_ext_data->anc_ff_path,
                                            p_ext_data->ff_gain.coarse);
            }
            else
            {
                stream_anc_set_anc_coarse_gain(p_ext_data->anc_channel,
                                            p_ext_data->anc_ff_path,
                                            p_ext_data->ff_gain.coarse);
            }
        p_ext_data->ff_gain_prev.coarse = p_ext_data->ff_gain.coarse;
    }

    return TRUE;
}
#endif /* RUNNING_ON_KALSIM */

/**
 * \brief  Update touched terminals for the capability
 *
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Because this is solely dependent on the terminal connections it can be
 * calculated in connect/disconnect rather than in every process_data loop.
 */
static bool update_touched_sink_sources(AANC_OP_DATA *p_ext_data)
{
    int i;
    unsigned touched_sinks = 0;
    unsigned touched_sources = 0;

    /* Update touched sinks */
    for (i = 0; i < AANC_MAX_SINKS; i++)
    {
        if (p_ext_data->inputs[i] != NULL)
        {
            touched_sinks |= (uint16)(1 << i);
        }
    }

    /* Update touched sources */
    for (i = 0; i < AANC_MAX_SOURCES; i++)
    {
        if (p_ext_data->outputs[i] != NULL)
        {
            touched_sources |= (uint16)(1 << i);
        }
    }

    p_ext_data->touched_sinks = (uint16)touched_sinks;
    p_ext_data->touched_sources = (uint16)touched_sources;

    /* Generate a reinitialization because terminals have changed */
    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

/**
 * \brief  Override the gain value from a SET_CONTROL message.
 *
 * \param  p_ext_data  Address of the AANC extra_op_data.
 * \param  ctrl_value  Value to override with
 * \param  coarse_value  Whether the gain control is coarse or not
 * \param  gain_offset  Offset to the specified gain bank within p_ext_data
 *
 * \return  boolean indicating success or failure.
 */
static bool override_gain(AANC_OP_DATA *p_ext_data, uint16 ctrl_value,
                          bool coarse_value, uint16 gain_offset)
{
    void *p_target;

    if (!((p_ext_data->cur_mode == AANC_SYSMODE_FREEZE) ||
          (p_ext_data->cur_mode == AANC_SYSMODE_STATIC)))
    {
        return FALSE;
    }

    /* Mask for bottom 16 bits */
    ctrl_value &= 0xFFFF;

    if (coarse_value == TRUE)
    {
        /* B-308001: Backwards compatibility with uint4 from QACT.
         * QACT will send 15 = -1 .. 8 = -8 but these need to be in full
         * (u)int16.
         */
        if (ctrl_value > 7 && ctrl_value < 16)
        {
            ctrl_value = (uint16)((65536 - 16) + ctrl_value);
        }
    }

    /* Set the gain: fine gain needs an additional offset */
    if (coarse_value == FALSE)
    {
        gain_offset += offsetof(AANC_GAIN, fine);
    }
    p_target = (void *)((uintptr_t)p_ext_data + (unsigned)gain_offset);
    *((uint16 *)p_target) = ctrl_value;
    L4_DBG_MSG1("AANC gain override: %hu", *(uint16 *)p_target);

    return TRUE;
}

static inline void aanc_clear_event(AANC_EVENT *p_event)
{
       p_event->frame_counter =p_event->set_frames;
       p_event->running = AANC_EVENT_CLEAR;
}

/**
 * \brief  Sent an event trigger message.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  detect Boolean indicating whether it is a positive (TRUE) or negative
 *                (FALSE) trigger.
 * \param  id  ID for the event message
 * \param  payload Payload for the event message
 *
 * \return  bool indicating success
 */
static bool aanc_send_event_trigger(OPERATOR_DATA *op_data, bool detect,
                                    uint16 id, uint16 payload)
{
    unsigned msg_size;
    unsigned *trigger_message = NULL;
    OPMSG_REPLY_ID message_id = OPMSG_REPLY_ID_AANC_EVENT_TRIGGER;
    if (!detect)
    {
        message_id = OPMSG_REPLY_ID_AANC_EVENT_NEGATIVE_TRIGGER;
    }

    msg_size = OPMSG_UNSOLICITED_AANC_EVENT_TRIGGER_WORD_SIZE;
    trigger_message = xpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        L2_DBG_MSG("Failed to send AANC event message");
        return FALSE;
    }

    OPMSG_CREATION_FIELD_SET(trigger_message,
                             OPMSG_UNSOLICITED_AANC_EVENT_TRIGGER,
                             ID,
                             id);
    OPMSG_CREATION_FIELD_SET(trigger_message,
                             OPMSG_UNSOLICITED_AANC_EVENT_TRIGGER,
                             PAYLOAD,
                             payload);

    L4_DBG_MSG2("AANC Event Sent: [%u, %u]", trigger_message[0],
                trigger_message[1]);
    common_send_unsolicited_message(op_data, (unsigned)message_id, msg_size,
                                    trigger_message);

    pdelete(trigger_message);

    return TRUE;
}

/**
 * \brief  Process an event clear condition.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  p_event  Address of the event to process
 * \param  id  ID for the negative event message
 * \param  payload Payload for the negative event message
 *
 * \return  void.
 */
static void aanc_process_event_clear_condition(OPERATOR_DATA *op_data,
                                               AANC_EVENT *p_event,
                                               uint16 id, uint16 payload)
{
    switch (p_event->running)
    {
            case AANC_EVENT_CLEAR:
                /* Clear needs to fall through so that initialization behavior
                   is correct.
                */
            case AANC_EVENT_DETECTED:
                /* Have detected but not sent message so clear */
                aanc_clear_event(p_event);
                break;
            case AANC_EVENT_SENT:
                aanc_send_event_trigger(op_data, FALSE, id, payload);
                aanc_clear_event(p_event);
                break;
    }
}

/**
 * \brief  Initialize events for messaging.
 *
 * \param  op_data  Address of the operator data
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  void.
 */
static void aanc_initialize_events(OPERATOR_DATA *op_data, AANC_OP_DATA *p_ext_data)
{
    AANC_PARAMETERS *p_params = &p_ext_data->aanc_cap_params;
    unsigned set_frames;

    set_frames = (p_params->OFFSET_EVENT_GAIN_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Gain Event Initialized at %u frames", set_frames);
    p_ext_data->gain_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->gain_event,
                                       AANC_EVENT_ID_GAIN, 0);

    set_frames = (p_params->OFFSET_EVENT_ED_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC ED Event Initialized at %u frames", set_frames);
    p_ext_data->ed_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->ed_event,
                                       AANC_EVENT_ID_ED, 0);

    set_frames = (p_params->OFFSET_EVENT_QUIET_DETECT * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Quiet Mode Detect Initialized at %u frames", set_frames);
    p_ext_data->quiet_event_detect.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->quiet_event_detect,
                                       AANC_EVENT_ID_QUIET, 0);

    set_frames = (p_params->OFFSET_EVENT_QUIET_CLEAR * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Quiet Mode Cleared Initialized at %u frames", set_frames);
    p_ext_data->quiet_event_clear.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->quiet_event_clear,
                                       AANC_EVENT_ID_QUIET, 0);

    set_frames = (p_params->OFFSET_EVENT_CLIP_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Clip Event Initialized at %u frames", set_frames);
    p_ext_data->clip_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->clip_event,
                                       AANC_EVENT_ID_CLIP, 0);

    set_frames = (p_params->OFFSET_EVENT_SAT_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Saturation Event Initialized at %u frames", set_frames);
    p_ext_data->sat_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->sat_event,
                                       AANC_EVENT_ID_SAT, 0);

    set_frames = (p_params->OFFSET_EVENT_SELF_TALK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Self-Talk Event Initialized at %u frames", set_frames);
    p_ext_data->self_talk_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->self_talk_event,
                                       AANC_EVENT_ID_SELF_TALK, 0);
    set_frames = (p_params->OFFSET_EVENT_SPL * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC SPL Event Initialized at %u frames", set_frames);
    p_ext_data->spl_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->spl_event,
                                       AANC_EVENT_ID_SPL, 0);
    set_frames = 1;
    L4_DBG_MSG1("AANC Gentle Mute Event Initialized at %u frames", set_frames);
    p_ext_data->gentle_mute_event.set_frames = set_frames;
    aanc_clear_event(&p_ext_data->gentle_mute_event);
}

/**
 * \brief  Process an event detection condition.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  p_event  Address of the event to process
 * \param  id  ID for the event message
 * \param  payload Payload for the event message
 *
 * \return  void.
 */
static void aanc_process_event_detect_condition(OPERATOR_DATA *op_data,
                                                AANC_EVENT *p_event,
                                                uint16 id, uint16 payload)
{
    switch (p_event->running)
    {
        case AANC_EVENT_CLEAR:
            p_event->frame_counter -= 1;
            p_event->running = AANC_EVENT_DETECTED;
            break;
        case AANC_EVENT_DETECTED:
            if (p_event->frame_counter > 0)
            {
                p_event->frame_counter -= 1;
            }
            else
            {
                aanc_send_event_trigger(op_data, TRUE, id, payload);
                p_event->running = AANC_EVENT_SENT;
            }
            break;
        case AANC_EVENT_SENT:
            break;
    }
}

/**
 * \brief  Calculate events for messaging.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 */
static bool aanc_process_events(OPERATOR_DATA *op_data,
                                AANC_OP_DATA *p_ext_data)
{
    bool cur_ed, prev_ed;
    bool cur_qm, prev_qm;
    bool cur_clip, prev_clip;
    bool cur_sat, prev_sat;
    int cur_ext, cur_int, delta_ext;

    /* Adaptive gain event: reset if ED detected */
    if (p_ext_data->flags & AANC_ED_FLAG_MASK)
    {
        /* If we had previously sent a message then send the negative trigger */
        if (p_ext_data->gain_event.running == AANC_EVENT_SENT)
        {
            aanc_send_event_trigger(op_data, FALSE, AANC_EVENT_ID_GAIN, 0);
        }
        aanc_clear_event(&p_ext_data->gain_event);
    }
    /* Condition holds */
    else if (p_ext_data->ff_gain.fine == p_ext_data->ff_gain_prev.fine)
    {
        aanc_process_event_detect_condition(op_data, &p_ext_data->gain_event,
                                            AANC_EVENT_ID_GAIN,
                                            p_ext_data->ff_gain.fine);
    }
    /* Condition cleared */
    else
    {
        aanc_process_event_clear_condition(op_data, &p_ext_data->gain_event,
                                           AANC_EVENT_ID_GAIN,
                                           p_ext_data->ff_gain.fine);
    }

    /* ED event */
    cur_ed = p_ext_data->flags & AANC_ED_FLAG_MASK;
    prev_ed = p_ext_data->prev_flags & AANC_ED_FLAG_MASK;
    if (cur_ed)
    {
        /* Non-zero flags and no change starts/continues event */
        if (cur_ed == prev_ed)
        {
            aanc_process_event_detect_condition(op_data, &p_ext_data->ed_event,
                                                AANC_EVENT_ID_ED,
                                                (uint16)cur_ed);
        }
    }
    else
    {
        /* Flags reset causes event to be reset */
        if (cur_ed != prev_ed)
        {
            aanc_process_event_clear_condition(op_data, &p_ext_data->ed_event,
                                               AANC_EVENT_ID_ED,
                                               (uint16)cur_ed);
        }
    }

    /* Quiet mode has positive and negative triggers */
    cur_qm = p_ext_data->flags & AANC_FLAGS_QUIET_MODE;
    prev_qm = p_ext_data->prev_flags & AANC_FLAGS_QUIET_MODE;

    if (cur_qm)
    {
        if (prev_qm) /* Steady state for quiet mode detect event */
        {
            if (p_ext_data->quiet_event_detect.running == AANC_EVENT_DETECTED)
            {
                p_ext_data->quiet_event_detect.frame_counter -= 1;
                if (p_ext_data->quiet_event_detect.frame_counter <= 0)
                {
                    aanc_send_event_trigger(op_data, TRUE,
                                            AANC_EVENT_ID_QUIET, 0);
                    p_ext_data->quiet_event_detect.running = AANC_EVENT_SENT;
                }
            }
        }
        else /* Rising edge for quiet mode detect event */
        {
            p_ext_data->quiet_event_detect.frame_counter -= 1;
            p_ext_data->quiet_event_detect.running = AANC_EVENT_DETECTED;
            aanc_clear_event(&p_ext_data->quiet_event_clear);
        }
    }
    else
    {
        if (prev_qm) /* Falling edge for quiet mode clear event */
        {
            p_ext_data->quiet_event_clear.frame_counter -= 1;
            p_ext_data->quiet_event_clear.running = AANC_EVENT_DETECTED;
            aanc_clear_event(&p_ext_data->quiet_event_detect);
        }
        else /* Steady state for quite mode clear event */
        {
            if (p_ext_data->quiet_event_clear.running == AANC_EVENT_DETECTED)
            {
                p_ext_data->quiet_event_clear.frame_counter -= 1;
                if (p_ext_data->quiet_event_clear.frame_counter <= 0)
                {
                    aanc_send_event_trigger(op_data, FALSE,
                                            AANC_EVENT_ID_QUIET, 0);
                    p_ext_data->quiet_event_clear.running = AANC_EVENT_SENT;
                }
            }
        }
    }

    /* Clipping event */
    cur_clip = p_ext_data->flags & AANC_CLIPPING_FLAG_MASK;
    prev_clip = p_ext_data->prev_flags & AANC_CLIPPING_FLAG_MASK;
    if (cur_clip)
    {
        /* Non-zero flags and no change starts/continues event */
        if (cur_clip == prev_clip)
        {
            aanc_process_event_detect_condition(op_data,
                                                &p_ext_data->clip_event,
                                                AANC_EVENT_ID_CLIP,
                                                (uint16)cur_clip);
        }
    }
    else
    {
        /* Flags reset causes event to be reset */
        if (cur_clip != prev_clip)
        {
            aanc_process_event_clear_condition(op_data, &p_ext_data->clip_event,
                                               AANC_EVENT_ID_CLIP,
                                               (uint16)cur_clip);
        }
    }

    /* Saturation event */
    cur_sat = p_ext_data->flags & AANC_SATURATION_FLAG_MASK;
    prev_sat = p_ext_data->prev_flags & AANC_SATURATION_FLAG_MASK;
    if (cur_sat)
    {
        /* Non-zero flags and no change starts/continues event */
        if (cur_sat == prev_sat)
        {
            aanc_process_event_detect_condition(op_data, &p_ext_data->sat_event,
                                                AANC_EVENT_ID_SAT,
                                                (uint16)cur_sat);
        }
    }
    else
    {
        /* Flags reset causes event to be reset */
        if (cur_sat != prev_sat)
        {
            aanc_process_event_clear_condition(op_data, &p_ext_data->sat_event,
                                               AANC_EVENT_ID_SAT,
                                               (uint16)cur_sat);
        }
    }

    /* Self-talk event */
    cur_ext = p_ext_data->ag->p_ed_ext->spl;
    cur_int = p_ext_data->ag->p_ed_int->spl;
    delta_ext = cur_int - cur_ext;
    if (delta_ext > 0)
    {
        aanc_process_event_detect_condition(op_data,
                                            &p_ext_data->self_talk_event,
                                            AANC_EVENT_ID_SELF_TALK,
                                            (uint16)(delta_ext >> 16));
    }
    else
    {
        aanc_process_event_clear_condition(op_data,
                                           &p_ext_data->self_talk_event,
                                           AANC_EVENT_ID_SELF_TALK,
                                           (uint16)(delta_ext >> 16));
    }
    /* SPL event */
    if (cur_ext > p_ext_data->aanc_cap_params.OFFSET_EVENT_SPL_THRESHOLD)
    {
        aanc_process_event_detect_condition(op_data,
                                            &p_ext_data->spl_event,
                                            AANC_EVENT_ID_SPL,
                                            (uint16)(cur_ext >> 16));
    }
    else
    {
        aanc_process_event_clear_condition(op_data,
                                           &p_ext_data->spl_event,
                                           AANC_EVENT_ID_SPL,
                                           (uint16)(cur_ext >> 16));
    }

    /* Gentle mute event */
    if (p_ext_data->cur_mode == AANC_SYSMODE_GENTLE_MUTE ||
        p_ext_data->cur_mode == AANC_SYSMODE_QUIET)
    {
        if (p_ext_data->ff_gain.fine == 0)
        {
            aanc_process_event_detect_condition(op_data, &p_ext_data->gentle_mute_event,
                                                AANC_EVENT_ID_GENTLE_MUTE,
                                                p_ext_data->ff_gain.fine);            
        }
        else
        {
            aanc_clear_event(&p_ext_data->gentle_mute_event);
        }
    }

    return TRUE;
}

/**
 * \brief  Initialize a ramp on FF or FB fine gains
 *
 * \param p_ramp  Pointer to AANC_RAMP struct
 * \param target  Target gain for the end of the ramp
 * \param timer_param  Ramp duration (seconds, QACT parameter, Q12.20)
 * \param delay_param  Ramp delay (seconds, QACT parameter, Q12.20)
 *
 * \return  Nothing
 */
static void aanc_initialize_ramp(AANC_RAMP *p_ramp,
                                 uint16 target,
                                 unsigned timer_param,
                                 unsigned delay_param)
{
    uint16 timer_duration, delay_duration;

    if (timer_param == 0 && delay_param == 0)
    {
        *p_ramp->p_gain = target;
        p_ramp->state = AANC_RAMP_FINISHED;
        return;
    }

    /* Calculate number of frames from timer parameter */
    timer_duration = (uint16) (
                        (timer_param * AANC_FRAME_RATE) >> 20);
    delay_duration = (uint16) (
                        (delay_param * AANC_FRAME_RATE) >> 20);

    p_ramp->value = *p_ramp->p_gain << 16;
    p_ramp->target = target;
    p_ramp->rate = ((target << 16) - p_ramp->value) / timer_duration;
    p_ramp->duration = timer_duration;
    if (delay_duration == 0)
    {
        p_ramp->state = AANC_RAMP_RUNNING;
    }
    else
    {
        p_ramp->state = AANC_RAMP_WAITING;
    }
    p_ramp->frame_counter = (uint16)(p_ramp->duration + delay_duration);
}

/**
 * \brief  FF/FB fine gain ramp state machine
 *
 * \param p_ramp  Pointer to AANC_RAMP struct
 *
 *  The INITIALIZED state is reserved for future use.
 *  If there is a delay the frame counter will initially count down to the
 *  ramp duration during AANC_RAMP_WAITING.
 *  During AANC_RAMP_RUNNING the ramp is implemented and the gain updated
 *  When the ramp is finished the state is moved on to AANC_RAMP_FINISHED
 *
 * \return  Nothing
 */
static void aanc_process_ramp(AANC_RAMP *p_ramp)
{
    unsigned rounded_gain;

    switch (p_ramp->state)
    {
        case AANC_RAMP_INITIALIZED:
            p_ramp->state = AANC_RAMP_WAITING;
        case AANC_RAMP_WAITING:
            p_ramp->frame_counter -= 1;
            if (p_ramp->frame_counter <= p_ramp->duration)
            {
                p_ramp->state = AANC_RAMP_RUNNING;
            }
            break;
        case AANC_RAMP_RUNNING:
            p_ramp->frame_counter -= 1;
            if (p_ramp->frame_counter <= 0)
            {
                p_ramp->state = AANC_RAMP_FINISHED;
                /* Make the ramp finishes */
                *p_ramp->p_gain = p_ramp->target;
            }
            else
            {
                p_ramp->value += p_ramp->rate;
                rounded_gain = (p_ramp->value + (1 << 15)) >> 16;
                *p_ramp->p_gain = (uint16)rounded_gain;
            }
            break;
        case AANC_RAMP_FINISHED:
            break;
        default:
            break;
    }

    return;
}

/****************************************************************************
Capability API Handlers
*/

bool aanc_create(OPERATOR_DATA *op_data, void *message_data,
                 unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    int i;
    unsigned *p_default_params; /* Pointer to default params */
    unsigned *p_cap_params;     /* Pointer to capability params */

    /* NB: create is passed a zero-initialized structure so any fields not
     * explicitly initialized are 0.
     */

    L5_DBG_MSG1("AANC Create: p_ext_data at %p", p_ext_data);

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
    for (i = 0; i < AANC_MAX_SINKS; i++)
    {
        p_ext_data->inputs[i] = NULL;
    }
    for (i = 0; i < AANC_MAX_SOURCES; i++)
    {
        p_ext_data->outputs[i] = NULL;
    }

    for (i = 0; i < AANC_NUM_METADATA_CHANNELS; i++)
    {
        p_ext_data->metadata_ip[i] = NULL;
        p_ext_data->metadata_op[i] = NULL;
    }

    /* Initialize capid and sample rate fields */
    p_ext_data->cap_id = AANC_MONO_16K_CAP_ID;
    p_ext_data->sample_rate = 16000;

    /* Initialize parameters */
    p_default_params = (unsigned*) AANC_GetDefaults(p_ext_data->cap_id);
    p_cap_params = (unsigned*) &p_ext_data->aanc_cap_params;
    if(!cpsInitParameters(&p_ext_data->params_def, p_default_params,
                          p_cap_params, sizeof(AANC_PARAMETERS)))
    {
       return TRUE;
    }

    /* Initialize system mode */
    p_ext_data->cur_mode = AANC_SYSMODE_FULL;
    p_ext_data->host_mode = AANC_SYSMODE_FULL;
    p_ext_data->qact_mode = AANC_SYSMODE_FULL;

    /* Trigger re-initialization at start */
    p_ext_data->re_init_flag = TRUE;
    p_ext_data->re_init_hard = TRUE;

    if (!aanc_proc_create(&p_ext_data->ag, p_ext_data->sample_rate))
    {
        L4_DBG_MSG("Failed to create AG data");
        return TRUE;
    }

    p_ext_data->filter_config = AANC_FILTER_CONFIG_SINGLE;
    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC0_ID;
    /* Default to hybrid: ff path is FFB, fb path is FFA */
    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFB_ID;
    p_ext_data->anc_fb_path = AANC_ANC_PATH_FFA_ID;
    p_ext_data->anc_clock_check_value = AANC_HYBRID_ENABLE;

#ifdef USE_AANC_LICENSING
    p_ext_data->license_status = AANC_LICENSE_STATUS_LICENSING_BUILD_STATUS;
#endif

    p_ext_data->ff_ramp.p_gain = &p_ext_data->ff_gain.fine;
    p_ext_data->ff_ramp.p_static = &p_ext_data->ff_static_gain.fine;
    p_ext_data->fb_ramp.p_gain = &p_ext_data->fb_gain.fine;
    p_ext_data->fb_ramp.p_static = &p_ext_data->fb_static_gain.fine;

    p_ext_data->freeze_mode_state = AANC_FFGAIN_EXIT_FREEZE;
    /* Operator creation was succesful, change respone to STATUS_OK*/
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("AANC: Created");
    return TRUE;
}

bool aanc_destroy(OPERATOR_DATA *op_data, void *message_data,
                  unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    /* call base_op destroy that creates and fills response message, too */
    if (!base_op_destroy(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* patch_fn_shared(aanc_capability); TODO: patch functions */

    if (p_ext_data != NULL)
    {
        aanc_proc_destroy(&p_ext_data->ag);

        L4_DBG_MSG("AANC: Cleanup complete.");
    }

    L4_DBG_MSG("AANC: Destroyed");
    return TRUE;
}

bool aanc_start(OPERATOR_DATA *op_data, void *message_data,
                unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* patch_fn_shared(aanc_capability); TODO: patch functions */

    /* FF, FB fine gain ramp variables: duration, delay, target */
    unsigned ff_dur, fb_dur, fb_dly;
    uint16 ff_tgt, fb_tgt;

    /* Start with the assumption that we fail and change later if we succeed */
    if (!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, resp_data))
    {
        return FALSE;
    }

    /* Initialize coarse gains to static values */
    p_ext_data->ec_gain.coarse = p_ext_data->ec_static_gain.coarse;
    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;

    /* EC fine gain is not mode depedent */
    p_ext_data->ec_gain.fine = p_ext_data->ec_static_gain.fine;

    /* FF and FB fine gains are updated in the first process frame */
    p_ext_data->fb_gain.fine = 0;
    p_ext_data->ff_gain.fine = 0;

    ff_dur = p_ext_data->aanc_cap_params.OFFSET_FF_FINE_RAMP_UP_TIMER;
    fb_dur = p_ext_data->aanc_cap_params.OFFSET_FB_FINE_RAMP_UP_TIMER;
    fb_dly = p_ext_data->aanc_cap_params.OFFSET_FB_FINE_RAMP_DELAY_TIMER;

    switch (p_ext_data->cur_mode)
    {
        /* Static ramps FF and FB fine gains */
        case AANC_SYSMODE_STATIC:
            ff_tgt = p_ext_data->ff_static_gain.fine;
            aanc_initialize_ramp(&p_ext_data->ff_ramp, ff_tgt, ff_dur, 0);

            fb_tgt = p_ext_data->fb_static_gain.fine;
            aanc_initialize_ramp(&p_ext_data->fb_ramp, fb_tgt, fb_dur, fb_dly);
            break;
        /* Full ramps FF and FB fine gains */
        case AANC_SYSMODE_FULL:
            ff_tgt = (uint16)p_ext_data->aanc_cap_params.OFFSET_FXLMS_INITIAL_VALUE;
            aanc_initialize_ramp(&p_ext_data->ff_ramp, ff_tgt, ff_dur, 0);

            fb_tgt = p_ext_data->fb_static_gain.fine;
            aanc_initialize_ramp(&p_ext_data->fb_ramp, fb_tgt, fb_dur, fb_dly);
            break;
        /* Quiet ramps FB fine gain to static/2 */
        case AANC_SYSMODE_QUIET:
            fb_tgt = (uint16)(p_ext_data->fb_static_gain.fine >> 1);
            aanc_initialize_ramp(&p_ext_data->fb_ramp, fb_tgt, fb_dur, fb_dly);
            break;
        default:
            break;
    }

    aanc_update_gain(op_data, p_ext_data);

    /* Set reinitialization flags to ensure first run behavior */
    p_ext_data->re_init_flag = TRUE;
    p_ext_data->re_init_hard = TRUE;

    /* All good */
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("AANC Started");
    return TRUE;
}

bool aanc_reset(OPERATOR_DATA *op_data, void *message_data,
                unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!base_op_reset(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    p_ext_data->re_init_flag = TRUE;
    p_ext_data->re_init_hard = TRUE;

    L4_DBG_MSG("AANC: Reset");
    return TRUE;
}

bool aanc_connect(OPERATOR_DATA *op_data, void *message_data,
                  unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id, terminal_num, max_value;
    tCbuffer* pterminal_buf;
    tCbuffer** selected_buffer;
    tCbuffer** selected_metadata;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* can't connect while running if adaptive gain is not disabled */
    if (opmgr_op_is_running(op_data))
    {
        if (p_ext_data->aanc_cap_params.OFFSET_DISABLE_AG_CALC == 0)
        {
            base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
            return TRUE;
        }
    }

    /* Determine whether sink or source terminal being connected */
    terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    terminal_num = terminal_id & TERMINAL_NUM_MASK;

    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC connect: sink terminal %u", terminal_num);
        max_value = AANC_MAX_SINKS;
        selected_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC connect: source terminal %u", terminal_num);
        max_value = AANC_MAX_SOURCES;
        selected_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC connect failed: invalid terminal %u", terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't connect if already connected */
    if (selected_buffer[terminal_num] != NULL)
    {
        L4_DBG_MSG1("AANC connect failed: terminal %u already connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    pterminal_buf = OPMGR_GET_OP_CONNECT_BUFFER(message_data);
    selected_buffer[terminal_num] = pterminal_buf;

    if (terminal_num == AANC_PLAYBACK_TERMINAL_ID)
    {
        /* playback metadata has its own metadata channel */
        if (selected_metadata[AANC_METADATA_PLAYBACK_ID] == NULL &&
            buff_has_metadata(pterminal_buf))
        {
            selected_metadata[AANC_METADATA_PLAYBACK_ID] = pterminal_buf;
        }
    }
    else
    {
        /* mic int/ext and fb mon metadata all muxed onto the same metadata
         * channel
         */
        if (selected_metadata[AANC_METADATA_MIC_ID] == NULL &&
            buff_has_metadata(pterminal_buf))
        {
            selected_metadata[AANC_METADATA_MIC_ID] = pterminal_buf;
        }
    }

    update_touched_sink_sources(p_ext_data);

    return TRUE;
}

bool aanc_disconnect(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id, terminal_num, max_value;

    /* Variables used for distinguishing source/sink */
    tCbuffer** selected_buffer;
    tCbuffer** selected_metadata;

    /* Variables used for finding an alternative metadata channel at
     * disconnect.
     */
    unsigned i;
    bool found_alternative;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* can't disconnect while running if adaptive gain is not disabled */
    if (opmgr_op_is_running(op_data))
    {
        if (p_ext_data->aanc_cap_params.OFFSET_DISABLE_AG_CALC == 0)
        {
            base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
            return TRUE;
        }
    }

    /* Determine whether sink or source terminal being disconnected */
    terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    terminal_num = terminal_id & TERMINAL_NUM_MASK;

    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC disconnect: sink terminal %u", terminal_num);
        max_value = AANC_MAX_SINKS;
        selected_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC disconnect: source terminal %u", terminal_num);
        max_value = AANC_MAX_SOURCES;
        selected_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC disconnect failed: invalid terminal %u",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    found_alternative = FALSE;
    /* Can't disconnect if not connected */
    if (selected_buffer[terminal_num] == NULL)
    {
        L4_DBG_MSG1("AANC disconnect failed: terminal %u not connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (terminal_num == AANC_PLAYBACK_TERMINAL_ID)
    {
        /* playback metadata has its own metadata channel */
        if (selected_metadata[AANC_METADATA_PLAYBACK_ID] != NULL)
        {
            selected_metadata[AANC_METADATA_PLAYBACK_ID] = NULL;
        }
    }
    else
    {
        /* Mic int/ext and fb mon metadata all muxed onto the same metadata
         * channel. Try to find an alternative channel to set the metadata to if
         * we're disconnecting the existing metadata channel. */
        if (selected_metadata[AANC_METADATA_MIC_ID] ==
            selected_buffer[terminal_num])
        {
            for (i = 1; i < max_value; i++)
            {
                if (i == terminal_num)
                {
                    continue;
                }
                if (selected_buffer[i] != NULL &&
                    buff_has_metadata(selected_buffer[i]))
                {
                    selected_metadata[AANC_METADATA_MIC_ID] = selected_buffer[i];
                    found_alternative = TRUE;
                    break;
                }
            }
            if (!found_alternative)
            {
                selected_metadata[AANC_METADATA_MIC_ID] = NULL;
            }
        }
    }

    selected_buffer[terminal_num] = NULL;

    update_touched_sink_sources(p_ext_data);

    return TRUE;
}

bool aanc_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id;
#ifndef DISABLE_IN_PLACE
    unsigned terminal_num;
#endif

    /* Variables used for distinguishing source/sink */
    unsigned max_value;
    tCbuffer** opposite_buffer;
    tCbuffer** selected_metadata;

    if (!base_op_buffer_details(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* Response pointer */
    OP_BUF_DETAILS_RSP *p_resp = (OP_BUF_DETAILS_RSP*) *resp_data;

#ifdef DISABLE_IN_PLACE
    p_resp->runs_in_place = FALSE;
    p_resp->b.buffer_size = AANC_DEFAULT_BUFFER_SIZE;
#else

    /* Determine whether sink or source terminal being disconnected */
    terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    terminal_num = terminal_id & TERMINAL_NUM_MASK;

    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC buffer details: sink buffer %u", terminal_num);
        max_value = AANC_MAX_SINKS;
        opposite_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC buffer details: source buffer %u", terminal_num);
        max_value = AANC_MAX_SOURCES;
        opposite_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC buffer details failed: invalid terminal %u",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    p_resp->runs_in_place = TRUE;
    p_resp->b.in_place_buff_params.in_place_terminal = \
        terminal_id ^ TERMINAL_SINK_MASK;
    p_resp->b.in_place_buff_params.size = AANC_DEFAULT_BUFFER_SIZE;
    p_resp->b.in_place_buff_params.buffer = opposite_buffer[terminal_num];
    L4_DBG_MSG1("aanc_playback_buffer_details: %u",
                p_resp->b.buffer_size);

    p_resp->supports_metadata = TRUE;

    if (terminal_num == AANC_PLAYBACK_TERMINAL_ID)
    {
        p_resp->metadata_buffer = selected_metadata[AANC_METADATA_PLAYBACK_ID];
    }
    else
    {
        p_resp->metadata_buffer = selected_metadata[AANC_METADATA_MIC_ID];
    }

#endif /* DISABLE_IN_PLACE */
    return TRUE;
}

bool aanc_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
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
    resp->block_size = AANC_DEFAULT_BLOCK_SIZE;

    return TRUE;
}

/****************************************************************************
Opmsg handlers
*/
bool aanc_opmsg_set_control(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *resp_length,
                            OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    unsigned i;
    unsigned num_controls;

    AANC_GAIN_OVERRIDE sel_override;
    OPMSG_RESULT_STATES result;

    /* FF, FB fine gain ramp: duration, delay, target, ramp pointers */
    unsigned mt_dur, fb_dur, ff_dur, fb_dly;
    uint16 ff_tgt, fb_tgt;
    AANC_RAMP *p_ff_ramp, *p_fb_ramp;

    if(!cps_control_setup(message_data, resp_length, resp_data, &num_controls))
    {
       return FALSE;
    }

    /* Iterate through the control messages looking for mode and gain override
     * messages */
    result = OPMSG_RESULT_STATES_NORMAL_STATE;
    for (i=0; i<num_controls; i++)
    {
        CPS_CONTROL_SOURCE ctrl_src;
        unsigned ctrl_value, ctrl_id;

        ctrl_id = cps_control_get(message_data, i, &ctrl_value, &ctrl_src);

        /* Mode override */
        if (ctrl_id == OPMSG_CONTROL_MODE_ID)
        {

            /* Check for valid mode */
            ctrl_value &= AANC_SYSMODE_MASK;
            if (ctrl_value >= AANC_SYSMODE_MAX_MODES)
            {
                result = OPMSG_RESULT_STATES_INVALID_CONTROL_VALUE;
                break;
            }

            /* Re-initialize event states if not in quiet mode */
            if (ctrl_value != AANC_SYSMODE_QUIET &&
                ctrl_value != AANC_SYSMODE_GENTLE_MUTE)
            {
                aanc_initialize_events(op_data, p_ext_data);
            }

            /* Gain update logic */
            mt_dur = p_ext_data->aanc_cap_params.OFFSET_GENTLE_MUTE_TIMER;
            fb_dur = p_ext_data->aanc_cap_params.OFFSET_FB_FINE_RAMP_UP_TIMER;
            ff_dur = p_ext_data->aanc_cap_params.OFFSET_FF_FINE_RAMP_UP_TIMER;
            fb_dly = p_ext_data->aanc_cap_params.OFFSET_FB_FINE_RAMP_DELAY_TIMER;

            p_ff_ramp = &p_ext_data->ff_ramp;
            p_fb_ramp = &p_ext_data->fb_ramp;

            switch (ctrl_value)
            {
                case AANC_SYSMODE_STANDBY:
                    /* Standby doesn't change gains */
                case AANC_SYSMODE_FREEZE:
                    /* Freeze doesn't change gains */
                    break;
                case AANC_SYSMODE_GENTLE_MUTE:
                    /* Gentle mute will ramp the FF and FB fine gains down to
                     * 0.
                     */
                    aanc_initialize_ramp(p_ff_ramp, 0, mt_dur, 0);
                    aanc_initialize_ramp(p_fb_ramp, 0, mt_dur, 0);
                    break;
                case AANC_SYSMODE_MUTE_ANC:
                    /* Mute FF and FB gains */
                    p_ext_data->ff_gain.fine = 0;
                    p_ext_data->fb_gain.fine = 0;
                    break;
                case AANC_SYSMODE_STATIC:
                    /* Set all gains to static values. FF and FB gains will
                     * be ramped from 0 to the static value.
                     */
                    p_ext_data->ec_gain = p_ext_data->ec_static_gain;

                    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
                    p_ext_data->ff_gain.fine = 0;
                    ff_tgt = p_ext_data->ff_static_gain.fine;
                    aanc_initialize_ramp(p_ff_ramp, ff_tgt, ff_dur, 0);

                    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
                    p_ext_data->fb_gain.fine = 0;
                    fb_tgt = p_ext_data->fb_static_gain.fine;
                    aanc_initialize_ramp(p_fb_ramp, fb_tgt, fb_dur, fb_dly);
                    break;
                case AANC_SYSMODE_FULL:
                    /* Set gains to static. FB fine gain will be ramped from 0
                     * from 0 to its static value, FF fine gain ramped to its
                     * initial value.
                     */
                    p_ext_data->ec_gain = p_ext_data->ec_static_gain;

                    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
                    p_ext_data->ff_gain.fine = 0;
                    ff_tgt = (uint16)p_ext_data->aanc_cap_params.OFFSET_FXLMS_INITIAL_VALUE;
                    aanc_initialize_ramp(p_ff_ramp, ff_tgt, ff_dur, 0);

                    /* Quiet mode keeps FB fine gain at current value */
                    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
                    if (p_ext_data->cur_mode != AANC_SYSMODE_QUIET)
                    {
                        p_ext_data->fb_gain.fine = 0;
                    }
                    fb_tgt = p_ext_data->fb_static_gain.fine;
                    aanc_initialize_ramp(p_fb_ramp, fb_tgt, fb_dur, fb_dly);
                    break;
                case AANC_SYSMODE_QUIET:
                    /* Quiet mode sets gains to static and leaves FF and FB
                     * gains at their current value. Initializing the ramps
                     * ensures the gains will be ramped down to the target
                     * value.
                     */
                    p_ext_data->ec_gain = p_ext_data->ec_static_gain;

                    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
                    aanc_initialize_ramp(p_ff_ramp, 0, ff_dur, 0);

                    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
                    fb_tgt = (uint16)(p_ext_data->fb_static_gain.fine >> 1);
                    aanc_initialize_ramp(p_fb_ramp, fb_tgt, fb_dur, 0);
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
                * &= is used to preserve the state of the gain bits in the
                * override word.
                */
                if (ctrl_src == CPS_SOURCE_OBPM_ENABLE)
                {
                    p_ext_data->ovr_control |= AANC_CONTROL_MODE_OVERRIDE;
                }
                else
                {
                    p_ext_data->ovr_control &= AANC_OVERRIDE_MODE_MASK;
                }
            }

            continue;
        }

        /* In/Out of Ear control */
        else if (ctrl_id == AANC_CONSTANT_IN_OUT_EAR_CTRL)
        {
            ctrl_value &= 0x01;
            p_ext_data->in_out_status = ctrl_value;

            /* No override flags indicated for in/out of ear */
            continue;
        }

        /* Channel control */
        else if (ctrl_id == AANC_CONSTANT_CHANNEL_CTRL)
        {
            /* Channel can only be updated from the host */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                ctrl_value &= 0x1;
                if (ctrl_value == 0)
                {
                    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC0_ID;
                }
                else
                {
                    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC1_ID;
                }
                L4_DBG_MSG1("AANC channel override: %d",
                            p_ext_data->anc_channel);

            }

            /* No override flags indicated for channel */
            continue;
        }

        /* Feedforward control */
        else if (ctrl_id == AANC_CONSTANT_FEEDFORWARD_CTRL)
        {
            /* Feedforward can only be updated from the host */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                ctrl_value &= 0x1;
                if (ctrl_value == 0)
                {
                    /* hybrid */
                    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFB_ID;
                    p_ext_data->anc_fb_path = AANC_ANC_PATH_FFA_ID;
                    p_ext_data->anc_clock_check_value = AANC_HYBRID_ENABLE;
                }
                else
                {
                    /* feedforward only */
                    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFA_ID;
                    p_ext_data->anc_fb_path = 0;
                    p_ext_data->anc_clock_check_value = AANC_FEEDFORWARD_ENABLE;
                }
                L4_DBG_MSG2("AANC feedforward override: %d - %d",
                            p_ext_data->anc_ff_path, p_ext_data->anc_fb_path);
            }

            /* No override flags indicated for feedforward */
            continue;
        }

        else if (ctrl_id == AANC_CONSTANT_FF_FINE_GAIN_CTRL ||
                 (ctrl_id >= AANC_CONSTANT_FF_COARSE_GAIN_CTRL &&
                  ctrl_id <= AANC_CONSTANT_EC_COARSE_GAIN_CTRL))
        {

            sel_override = gain_override_table[ctrl_id];

            if (override_gain(p_ext_data,
                              (uint16)ctrl_value,
                              sel_override.coarse,
                              sel_override.offset))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }

        /* Filter config control */
        else if (ctrl_id == AANC_CONSTANT_FILTER_CONFIG_CTRL)
        {
            /* Channel can only be updated from the host */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                ctrl_value &= 0x1;
                /* Set ANC channel */
                FXLMS100_DMX *p_fxlms = p_ext_data->ag->p_fxlms;
                unsigned existing_configuration = \
                    p_fxlms->configuration & FXLMS100_CONFIG_LAYOUT_MASK_INV;
                switch (ctrl_value)
                {
                    case AANC_FILTER_CONFIG_SINGLE:
                        p_fxlms->configuration = \
                            existing_configuration | FXLMS100_CONFIG_SINGLE;
                        p_ext_data->filter_config = AANC_FILTER_CONFIG_SINGLE;
                        break;
                    case AANC_FILTER_CONFIG_PARALLEL:
                        p_fxlms->configuration = \
                            existing_configuration | FXLMS100_CONFIG_PARALLEL;
                        p_ext_data->filter_config = AANC_FILTER_CONFIG_PARALLEL;
                        break;
                    default:
                        p_fxlms->configuration = \
                            existing_configuration | FXLMS100_CONFIG_SINGLE;
                        p_ext_data->filter_config = AANC_FILTER_CONFIG_SINGLE;
                        break;
                }
                L4_DBG_MSG1("AANC filter configuration override: %u",
                            p_fxlms->configuration);

            }

            /* No override flags indicated for channel */
            continue;
        }

        result = OPMSG_RESULT_STATES_UNSUPPORTED_CONTROL;
    }

    /* Set current operating mode based on override */
    /* NB: double AND removes gain override bits from comparison */
    if ((p_ext_data->ovr_control & AANC_CONTROL_MODE_OVERRIDE)
        & AANC_CONTROL_MODE_OVERRIDE)
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

bool aanc_opmsg_get_params(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *resp_length,
                           OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                     resp_length, resp_data);
}

bool aanc_opmsg_get_defaults(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *resp_length,
                             OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetDefaultsMsgHandler(&p_ext_data->params_def, message_data,
                                    resp_length, resp_data);
}

bool aanc_opmsg_set_params(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *resp_length,
                           OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    bool success;
    /* patch_fn(aanc_opmsg_set_params); */

    success = cpsSetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                       resp_length, resp_data);

    if (success)
    {
        /* Set re-initialization flag for capability */
        p_ext_data->re_init_flag = TRUE;
    }
    else
    {
        L2_DBG_MSG("AANC Set Parameters Failed");
    }

    return success;
}

bool aanc_opmsg_get_status(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *resp_length,
                           OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* patch_fn_shared(aanc_capability);  TODO: patch functions */
    int i;
    unsigned *resp;

    AANC_STATISTICS stats;
    FXLMS100_DMX *p_fxlms;
    ED100_DMX *p_ed_ext, *p_ed_int, *p_ed_pb;
    ParamType *pparam;
    ADAPTIVE_GAIN *p_ag;

    /* Build the response */
    if(!common_obpm_status_helper(message_data, resp_length, resp_data,
                                  sizeof(AANC_STATISTICS), &resp))
    {
         return FALSE;
    }

    if (resp != NULL)
    {
        p_fxlms = p_ext_data->ag->p_fxlms;
        p_ed_ext = p_ext_data->ag->p_ed_ext;
        p_ed_int = p_ext_data->ag->p_ed_int;
        p_ed_pb = p_ext_data->ag->p_ed_pb;

#ifdef USE_AANC_LICENSING
        p_ext_data->license_status = AANC_LICENSE_STATUS_LICENSING_BUILD_STATUS;
        if (p_fxlms->licensed)
        {
            p_ext_data->license_status |= AANC_LICENSE_STATUS_FxLMS;
        }
        /* NB: License status won't be set if the block is disabled.
         * Given that all EDs use the same license check, OR a comparison
         * between them.
         */
        if (p_ed_ext->licensed || p_ed_int->licensed || p_ed_pb->licensed)
        {
            p_ext_data->license_status |= AANC_LICENSE_STATUS_ED;
        }
#endif /* USE_AANC_LICENSING */

        stats.OFFSET_CUR_MODE = p_ext_data->cur_mode;
        stats.OFFSET_OVR_CONTROL = p_ext_data->ovr_control;
        stats.OFFSET_IN_OUT_EAR_CTRL = p_ext_data->in_out_status;
        stats.OFFSET_CHANNEL = p_ext_data->anc_channel;
        stats.OFFSET_FILTER_CONFIG = p_fxlms->configuration;
        stats.OFFSET_FEEDFORWARD_PATH = p_ext_data->anc_ff_path;
        stats.OFFSET_LICENSE_STATUS = p_ext_data->license_status;
        stats.OFFSET_FLAGS = p_ext_data->flags;
        stats.OFFSET_AG_CALC = p_fxlms->adaptive_gain;
        /* Send previous gain values as stats because these are only updated
         * when the value is actually written to HW.
         */
        stats.OFFSET_FF_FINE_GAIN_CTRL = p_ext_data->ff_gain_prev.fine;
        stats.OFFSET_FF_COARSE_GAIN_CTRL = \
        p_ext_data->ff_gain_prev.coarse & AANC_COARSE_GAIN_MASK;
        stats.OFFSET_FF_GAIN_DB = aanc_proc_calc_gain_db(
            p_ext_data->ff_gain_prev.fine,
            (int16)p_ext_data->ff_gain_prev.coarse);
        stats.OFFSET_FB_FINE_GAIN_CTRL = p_ext_data->fb_gain_prev.fine;
        stats.OFFSET_FB_COARSE_GAIN_CTRL = \
            p_ext_data->fb_gain_prev.coarse & AANC_COARSE_GAIN_MASK;
        stats.OFFSET_FB_GAIN_DB = aanc_proc_calc_gain_db(
            p_ext_data->fb_gain_prev.fine,
            (int16)p_ext_data->fb_gain_prev.coarse);
        stats.OFFSET_EC_FINE_GAIN_CTRL = p_ext_data->ec_gain_prev.fine;
        stats.OFFSET_EC_COARSE_GAIN_CTRL = \
            p_ext_data->ec_gain_prev.coarse & AANC_COARSE_GAIN_MASK;
        stats.OFFSET_EC_GAIN_DB = aanc_proc_calc_gain_db(
            p_ext_data->ec_gain_prev.fine,
            (int16)p_ext_data->ec_gain_prev.coarse);
        stats.OFFSET_SPL_EXT = p_ed_ext->spl;
        stats.OFFSET_SPL_INT = p_ed_int->spl;
        stats.OFFSET_SPL_PB = p_ed_pb->spl;
        /* Read and reset peak meters */
        p_ag = p_ext_data->ag;
        stats.OFFSET_PEAK_EXT = p_ag->clip_ext.peak_value;
        p_ag->clip_ext.peak_value = 0;
        stats.OFFSET_PEAK_INT = p_ag->clip_int.peak_value;
        p_ag->clip_int.peak_value = 0;
        stats.OFFSET_PEAK_PB = p_ag->clip_pb.peak_value;
        p_ag->clip_pb.peak_value = 0;

        pparam = (ParamType*)(&stats);
        for (i=0; i<AANC_N_STAT/2; i++)
        {
            resp = cpsPack2Words(pparam[2*i], pparam[2*i+1], resp);
        }
        if ((AANC_N_STAT % 2) == 1) /* last one */
        {
            cpsPack1Word(pparam[AANC_N_STAT-1], resp);
        }
    }

    return TRUE;
}

bool ups_params_aanc(void* instance_data, PS_KEY_TYPE key,
                     PERSISTENCE_RANK rank, uint16 length,
                     unsigned* data, STATUS_KYMERA status,
                     uint16 extra_status_info)
{
    OPERATOR_DATA *op_data = (OPERATOR_DATA*) instance_data;
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    cpsSetParameterFromPsStore(&p_ext_data->params_def, length, data, status);

    /* Set the reinitialization flag after setting the parameters */
    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

bool aanc_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *resp_length,
                         OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    PS_KEY_TYPE key;
    bool success;

    success = cpsSetUcidMsgHandler(&p_ext_data->params_def, message_data,
                                  resp_length, resp_data);
    L5_DBG_MSG1("AANC cpsSetUcidMsgHandler Return Value %u", success);
    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(p_ext_data->cap_id,
                                         p_ext_data->params_def.ucid,
                                         OPMSG_P_STORE_PARAMETER_SUB_ID);

    ps_entry_read((void*)op_data, key, PERSIST_ANY, ups_params_aanc);

    L5_DBG_MSG1("AANC UCID Set to %u", p_ext_data->params_def.ucid);

    p_ext_data->re_init_flag = TRUE;

    return success;
}

bool aanc_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *resp_length,
                          OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetUcidMsgHandler(&p_ext_data->params_def, p_ext_data->cap_id,
                                message_data, resp_length, resp_data);
}

/****************************************************************************
Custom opmsg handlers
*/
bool aanc_opmsg_set_static_gain(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    uint16 coarse_gain, fine_gain;

    coarse_gain = OPMSG_FIELD_GET(message_data,
                                  OPMSG_SET_AANC_STATIC_GAIN,
                                  FF_COARSE_STATIC_GAIN);
    fine_gain = OPMSG_FIELD_GET(message_data,
                                OPMSG_SET_AANC_STATIC_GAIN,
                                FF_FINE_STATIC_GAIN);

    /* Add headroom for adaptive gain algorithm. If the fine gain is too
     * large, decrease it by 6dB and increment the coarse gain to compensate.
     */
    if (fine_gain > AANC_STATIC_GAIN_ADJUST_THRESHOLD)
    {
        fine_gain = fine_gain >> 1;
        coarse_gain++;
    }
    else if (fine_gain < AANC_STATIC_GAIN_FAIL_THRESHOLD)
    {
        L0_DBG_MSG2("AANC FF static fine gain too low: %hu (< %hu)",
                    fine_gain, AANC_STATIC_GAIN_FAIL_THRESHOLD);
    }

    p_ext_data->ff_static_gain.coarse = coarse_gain;
    p_ext_data->ff_static_gain.fine = fine_gain;
    L4_DBG_MSG2("AANC Set FF Static Gain: Coarse = %hu, Fine = %hu",
        p_ext_data->ff_static_gain.coarse, p_ext_data->ff_static_gain.fine);

    p_ext_data->fb_static_gain.coarse = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FB_COARSE_STATIC_GAIN);
    p_ext_data->fb_static_gain.fine = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FB_FINE_STATIC_GAIN);
    L4_DBG_MSG2("AANC Set FB Static Gain: Coarse = %hu, Fine = %hu",
        p_ext_data->fb_static_gain.coarse, p_ext_data->fb_static_gain.fine);

    p_ext_data->ec_static_gain.coarse = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, EC_COARSE_STATIC_GAIN);
    p_ext_data->ec_static_gain.fine = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, EC_FINE_STATIC_GAIN);
    L4_DBG_MSG2("AANC Set EC Static Gain: Coarse = %hu, Fine = %hu",
        p_ext_data->ec_static_gain.coarse, p_ext_data->ec_static_gain.fine);
    p_ext_data->flags |= AANC_FLAGS_STATIC_GAIN_LOADED;

    /* Allow a direct gain update if the sysmode is static without requiring
     * a follow-up gain override
     */
    if (p_ext_data->cur_mode == AANC_SYSMODE_STATIC)
    {
        p_ext_data->ff_gain = p_ext_data->ff_static_gain;
        p_ext_data->fb_gain = p_ext_data->fb_static_gain;
        p_ext_data->ec_gain = p_ext_data->ec_static_gain;
    }

    return TRUE;
}

bool aanc_opmsg_set_plant_model(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!aanc_fxlms100_set_plant_model(p_ext_data->ag->p_fxlms, message_data))
    {
        L4_DBG_MSG("AANC set plant coefficients failed");
        return FALSE;
    }

    p_ext_data->flags |= AANC_FLAGS_PLANT_MODEL_LOADED;

    return TRUE;
}

bool aanc_opmsg_set_control_model(OPERATOR_DATA *op_data,
                                  void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    int destination;

    if (!aanc_fxlms100_set_control_model(p_ext_data->ag->p_fxlms, message_data,
                                         &destination))
    {
        L4_DBG_MSG("AANC set control coefficients failed");
        return FALSE;
    }

    if (destination)
    {
        p_ext_data->flags |= AANC_FLAGS_CONTROL_1_MODEL_LOADED;
    }
    else
    {
        p_ext_data->flags |= AANC_FLAGS_CONTROL_0_MODEL_LOADED;
    }

    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

/****************************************************************************
Data processing function
*/
void aanc_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    int i = 0;
    unsigned initial_value;

    /* Certain conditions require an "early exit" that will just discard any
     * data in the input buffers and not do any other processing
     */
    bool exit_early = FALSE;

    /* track the number of samples process */
    int sample_count;

    /* After data is processed flags are tested to determine the equivalent
     * operating state. This is an input to the gain update decision state
     * machine.
     */
    unsigned mode_after_flags = p_ext_data->cur_mode;

    /* Reference the calculated gain */
    unsigned *p_gain_calc = &p_ext_data->ag->p_fxlms->adaptive_gain;

    /* Reference the capability parameters */
    AANC_PARAMETERS *p_params = &p_ext_data->aanc_cap_params;

    bool calculate_gain = TRUE;

#ifdef RUNNING_ON_KALSIM
    unsigned pre_process_flags = p_ext_data->flags;
#endif /* RUNNING_ON_KALSIM */

    int samples_to_process = INT_MAX;

    /*********************
     * Early exit testing
     *********************/

    /* Without adequate data or space we can just return */

    /* Determine whether to copy any input data to output terminals */
    samples_to_process = aanc_calc_samples_to_process(p_ext_data);

    /* Return early if int and ext mic input terminals are not connected */
    if (samples_to_process == INT_MAX)
    {
        L5_DBG_MSG("Minimum number of ports (int and ext mic) not connected");
        return;
    }

     /* Return early if no data or not enough space to process */
    if (samples_to_process < AANC_DEFAULT_FRAME_SIZE)
    {
        L5_DBG_MSG1("Not enough data/space to process (%d)", samples_to_process);
        return;
    }

    /* Other conditions that are invalid for running AANC need to discard
     * input data if it exists.
     */

    /* Don't do any processing in standby */
    if (p_ext_data->cur_mode == AANC_SYSMODE_STANDBY)
    {
        exit_early = TRUE;
    }

    /* Don't do any processing if out of ear */
    bool disable_ear_check = (p_params->OFFSET_AANC_DEBUG &
                              AANC_CONFIG_AANC_DEBUG_DISABLE_EAR_STATUS_CHECK);
    if ((p_ext_data->in_out_status != AANC_IN_EAR) && !disable_ear_check)
    {
        exit_early = TRUE;
    }

    /* Don't do any processing if ANC HW clocks are invalid */
#ifndef RUNNING_ON_KALSIM
    uint16 anc0_enable;
    uint16 anc1_enable;
    uint16 *anc_selected = &anc0_enable;

    stream_get_anc_enable(&anc0_enable, &anc1_enable);

    if (p_ext_data->anc_channel == AANC_ANC_INSTANCE_ANC1_ID)
    {
        anc_selected = &anc1_enable;
    }

    bool anc_is_running = *anc_selected == p_ext_data->anc_clock_check_value;
    bool disable_clock_check = (p_params->OFFSET_AANC_DEBUG &
                                AANC_CONFIG_AANC_DEBUG_DISABLE_ANC_CLOCK_CHECK);
    /* Don't do any processing if HW clocks aren't running */
    if (!anc_is_running && !disable_clock_check)
    {
        L2_DBG_MSG1("AANC invalid clocks detected: %hu", *anc_selected);
        exit_early = TRUE;
    }
#endif

    sample_count = 0;
    if (exit_early)
    {
        bool discard_data = TRUE;

        /* There is at least 1 frame to process */
        do {
            sample_count += AANC_DEFAULT_FRAME_SIZE;
            /* Iterate through all sinks */
            for (i = 0; i < AANC_MAX_SINKS; i++)
            {
                if (p_ext_data->inputs[i] != NULL)
                {
                    /* Discard a frame of data */
                    cbuffer_discard_data(p_ext_data->inputs[i],
                                         AANC_DEFAULT_FRAME_SIZE);

                    /* If there isn't a frame worth of data left then don't
                     * iterate through the input terminals again.
                     */
                    samples_to_process = cbuffer_calc_amount_data_in_words(
                        p_ext_data->inputs[i]);

                    if (samples_to_process < AANC_DEFAULT_FRAME_SIZE)
                    {
                        discard_data = FALSE;
                    }
                }
            }
        } while (discard_data);
        for (i = 0; i < AANC_NUM_METADATA_CHANNELS; i++)
        {
            /* Input is discarded, so consume and delete incoming metadata tags
               This can be done by transporting input metadata to NULL ptr */
            metadata_strict_transport(p_ext_data->metadata_ip[i],
                                      NULL,
                                      sample_count * OCTETS_PER_SAMPLE);

        }
        /* Return on early exit */
        return;
    }

    /***************************
     * Adaptive gain processing
     ***************************/

    if (p_ext_data->re_init_flag == TRUE)
    {
        ADAPTIVE_GAIN *p_ag = p_ext_data->ag;
        p_ext_data->re_init_flag = FALSE;

        /* Copy terminal buffer pointers */
        p_ag->p_playback_ip = p_ext_data->inputs[AANC_PLAYBACK_TERMINAL_ID];
        p_ag->p_fbmon_ip = p_ext_data->inputs[AANC_FB_MON_TERMINAL_ID];
        p_ag->p_mic_int_ip = p_ext_data->inputs[AANC_MIC_INT_TERMINAL_ID];
        p_ag->p_mic_ext_ip = p_ext_data->inputs[AANC_MIC_EXT_TERMINAL_ID];

        p_ag->p_playback_op = p_ext_data->outputs[AANC_PLAYBACK_TERMINAL_ID];
        p_ag->p_fbmon_op = p_ext_data->outputs[AANC_FB_MON_TERMINAL_ID];
        p_ag->p_mic_int_op = p_ext_data->outputs[AANC_MIC_INT_TERMINAL_ID];
        p_ag->p_mic_ext_op = p_ext_data->outputs[AANC_MIC_EXT_TERMINAL_ID];

        aanc_initialize_events(op_data, p_ext_data);

        aanc_proc_initialize(p_params, p_ag, p_params->OFFSET_FXLMS_INITIAL_VALUE,
                             &p_ext_data->flags, p_ext_data->re_init_hard);
    }

    /* Identify whether to do the gain calculation step */
    if ((p_params->OFFSET_DISABLE_AG_CALC & 0x1) ||
        (p_ext_data->cur_mode != AANC_SYSMODE_FULL) ||
        (p_ext_data->frames_to_freez > 0))
    {
        calculate_gain = FALSE;
    }

    sample_count = 0;
    /* Consume all the data in the input buffer, or until there isn't space
     * available.
     */
    while (samples_to_process >= AANC_DEFAULT_FRAME_SIZE)
    {
        aanc_proc_process_data(p_ext_data->ag, calculate_gain);

        samples_to_process = aanc_calc_samples_to_process(p_ext_data);

        sample_count += AANC_DEFAULT_FRAME_SIZE;

        /*********************************************
         * Send unsolicited message (simulation only)
         ********************************************/
#ifdef RUNNING_ON_KALSIM
        if (pre_process_flags != p_ext_data->flags)
        {
            aanc_update_gain(op_data, p_ext_data);
        }
#endif /* RUNNING_ON_KALSIM */

        /*************************
         * Check processing flags
         *************************/
        if (p_ext_data->flags & AANC_ED_FLAG_MASK)
        {
            L5_DBG_MSG1("AANC ED detected: %u",
                        p_ext_data->flags & AANC_ED_FLAG_MASK);
            mode_after_flags = AANC_SYSMODE_FREEZE;
        }

        if (p_ext_data->flags & AANC_CLIPPING_FLAG_MASK)
        {
            L5_DBG_MSG1("AANC Clipping detected: %u",
                        p_ext_data->flags & AANC_CLIPPING_FLAG_MASK);
            mode_after_flags = AANC_SYSMODE_FREEZE;
        }

        if (p_ext_data->flags & AANC_SATURATION_FLAG_MASK)
        {
            L5_DBG_MSG1("AANC Saturation detected: %u",
                        p_ext_data->flags & AANC_SATURATION_FLAG_MASK);
            mode_after_flags = AANC_SYSMODE_FREEZE;
        }

        /**************
         * Update gain
         **************/
        /* Check SYSMODE state as this is the primary control */
        switch (p_ext_data->cur_mode)
        {
            case AANC_SYSMODE_STANDBY:
                /* Shouldn't ever get here */
            case AANC_SYSMODE_MUTE_ANC:
                /* Mute action is taken in SET_CONTROL */
            case AANC_SYSMODE_FREEZE:
                /* Freeze does nothing to change the gains */
                break;
            case AANC_SYSMODE_FULL:
                if (p_ext_data->ff_ramp.state == AANC_RAMP_FINISHED)
                {
                    /* Not ramping FF fine gain, so fall through to state
                     * machine.
                     */
                    if (mode_after_flags == AANC_SYSMODE_FREEZE)
                    {
                        L4_DBG_MSG1("AANC FULL Mode, FREEZE: gain = %hu",
                                    p_ext_data->ff_gain.fine);
                    }
                    else if (mode_after_flags == AANC_SYSMODE_MUTE_ANC)
                    {
                        L4_DBG_MSG("AANC FULL Mode, MUTE: gain = 0");
                        p_ext_data->ff_gain.fine = 0;
                    }
                    else
                    {
                        L4_DBG_MSG2("AANC FULL mode, FULL: gain = %u frames_to_freez = %u",
                                    *p_gain_calc, p_ext_data->frames_to_freez);

                        /* Check mode of FF gain update before updating on hardware */
                        switch (p_ext_data->freeze_mode_state)
                        {
                            case AANC_FFGAIN_NO_FREEZE:
                                p_ext_data->ff_gain.fine = (uint16) *p_gain_calc;
                                
                                if(p_ext_data->ff_gain.fine 
                                   <= (p_ext_data->aanc_cap_params.OFFSET_FXLMS_MIN_BOUND >>
                                   AANC_FXLMS_MIN_BOUND_SHIFT))
                                {
                                    /* Convert time specified into number of frames */
                                    p_ext_data->frames_to_freez = (uint16)
                                    ((p_ext_data->aanc_cap_params.OFFSET_GAIN_MIN_FREEZ_TIME
                                        * AANC_FRAME_RATE)
                                        >> TIMER_PARAM_SHIFT);

                                    calculate_gain = FALSE;
                                    p_ext_data->freeze_mode_state = AANC_FFGAIN_IN_FREEZE;
                                }
                                break;
                            case AANC_FFGAIN_IN_FREEZE:
                                p_ext_data->frames_to_freez--;
                                if(p_ext_data->frames_to_freez == 0)
                                {
                                    p_ext_data->freeze_mode_state = AANC_FFGAIN_EXIT_FREEZE;
                                    calculate_gain = TRUE;
                                }
                                break;
                            case AANC_FFGAIN_EXIT_FREEZE:
                                p_ext_data->ff_gain.fine = (uint16) *p_gain_calc;
                                /* Don't enter to freeze mode again until the FF gain reach
                                 * OFFSET_FXLMS_MIN_BOUND + AANC_RE_FREEZE_FFGAIN_THRESHOLD */
                                if(p_ext_data->ff_gain.fine > 
                                   ((p_ext_data->aanc_cap_params.OFFSET_FXLMS_MIN_BOUND >>
                                   AANC_FXLMS_MIN_BOUND_SHIFT) +
                                   AANC_RE_FREEZE_FFGAIN_THRESHOLD))
                                {
                                    p_ext_data->freeze_mode_state = AANC_FFGAIN_NO_FREEZE;
                                }
                                break;
                            default:
                                L2_DBG_MSG1("AANC FFGain invalid state: %u",
                                            p_ext_data->freeze_mode_state);
                                break;
                        }
                    }
                }
                else
                {
                    /* Initialize the FxLMS algorithm for when the ramp
                     * finishes.
                     */
                    initial_value = p_params->OFFSET_FXLMS_INITIAL_VALUE;
                    aanc_fxlms100_update_gain(p_ext_data->ag->p_fxlms,
                                                (uint16)initial_value);
                    /* Process the FF fine gain ramp. */
                    aanc_process_ramp(&p_ext_data->ff_ramp);
                }
                aanc_process_ramp(&p_ext_data->fb_ramp);
                break;
            case AANC_SYSMODE_STATIC:
                /* Static mode may need to ramp FF/FB fine gains */
                aanc_process_ramp(&p_ext_data->ff_ramp);
                aanc_process_ramp(&p_ext_data->fb_ramp);
                break;
            case AANC_SYSMODE_QUIET:
                /* Fall through as action is taken in gentle mute */                
            case AANC_SYSMODE_GENTLE_MUTE:
                /* Gentle mute ramps gain down to 0 */
                aanc_process_ramp(&p_ext_data->ff_ramp);
                aanc_process_ramp(&p_ext_data->fb_ramp);
                aanc_fxlms100_update_gain(p_ext_data->ag->p_fxlms,
                                          p_ext_data->ff_gain.fine);
                break;
            default:
                L2_DBG_MSG1("AANC SYSMODE invalid: %u", p_ext_data->cur_mode);
                break;
        }

        /* If the fine gain is decreasing continuously for 3 frames(12ms),
         * update Mu with higher value to converge faster
         * other wise use regular value of Mu. */
        if(p_ext_data->aanc_cap_params.OFFSET_MU_STEEP_FALL != 0)
        {
            if(p_ext_data->ff_gain_prev.fine > p_ext_data->ff_gain.fine)
            {
                p_ext_data->cont_gain_drop_cnt++;
                if(p_ext_data->cont_gain_drop_cnt 
                   >= p_ext_data->aanc_cap_params.OFFSET_GAIN_DROP_FRAME_COUNT)
                {
                    /* Change Mu to MU_STEEP_FALL */
                    p_ext_data->ag->p_fxlms->mu = 
                      p_ext_data->aanc_cap_params.OFFSET_MU_STEEP_FALL;
                    L4_DBG_MSG1("Setting MU_STEEP_FALL = %u",
                                p_ext_data->ag->p_fxlms->mu);
                }
            }
            else
            {
                /* Revert Mu to default */
                p_ext_data->ag->p_fxlms->mu = p_ext_data->aanc_cap_params.OFFSET_MU;
                p_ext_data->cont_gain_drop_cnt = 0;           
            }
        }

        /* Clear Frames to freeze if there is change in ANC mode */
        if(p_ext_data->cur_mode != AANC_SYSMODE_FULL)
        {
            p_ext_data->frames_to_freez = 0;
        }

        /* Evaluate event messaging criteria */
        if (!(p_params->OFFSET_AANC_DEBUG &
            AANC_CONFIG_AANC_DEBUG_DISABLE_EVENT_MESSAGING))
        {
            aanc_process_events(op_data, p_ext_data);
            p_ext_data->prev_flags = p_ext_data->flags;
        }

    }
    /* "Hard initialization" is associated with first-time process, so
    * set the FB fine gain to its static value.
    *
    * Clear "hard" reinitializion so that FB gain is not touched in
    * subsequent iterations.
    */
    if (p_ext_data->re_init_hard)
    {
        p_ext_data->re_init_hard = FALSE;
    }

    aanc_update_gain(op_data, p_ext_data);

    /****************
     * Pass Metadata
     ****************/
    for (i = 0; i < AANC_NUM_METADATA_CHANNELS; i++)
    {
        metadata_strict_transport(p_ext_data->metadata_ip[i],
                                  p_ext_data->metadata_op[i],
                                  sample_count * OCTETS_PER_SAMPLE);
    }

    /***************************
     * Update touched terminals
     ***************************/
    touched->sinks = (unsigned) p_ext_data->touched_sinks;
    touched->sources = (unsigned) p_ext_data->touched_sources;

    L5_DBG_MSG("AANC process channel data completed");

    return;
}
