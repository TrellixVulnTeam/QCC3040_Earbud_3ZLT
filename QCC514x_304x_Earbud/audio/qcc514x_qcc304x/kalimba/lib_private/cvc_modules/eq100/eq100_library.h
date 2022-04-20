// *****************************************************************************
// Copyright (c) 2007 - 2017 Qualcomm Technologies International, Ltd.
// %%version
//
// $Change: 3641027 $  $DateTime: 2021/04/08 22:46:31 $
// *****************************************************************************
#include "portability_macros.h"

#ifndef _EQ100_LIB_H
#define _EQ100_LIB_H

    // **************************************
    // constants used in eq100 library
    // **************************************
   .CONST $eq100.MAPCOEFF_Q_INT                       16;    
   
   // *****************************************************************************
   //                               eq100 data object structure
   // *****************************************************************************
   // pointer to internal mic data
   // format : integer
   .CONST $eq100.INTMIC_PTR_FIELD                     0;    
   // pointer to external mic data
   // format : integer
   .CONST $eq100.EXTMIC_PTR_FIELD                     ADDR_PER_WORD + $eq100.INTMIC_PTR_FIELD;
   // pointer to eq100 params
   // format : integer
   .CONST $eq100.PARAM_PTR_FIELD                      ADDR_PER_WORD + $eq100.EXTMIC_PTR_FIELD;
   // Pointer to cvc variant variable
   // format : integer
   .CONST $eq100.PTR_VARIANT_FIELD                    ADDR_PER_WORD + $eq100.PARAM_PTR_FIELD;
   // Pointer to nproto
   // format : integer
   .CONST $eq100.NPROTO_FRAME_FIELD                   ADDR_PER_WORD + $eq100.PTR_VARIANT_FIELD;
   // Pointer to hold flag
   // format : integer
   .CONST $eq100.HOLD_FLAG_PTR_FIELD                  ADDR_PER_WORD + $eq100.NPROTO_FRAME_FIELD;
   // @DOC_FIELD_TEXT Pointer to map coefficients
   // @DOC_FIELD_FORMAT q.24
   .CONST $eq100.SMOOTHED_RATIO_PTR_FIELD             ADDR_PER_WORD + $eq100.HOLD_FLAG_PTR_FIELD;
   // @DOC_FIELD_TEXT Pointer to scratch
   // @DOC_FIELD_FORMAT integer
   .CONST $eq100.EQ100_SCRATCH_PTR                    ADDR_PER_WORD + $eq100.SMOOTHED_RATIO_PTR_FIELD;;
    
   // =========================== START OF INTERNAL FIELDS ============================================
   
   // FFT length field
   // format : integer
   .CONST $eq100.FFTLEN_FIELD                         ADDR_PER_WORD + $eq100.EQ100_SCRATCH_PTR;
   // Null frequency bin index
   // format : integer
   .CONST $eq100.NULL_FREQ_IDX                        ADDR_PER_WORD + $eq100.FFTLEN_FIELD;
   // adapt_enable frames counter
   // format : integer
   .CONST $eq100.ADAPT_FRAME_COUNT                    ADDR_PER_WORD + $eq100.NULL_FREQ_IDX;
   //initial_alpha field
   .CONST $eq100.INITIAL_ALPHA_FIELD                  ADDR_PER_WORD + $eq100.ADAPT_FRAME_COUNT;
   //steady_alpha field
   .CONST $eq100.STEADY_ALPHA_FIELD                   ADDR_PER_WORD + $eq100.INITIAL_ALPHA_FIELD;
   //initial frame field
   .CONST $eq100.NUM_INITIAL_FRAMES_FIELD             ADDR_PER_WORD + $eq100.STEADY_ALPHA_FIELD;
           
   // eq100 structure size
   // format : integer
   .CONST $eq100.STRUC_SIZE                           1 +  ($eq100.NUM_INITIAL_FRAMES_FIELD >> LOG2_ADDR_PER_WORD);
   
   
   // *****************************************************************************
   //                               eq100 parameters object structure
   // *****************************************************************************
   // Frequency below which map output is zero
   // format : integer
   .CONST $eq100.param.NULL_FREQ                      0;
   // Maximum amplification per bin on the int mic
   // format : q16.16
   .CONST $eq100.param.MAX_AMPLIFICATION              ADDR_PER_WORD + $eq100.param.NULL_FREQ;
   //EQ initial_tau 
   // format : q8.24
   .CONST $eq100.param.INIT_TAU                       ADDR_PER_WORD + $eq100.param.MAX_AMPLIFICATION;
   //EQ steady_tau 
   // format : q8.24
   .CONST $eq100.param.STEADY_TAU                     ADDR_PER_WORD + $eq100.param.INIT_TAU;
   //EQ L_initial 
   // format : q8.24
   .CONST $eq100.param.L_INITIAL                     ADDR_PER_WORD + $eq100.param.STEADY_TAU;
   
   // eq100 params structure size
   // format : integer
   .CONST $eq100.params.STRUC_SIZE                    1 +  ($eq100.param.L_INITIAL >> LOG2_ADDR_PER_WORD);
   
   
   
   
#endif  
