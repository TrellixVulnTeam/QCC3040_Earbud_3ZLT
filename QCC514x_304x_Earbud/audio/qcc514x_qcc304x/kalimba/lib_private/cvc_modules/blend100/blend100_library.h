// *****************************************************************************
// Copyright (c) 2007 - 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************
#include "portability_macros.h"

#ifndef _BLEND_LIB_H
#define _BLEND_LIB_H
   // --------------------------------------------------------------------------
   // BLEND MODE
   // --------------------------------------------------------------------------
   .CONST $blend100.mode.PASS_EXT                     0;
   .CONST $blend100.mode.PASS_INT                     1;
   .CONST $blend100.mode.OCCLUDED                     2;
   .CONST $blend100.mode.NON_OCCLUDED                 3;

   // --------------------------------------------------------------------------
   //                               blend data object structure
   // --------------------------------------------------------------------------
   // pointer to internal mic data
   // format : integer
   .CONST $blend100.INTMIC_PTR_FIELD                     0;    
   // pointer to external mic data
   // format : integer
   .CONST $blend100.EXTMIC_PTR_FIELD                     ADDR_PER_WORD + $blend100.INTMIC_PTR_FIELD;
   // pointer to blend params
   // format : integer
   .CONST $blend100.PARAM_PTR_FIELD                      ADDR_PER_WORD + $blend100.EXTMIC_PTR_FIELD;
   // Pointer to cvc variant variable
   // format : integer
   .CONST $blend100.PTR_VARIANT_FIELD                    ADDR_PER_WORD + $blend100.PARAM_PTR_FIELD;
   // pointer to blend scratch
   // format : integer
   .CONST $blend100.BLEND_SCRPTR_FIELD                   ADDR_PER_WORD + $blend100.PTR_VARIANT_FIELD;
   // Pointer to Mask
   .CONST $blend100.PTR_MASK1_FIELD                      ADDR_PER_WORD + $blend100.BLEND_SCRPTR_FIELD;
   .CONST $blend100.PTR_MASK2_FIELD                      ADDR_PER_WORD + $blend100.PTR_MASK1_FIELD;
   // blend mode 0:full_proc 1:extmic PT 2:intmic PT
   // format : integer
   .CONST $blend100.BLEND_MODE_FIELD                     ADDR_PER_WORD + $blend100.PTR_MASK2_FIELD;
   // FFT length field
   // format : integer
   .CONST $blend100.FFTLEN_FIELD                         ADDR_PER_WORD + $blend100.BLEND_MODE_FIELD;
   // pointer to internal scratch1
   // format : integer
   .CONST $blend100.BLEND_SCRPTR1_FIELD                  ADDR_PER_WORD +  $blend100.FFTLEN_FIELD;
   // BLEND structure size
   // format : integer
   .CONST $blend100.STRUC_SIZE                           1 +  ($blend100.BLEND_SCRPTR1_FIELD >> LOG2_ADDR_PER_WORD);
   
   // --------------------------------------------------------------------------
   //                               blend parameters object structure
   // --------------------------------------------------------------------------
   // Blending slope
   // format : q1.31
   .CONST $blend100.param.SLOPE_FIELD                    0;
   // Blending bias sensitivity to weights
   // format : integer
   .CONST $blend100.param.CROSSOVER_FREQ_FIELD          ADDR_PER_WORD + $blend100.param.SLOPE_FIELD;
   // blend params structure size
   // format : integer
   .CONST $blend100.params.STRUC_SIZE                    1 +  ($blend100.param.CROSSOVER_FREQ_FIELD >> LOG2_ADDR_PER_WORD);
   
   
   // *****************************************************************************
   //                               internal constants
   // *****************************************************************************
   #define $blend100.C_WEIGHT_FIELD                      $blend100.BLEND_SCRPTR3_FIELD

#endif  
