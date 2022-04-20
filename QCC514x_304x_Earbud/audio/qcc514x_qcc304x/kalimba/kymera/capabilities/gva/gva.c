/****************************************************************************
 * Copyright (c) 2014 - 2018 Qualcomm Technologies International, Ltd
****************************************************************************/
/**
 * \file  va_google.c
 * \ingroup  capabilities
 *
 *  A capability wrapper for the Google Voice Assistant that makes use of the
 *  common Wake Word Engine capability wrapper class.
 *
 */

#include <string.h>
#include "capabilities.h"
#include "op_msg_helpers.h"
#include "obpm_prim.h"
#include "accmd_prim.h"
#include "ttp/ttp.h"
#include "ttp_utilities.h"
#include "file_mgr_for_ops.h"
#include "opmgr/opmgr_for_ops.h"
#include "cap_id_prim.h"
#include "aov_interface/aov_interface.h"
#include "mem_utils/scratch_memory.h"
#include "ext_buffer/ext_buffer.h"

#include "wwe/wwe_cap.h"
#include "gva.h"
#include "gva_gen_c.h"

#ifdef RUNNING_ON_KALSIM
#include "gva_default_model.h"
#endif


#if (GVA_SYSMODE_STATIC != WWE_SYSMODE_STATIC)
#error GVA SYSMODE STATIC != WWE SYSMODE STATIC
#endif

#if (GVA_SYSMODE_FULL_PROC != WWE_SYSMODE_FULL_PROC)
#error GVA SYSMODE FULL PROC != WWE SYSMODE FULL PROC
#endif

#if (GVA_SYSMODE_PASS_THRU!= WWE_SYSMODE_PASS_THRU)
#error GVA SYSMODE PASS THRU != WWE SYSMODE PASS THRU
#endif

#if (GVA_CONTROL_MODE_OVERRIDE != WWE_CONTROL_MODE_OVERRIDE)
#error GVA CONTROL MODE OVERRIDE != WWE CONTROL MODE OVERRIDE
#endif

/****************************************************************************
Private Function Definitions
*/
static bool va_google_create(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *response_id, void **resp_data);
static bool va_google_destroy(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data);
static bool va_google_reset_engine(OPERATOR_DATA *op_data,
                                   TOUCHED_TERMINALS *touched);
static bool va_google_process_data(OPERATOR_DATA *op_data,
                                   TOUCHED_TERMINALS *touched);
static bool va_google_opmsg_obpm_get_status(OPERATOR_DATA *op_data,
                                            void *message_data,
                                            unsigned *resp_length,
                                            OP_OPMSG_RSP_PAYLOAD **resp_data);


static unsigned int uint32_read(uint8 *bytes, unsigned int *offset);
static bool gva_start_engine(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **response_data);

#ifndef RUNNING_ON_KALSIM
static bool va_google_trigger_phrase_load(OPERATOR_DATA *op_data,
                                          void *message_data,
                                          unsigned *resp_length,
                                          OP_OPMSG_RSP_PAYLOAD **resp_data,
                                          DATA_FILE *trigger_file);
static bool va_google_trigger_phrase_unload(OPERATOR_DATA *op_data,
                                          void *message_data,
                                          unsigned *resp_length,
                                          OP_OPMSG_RSP_PAYLOAD **resp_data,
                                          uint16 file_id);
static bool va_google_trigger_phrase_check(OPERATOR_DATA *op_data,
                                          void *message_data,
                                          unsigned *resp_length,
                                          OP_OPMSG_RSP_PAYLOAD **resp_data,
                                          DATA_FILE *trigger_file);

#else
static bool va_google_static_load(OPERATOR_DATA *op_data, void *message_data,
                                  unsigned *response_id, void **response_data);
#endif /* ifndef RUNNING_ON_KALSIM */

static bool gva_hotword_lib_version_req(OPERATOR_DATA *op_data,
                                            void *message_data,
                                            unsigned *resp_length,
                                            OP_OPMSG_RSP_PAYLOAD **resp_data);

