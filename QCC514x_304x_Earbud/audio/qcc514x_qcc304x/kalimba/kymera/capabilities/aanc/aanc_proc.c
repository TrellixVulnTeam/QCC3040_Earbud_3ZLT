/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 *
 * \ingroup capabilities
 * \file  aanc_proc.c
 * \ingroup AANC
 *
 * AANC processing library.
 */

#include "aanc_proc.h"

/******************************************************************************
Private Function Definitions
*/

/**
 * \brief  Create cbuffers with a particular malloc preference.
 *
 * \param  pp_buffer  Address of the pointer to Cbuffer to be allocated.
 * \param  malloc_pref  Malloc preference.
 *
 * \return  boolean indicating success or failure.
 */
static bool aanc_proc_create_cbuffer(tCbuffer **pp_buffer, unsigned malloc_pref)
{
    /* Allocate buffer memory explicitly */
    int *ptr = xzppnewn(AANC_INTERNAL_BUFFER_SIZE, int, malloc_pref);

    if (ptr == NULL)
    {
        return FALSE;
    }

    /* Wrap allocated memory in a cbuffer */
    *pp_buffer = cbuffer_create(ptr, AANC_INTERNAL_BUFFER_SIZE,
                               BUF_DESC_SW_BUFFER);
    if (*pp_buffer == NULL)
    {
        pdelete(ptr);
        ptr = NULL;

        return FALSE;
    }

    return TRUE;
}

/**
 * \brief  Process a clip detection.
 *
 * \param  p_clip  Pointer to the clip struct.
 *
 * This monitors the frame detection and allows a counter to hold a detection
 * for a given duration (frames).
 */
static void aanc_proc_process_clip_detect(AANC_CLIP_DETECT *p_clip)
{
    /* Disabled resets the flag */
    if (p_clip->disabled)
    {
        p_clip->detected = FALSE;
    }
    else
    {
        /* Detection sets the flag and resets the counter */
        if (p_clip->frame_detect)
        {
            p_clip->counter = p_clip->duration;
            p_clip->detected = TRUE;
        }
        else
        {
            /* No detection decrements the counter until 0 */
            if (p_clip->counter > 0)
            {
                p_clip->counter--;
            }
            else
            {
                p_clip->detected = FALSE;
            }
        }
    }
}

/**
 * \brief  Initialize a clip detection struct.
 *
 * \param  p_clip  Pointer to the clip struct.
 * \param  duration  Duration of the clip detection (seconds, Q12.N)
 *
 * This monitors the frame detection and allows a counter to hold a detection
 * for a given duration (frames).
 */
static void aanc_proc_initialize_clip_detect(AANC_CLIP_DETECT *p_clip,
                                             unsigned duration)
{
    /* Convert duration in seconds to frames */
    p_clip->duration = (uint16)((duration * AANC_FRAME_RATE) >> 20);
    p_clip->counter = 0;
    p_clip->detected = FALSE;
}

/******************************************************************************
Public Function Implementations
*/

