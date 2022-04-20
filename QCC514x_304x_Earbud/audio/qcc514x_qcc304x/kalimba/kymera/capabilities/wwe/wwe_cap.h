/****************************************************************************
 * Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd
****************************************************************************/
/**
 * \file  wwe.h
 * \ingroup capabilities
 *
 * Stub Capability public header file. <br>
 *
 */

#ifndef WWE_CAP_H
#define WWE_CAP_H

#include "wwe_struct.h"

/****************************************************************************
 Private Constant Definitions
 */

/*****************************************************************************
Private Function Definitions
*/

/** The capability data structure for stub capability */
extern const CAPABILITY_DATA wwe_cap_data;

/** Assembly processing function */
extern unsigned cbuffer_copy_and_shift(tCbuffer *src, int16* dst,
                                       unsigned size);

/* Capability API handlers */
extern bool wwe_base_class_init(OPERATOR_DATA *op_data,
                                WWE_CLASS_DATA *p_class_data,
                                const WWE_CAP_VIRTUAL_TABLE *const vt);


/* Common initialization called by specific WWE create() */
extern bool wwe_base_create(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *response_id, void **resp_data,
                            unsigned block_size, unsigned buffer_size,
                            unsigned frame_size);

/* Common functions called by specific WWE start() */
extern bool wwe_start(OPERATOR_DATA *op_data, void *message_data,
                      unsigned *response_id, void **response_data);
extern bool wwe_stop(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **response_data);

extern bool wwe_connect(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *response_id, void **response_data);
extern bool wwe_disconnect(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *response_id, void **resp_data);

extern bool wwe_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *response_id, void **response_data);
extern bool wwe_sched_info(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *response_id, void **response_data);

/* Op msg handlers */
extern bool wwe_opmsg_obpm_set_control(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_opmsg_obpm_get_params(OPERATOR_DATA *op_data,
                                      void *message_data,
                                      unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_opmsg_obpm_get_defaults(OPERATOR_DATA *op_data,
                                        void *message_data,
                                        unsigned *resp_length,
                                        OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_opmsg_obpm_set_params(OPERATOR_DATA *op_data,
                                      void *message_data, unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data,
                               unsigned *resp_length,
                               OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_reset_status(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *resp_length,
                             OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool wwe_mode_change(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *resp_length,
                            OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Model load/unload */
extern bool wwe_trigger_phrase_load(OPERATOR_DATA *op_data,
                                    void *message_data, unsigned *resp_length,
                                    OP_OPMSG_RSP_PAYLOAD **resp_data);

extern bool wwe_trigger_phrase_unload(OPERATOR_DATA *op_data,
                                      void *message_data, unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Processing */
extern void wwe_process_data(OPERATOR_DATA *op_data,
                             TOUCHED_TERMINALS *touched);
extern void wwe_handle_data_flow(WWE_COMMON_DATA *p_ext_data,
                                 TOUCHED_TERMINALS *touched);
extern void wwe_send_notification(OPERATOR_DATA *op_data,
                                  WWE_COMMON_DATA *p_ext_data);

extern bool wwe_set_sample_rate(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data);

extern bool wwe_reinit_algorithm(OPERATOR_DATA *op_data,
                                      void *message_data,
                                      unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data);                              

#endif /* WWE_CAP_H */