/****************************************************************************
Private Constant Declarations
*/

#define VA_GOOGLE_BUFFER_SIZE 320
#define VA_GOOGLE_BLOCK_SIZE 160
#define VA_GOOGLE_FRAME_SIZE 160
#define VA_GOOGLE_FFT_SIZE 512

#define N_STAT ( sizeof(GVA_STATISTICS) / sizeof(ParamType) )

#ifdef CAPABILITY_DOWNLOAD_BUILD
#define GVA_CAP_ID CAP_ID_DOWNLOAD_GVA
#else
#define GVA_CAP_ID CAP_ID_GVA
#endif /* CAPABILITY_DOWNLOAD_BUILD */


#if defined(CAPABILITY_DOWNLOAD_BUILD)
    #pragma unitsuppress ExplicitQualifier
#endif /* defined(CAPABILITY_DOWNLOAD_BUILD) */

/** The stub capability function handler table */
const handler_lookup_struct va_google_handler_table =
{
    va_google_create,           /* OPCMD_CREATE */
    va_google_destroy,          /* OPCMD_DESTROY */
    wwe_start,                  /* OPCMD_START */
    base_op_stop,               /* OPCMD_STOP */
    base_op_reset,              /* OPCMD_RESET */
    wwe_connect,                /* OPCMD_CONNECT */
    wwe_disconnect,             /* OPCMD_DISCONNECT */
    wwe_buffer_details,         /* OPCMD_BUFFER_DETAILS */
    base_op_get_data_format,    /* OPCMD_DATA_FORMAT */
    wwe_sched_info              /* OPCMD_GET_SCHED_INFO */
};

/* Null terminated operator message handler table - this is the set of operator
 * messages that the capability understands and will attempt to service. */
const opmsg_handler_lookup_table_entry va_google_opmsg_handler_table[] =
{
    {OPMSG_COMMON_ID_GET_CAPABILITY_VERSION, base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_CONTROL,            wwe_opmsg_obpm_set_control},
    {OPMSG_COMMON_ID_GET_STATUS,             va_google_opmsg_obpm_get_status},
    {OPMSG_COMMON_ID_GET_PARAMS,             wwe_opmsg_obpm_get_params},
    {OPMSG_COMMON_ID_GET_DEFAULTS,           wwe_opmsg_obpm_get_defaults},
    {OPMSG_COMMON_ID_SET_PARAMS,             wwe_opmsg_obpm_set_params},
    {OPMSG_COMMON_ID_SET_UCID,               wwe_opmsg_set_ucid},
    {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,      wwe_opmsg_get_ps_id},
    {OPMSG_WWE_ID_RESET_STATUS,              wwe_reset_status},
    {OPMSG_WWE_ID_MODE_CHANGE,               wwe_mode_change},
    {OPMSG_WWE_ID_TRIGGER_PHRASE_LOAD,       wwe_trigger_phrase_load},
    {OPMSG_WWE_ID_TRIGGER_PHRASE_UNLOAD,     wwe_trigger_phrase_unload},
    {OPMSG_COMMON_SET_SAMPLE_RATE,           wwe_set_sample_rate},
    {OPMSG_COMMON_REINIT_ALGORITHM,          wwe_reinit_algorithm},
    {OPMSG_GVA_ID_GET_HOTWORD_LIBRARY_VERSION,  gva_hotword_lib_version_req},
    {0, NULL}
};


/* Capability data - This is the definition of the capability that Opmgr uses to
 * create the capability from. */
const CAPABILITY_DATA gva_cap_data =
{
    GVA_CAP_ID,                     /* Capability ID */
    GVA_GVA_VERSION_MAJOR, 1,       /* Version information - hi and lo parts */
    1, 1,                           /* Max number of sinks/inputs and sources/outputs */
    &va_google_handler_table,       /* Pointer to message handler function table */
    va_google_opmsg_handler_table,  /* Pointer to operator message handler function table */
    wwe_process_data,               /* Pointer to data processing function */
    0,                              /* This would hold processing time information */
    sizeof(VA_GOOGLE_OP_DATA)       /* Size of capability-specific per-instance data */
};