bool aanc_proc_create(ADAPTIVE_GAIN **pp_ag, unsigned sample_rate)
{

    ADAPTIVE_GAIN *p_ag = xzpnew(ADAPTIVE_GAIN);
    uint16 fxlms_dmx_words, fxlms_dm_words;
    uint16 ed_dmx_words, ed_dm1_words;
    FXLMS100_FILTER_COEFFS* coeffs;

    if (p_ag == NULL)
    {
        *pp_ag = NULL;
        L2_DBG_MSG("AANC_PROC failed to allocate adaptive gain");
        return FALSE;
    }

    *pp_ag = p_ag;

    p_ag->p_aanc_reinit_flag = NULL;

    /* Allocate internal input cbuffer in DM1 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_int_ip, MALLOC_PREFERENCE_DM1))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate int mic input buffer");
        return FALSE;
    }

    /* Allocate external input cbuffer in DM2 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_ext_ip, MALLOC_PREFERENCE_DM2))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate ext mic input buffer");
        return FALSE;
    }

    /* Allocate int mic output cbuffer in DM2 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_int_op, MALLOC_PREFERENCE_DM2))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate int mic output buffer");
        return FALSE;
    }

    /* Allocate ext mic output cbuffer in DM2 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_ext_op, MALLOC_PREFERENCE_DM2))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate ext mic output buffer");
        return FALSE;
    }

    /* Create playback cbuffer without specific bank allocation */
    p_ag->p_tmp_pb_ip = cbuffer_create_with_malloc(AANC_INTERNAL_BUFFER_SIZE,
                                                   BUF_DESC_SW_BUFFER);
    if (p_ag->p_tmp_pb_ip == NULL)
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate playback cbuffer");
        return FALSE;
    }

    /* Register and reserve scratch memory */
    if (!scratch_register())
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to register scratch memory");
        return FALSE;
    }
    p_ag->scratch_registered = TRUE;

    if (!scratch_reserve(FXLMS100_SCRATCH_MEMORY, MALLOC_PREFERENCE_NONE) ||
        !scratch_reserve(FXLMS100_SCRATCH_MEMORY, MALLOC_PREFERENCE_NONE))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate fxlms scratch memory");
        return FALSE;
    }

    /* Allocate FxLMS and ED100 memory using the mem_table API */
    fxlms_dmx_words = (uint16)(aanc_fxlms100_dmx_bytes()/sizeof(unsigned));
    fxlms_dm_words = (uint16)(AANC_PROC_FXLMS_DM_BYTES/sizeof(unsigned));
    ed_dmx_words = (uint16)(aanc_ed100_dmx_bytes()/sizeof(unsigned));
    ed_dm1_words = (uint16)(aanc_ed100_dm1_bytes()/sizeof(unsigned));

    p_ag->p_table = xzpnewn(AANC_PROC_MEM_TABLE_SIZE, malloc_t_entry);
    p_ag->p_table[0] = (malloc_t_entry){
        fxlms_dmx_words, MALLOC_PREFERENCE_NONE,
        offsetof(ADAPTIVE_GAIN, p_fxlms)};
    p_ag->p_table[1] = (malloc_t_entry){
        fxlms_dm_words, MALLOC_PREFERENCE_DM1,
        offsetof(ADAPTIVE_GAIN, p_fxlms_dm1)};
    p_ag->p_table[2] = (malloc_t_entry){
        fxlms_dm_words, MALLOC_PREFERENCE_DM2,
        offsetof(ADAPTIVE_GAIN, p_fxlms_dm2)};
    p_ag->p_table[3] = (malloc_t_entry){
        ed_dmx_words, MALLOC_PREFERENCE_NONE,
        offsetof(ADAPTIVE_GAIN, p_ed_int)};
    p_ag->p_table[4] = (malloc_t_entry){
        ed_dm1_words, MALLOC_PREFERENCE_DM1,
        offsetof(ADAPTIVE_GAIN, p_ed_int_dm1)};
    p_ag->p_table[5] = (malloc_t_entry){
        ed_dmx_words, MALLOC_PREFERENCE_NONE,
        offsetof(ADAPTIVE_GAIN, p_ed_ext)};
    p_ag->p_table[6] = (malloc_t_entry){
        ed_dm1_words, MALLOC_PREFERENCE_DM1,
        offsetof(ADAPTIVE_GAIN, p_ed_ext_dm1)};
    p_ag->p_table[7] = (malloc_t_entry){
        ed_dmx_words, MALLOC_PREFERENCE_NONE,
        offsetof(ADAPTIVE_GAIN, p_ed_pb)};
    p_ag->p_table[8] = (malloc_t_entry){
        ed_dm1_words, MALLOC_PREFERENCE_DM1,
        offsetof(ADAPTIVE_GAIN, p_ed_pb_dm1)};

    if (!mem_table_zalloc((void *)p_ag, p_ag->p_table, AANC_PROC_MEM_TABLE_SIZE))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate memory");
        return FALSE;
    }

    /* Create shared ED cbuffer without specific bank allocation */
    p_ag->p_tmp_ed = cbuffer_create_with_malloc(ED100_DEFAULT_BUFFER_SIZE,
                                                BUF_DESC_SW_BUFFER);
    if (p_ag->p_tmp_ed == NULL)
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate ED cbuffer");
        return FALSE;
    }

    aanc_ed100_create(p_ag->p_ed_int, p_ag->p_ed_int_dm1, sample_rate);
    aanc_ed100_create(p_ag->p_ed_ext, p_ag->p_ed_ext_dm1, sample_rate);
    aanc_ed100_create(p_ag->p_ed_pb, p_ag->p_ed_pb_dm1, sample_rate);

    /* Initialize number of taps to allow correct buffer alignment in create */
    p_ag->p_fxlms->p_plant.num_coeffs = AANC_PROC_NUM_TAPS_PLANT;
    p_ag->p_fxlms->p_plant.full_num_coeffs = AANC_PROC_NUM_TAPS_PLANT;
    p_ag->p_fxlms->p_control_0.num_coeffs = AANC_PROC_NUM_TAPS_CONTROL;
    p_ag->p_fxlms->p_control_0.full_num_coeffs = AANC_PROC_NUM_TAPS_CONTROL;
    p_ag->p_fxlms->p_control_1.num_coeffs = AANC_PROC_NUM_TAPS_CONTROL;
    p_ag->p_fxlms->p_control_1.full_num_coeffs = AANC_PROC_NUM_TAPS_CONTROL;
    p_ag->p_fxlms->p_bp_int.num_coeffs = AANC_PROC_NUM_TAPS_BP;
    p_ag->p_fxlms->p_bp_int.full_num_coeffs = AANC_PROC_NUM_TAPS_BP;
    p_ag->p_fxlms->p_bp_ext.num_coeffs = AANC_PROC_NUM_TAPS_BP;
    p_ag->p_fxlms->p_bp_ext.full_num_coeffs = AANC_PROC_NUM_TAPS_BP;

    aanc_fxlms100_create(p_ag->p_fxlms, p_ag->p_fxlms_dm1, p_ag->p_fxlms_dm2);

    /* Initialize plant model as pass-through */
    coeffs = &p_ag->p_fxlms->p_plant.coeffs;
    coeffs->p_num[0] = FXLMS100_MODEL_COEFF0;
    coeffs->p_den[0] = FXLMS100_MODEL_COEFF0;

    /* Initialize control 0 model as pass-through */
    coeffs = &p_ag->p_fxlms->p_control_0.coeffs;
    coeffs->p_num[0] = FXLMS100_MODEL_COEFF0;
    coeffs->p_den[0] = FXLMS100_MODEL_COEFF0;

    p_ag->clip_threshold = AANC_PROC_CLIPPING_THRESHOLD;

    if (!load_aanc_handle(&p_ag->f_handle))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to load feature handle");
        return FALSE;
    }

    return TRUE;
}

