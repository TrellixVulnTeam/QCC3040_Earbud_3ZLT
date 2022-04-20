// *****************************************************************************
// Copyright (c) 2014 - 2021 Qualcomm Technologies International, Ltd.
// %% version
//
// $Change: 3641027 $  $DateTime: 2021/04/08 22:46:31 $
// *****************************************************************************

#include "portability_macros.h"

#ifndef _INT_MALFUNC_LIB_H
#define _INT_MALFUNC_LIB_H
    
   // *****************************************************************************
   //                               int_malfunc data object structure
   // *****************************************************************************
   // pointer to primary extmic
   // format : integer
   .CONST $int_malfunc.D0_FIELD                             0;    
   // pointer to intmic
   // format : integer
   .CONST $int_malfunc.D_INT_FIELD                          ADDR_PER_WORD + $int_malfunc.D0_FIELD;
   
   .CONST $int_malfunc.D_INT_SCRATCH_FIELD                  ADDR_PER_WORD + $int_malfunc.D_INT_FIELD;
   
   .CONST $int_malfunc.D_INT_MAP_SCRATCH_FIELD              ADDR_PER_WORD + $int_malfunc.D_INT_SCRATCH_FIELD;
   
   .CONST $int_malfunc.LIN_WEIGHTS_SCRATCH_FIELD            ADDR_PER_WORD + $int_malfunc.D_INT_MAP_SCRATCH_FIELD; 
   // pointer to int_malfunc params
   // format : integer
   .CONST $int_malfunc.PARAM_PTR_FIELD                      ADDR_PER_WORD + $int_malfunc.LIN_WEIGHTS_SCRATCH_FIELD;
   // Pointer to cvc variant variable
   // format : integer
   .CONST $int_malfunc.PTR_VARIANT_FIELD                    ADDR_PER_WORD + $int_malfunc.PARAM_PTR_FIELD;
   // low freq index
   // format : integer
   .CONST $int_malfunc.LOW_FREQ_IDX                         ADDR_PER_WORD + $int_malfunc.PTR_VARIANT_FIELD;
   // number of frequency bands
   // format : integer
   .CONST $int_malfunc.NUMBANDS_FIELD                       ADDR_PER_WORD + $int_malfunc.LOW_FREQ_IDX;
                       
   // smoothing factor
   // format : q1.31
   .CONST $int_malfunc.SMOOTH_FACTOR                        ADDR_PER_WORD + $int_malfunc.NUMBANDS_FIELD;
   // int_malfunc output flag
   // format : integer
   .CONST $int_malfunc.INT_MALFUNC_FLAG                     ADDR_PER_WORD + $int_malfunc.SMOOTH_FACTOR;
    
   .CONST $int_malfunc.HANG_UP_N_FIELD                      ADDR_PER_WORD + $int_malfunc.INT_MALFUNC_FLAG;   

   .CONST $int_malfunc.HANG_DOWN_N_FIELD                    ADDR_PER_WORD + $int_malfunc.HANG_UP_N_FIELD;   
   
   .CONST $int_malfunc.P_PRIM_MIC                           ADDR_PER_WORD + $int_malfunc.HANG_DOWN_N_FIELD;
   
   .CONST $int_malfunc.P_PRIM_MIC1                          ADDR_PER_WORD + $int_malfunc.P_PRIM_MIC;
   
   .CONST $int_malfunc.POW_DIFF                             ADDR_PER_WORD + $int_malfunc.P_PRIM_MIC1;
   
   .CONST $int_malfunc.I_HANG_UP                            ADDR_PER_WORD + $int_malfunc.POW_DIFF;
   
   .CONST $int_malfunc.I_HANG_DOWN                          ADDR_PER_WORD + $int_malfunc.I_HANG_UP;
   // int_malfunc structure size
   // format : integer
   .CONST $int_malfunc.STRUC_SIZE                           1 +  ($int_malfunc.I_HANG_DOWN >> LOG2_ADDR_PER_WORD);
   
   
   // *****************************************************************************
   //                               int_malfunc parameters object structure
   // *****************************************************************************
   
   // Decision Threshold power in Q8.24 in log2
   .CONST $int_malfunc.param.POWER_DIFF_THRESH          0;

   .CONST $int_malfunc.params.STRUC_SIZE                1 +  ($int_malfunc.param.POWER_DIFF_THRESH >> LOG2_ADDR_PER_WORD);
   
   
   
   
#endif  