#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_GVA, VA_GOOGLE_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_GVA, VA_GOOGLE_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

const WWE_CAP_VIRTUAL_TABLE gva_vt =
{
    gva_start_engine,               /* Start */
    NULL,                           /* Stop */
    va_google_reset_engine,         /* Reset */
    va_google_process_data,         /* Process */
#ifndef RUNNING_ON_KALSIM
    va_google_trigger_phrase_load,  /* Load */
    va_google_trigger_phrase_check, /* Check */
    va_google_trigger_phrase_unload /* Unload */
#else
    va_google_static_load           /* Static Load */
#endif /* ifndef RUNNING_ON_KALSIM */
};

__asm void alloc_fft_twiddle_factors(unsigned int N)
{
    call $math.fft_twiddle.alloc;
}

__asm void free_fft_twiddle_factors(unsigned int N)
{
    call $math.fft_twiddle.release;
}

static unsigned int uint32_read(uint8 *bytes, unsigned int *offset)
{
    unsigned int ret;
    ret = (((unsigned int)bytes[*offset] << 24) & 0xFF000000U) |
          (((unsigned int)bytes[*offset + 1] << 16) & 0x00FF0000U) |
          (((unsigned int)bytes[*offset + 2] << 8) & 0x0000FF00U) |
          ((unsigned int)bytes[*offset + 3] & 0x000000FFU);
    *offset += 4;
    return ret;
}

/* Accessing the capability-specific per-instance data function */
static inline VA_GOOGLE_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (VA_GOOGLE_OP_DATA *) base_op_get_instance_data(op_data);
}

static inline WWE_COMMON_DATA *get_wwe_data(OPERATOR_DATA *op_data)
{
    VA_GOOGLE_OP_DATA *param_data = base_op_get_instance_data(op_data);
    return &param_data->wwe_class.wwe;
}

static bool gva_start_engine(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **response_data)
{

    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);
    L0_DBG_MSG("VA_GOOGLE: start engine\0");

    if (p_ext_data->memory_handle == NULL)
    {
        if ((p_ext_data->memory_handle = GoogleHotwordDspMultiBankInit(p_ext_data->memory_banks_array, num_memory_banks)) == NULL)
        {
            L0_DBG_MSG("VA_GOOGLE: failed to initialize engine\0");

            pfree(p_ext_data->memory_banks_array[memory_bank_dm1]);
            pfree(p_ext_data->memory_banks_array[memory_bank_dm2]);
            
            for (int i = 0; i < num_memory_banks; i++)
            {
                p_ext_data->memory_banks_array[i] = NULL;
            }

#ifndef RUNNING_ON_KALSIM
            file_mgr_release_file(p_wwe_data->file_id);
#endif /* #ifndef RUNNING_ON_KALSIM */

            p_wwe_data->file_id = 0;
            p_wwe_data->p_model = NULL;
            p_wwe_data->n_model_size = 0;
            return FALSE;
        }
    }
    else
    {
        /* Reset engine when using internal buffer */
        GoogleHotwordDspMultiBankReset(p_ext_data->memory_handle);
    }

    return TRUE;
}

static bool va_google_create(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *response_id, void **resp_data)
{
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);

    p_ext_data->memory_handle = NULL;
    for (int i = 0; i < num_memory_banks; i++)
    {
        p_ext_data->memory_banks_array[i] = NULL;
    }

    if (!wwe_base_class_init(op_data, &p_ext_data->wwe_class, &gva_vt))
    {
        return FALSE;
    }

    if (!wwe_base_create(op_data, message_data, response_id, resp_data,
                         VA_GOOGLE_BLOCK_SIZE, VA_GOOGLE_BUFFER_SIZE,
                         VA_GOOGLE_FRAME_SIZE))
    {
        return FALSE;
    }

    base_op_change_response_status(resp_data, STATUS_CMD_FAILED);

    if (load_gva_handle(&p_ext_data->f_handle) == FALSE)
    {
        return TRUE;
    }

    if (scratch_register())
    {
        if (scratch_reserve(VA_GOOGLE_FRAME_SIZE * sizeof(int16),
                            MALLOC_PREFERENCE_NONE))
        {
            /* Operator creation was succesful, change respone to STATUS_OK*/
            base_op_change_response_status(resp_data, STATUS_OK);
            alloc_fft_twiddle_factors(VA_GOOGLE_FFT_SIZE);

            /* Engine info */
            p_ext_data->hotword_lib_version = GoogleHotwordVersion();

            L0_DBG_MSG1("VA_GOOGLE: created version=%d \0",
                        p_ext_data->hotword_lib_version);

            return TRUE;
        }

        scratch_deregister();
    }

    return TRUE;
}

