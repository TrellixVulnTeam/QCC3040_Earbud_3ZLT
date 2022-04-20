// *****************************************************************************
// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

// *****************************************************************************
// NOTICE TO ANYONE CHANGING THIS FILE:
// IF YOU UPDATE THE SVAD STRUCTURE WITH NEW FIELD(S) THEN
// REMEMBER TO CHANGE THE CORRESPONDING ASM HEADER FILE 'filter_bank_library.h'
// WITH THE NEW FIELD(S) AS WELL
// *****************************************************************************

#ifndef FILTER_BANK_C_H
#define FILTER_BANK_C_H

/*****************************************e************************************
Include Files
*/
#include "buffer/cbuffer_c.h"
#include "platform/pl_fractional.h"
#include "cvclib_c.h"
#include "cvc_stream.h"

#define filter_bank_parameters_frame120         120
#define filter_bank_parameters_proto240         240
#define filter_bank_parameters_proto256         256
#define filter_bank_parameters_fft256_scale     7
#define filter_bank_parameters_fft256_num_bin   129           

// do not edit this without updating filter_bank_library.h
typedef struct
{  
   int frame_size;
   int window_size;
   int zero_padded_window_size;
   int scale;
   unsigned int *window_ptr;
} t_filter_bank_config_object;

// do not edit this without updating filter_bank_library.h
typedef struct
{  
   int  num_points;
   int *real_scratch_ptr;
   int *imag_scratch_ptr;
   int *fft_scratch_ptr;
   int *split_cos_table_ptr;
   int fft_extra_scale;
   int ifft_extra_scale;
   int q_dat_in;
   int q_dat_out;
} t_fft_object;

// do not edit this without updating filter_bank_library.h
typedef struct
{  
   t_filter_bank_config_object *config_ptr;
   cVcStream *input_stream_object_ptr;
   int *history_ptr;
   t_fft_object  *fft_object_ptr;
   t_filter_bank_channel_object *freq_output_object_ptr;
   int hfp_enable;
} t_analysis_filter_bank_object;

// do not edit this without updating filter_bank_library.h
typedef struct
{  
   unsigned int *config_ptr;
   cVcStream *output_stream_object_ptr;
   int *history_ptr;
   t_fft_object *fft_object_ptr;
   t_filter_bank_channel_object *input_complex_object_ptr;
   int zero_nyquist_bin_enable;
   int saturation_protection_enable;
} t_synthesis_filter_bank_object;


#endif /* FILTERBANK_C_H */