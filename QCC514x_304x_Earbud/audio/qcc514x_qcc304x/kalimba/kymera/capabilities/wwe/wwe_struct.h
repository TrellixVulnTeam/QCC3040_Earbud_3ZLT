/****************************************************************************
 * Copyright (c) 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
* \defgroup wwe
* \file  wwe_struct.h
* \ingroup capabilities
*
* WWE capability private header file. <br>
*
 */

#ifndef WWE_CAP_STRUCT_H
#define WWE_CAP_STRUCT_H

/*****************************************************************************
Include Files
*/
#include "capabilities.h"
#include "op_msg_utilities.h"

#ifndef RUNNING_ON_KALSIM
#include "file_mgr_for_ops.h"
#endif /* #ifndef RUNNING_ON_KALSIM */

#include "wwe_defs.h"

/****************************************************************************
Public Type Definitions
*/

/** Function prototype of function that starts the wakeword engine. **/
typedef bool (*WWE_START_FN)(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **response_data);

/** Function prototype of function that stops the wakeword engine. **/
typedef bool (*WWE_STOP_FN)(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *response_id, void **response_data);

/** Function prototype of function that resets the wakeword engine. **/
typedef bool (*WWE_RESET_FN)(OPERATOR_DATA *op_data,
                             TOUCHED_TERMINALS *touched);

/** Function prototype of function that processes wakeword data. **/
typedef bool (*WWE_PROCESS_FN)(OPERATOR_DATA *op_data,
                               TOUCHED_TERMINALS *touched);

/** Function prototype of function that loads the wakeword engine model. */
#ifndef RUNNING_ON_KALSIM
typedef bool (*WWE_LOAD_FN)(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data,
                        DATA_FILE *trigger_file);

/** Function prototype of function that checks the loaded wakeword engine model.
 */
typedef bool (*WWE_CHECK_LOAD_FN)(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *resp_length,
                              OP_OPMSG_RSP_PAYLOAD **resp_data,
                              DATA_FILE *trigger_file);

/** Function prototype of function that unloads the wakeword engine model. */
typedef bool (*WWE_UNLOAD_FN)(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *resp_length,
                          OP_OPMSG_RSP_PAYLOAD **resp_data, uint16 file_id);

#else
typedef bool (*WWE_STATIC_LOAD_FN)(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *response_id, void **response_data);

#endif /* #ifndef RUNNING_ON_KALSIM */

/** Constant attributes that an inheriting class implements. */
typedef struct WWE_CAP_VIRTUAL_TABLE
{
    /* The function that starts the wakeword engine */
    WWE_START_FN start_fn;

    /* The function that stops the wakeword engine */
    WWE_STOP_FN stop_fn;

    /* The function that resets the wakeword engine */
    WWE_RESET_FN reset_fn;

    /* The function that processes data */
    WWE_PROCESS_FN process_fn;

#ifndef RUNNING_ON_KALSIM
    /* The function that loads the wakeword engine */
    WWE_LOAD_FN load_fn;

    /* The function that checks the loaded wakeword engine */
    WWE_CHECK_LOAD_FN check_load_fn;

    /* The function that unloads the wakeword engine */
    WWE_UNLOAD_FN unload_fn;
#else
    /* The function that does static loading of the wakeword engine */
    WWE_STATIC_LOAD_FN static_load_fn;
#endif /* #ifndef RUNNING_ON_KALSIM */

} WWE_CAP_VIRTUAL_TABLE;

typedef struct WWE_TIMESTAMPS
{
    /** Timestamp which indicates the trigger start timestamp.
     * Derived from phrase length and trigger end timestamp */
    TIME trigger_start_timestamp;

    /** Timestamp which indicates the trigger end timestamp.
     * Only accurate after a positive trigger! */
    TIME trigger_end_timestamp;

    /** offset, trigger lag (in us)
     * trigger start and and timestamps will be adjusted by this */
    unsigned trigger_offset;

} WWE_TIMESTAMPS;


/** Common WWE data **/
typedef struct WWE_COMMON_DATA
{
    unsigned sample_rate;
    /** The input and output buffers with metadata to transport from **/
    tCbuffer *metadata_ip_buffer;
    tCbuffer *metadata_op_buffer;

    /** The buffer at the input terminal for this channel */
    tCbuffer *ip_buffer;
    tCbuffer *op_buffer;

    /** Model details **/
    uint8 *p_model;
    int32 n_model_size;
    int16 file_id;

    /** Shared statistics **/
    unsigned keyword_detected;
    unsigned result_confidence;
    unsigned keyword_len;
    bool trigger;

    CPS_PARAM_DEF parms_def;
    unsigned cur_mode;
    unsigned host_mode;
    unsigned obpm_mode;
    unsigned ovr_control;

    unsigned block_size;
    unsigned buffer_size;
    unsigned frame_size;
    bool init_flag;

    WWE_TIMESTAMPS ts;
    /** The last consumed sample's timestamp. */
    TIME last_timestamp;
} WWE_COMMON_DATA;

typedef struct WWE_CLASS_DATA
{
    WWE_COMMON_DATA wwe;
    const WWE_CAP_VIRTUAL_TABLE *vt;
} WWE_CLASS_DATA;


#endif /* WWE_CAP_STRUCT_H */