static bool va_google_destroy(OPERATOR_DATA *op_data, void *message_data,
                              unsigned *response_id, void **resp_data)
{
    /* call base_op destroy that creates and fills response message, too */
    if(!base_op_destroy(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);
    
#if !defined(RUNNING_ON_KALSIM) || !defined(CAPABILITY_DOWNLOAD_BUILD)
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
#endif

    pfree(p_ext_data->memory_banks_array[memory_bank_dm1]);
    pfree(p_ext_data->memory_banks_array[memory_bank_dm2]);
    
    for (int i = 0; i < num_memory_banks; i++)
    {
        p_ext_data->memory_banks_array[i] = NULL;
    }

#ifndef RUNNING_ON_KALSIM
    if (p_wwe_data->file_id)
    {
        file_mgr_release_file(p_wwe_data->file_id);
        p_wwe_data->p_model = NULL;
    }

#else /*#ifndef RUNNING_ON_KALSIM*/

/* No need to free if using default model in downloadable build */
#ifndef CAPABILITY_DOWNLOAD_BUILD
    if (p_wwe_data->file_id == MODEL_ID_DEFAULT)
    {
        pfree(p_wwe_data->p_model);
        p_wwe_data->p_model = NULL;
    }

#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif /* #ifndef RUNNING_ON_KALSIM) */

    free_fft_twiddle_factors(VA_GOOGLE_FFT_SIZE);
    scratch_deregister();

    unload_wwe_handle(p_ext_data->f_handle);
    p_ext_data->f_handle = NULL;

    L0_DBG_MSG("VA_GOOGLE: destroyed\0");

    return TRUE;
}

#ifdef RUNNING_ON_KALSIM
static bool va_google_static_load(OPERATOR_DATA *op_data, void *message_data,
                                  unsigned *response_id, void **response_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

#ifdef CAPABILITY_DOWNLOAD_BUILD
    p_wwe_data->p_model = (uint8* ) &model_raw_gva;

#else /* CAPABILITY_DOWNLOAD_BUILD */
    p_wwe_data->p_model = (uint8* ) xppmalloc(GVA_DEFAULT_MODEL_SIZE * 4,
                                               MALLOC_PREFERENCE_NONE);

    if (p_wwe_data->p_model == NULL)
    {
        L0_DBG_MSG("VA_GOOGLE: Could not allocate memory for default model\0");
        return FALSE;
    }

    /* The GoogleHotwordDspMultiBankInit function that is called in start_engine()
     * will attempt to write data into the model. Since the statically defined model
     * lives in CONST memory, we need to copy it to RAM before we can write into it.
     * Unless ofcourse it's a downloadable build*/
    memcpy(p_wwe_data->p_model, &model_raw_gva, GVA_DEFAULT_MODEL_SIZE * 4 );

#endif /* CAPABILITY_DOWNLOAD_BUILD */

    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (p_ext_data->memory_handle == NULL)
    {
        uint8 *base_addr = (uint8 *)p_wwe_data->p_model;
        unsigned int offset = 0;
        unsigned int var;

        // Header version
        var = uint32_read(base_addr, &offset);
        L0_DBG_MSG1("VA_GOOGLE: model file header ver %u\0", var);

        // Lib version
        var = uint32_read(base_addr, &offset);
        L0_DBG_MSG1("VA_GOOGLE: model file lib ver %d\0", (int)var);
        int expected = GoogleHotwordVersion();
        if ((int)var != expected)
        {
            L1_DBG_MSG2("VA_GOOGLE: model file mismatch? exp %d get %d\0", expected, (int)var);
        }

        // Architecture string length
        var = uint32_read(base_addr, &offset);

        // Architecture string
        char arch[17] = {0};
        memmove(arch, base_addr + offset, var);
        arch[var] = 0;
        offset += 16;
        L0_DBG_MSG1("VA_GOOGLE: model file arch %s\0", arch);

        // mmap count length
        var = uint32_read(base_addr, &offset);

        bool parsed = TRUE;

        for (unsigned int i = 0; i < var && parsed; i++)
        {
            unsigned int type = uint32_read(base_addr, &offset);
            unsigned int data_offset = uint32_read(base_addr, &offset);
            unsigned int data_length = uint32_read(base_addr, &offset);

            switch (type)
            {
                case GSOUND_HOTWORD_MMAP_TEXT:
                {
                    if (p_ext_data->memory_banks_array[memory_bank_ro])
                    {
                        L0_DBG_MSG("VA_GOOGLE: TEXT/RO section already exists!\0");
                        parsed = FALSE;
                    }
                    else
                    {
                        p_ext_data->memory_banks_array[memory_bank_ro] = &base_addr[data_offset];
                    }
                }
                break;

                case GSOUND_HOTWORD_MMAP_DATA:
                {
                    if (p_ext_data->memory_banks_array[memory_bank_dm])
                    {
                        L0_DBG_MSG("VA_GOOGLE: DATA/DM section already exists!\0");
                        parsed = FALSE;
                    }
                    else
                    {
                        p_ext_data->memory_banks_array[memory_bank_dm] = &base_addr[data_offset];
                    }

                }
                break;

                case GSOUND_HOTWORD_MMAP_BSS:
                {
                    if (p_ext_data->memory_banks_array[memory_bank_dm1] == NULL)
                    {
                        p_ext_data->memory_banks_array[memory_bank_dm1] = zppmalloc(data_length, MALLOC_PREFERENCE_DM1);
                    
                        if (p_ext_data->memory_banks_array[memory_bank_dm1] == NULL)
                        {
                            parsed = FALSE;
                        }
                    }
                    else if (p_ext_data->memory_banks_array[memory_bank_dm2] == NULL)
                    {
                        p_ext_data->memory_banks_array[memory_bank_dm2] = zppmalloc(data_length, MALLOC_PREFERENCE_DM2);
                    
                        if (p_ext_data->memory_banks_array[memory_bank_dm2] == NULL)
                        {
                            parsed = FALSE;
                        }
                    }
                    else
                    {
                        L0_DBG_MSG("VA_GOOGLE: BSS/DM sections already exists!\0");
                        parsed = FALSE;
                    }
                }
                break;

                default:
                L0_DBG_MSG1("VA_GOOGLE: unknown type! %d\0", type);
                parsed = FALSE;
                break;
            }
        }

        if (!parsed)
        {

            pfree(p_ext_data->memory_banks_array[memory_bank_dm1]);
            pfree(p_ext_data->memory_banks_array[memory_bank_dm2]);
            
            for (int i = 0; i < num_memory_banks; i++)
            {
                p_ext_data->memory_banks_array[i] = NULL;
            }

            L0_DBG_MSG("VA_GOOGLE: failed to parse model file!\0");
            return FALSE;
        }
    }

    p_wwe_data->file_id = MODEL_ID_DEFAULT;
    p_wwe_data->n_model_size = GVA_DEFAULT_MODEL_SIZE * 4 ; // in bytes

    return TRUE;
}
#endif /* #ifdef RUNNING_ON_KALSIM */

static bool va_google_reset_engine(OPERATOR_DATA *op_data,
                                   TOUCHED_TERMINALS *touched)
{
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);
    GoogleHotwordDspMultiBankReset(p_ext_data->memory_handle);
    return TRUE;
}

