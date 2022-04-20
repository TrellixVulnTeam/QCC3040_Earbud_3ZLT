/****************************************************************************
 * Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC_LITE
 * \ingroup capabilities
 * \file  aanc_lite_cap.h
 * \ingroup AANC_LITE
 *
 * Adaptive ANC (AANC) Lite operator private header file.
 *
 */

#ifndef _AANC_LITE_CAP_H_
#define _AANC_LITE_CAP_H_

/******************************************************************************
Include Files
*/

#include "capabilities.h"

#include "aanc_lite.h"
#include "aanc_lite_gen_c.h"
#include "aanc_lite_defs.h"

/* ANC HAL features */
#include "stream/stream_anc.h"

/******************************************************************************
ANC HW type definitions
*/

/* Represent ANC channels */
typedef enum
{
    AANC_ANC_INSTANCE_NONE_ID = 0x0000,
    AANC_ANC_INSTANCE_ANC0_ID = 0x0001,
    AANC_ANC_INSTANCE_ANC1_ID = 0x0002
} AANC_ANC_INSTANCE;

/* Represent ANC paths */
typedef enum
{
    AANC_ANC_PATH_NONE_ID = 0x0000,
    AANC_ANC_PATH_FFA_ID = 0x0001,
    AANC_ANC_PATH_FFB_ID = 0x0002,
    AANC_ANC_PATH_FB_ID = 0x0003,
    AANC_ANC_PATH_SM_LPF_ID = 0x0004
} AANC_ANC_PATH;

/* Represent ANC clock enables */
typedef enum
{
    AANC_ANC_ENABLE_FFA_MASK = 0x0001,
    AANC_ANC_ENABLE_FFB_MASK = 0x0002,
    AANC_ANC_ENABLE_FB_MASK = 0x0004,
    AANC_ANC_ENABLE_OUT_MASK = 0x0008
} AANC_ANC_ENABLE;

/******************************************************************************
Private Constant Definitions
*/
/* Number of statistics reported by the capability */
#define AANC_LITE_N_STAT               (sizeof(AANC_LITE_STATISTICS)/sizeof(ParamType))

/* Mask for the number of system modes */
#define AANC_LITE_SYSMODE_MASK         0x3

/* Label terminals */
#define AANC_LITE_MIC_INT_TERMINAL_ID  0
#define AANC_LITE_MIC_EXT_TERMINAL_ID  1

#define AANC_LITE_MAX_SOURCES          2
#define AANC_LITE_MAX_SINKS            2
#define AANC_LITE_MIN_VALID_SINKS      ((1 << AANC_LITE_MIC_INT_TERMINAL_ID) | \
                                        (1 << AANC_LITE_MIC_EXT_TERMINAL_ID))

/* Capability minor version */
#define AANC_LITE_CAP_VERSION_MINOR    0

/* Timer parameter is Q12.N */
#define AANC_LITE_TIMER_PARAM_SHIFT    20
/* Max FF fine gain value */
#define AANC_LITE_MAX_GAIN             255
/* Full precision gain is Q9.N */
#define AANC_LITE_GAIN_SHIFT           23

/******************************************************************************
Public Type Declarations
*/

/* AANC Lite operator data */
typedef struct aanc_lite_exop
{
    /* Input buffers: internal mic, external mic */
    tCbuffer *inputs[AANC_LITE_MAX_SINKS];

    /* Output buffers: internal mic, external mic */
    tCbuffer *outputs[AANC_LITE_MAX_SOURCES];

    /* Metadata buffers:  */
    tCbuffer *metadata_ip;
    tCbuffer *metadata_op;

    /* Sample rate & cap id */
    unsigned sample_rate;
    unsigned cap_id;

    /* AANC Lite parameters */
    AANC_LITE_PARAMETERS aanc_lite_cap_params;

    /* Mode control */
    unsigned cur_mode;

    /* Touched terminals */
    uint16 touched_sinks;
    uint16 touched_sources;

    /* Full precision gain value used for gain updates */
    unsigned gain_full_precision;
    /* Rate of gain change */
    unsigned gain_ramp_rate;
    /* Gain ramp duration in frames */
    uint16 gain_ramp_duration;

    /* Current gain */
    uint16 ff_fine_gain;
    /* Previous gain */
    uint16 ff_fine_gain_prev;
    /* Static gain */
    uint16 ff_static_fine_gain;

    /* ANC channel controlled by the capability */
    AANC_ANC_INSTANCE anc_channel;
    AANC_ANC_PATH anc_ff_path;

    /* Reinitialization */
    bool re_init_flag;

    /* Gain ramp init flag */
    bool gain_ramp_init_flag;

    /* Standard CPS object */
    CPS_PARAM_DEF params_def;

} AANC_LITE_OP_DATA;

/******************************************************************************
Private Function Definitions
*/

/* Standard Capability API handlers */
extern bool aanc_lite_create(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *response_id, void **resp_data);
extern bool aanc_lite_start(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *response_id, void **resp_data);
extern bool aanc_lite_reset(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *response_id, void **resp_data);
extern bool aanc_lite_connect(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data);
extern bool aanc_lite_disconnect(OPERATOR_DATA *op_data, void *message_data,
                                 unsigned *response_id, void **resp_data);
extern bool aanc_lite_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *response_id, void **resp_data);
extern bool aanc_lite_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *response_id, void **resp_data);

/* Standard Opmsg handlers */
extern bool aanc_lite_opmsg_set_control(OPERATOR_DATA *op_data,
                                        void *message_data,
                                        unsigned *resp_length,
                                        OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_lite_opmsg_get_params(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_lite_opmsg_get_defaults(OPERATOR_DATA *op_data,
                                         void *message_data,
                                         unsigned *resp_length,
                                         OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_lite_opmsg_set_params(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_lite_opmsg_get_status(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_lite_opmsg_set_ucid(OPERATOR_DATA *op_data,
                                     void *message_data,
                                     unsigned *resp_length,
                                     OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_lite_opmsg_get_ps_id(OPERATOR_DATA *op_data,
                                      void *message_data,
                                      unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Custom OPMSG handlers. Arguments follow standard OPMSG handlers. */

/**
 * \brief  Set the AANC Lite Static Gain
 *
 * \param  op_data  Address of operator data
 * \param  message_data  Address of message data
 * \param  resp_length  Address of the response length
 * \param  resp_data  Pointer to the address of the response data
 *
 * \return  boolean indicating success or failure.
 *
 * Custom OPMSG handler for setting the AANC Lite static gain values.
 */
extern bool aanc_lite_opmsg_set_static_gain(OPERATOR_DATA *op_data,
                                            void *message_data,
                                            unsigned *resp_length,
                                            OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Standard data processing function */
extern void aanc_lite_process_data(OPERATOR_DATA *op_data,
                                   TOUCHED_TERMINALS *touched);

/* Standard parameter function to handle persistence */
extern bool ups_params_aanc_lite(void* instance_data, PS_KEY_TYPE key,
                                 PERSISTENCE_RANK rank, uint16 length,
                                 unsigned* data, STATUS_KYMERA status,
                                 uint16 extra_status_info);

#endif /* _AANC_LITE_CAP_H_ */