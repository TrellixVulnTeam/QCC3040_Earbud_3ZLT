/****************************************************************************
 * Copyright (c) 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup Earbud Fit Test
 * \ingroup capabilities
 * \file  earbud_fit_test_cap.h
 * \ingroup Earbud Fit Test
 *
 * Earbud Fit Test operator private header file.
 *
 */

#ifndef _EARBUD_FIT_TEST_CAP_H_
#define _EARBUD_FIT_TEST_CAP_H_

/******************************************************************************
Include Files
*/

#include "capabilities.h"

/* Imports scratch memory definitions */
#include "mem_utils/scratch_memory.h"

/* Math FFT interface */
#include "aanc_afb_twiddle_alloc_c_stubs.h"

#include "earbud_fit_test.h"
#include "earbud_fit_test_gen_c.h"
#include "earbud_fit_test_defs.h"

/******************************************************************************
Private Constant Definitions
*/
/* Number of statistics reported by the capability */
#define EFT_N_STAT               (sizeof(EARBUD_FIT_TEST_STATISTICS)/sizeof(ParamType))

/* Mask for the number of system modes */
#define EFT_SYSMODE_MASK         0x3

/* Mask for override control word */
#define EFT_OVERRIDE_MODE_MASK        (0xFFFF ^ EARBUD_FIT_TEST_CONTROL_MODE_OVERRIDE)

/* Label terminals */
#define EFT_PLAYBACK_TERMINAL_ID  0
#define EFT_MIC_INT_TERMINAL_ID   1

/* Label metadata channels */
#define EFT_METADATA_PLAYBACK_ID       0
#define EFT_METADATA_INT_ID            1
#define EFT_NUM_METADATA_CHANNELS      2

#define EFT_MAX_SINKS             2
#define EFT_MIN_VALID_SINKS      ((1 << EFT_PLAYBACK_TERMINAL_ID) | \
                                  (1 << EFT_MIC_INT_TERMINAL_ID))

/* Label in/out of ear states */
#define EFT_IN_EAR                     TRUE
#define EFT_OUT_EAR                    FALSE

/* Capability minor version */
#define EARBUD_FIT_TEST_CAP_VERSION_MINOR    0

/* Timer parameter is Q12.N */
#define EFT_TIMER_PARAM_SHIFT    20

/* Event IDs */
#define EFT_EVENT_ID_FIT          0

/* Event payloads */
#define EFT_EVENT_PAYLOAD_BAD     0
#define EFT_EVENT_PAYLOAD_GOOD    1
#define EFT_EVENT_PAYLOAD_UNUSED  2

/******************************************************************************
Public Type Declarations
*/

/* Represent the state of an EFT event */
typedef enum
{
    EFT_EVENT_CLEAR,
    EFT_EVENT_DETECTED,
    EFT_EVENT_SENT
} EFT_EVENT_STATE;

/* Represent EFT event messaging states */
typedef struct _EFT_EVENT
{
    unsigned frame_counter;
    unsigned set_frames;
    EFT_EVENT_STATE running;
} EFT_EVENT;

/* Earbud Fit Test operator data */
typedef struct eft_exop
{
    /* Input buffers: internal mic, external mic */
    tCbuffer *inputs[EFT_MAX_SINKS];

    /* Metadata input buffers:  */
    tCbuffer *metadata_ip[EFT_NUM_METADATA_CHANNELS];

    tCbuffer *p_tmp_ref_ip;              /* Pointer to temp ref mic ip */
    tCbuffer *p_tmp_int_ip;              /* Pointer to temp int mic ip */

    /* Sample rate & cap id */
    unsigned sample_rate;
    CAP_ID cap_id;

    /* Earbud Fit Test parameters */
    EARBUD_FIT_TEST_PARAMETERS eft_cap_params;

    /* Mode control */
    unsigned cur_mode;
    unsigned ovr_control;
    unsigned host_mode;
    unsigned qact_mode;

    /* In/Our of ear status */
    bool in_out_status:8;

    /* Fit quality flag */
    bool fit_quality:8;

    /* Previous Fit quality flag */
    bool prev_fit_quality:8;

    /* Reinitialization */
    bool re_init_flag:8;

    /* Standard CPS object */
    CPS_PARAM_DEF params_def;

    /* Fit detect event */
    EFT_EVENT fit_event_detect;

    AANC_AFB *p_afb_ref;                 /* Pointer to reference AFB */
    AANC_AFB *p_afb_int;                 /* Pointer to internal mic AFB */
    FIT100   *p_fit;                     /* Pointer to fit object */

    void *f_handle;                      /* Pointer to feature handle */

    bool scratch_registered:8;           /* Track scratch registration */
    bool twiddle_registered:8;           /* Track FFT twiddle registration */

} EFT_OP_DATA;

/******************************************************************************
Private Function Definitions
*/

/* Standard Capability API handlers */
extern bool eft_create(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *response_id, void **resp_data);
extern bool eft_destroy(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **resp_data);
extern bool eft_start(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *response_id, void **resp_data);
extern bool eft_reset(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *response_id, void **resp_data);
extern bool eft_connect(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data);
extern bool eft_disconnect(OPERATOR_DATA *op_data, void *message_data,
                                 unsigned *response_id, void **resp_data);
extern bool eft_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *response_id, void **resp_data);
extern bool eft_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *response_id, void **resp_data);

/* Standard Opmsg handlers */
extern bool eft_opmsg_set_control(OPERATOR_DATA *op_data,
                                        void *message_data,
                                        unsigned *resp_length,
                                        OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool eft_opmsg_get_params(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool eft_opmsg_get_defaults(OPERATOR_DATA *op_data,
                                         void *message_data,
                                         unsigned *resp_length,
                                         OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool eft_opmsg_set_params(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool eft_opmsg_get_status(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool eft_opmsg_set_ucid(OPERATOR_DATA *op_data,
                                     void *message_data,
                                     unsigned *resp_length,
                                     OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool eft_opmsg_get_ps_id(OPERATOR_DATA *op_data,
                                      void *message_data,
                                      unsigned *resp_length,
                                      OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Standard data processing function */
extern void eft_process_data(OPERATOR_DATA *op_data,
                                   TOUCHED_TERMINALS *touched);

/* Standard parameter function to handle persistence */
extern bool ups_params_eft(void* instance_data, PS_KEY_TYPE key,
                                 PERSISTENCE_RANK rank, uint16 length,
                                 unsigned* data, STATUS_KYMERA status,
                                 uint16 extra_status_info);

#endif /* _EARBUD_FIT_TEST_CAP_H_ */