/* Data processing function */
static bool va_google_process_data(OPERATOR_DATA *op_data,
                                   TOUCHED_TERMINALS *touched)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);

    int16 *p_buffer = scratch_commit(VA_GOOGLE_FRAME_SIZE * sizeof(int16),
                                     MALLOC_PREFERENCE_NONE);
    cbuffer_copy_and_shift(p_wwe_data->ip_buffer, p_buffer,
                           VA_GOOGLE_FRAME_SIZE);

    /* Process */
    if (p_wwe_data->cur_mode == GVA_SYSMODE_FULL_PROC && p_ext_data->memory_handle != NULL)
    {
        int preamble_length_ms;

        int result = GoogleHotwordDspMultiBankProcessData(p_ext_data->f_handle, p_buffer, VA_GOOGLE_FRAME_SIZE,
                                          &preamble_length_ms, p_ext_data->memory_handle);

        if (result != 0 && result != GVA_ERROR)
        {
            /*keyword has been detected*/

            p_wwe_data->keyword_detected++;
            p_wwe_data->result_confidence = 0;
            unsigned keyword_len = ((preamble_length_ms / 1000) *
                                    p_wwe_data->sample_rate);
            p_wwe_data->keyword_len = keyword_len;
            /* Convert ms to us */
            TIME keyword_duration = (TIME)preamble_length_ms * (TIME)1000ULL;
            if (p_wwe_data->last_timestamp > keyword_duration)
            {
                p_wwe_data->ts.trigger_start_timestamp = time_sub(
                    p_wwe_data->last_timestamp, keyword_duration);
                L0_DBG_MSG1("Preamble length %d (us)\0", keyword_duration);
            }
            else
            {
                L0_DBG_MSG2(
                    "Preamble length %d (us) greater than last timestamp %d\0",
                    keyword_duration, p_wwe_data->last_timestamp);
                p_wwe_data->ts.trigger_start_timestamp = \
                    p_wwe_data->last_timestamp;
            }
            // p_wwe_data->ts.trigger_end_timestamp = p_wwe_data->last_timestamp;

            p_wwe_data->ts.trigger_offset = 0;

            p_wwe_data->init_flag = TRUE;
            p_wwe_data->trigger = TRUE;
        }
    }

    scratch_free();

    return TRUE;
}

