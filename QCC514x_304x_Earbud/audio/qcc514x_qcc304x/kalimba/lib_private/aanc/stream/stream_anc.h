/****************************************************************************
 * Copyright (c) 2013-19 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  stream_anc.h
 *
 */

#include "stream_prim.h"

extern bool stream_anc_set_anc_fine_gain(STREAM_ANC_INSTANCE anc_instance, STREAM_ANC_PATH path_id, uint16 gain);
extern bool stream_anc_set_anc_coarse_gain(STREAM_ANC_INSTANCE anc_instance, STREAM_ANC_PATH path_id, uint16 shift);
extern bool stream_anc_get_anc_fine_gain(STREAM_ANC_INSTANCE anc_instance, STREAM_ANC_PATH path_id, uint16 *gain);
extern bool stream_anc_get_anc_coarse_gain(STREAM_ANC_INSTANCE anc_instance, STREAM_ANC_PATH path_id, uint16 *shift);
extern bool stream_anc_get_anc_iir_coeffs(STREAM_ANC_INSTANCE anc_instance, STREAM_ANC_PATH path_id, unsigned num_coeffs, uint16 *coeffs);
