/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file hl_limiter_struct.h
 *
 *  private header file for howling limiter data structures. <br>
 *
 */

#ifndef __HL_LIMITER_STRUCT_H__
#define __HL_LIMITER_STRUCT_H__

#include "types.h"

/* Howling limiter data structure */
typedef struct HL_LIMITER_DATA {

    int32 hl_abs_max;
    int32 hl_abs_peak;
    int32 hl_abs_rms;
    int32 hl_abs_rms_fast;
    int32 hl_limiter_gain;
    int32 hl_detect_gain;
    int32 hl_detect_cnt;
    int32 hl_detect_release_cnt;
    int32 hl_ref_level_release;
    uint32 hl_sample_cnt;
    int32 hl_hp_z;                      // detection highpass

    // derived from ui parameters
    int32 hl_tc_dec_attack;
    int32 hl_tc_dec;
    int32 hl_tc_dec_fast;
    int32 hl_ref_level;
    int32 hl_ref_level_fast;
    int32 hl_detect_cnt_init;
    int32 hl_detect_release_cnt_init;
    int32 hl_detect_fc;
    int32 hl_switch;
} HL_LIMITER_DATA;

/* User interface parameter structure */
typedef struct HL_LIMITER_UI {

    int32 adc_gain1;
    int32 hl_switch;
    int32 hl_limit_level;
    int32 hl_limit_threshold;
    int32 hl_limit_hold_ms;
    int32 hl_limit_detect_fc;
    int32 hl_limit_tc;
} HL_LIMITER_UI;

#endif /* __HL_LIMITER_STRUCT_H__ */