/* *********************Operator Message Handle functions ******************* */

bool va_google_opmsg_obpm_get_status(OPERATOR_DATA *op_data,
                                     void *message_data,
                                     unsigned *resp_length,
                                     OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    unsigned *resp = NULL;
    int i;

    if(!common_obpm_status_helper(message_data, resp_length, resp_data,
                                  sizeof(GVA_STATISTICS), &resp))
    {
         return FALSE;
    }

    if (resp)
    {
        GVA_STATISTICS stats;
        GVA_STATISTICS *pstats = &stats;
        ParamType* pparam = (ParamType*)&stats;

        // ch1 statistics
        pstats->OFFSET_GVA_DETECTED = p_wwe_data->keyword_detected;
        pstats->OFFSET_GVA_KEYWORD_LEN = p_wwe_data->keyword_len;

        // other statistics
        pstats->OFFSET_CUR_MODE = p_wwe_data->cur_mode;
        pstats->OFFSET_OVR_CONTROL = p_wwe_data->ovr_control;

        for (i=0; i<N_STAT/2; i++)
        {
            resp = cpsPack2Words(pparam[2*i], pparam[2*i+1], resp);
        }
        if (N_STAT % 2) // last one
        {
            cpsPack1Word(pparam[N_STAT-1], resp);
        }
    }

    return TRUE;
}