bool aanc_proc_destroy(ADAPTIVE_GAIN **pp_ag)
{
    ADAPTIVE_GAIN *p_ag = *pp_ag;

    if (p_ag == NULL)
    {
        return TRUE;
    }

    if (p_ag->scratch_registered)
    {
        scratch_deregister();
        p_ag->scratch_registered = FALSE;
    }

    aanc_ed100_destroy(p_ag->p_ed_int);
    aanc_ed100_destroy(p_ag->p_ed_ext);
    aanc_ed100_destroy(p_ag->p_ed_pb);

    if (p_ag->p_table != NULL)
    {
        mem_table_free((void *)p_ag, p_ag->p_table, AANC_PROC_MEM_TABLE_SIZE);
        pdelete(p_ag->p_table);
    }

    cbuffer_destroy(p_ag->p_tmp_ed);

    cbuffer_destroy(p_ag->p_tmp_int_ip);
    cbuffer_destroy(p_ag->p_tmp_ext_ip);
    cbuffer_destroy(p_ag->p_tmp_pb_ip);

    cbuffer_destroy(p_ag->p_tmp_int_op);
    cbuffer_destroy(p_ag->p_tmp_ext_op);

    unload_aanc_handle(p_ag->f_handle);

    pdelete(p_ag);
    *pp_ag = NULL;

    return TRUE;
}

