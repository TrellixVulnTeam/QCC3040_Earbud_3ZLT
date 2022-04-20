// *****************************************************************************
// Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
// *****************************************************************************
#include "cvclib.h"
#include "portability_macros.h"

#ifndef _INT_FIT_EVAL_LIB_H
#define _INT_FIT_EVAL_LIB_H

   // *****************************************************************************
   //                               internal mic fit module data object structure
   // *****************************************************************************
   // pointer to processed internal mic
   // format : integer
   .CONST $int_fit_eval100.D_INT_MIC                            0;
   // pointer to internal mic fit params
   // format : integer
   .CONST $int_fit_eval100.PARAM_PTR_FIELD                      ADDR_PER_WORD + $int_fit_eval100.D_INT_MIC;
   // Pointer to cvc variant variable
   // format : integer
   .CONST $int_fit_eval100.PTR_VARIANT_FIELD                    ADDR_PER_WORD + $int_fit_eval100.PARAM_PTR_FIELD;
   // Pointer to self clean flag
   // format : integer
   .CONST $int_fit_eval100.FFTLEN_FIELD                         ADDR_PER_WORD + $int_fit_eval100.PTR_VARIANT_FIELD;
   // The loose fit flag
   // format : integer
   .CONST $int_fit_eval100.LOOSE_FIT_FLAG                       ADDR_PER_WORD + $int_fit_eval100.FFTLEN_FIELD;
   // smoothing factor
   // format : q.31
   .CONST $int_fit_eval100.ALPHA                                ADDR_PER_WORD + $int_fit_eval100.LOOSE_FIT_FLAG;
   // Lower frequency band 1
   // format : integer
   .CONST $int_fit_eval100.L_BAND_INDX_1                        ADDR_PER_WORD + $int_fit_eval100.ALPHA;
   // Number of bins of frequency band 1
   // format : integer
   .CONST $int_fit_eval100.NUM_BINS_1                           ADDR_PER_WORD + $int_fit_eval100.L_BAND_INDX_1;
   // Lower frequency band 2
   // format : integer
   .CONST $int_fit_eval100.L_BAND_INDX_2                        ADDR_PER_WORD + $int_fit_eval100.NUM_BINS_1;
   // Number of bins of frequency band 2
   // format : integer
   .CONST $int_fit_eval100.NUM_BINS_2                           ADDR_PER_WORD + $int_fit_eval100.L_BAND_INDX_2;
   // Block exponent of the previous frame
   // format : integer
   .CONST $int_fit_eval100.PREV_BEXP                            ADDR_PER_WORD + $int_fit_eval100.NUM_BINS_2;
   // nHangUp
   // format : integer
   .CONST $int_fit_eval100.N_HANG_UP                            ADDR_PER_WORD + $int_fit_eval100.PREV_BEXP;
   // nHangDown
   // format : integer
   .CONST $int_fit_eval100.N_HANG_DOWN                          ADDR_PER_WORD + $int_fit_eval100.N_HANG_UP;
   // iHangUp
   // format : integer
   .CONST $int_fit_eval100.I_HANG_UP                            ADDR_PER_WORD + $int_fit_eval100.N_HANG_DOWN; 
   // iHangDown
   // format : integer
   .CONST $int_fit_eval100.I_HANG_DOWN                          ADDR_PER_WORD + $int_fit_eval100.I_HANG_UP;
   // Previous power difference
   // format : q8.24
   .CONST $int_fit_eval100.POW_DIFF                             ADDR_PER_WORD + $int_fit_eval100.I_HANG_DOWN;
   // Power difference treshold
   // format : q8.24
   .CONST $int_fit_eval100.POW_DIFF_TH                          ADDR_PER_WORD + $int_fit_eval100.POW_DIFF;
   // Previous low band power, shifted by previous block exponent
   // format : q8.24
   .CONST $int_fit_eval100.P_INT_MIC_L_BAND                     ADDR_PER_WORD + $int_fit_eval100.POW_DIFF_TH;
   // Previous high band power, shifted by previous block exponent
   // format : q8.24
   .CONST $int_fit_eval100.P_INT_MIC_H_BAND                     ADDR_PER_WORD + $int_fit_eval100.P_INT_MIC_L_BAND;

   // Internal mic fit structure size
   // format : integer
   .CONST $int_fit_eval100.STRUC_SIZE                           1 +  ($int_fit_eval100.P_INT_MIC_H_BAND >> LOG2_ADDR_PER_WORD);


   // *****************************************************************************
   //                               int_fit_eval parameters object structure
   // *****************************************************************************
   // decision making threshold in log2() domain
   // format : q8.24
   .CONST $int_fit_eval100.param.POW_DIFF_TH                      0;

   // internal mic fit module params structure size
   // format : integer
   .CONST $int_fit_eval100.params.STRUC_SIZE                      1 +  ($int_fit_eval100.param.POW_DIFF_TH >> LOG2_ADDR_PER_WORD);

#endif  