#ifndef RUNNING_ON_KALSIM
static bool va_google_trigger_phrase_load(OPERATOR_DATA *op_data,
                                          void *message_data,
                                          unsigned *resp_length,
                                          OP_OPMSG_RSP_PAYLOAD **resp_data,
                                          DATA_FILE *trigger_file)
{
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);

    /* 4 bytes header version (0)
     * 4 bytes lib version
     * 4 bytes arch string length (7)
     * 16 bytes arch string ("kalimba")
     * 4 bytes mmap count (4)
     *
     * For each section
     *  4 bytes type TEXT/RO(0), DATA/RW(1), BSS/DM(2)
     *  4 bytes offset from beginning of the model file to mmap section
     *  4 bytes mmap section length
     */
    uint8 *base_addr = (uint8 *)trigger_file->u.file_data->base_addr;
    unsigned int offset = 0;
    unsigned int var;

    // Header version
    var = uint32_read(base_addr, &offset);
    L0_DBG_MSG1("VA_GOOGLE: model file header ver %u\0", var);

    // Lib version
    var = uint32_read(base_addr, &offset);
    L0_DBG_MSG1("VA_GOOGLE: model file lib ver %d\0", (int)var);
    int expected = GoogleHotwordVersion();
    if ((int)var != expected)
    {
        L1_DBG_MSG2("VA_GOOGLE: model file mismatch? exp %d get %d\0", expected, (int)var);
    }

    // Architecture string length
    var = uint32_read(base_addr, &offset);

    // Architecture string
    char arch[17] = {0};
    memmove(arch, base_addr + offset, var);
    arch[var] = 0;
    offset += 16;
    L0_DBG_MSG1("VA_GOOGLE: model file arch %s\0", arch);

    // mmap count length
    var = uint32_read(base_addr, &offset);

    bool parsed = TRUE;

    for (unsigned int i = 0; i < var && parsed; i++)
    {
        unsigned int type = uint32_read(base_addr, &offset);
        unsigned int data_offset = uint32_read(base_addr, &offset);
        unsigned int data_length = uint32_read(base_addr, &offset);

        switch (type)
        {
            case GSOUND_HOTWORD_MMAP_TEXT:
            {
                if (p_ext_data->memory_banks_array[memory_bank_ro])
                {
                    L0_DBG_MSG("VA_GOOGLE: TEXT/RO section already exists!\0");
                    parsed = FALSE;
                }
                else
                {
                    p_ext_data->memory_banks_array[memory_bank_ro] = &base_addr[data_offset];
                }
            }
            break;

            case GSOUND_HOTWORD_MMAP_DATA:
            {
                if (p_ext_data->memory_banks_array[memory_bank_dm])
                {
                    L0_DBG_MSG("VA_GOOGLE: DATA/DM section already exists!\0");
                    parsed = FALSE;
                }
                else
                {
                    p_ext_data->memory_banks_array[memory_bank_dm] = &base_addr[data_offset];
                }

            }
            break;

            case GSOUND_HOTWORD_MMAP_BSS:
            {
                if (p_ext_data->memory_banks_array[memory_bank_dm1] == NULL)
                {
                    p_ext_data->memory_banks_array[memory_bank_dm1] = zppmalloc(data_length, MALLOC_PREFERENCE_DM1);
                    
                    if (p_ext_data->memory_banks_array[memory_bank_dm1] == NULL)
                    {
                        parsed = FALSE;
                    }
                }
                else if (p_ext_data->memory_banks_array[memory_bank_dm2] == NULL)
                {
                    p_ext_data->memory_banks_array[memory_bank_dm2] = zppmalloc(data_length, MALLOC_PREFERENCE_DM2);
                    
                    if (p_ext_data->memory_banks_array[memory_bank_dm2] == NULL)
                    {
                        parsed = FALSE;
                    }
                }
                else
                {
                    L0_DBG_MSG("VA_GOOGLE: BSS/DM sections already exists!\0");
                    parsed = FALSE;
                }
            }
            break;

            default:
            L0_DBG_MSG1("VA_GOOGLE: unknown type! %d\0", type);
            parsed = FALSE;
            break;
        }
    }

    if (!parsed)
    {
        pfree(p_ext_data->memory_banks_array[memory_bank_dm1]);
        pfree(p_ext_data->memory_banks_array[memory_bank_dm2]);

        for (int i = 0; i < num_memory_banks; i++)
        {
            p_ext_data->memory_banks_array[i] = NULL;
        }

        L0_DBG_MSG("VA_GOOGLE: failed to parse model file!\0");
        return FALSE;
    }

    L0_DBG_MSG("VA_GOOGLE: model data in internal buffer loaded\0");

    p_wwe_data->p_model = p_ext_data->memory_banks_array[memory_bank_ro];
    p_wwe_data->n_model_size = cbuffer_calc_amount_data_in_words(
                trigger_file->u.file_data) * 4;;

    return TRUE;
}

