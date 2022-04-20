/****************************************************************************
 * Copyright (c) 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  hl_limiter.h
 *
 * Howling Prevention Limiter public header file. <br>
 *
 */

#ifndef __AEC_HL_LIMITER_H__
#define __AEC_HL_LIMITER_H__

#include "hl_limiter_defs.h"
#include "hl_limiter_struct.h"
#include "cbuffer_c.h"

void hl_limiter_init(
    HL_LIMITER_DATA *hl_data,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui);


void hl_limiter_param_config(
    HL_LIMITER_DATA *hl_data,
    unsigned mic_sampling_rate,
    HL_LIMITER_UI *hl_ui);


void hl_limiter_process(
    tCbuffer *stbuf,
    HL_LIMITER_DATA *hl_data,
    unsigned samples_to_process);

#endif /* __AEC_HL_LIMITER_H__ */
