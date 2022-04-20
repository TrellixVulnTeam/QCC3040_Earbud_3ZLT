// *****************************************************************************
// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************
// *****************************************************************************
// NAME:
//    GVA secure processing functions
//
//
// *****************************************************************************

#ifndef GVA_PROC_LIB_H
#define GVA_PROC_LIB_H

#include "hotword_dsp_multi_bank_api.h"

#define GVA_ERROR -1

/**
@brief Initialize the feature handle for GVA
*/
bool load_gva_handle(void** f_handle);

/**
@brief Secure process data function for GVA
*/
int GoogleHotwordDspMultiBankProcessData(void *f_handle, const void* samples, int frame_size,
                                     int* preamble_length_ms, void* memory_handle);

/**
@brief Unload the feature handle for VAD
*/
void unload_wwe_handle(void* f_handle);


#endif /* GVA_PROC_LIB_H */