static bool va_google_trigger_phrase_check(OPERATOR_DATA *op_data,
                                          void *message_data,
                                          unsigned *resp_length,
                                          OP_OPMSG_RSP_PAYLOAD **resp_data,
                                          DATA_FILE *trigger_file)
{
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);

    /* Check model */
    p_ext_data->memory_handle = GoogleHotwordDspMultiBankInit(p_ext_data->memory_banks_array, num_memory_banks);
    if (p_ext_data->memory_handle == NULL)
    {
        L0_DBG_MSG("VA_GOOGLE: failed to initialize engine\0");

        pfree(p_ext_data->memory_banks_array[memory_bank_dm1]);
        pfree(p_ext_data->memory_banks_array[memory_bank_dm2]);
        
        for (int i = 0; i < num_memory_banks; i++)
        {
            p_ext_data->memory_banks_array[i] = NULL;
        }

        file_mgr_release_file(p_wwe_data->file_id);
        p_wwe_data->p_model = NULL;
        p_wwe_data->n_model_size = 0;
        p_wwe_data->file_id = 0;
        return FALSE;
    }

    L0_DBG_MSG1("VA_GOOGLE: model loaded size=%d\0", p_wwe_data->n_model_size);

    return TRUE;
}

static bool va_google_trigger_phrase_unload(OPERATOR_DATA *op_data,
                                                void *message_data,
                                                unsigned *resp_length,
                                                OP_OPMSG_RSP_PAYLOAD **resp_data,
                                                uint16 file_id)
{

    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);
    WWE_COMMON_DATA *p_wwe_data = get_wwe_data(op_data);


    pfree(p_ext_data->memory_banks_array[memory_bank_dm1]);  
    pfree(p_ext_data->memory_banks_array[memory_bank_dm2]);

    for (int i = 0; i < num_memory_banks; i++)
    {
        p_ext_data->memory_banks_array[i] = NULL;
    }

    file_mgr_release_file(p_wwe_data->file_id);

    p_wwe_data->file_id = 0;

    p_wwe_data->n_model_size = 0;
    p_wwe_data->p_model = NULL;

    L0_DBG_MSG("GVA: model unloaded\0");

    return TRUE;
}

#endif /* #ifndef RUNNING_ON_KALSIM */

static bool gva_hotword_lib_version_req(OPERATOR_DATA *op_data,
                                            void *message_data,
                                            unsigned *resp_length,
                                            OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    VA_GOOGLE_OP_DATA *p_ext_data = get_instance_data(op_data);
    
    uint16 lsw_verion_number = (uint16)(0x0000FFFF & p_ext_data->hotword_lib_version);
    uint16 msw_verion_number = (uint16)(0x0000FFFF & (p_ext_data->hotword_lib_version >> 16));
    
    *resp_length = OPMSG_RSP_PAYLOAD_SIZE_RAW_DATA(2);
    *resp_data = (OP_OPMSG_RSP_PAYLOAD *) xzpnew(OPMSG_GVA_GET_HOTWORD_LIBRARY_VERSION_RESP);
    if (*resp_data == NULL)
    {
        return FALSE;
    }
    /* echo the opmsgID/keyID */
    (*resp_data)->msg_id = OPMGR_GET_OPCMD_MESSAGE_MSG_ID((OPMSG_HEADER*)message_data);

    (*resp_data)->u.raw_data[0] = msw_verion_number;
    (*resp_data)->u.raw_data[1] = lsw_verion_number;

    return TRUE;
}
