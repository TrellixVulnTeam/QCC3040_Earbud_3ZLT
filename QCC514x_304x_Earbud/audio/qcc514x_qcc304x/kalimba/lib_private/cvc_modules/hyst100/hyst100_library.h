// *****************************************************************************
// Copyright (c) 2007 - 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

#include "portability_macros.h"

#ifndef _HYST_LIB_H
#define _HYST_LIB_H


   // --------------------------------------------------------------------------
   //                               hyst data object structure
   // --------------------------------------------------------------------------   
   // Pointer to cvc variant variable
   // format : integer (variant enumeration)
   .CONST $hyst100.PTR_VARIANT_FIELD                0;
   // pointer to input on which hysteresis is desired
   // format : none (not interpreted)
   .CONST $hyst100.IMM_PTR_FIELD                    MK1 * 1; 
   // pointer to hyst params
   // format : pointer
   .CONST $hyst100.PARAM_PTR_FIELD                  MK1 * 2;

   // --------------------------------------------------------------------------
   //                               hyst private members
   // --------------------------------------------------------------------------   

   // hyst same_counter: counts number of same values
   // format : integer (counter)
   .CONST $hyst100.SAME_COUNT_FIELD                 MK1 * 3;
   // hyst previous value
   // format : none (not intepreted)
   .CONST $hyst100.PREV_IMM_FIELD                   MK1 * 4;
   // Hyst clear threshold in frames
   // format : integer (frame count)
   .CONST $hyst100.HYST_CLEAR_THRES_N_FIELD         MK1 * 5;
   // Hyst assert threshold in frames
   // format : integer (frame count)
   .CONST $hyst100.HYST_ASSERT_THRES_N_FIELD        MK1 * 6;
   // hyst output (imm with hysteresis applied)
   // format : none
   .CONST $hyst100.FLAG_FIELD                       MK1 * 7;
   // pointer to hyst flag
   .CONST $hyst100.HYST_FLAG_PTR_FIELD              MK1 * 8;
   // hyst structure size
   // format : integer
   .CONST $hyst100.STRUC_SIZE                       1 +  ($hyst100.HYST_FLAG_PTR_FIELD >> LOG2_ADDR_PER_WORD);   


   // --------------------------------------------------------------------------
   //                               hyst parameters object structure
   // --------------------------------------------------------------------------
   // Hyst clear time threshold in seconds
   // format : q7.25
   .CONST $hyst100.param.HYST_CLEAR_THRES          0;
   // Hyst assert time threshold in seconds
   // format : q7.25
   .CONST $hyst100.param.HYST_ASSERT_THRES        ADDR_PER_WORD + $hyst100.param.HYST_CLEAR_THRES;
   // hyst params structure size
   // format : integer
   .CONST $hyst100.params.STRUC_SIZE               1 +  ($hyst100.param.HYST_ASSERT_THRES >> LOG2_ADDR_PER_WORD);











#endif  