bool aanc_proc_initialize(AANC_PARAMETERS *p_params, ADAPTIVE_GAIN *p_ag,
                          unsigned ag_start, unsigned *p_flags,
                          bool hard_initialize)
{
    /* Loop counter */
    int i;
    AANC_PARAMETERS *p_params_tmp;
    FXLMS100_DMX *p_dmx;
    bool ext_ed_disable_e_filter_check;
    bool int_ed_disable_e_filter_check;
    bool pb_ed_disable_e_filter_check;

    /* Initialize pointers to parameters and flags */
    p_ag->p_aanc_params = p_params;
    p_ag->p_aanc_flags = p_flags;

    /**************************************************
     * Initialize the FXLMS                           *
     **************************************************/
    p_dmx = p_ag->p_fxlms;

    /* Initialize buffer pointers */
    p_dmx->p_int_ip = p_ag->p_tmp_int_ip;
    p_dmx->p_int_op = p_ag->p_tmp_int_op;
    p_dmx->p_ext_ip = p_ag->p_tmp_ext_ip;
    p_dmx->p_ext_op = p_ag->p_tmp_ext_op;

    /* Set FxLMS parameters */
    p_dmx->target_nr = p_params->OFFSET_TARGET_NOISE_REDUCTION;
    p_dmx->mu = p_params->OFFSET_MU;
    p_dmx->gamma = p_params->OFFSET_GAMMA;
    p_dmx->lambda = p_params->OFFSET_LAMBDA;
    p_dmx->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_dmx->min_bound = p_params->OFFSET_FXLMS_MIN_BOUND;
    p_dmx->max_bound = p_params->OFFSET_FXLMS_MAX_BOUND;
    p_dmx->max_delta = p_params->OFFSET_FXLMS_MAX_DELTA;

    /* Optimization to reduce the effective number of taps in plant and control
     * filters if there are both trailing numerator and denominator coefficients
     */
    p_params_tmp = p_ag->p_aanc_params;
    if ((p_params_tmp->OFFSET_AANC_DEBUG &
         AANC_CONFIG_AANC_DEBUG_DISABLE_FILTER_OPTIM) > 0)
    {
        p_dmx->p_plant.num_coeffs = AANC_PROC_NUM_TAPS_PLANT;
        p_dmx->p_control_0.num_coeffs = AANC_PROC_NUM_TAPS_CONTROL;
        p_dmx->p_control_1.num_coeffs = AANC_PROC_NUM_TAPS_CONTROL;
        L4_DBG_MSG("AANC_PROC filters set to default number of coefficients");
    }
    else
    {
        p_dmx->p_plant.num_coeffs = aanc_fxlms100_calculate_num_coeffs(
            &p_dmx->p_plant, AANC_PROC_NUM_TAPS_PLANT);
        p_dmx->p_control_0.num_coeffs = aanc_fxlms100_calculate_num_coeffs(
            &p_dmx->p_control_0, AANC_PROC_NUM_TAPS_CONTROL);
        p_dmx->p_control_1.num_coeffs = aanc_fxlms100_calculate_num_coeffs(
            &p_dmx->p_control_1, AANC_PROC_NUM_TAPS_CONTROL);

        L4_DBG_MSG3(
            "AANC_PROC filter coeffs: Plant=%hu, Control 0=%hu, Control 1=%hu",
            p_dmx->p_plant.num_coeffs, p_dmx->p_control_0.num_coeffs,
            p_dmx->p_control_1.num_coeffs);
    }

    if (hard_initialize)
    {
        p_dmx->initial_gain = ag_start;
    }

    /* Initialize FxLMS bandpass model */
    p_params_tmp = p_ag->p_aanc_params;
    int bp_num_coeffs_int[] = {
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_INT_0,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_INT_1,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_INT_2,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_INT_3,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_INT_4
    };

    int bp_den_coeffs_int[] = {
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_INT_0,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_INT_1,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_INT_2,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_INT_3,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_INT_4
    };

    int bp_num_coeffs_ext[] = {
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_EXT_0,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_EXT_1,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_EXT_2,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_EXT_3,
        p_params_tmp->OFFSET_BPF_NUMERATOR_COEFF_EXT_4
    };

    int bp_den_coeffs_ext[] = {
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_EXT_0,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_EXT_1,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_EXT_2,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_EXT_3,
        p_params_tmp->OFFSET_BPF_DENOMINATOR_COEFF_EXT_4
    };

    for (i = 0; i < p_dmx->p_bp_ext.num_coeffs; i++)
    {
        p_dmx->p_bp_ext.coeffs.p_num[i] = bp_num_coeffs_ext[i];
        p_dmx->p_bp_ext.coeffs.p_den[i] = bp_den_coeffs_ext[i];
        p_dmx->p_bp_int.coeffs.p_num[i] = bp_num_coeffs_int[i];
        p_dmx->p_bp_int.coeffs.p_den[i] = bp_den_coeffs_int[i];
    }

    aanc_fxlms100_initialize(p_ag->f_handle, p_ag->p_fxlms, hard_initialize);

    /**************************************************
     * Initialize the EDs                             *
     **************************************************/
    ext_ed_disable_e_filter_check = FALSE;
    p_params_tmp = p_ag->p_aanc_params;
    if (p_params_tmp->OFFSET_AANC_DEBUG & \
        AANC_CONFIG_AANC_DEBUG_DISABLE_ED_EXT_E_FILTER_CHECK)
    {
        ext_ed_disable_e_filter_check = TRUE;
    }
    int_ed_disable_e_filter_check = FALSE;
    if (p_params_tmp->OFFSET_AANC_DEBUG & \
        AANC_CONFIG_AANC_DEBUG_DISABLE_ED_INT_E_FILTER_CHECK)
    {
        int_ed_disable_e_filter_check = TRUE;
    }
    pb_ed_disable_e_filter_check = FALSE;
    if (p_params_tmp->OFFSET_AANC_DEBUG & \
        AANC_CONFIG_AANC_DEBUG_DISABLE_ED_PB_E_FILTER_CHECK)
    {
        pb_ed_disable_e_filter_check = TRUE;
    }

    p_ag->p_ed_int->p_input = p_ag->p_tmp_int_ip;
    p_ag->p_ed_int->p_tmp = p_ag->p_tmp_ed;
    p_ag->p_ed_int->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_ag->p_ed_int->attack_time = p_params_tmp->OFFSET_ED_INT_ATTACK;
    p_ag->p_ed_int->decay_time = p_params_tmp->OFFSET_ED_INT_DECAY;
    p_ag->p_ed_int->envelope_time = p_params_tmp->OFFSET_ED_INT_ENVELOPE;
    p_ag->p_ed_int->init_frame_time = p_params_tmp->OFFSET_ED_INT_INIT_FRAME;
    p_ag->p_ed_int->ratio = p_params_tmp->OFFSET_ED_INT_RATIO;
    p_ag->p_ed_int->min_signal = p_params_tmp->OFFSET_ED_INT_MIN_SIGNAL;
    p_ag->p_ed_int->min_max_envelope = p_params_tmp->OFFSET_ED_INT_MIN_MAX_ENVELOPE;
    p_ag->p_ed_int->delta_th = p_params_tmp->OFFSET_ED_INT_DELTA_TH;
    p_ag->p_ed_int->count_th = p_params_tmp->OFFSET_ED_INT_COUNT_TH;
    p_ag->p_ed_int->hold_frames = p_params_tmp->OFFSET_ED_INT_HOLD_FRAMES;
    p_ag->p_ed_int->e_min_threshold = p_params_tmp->OFFSET_ED_INT_E_FILTER_MIN_THRESHOLD;
    p_ag->p_ed_int->e_min_counter_threshold = p_params_tmp->OFFSET_ED_INT_E_FILTER_MIN_COUNTER_THRESHOLD;
    p_ag->p_ed_int->e_min_check_disabled = int_ed_disable_e_filter_check;
    aanc_ed100_initialize(p_ag->f_handle, p_ag->p_ed_int);

    p_ag->p_ed_ext->p_input = p_ag->p_tmp_ext_ip;
    p_ag->p_ed_ext->p_tmp = p_ag->p_tmp_ed;
    p_ag->p_ed_ext->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_ag->p_ed_ext->attack_time = p_params_tmp->OFFSET_ED_EXT_ATTACK;
    p_ag->p_ed_ext->decay_time = p_params_tmp->OFFSET_ED_EXT_DECAY;
    p_ag->p_ed_ext->envelope_time = p_params_tmp->OFFSET_ED_EXT_ENVELOPE;
    p_ag->p_ed_ext->init_frame_time = p_params_tmp->OFFSET_ED_EXT_INIT_FRAME;
    p_ag->p_ed_ext->ratio = p_params_tmp->OFFSET_ED_EXT_RATIO;
    p_ag->p_ed_ext->min_signal = p_params_tmp->OFFSET_ED_EXT_MIN_SIGNAL;
    p_ag->p_ed_ext->min_max_envelope = p_params_tmp->OFFSET_ED_EXT_MIN_MAX_ENVELOPE;
    p_ag->p_ed_ext->delta_th = p_params_tmp->OFFSET_ED_EXT_DELTA_TH;
    p_ag->p_ed_ext->count_th = p_params_tmp->OFFSET_ED_EXT_COUNT_TH;
    p_ag->p_ed_ext->hold_frames = p_params_tmp->OFFSET_ED_EXT_HOLD_FRAMES;
    p_ag->p_ed_ext->e_min_threshold = p_params_tmp->OFFSET_ED_EXT_E_FILTER_MIN_THRESHOLD;
    p_ag->p_ed_ext->e_min_counter_threshold = p_params_tmp->OFFSET_ED_EXT_E_FILTER_MIN_COUNTER_THRESHOLD;
    p_ag->p_ed_ext->e_min_check_disabled = ext_ed_disable_e_filter_check;
    aanc_ed100_initialize(p_ag->f_handle, p_ag->p_ed_ext);

    p_ag->p_ed_pb->p_input = p_ag->p_tmp_pb_ip;
    p_ag->p_ed_pb->p_tmp = p_ag->p_tmp_ed;
    p_ag->p_ed_pb->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_ag->p_ed_pb->attack_time = p_params_tmp->OFFSET_ED_PB_ATTACK;
    p_ag->p_ed_pb->decay_time = p_params_tmp->OFFSET_ED_PB_DECAY;
    p_ag->p_ed_pb->envelope_time = p_params_tmp->OFFSET_ED_PB_ENVELOPE;
    p_ag->p_ed_pb->init_frame_time = p_params_tmp->OFFSET_ED_PB_INIT_FRAME;
    p_ag->p_ed_pb->ratio = p_params_tmp->OFFSET_ED_PB_RATIO;
    p_ag->p_ed_pb->min_signal = p_params_tmp->OFFSET_ED_PB_MIN_SIGNAL;
    p_ag->p_ed_pb->min_max_envelope = p_params_tmp->OFFSET_ED_PB_MIN_MAX_ENVELOPE;
    p_ag->p_ed_pb->delta_th = p_params_tmp->OFFSET_ED_PB_DELTA_TH;
    p_ag->p_ed_pb->count_th = p_params_tmp->OFFSET_ED_PB_COUNT_TH;
    p_ag->p_ed_pb->hold_frames = p_params_tmp->OFFSET_ED_PB_HOLD_FRAMES;
    p_ag->p_ed_pb->e_min_threshold = p_params_tmp->OFFSET_ED_PB_E_FILTER_MIN_THRESHOLD;
    p_ag->p_ed_pb->e_min_counter_threshold = p_params_tmp->OFFSET_ED_PB_E_FILTER_MIN_COUNTER_THRESHOLD;
    p_ag->p_ed_pb->e_min_check_disabled = pb_ed_disable_e_filter_check;
    aanc_ed100_initialize(p_ag->f_handle, p_ag->p_ed_pb);

    /**************************************************
     * Initialize Clipping                            *
     **************************************************/
    /* TODO: set to a function */
    aanc_proc_initialize_clip_detect(
        &p_ag->clip_ext, p_params_tmp->OFFSET_CLIPPING_DURATION_EXT);
    aanc_proc_initialize_clip_detect(
        &p_ag->clip_int, p_params_tmp->OFFSET_CLIPPING_DURATION_INT);
    aanc_proc_initialize_clip_detect(
        &p_ag->clip_pb, p_params_tmp->OFFSET_CLIPPING_DURATION_PB);

    return TRUE;
}

