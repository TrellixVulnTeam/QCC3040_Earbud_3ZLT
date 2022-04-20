// *****************************************************************************
// %%fullcopyright(2008)        http://www.csr.com
// %%version
//
// $Change: 3639410 $  $DateTime: 2021/04/07 04:06:57 $
// *****************************************************************************
#include "portability_macros.h"

#ifndef _SVAD_LIB_H
#define _SVAD_LIB_H
    
   // *****************************************************************************
   //                               SVAD data object structure
   // *****************************************************************************
   // pointer to internal mic data
   // format : integer
   .CONST $svad100.INTMIC_PTR_FIELD                     0;    
   // pointer to external mic data
   // format : integer
   .CONST $svad100.EXTMIC_PTR_FIELD                     ADDR_PER_WORD + $svad100.INTMIC_PTR_FIELD;
   // pointer to svad params
   // format : integer
   .CONST $svad100.PARAM_PTR_FIELD                      ADDR_PER_WORD + $svad100.EXTMIC_PTR_FIELD;
   // IPD output pointer scratch
   // format : integer
   .CONST $svad100.IPD_SCRATCH_OUTPUT_PTR_FIELD         ADDR_PER_WORD + $svad100.PARAM_PTR_FIELD;
   // short term average of IPD output for self voice pointer
   // format : integer
   .CONST $svad100.IPDSHORTAVG_FIELD                    ADDR_PER_WORD + $svad100.IPD_SCRATCH_OUTPUT_PTR_FIELD;
   // short term average of IPD output for other voice pointer
   // format : integer
   .CONST $svad100.IPDSHORTAVGOTHER_FIELD               ADDR_PER_WORD + $svad100.IPDSHORTAVG_FIELD;
   // Pointer to cvc variant variable
   // format : integer
   .CONST $svad100.PTR_VARIANT_FIELD                    ADDR_PER_WORD + $svad100.IPDSHORTAVGOTHER_FIELD;
   // flag for self VAD output
   // format : integer
   .CONST $svad100.immediateVADH_FIELD                  ADDR_PER_WORD + $svad100.PTR_VARIANT_FIELD;
   // pointer to mapped ipd intmic real vector
   // format : integer
   .CONST $svad100.IPD_INTMIC_MAP_REAL_PTR_FIELD        ADDR_PER_WORD + $svad100.immediateVADH_FIELD;
   // pointer to mapped ipd intmic imag vector
   // format : integer
   .CONST $svad100.IPD_INTMIC_MAP_IMAG_PTR_FIELD        ADDR_PER_WORD + $svad100.IPD_INTMIC_MAP_REAL_PTR_FIELD;
   // pointer to mapped ipd extmic real vector
   // format : integer
   .CONST $svad100.IPD_EXTMIC_MAP_REAL_PTR_FIELD        ADDR_PER_WORD + $svad100.IPD_INTMIC_MAP_IMAG_PTR_FIELD;
   // pointer to mapped ipd extmic imag vector
   // format : integer
   .CONST $svad100.IPD_EXTMIC_MAP_IMAG_PTR_FIELD        ADDR_PER_WORD + $svad100.IPD_EXTMIC_MAP_REAL_PTR_FIELD;
   // flag for self voice indication
   // format : integer
   .CONST $svad100.HSELF_FIELD                          ADDR_PER_WORD + $svad100.IPD_EXTMIC_MAP_IMAG_PTR_FIELD;
   // number of frequency bands for IPD process
   // format : integer
   .CONST $svad100.IPD_NUMBANDS_FIELD                   ADDR_PER_WORD + $svad100.HSELF_FIELD;
   // SVAD structure size
   // format : integer
   .CONST $svad100.STRUC_SIZE                           1 +  ($svad100.IPD_NUMBANDS_FIELD >> LOG2_ADDR_PER_WORD);
   
   
   // *****************************************************************************
   //                               SVAD parameters object structure
   // *****************************************************************************
   // Number of frames to hold self voice decisions if set
   // format : integer
   .CONST $svad100.param.NHSELF_FIELD                   0;    
   // Lower IPD subband
   // format : integer
   .CONST $svad100.param.IPD_SUBBAND_LOWER              ADDR_PER_WORD + $svad100.param.NHSELF_FIELD;
   // Higher IPD subband
   // format : integer
   .CONST $svad100.param.IPD_SUBBAND_HIGHER             ADDR_PER_WORD + $svad100.param.IPD_SUBBAND_LOWER;
   // Learning rate for short-term IPDs
   // format : fractional
   .CONST $svad100.param.BETA_IPD_SHORT                 ADDR_PER_WORD + $svad100.param.IPD_SUBBAND_HIGHER;
   // Internal mic noise floor in dB
   // format : fractional
   .CONST $svad100.param.INTMIC_NOISE_FLOOR             ADDR_PER_WORD + $svad100.param.BETA_IPD_SHORT;
   // Confidence threshold
   // format : fractional, q31
   .CONST $svad100.param.CONFIDENCE_THRS                ADDR_PER_WORD + $svad100.param.INTMIC_NOISE_FLOOR;
   // SVAD params structure size
   // format : integer
   .CONST $svad100.params.STRUC_SIZE                    1 +  ($svad100.param.CONFIDENCE_THRS >> LOG2_ADDR_PER_WORD);
   
   
   // *****************************************************************************
   //                                         SVAD constants
   // *****************************************************************************
   .CONST $svad100.IPD_MAX_BINS                         32;
   
#endif  