bool aanc_proc_process_data(ADAPTIVE_GAIN *p_ag, bool calculate_gain)
{
    bool self_speech;

    unsigned flags_pre_proc, config, debug_config, mux_sel_algorithm, clip_det;
    AANC_PARAMETERS *p_params;
    int quiet_mode_lo_threshold, quiet_mode_hi_threshold;
    bool clip_int_disable, clip_ext_disable, clip_pb_disable, clip_disable;

    /* Copy input data to internal data buffers */
    cbuffer_copy(p_ag->p_tmp_int_ip, p_ag->p_mic_int_ip, AANC_DEFAULT_FRAME_SIZE);
    cbuffer_copy(p_ag->p_tmp_ext_ip, p_ag->p_mic_ext_ip, AANC_DEFAULT_FRAME_SIZE);

    /* Copy playback data to internal data buffers if connected */
    if (p_ag->p_playback_ip != NULL)
    {
        cbuffer_copy(p_ag->p_tmp_pb_ip, p_ag->p_playback_ip,
                     AANC_DEFAULT_FRAME_SIZE);
    }

    /* Copy fbmon data through if connected */
    if (p_ag->p_fbmon_ip != NULL)
    {
        if (p_ag->p_fbmon_op != NULL)
        {
            cbuffer_copy(p_ag->p_fbmon_op, p_ag->p_fbmon_ip,
                         AANC_DEFAULT_FRAME_SIZE);
        }
        else
        {
            cbuffer_discard_data(p_ag->p_fbmon_ip, AANC_DEFAULT_FRAME_SIZE);
        }
    }

    /* Clear all flags connected with processing data but persist quiet mode    */
    flags_pre_proc = *p_ag->p_aanc_flags & (AANC_MODEL_MASK | AANC_FLAGS_QUIET_MODE);

    /* Determine clip detection enable/disable  */
    debug_config = p_ag->p_aanc_params->OFFSET_AANC_DEBUG;
    clip_int_disable = debug_config & AANC_CONFIG_AANC_DEBUG_DISABLE_CLIPPING_DETECT_INT;
    clip_ext_disable = debug_config & AANC_CONFIG_AANC_DEBUG_DISABLE_CLIPPING_DETECT_EXT;
    clip_pb_disable = debug_config & AANC_CONFIG_AANC_DEBUG_DISABLE_CLIPPING_DETECT_PB;
    clip_disable = clip_int_disable && clip_ext_disable && clip_pb_disable;

    /* Clipping detection on the input mics */
    if (!(clip_disable))
    {
        aanc_proc_clipping_peak_detect(p_ag);
        aanc_proc_process_clip_detect(&p_ag->clip_ext);
        aanc_proc_process_clip_detect(&p_ag->clip_int);
        aanc_proc_process_clip_detect(&p_ag->clip_pb);

        clip_det = p_ag->clip_ext.detected * AANC_FLAGS_CLIPPING_EXT;
        clip_det |= p_ag->clip_int.detected * AANC_FLAGS_CLIPPING_INT;
        clip_det |= p_ag->clip_pb.detected * AANC_FLAGS_CLIPPING_PLAYBACK;

        if (clip_det > 0)
        {
            /* Copy input data to output if terminals are connected otherwise
             * discard data.
             */
            if (p_ag->p_mic_int_op != NULL)
            {
                cbuffer_copy(p_ag->p_mic_int_op, p_ag->p_tmp_int_ip,
                             AANC_DEFAULT_FRAME_SIZE);
            }
            else
            {
                cbuffer_discard_data(p_ag->p_tmp_int_ip,
                                     AANC_DEFAULT_FRAME_SIZE);
            }

            if (p_ag->p_mic_ext_op != NULL)
            {
                cbuffer_copy(p_ag->p_mic_ext_op, p_ag->p_tmp_ext_ip,
                             AANC_DEFAULT_FRAME_SIZE);
            }
            else
            {
                cbuffer_discard_data(p_ag->p_tmp_ext_ip,
                                     AANC_DEFAULT_FRAME_SIZE);
            }

            /* Copy or discard data on the playback stream */
            if (p_ag->p_playback_ip != NULL) {
                if (p_ag->p_playback_op != NULL)
                {
                    cbuffer_copy(p_ag->p_playback_op, p_ag->p_tmp_pb_ip,
                                AANC_DEFAULT_FRAME_SIZE);
                }
                else
                {
                    cbuffer_discard_data(p_ag->p_tmp_pb_ip,
                                        AANC_DEFAULT_FRAME_SIZE);
                }
            }

            flags_pre_proc |= clip_det;
            *p_ag->p_aanc_flags = flags_pre_proc;
            return FALSE;
        }
    }

    /* ED process ext mic */
    config = p_ag->p_aanc_params->OFFSET_AANC_CONFIG;
    if (!(config & AANC_CONFIG_AANC_CONFIG_DISABLE_ED_EXT))
    {
        aanc_ed100_process_data(p_ag->f_handle, p_ag->p_ed_ext);

        /* Catch external ED detection */
        if (p_ag->p_ed_ext->detection)
        {
            flags_pre_proc |= AANC_FLAGS_ED_EXT;
            L4_DBG_MSG("AANC_PROC ED Ext Detection");
        }

        p_params = p_ag->p_aanc_params;
        quiet_mode_lo_threshold = p_params->OFFSET_QUIET_MODE_LO_THRESHOLD;
        quiet_mode_hi_threshold = p_params->OFFSET_QUIET_MODE_HI_THRESHOLD;
        /* Threshold detect on external ED */
        if (p_ag->p_ed_ext->spl < quiet_mode_lo_threshold)
        {
            L4_DBG_MSG("AANC_PROC ED Ext below quiet mode low threshold");
            /* Set quiet mode flag */
            flags_pre_proc |= AANC_FLAGS_QUIET_MODE;
        }
        else if (p_ag->p_ed_ext->spl > quiet_mode_hi_threshold)
        {
            /* Reset quiet mode flag */
            flags_pre_proc &= AANC_PROC_QUIET_MODE_RESET_FLAG;
        }
    }

    /* ED process int mic */
    if (!(config & AANC_CONFIG_AANC_CONFIG_DISABLE_ED_INT))
    {
        aanc_ed100_process_data(p_ag->f_handle, p_ag->p_ed_int);
        if (p_ag->p_ed_int->detection)
        {
            flags_pre_proc |= AANC_FLAGS_ED_INT;
            L4_DBG_MSG("AANC_PROC: ED Int Detection");
        }
    }

    self_speech = FALSE;
    if (!(config & AANC_CONFIG_AANC_CONFIG_DISABLE_SELF_SPEECH))
    {
        /* ED process self-speech */
        self_speech = aanc_ed100_self_speech_detect(
            p_ag->p_ed_int, p_ag->p_ed_ext,
            p_ag->p_aanc_params->OFFSET_SELF_SPEECH_THRESHOLD);
        if (self_speech)
        {
            flags_pre_proc |= AANC_FLAGS_SELF_SPEECH;
            L4_DBG_MSG("AANC_PROC: Self Speech Detection");
        }
    }

    /* ED process playback */
    if (p_ag->p_playback_ip != NULL &&
        !(config & AANC_CONFIG_AANC_CONFIG_DISABLE_ED_PB))
    {
        aanc_ed100_process_data(p_ag->f_handle, p_ag->p_ed_pb);
        if (p_ag->p_ed_pb->detection)
        {
            flags_pre_proc |= AANC_FLAGS_ED_PLAYBACK;
            L4_DBG_MSG("AANC_PROC: ED Playback Detection");
        }
    }

    /* Update flags */
    *p_ag->p_aanc_flags = flags_pre_proc;

    /* Reference the working buffer used at the end to copy or discard data.
     * If adaptive gain calculation runs this is updated to the temporary output
     * buffers.
     */
    tCbuffer *p_int_working_buffer = p_ag->p_tmp_int_ip;
    tCbuffer *p_ext_working_buffer = p_ag->p_tmp_ext_ip;

    /* Call adaptive ANC function */
    if (!p_ag->p_ed_ext->detection && !p_ag->p_ed_int->detection &&
        !p_ag->p_ed_pb->detection  && !self_speech && calculate_gain)
    {
        L5_DBG_MSG("AANC_PROC: Calculate new gain");
        /* Commit scratch memory prior to processing */
        p_ag->p_fxlms->p_scratch_plant = \
            scratch_commit(FXLMS100_SCRATCH_MEMORY, MALLOC_PREFERENCE_NONE);
        p_ag->p_fxlms->p_scratch_control = \
            scratch_commit(FXLMS100_SCRATCH_MEMORY, MALLOC_PREFERENCE_NONE);

        /* Get control for whether the read pointer is updated or not
           If MUX_SEL_ALGORITHM we update the read pointer because the input
           buffer is not copied later. If not we don't update it so that the
           input buffer is correctly copied to the output.  */
        mux_sel_algorithm = \
        debug_config & AANC_CONFIG_AANC_DEBUG_MUX_SEL_ALGORITHM;
        p_ag->p_fxlms->read_ptr_upd = mux_sel_algorithm;

        if (aanc_fxlms100_process_data(p_ag->f_handle, p_ag->p_fxlms))
        {
            *p_ag->p_aanc_flags |= p_ag->p_fxlms->flags;
            if (mux_sel_algorithm)
            {
                p_int_working_buffer = p_ag->p_tmp_int_op;
                p_ext_working_buffer = p_ag->p_tmp_ext_op;
            }
        }

        p_ag->p_fxlms->p_scratch_plant = NULL;
        p_ag->p_fxlms->p_scratch_control = NULL;
        scratch_free();
    }

    /* Copy internal buffers to the output buffers if they are connected
    * otherwise discard the data.
    */
    if (p_ag->p_mic_int_op != NULL)
    {
        cbuffer_copy(p_ag->p_mic_int_op, p_int_working_buffer,
                        AANC_DEFAULT_FRAME_SIZE);
    }
    else
    {
        cbuffer_discard_data(p_int_working_buffer, AANC_DEFAULT_FRAME_SIZE);
    }

    if (p_ag->p_mic_ext_op != NULL)
    {
        cbuffer_copy(p_ag->p_mic_ext_op, p_ext_working_buffer,
                        AANC_DEFAULT_FRAME_SIZE);
    }
    else
    {
        cbuffer_discard_data(p_ext_working_buffer, AANC_DEFAULT_FRAME_SIZE);
    }

    /* Copy or discard data on the internal playback stream buffer */
    if (p_ag->p_playback_ip != NULL) {
        if (p_ag->p_playback_op != NULL)
        {
            cbuffer_copy(p_ag->p_playback_op, p_ag->p_tmp_pb_ip,
                        AANC_DEFAULT_FRAME_SIZE);
        }
        else
        {
            cbuffer_discard_data(p_ag->p_tmp_pb_ip,
                                 AANC_DEFAULT_FRAME_SIZE);
        }
    }

    return TRUE;
}
