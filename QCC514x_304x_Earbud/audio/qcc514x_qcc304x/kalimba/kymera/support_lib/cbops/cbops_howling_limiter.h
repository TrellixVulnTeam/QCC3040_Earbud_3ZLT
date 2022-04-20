// *****************************************************************************
// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

#ifndef CBOPS_HOWLING_LIMITER_HEADER_INCLUDED
#define CBOPS_HOWLING_LIMITER_HEADER_INCLUDED

/* Only for downloadable builds */
#ifdef CAPABILITY_DOWNLOAD_BUILD
#ifdef INSTALL_AEC_REFERENCE_HOWL_LIMITER

#include "hl_limiter_struct_asm_defs.h"

   .CONST   $cbops.howling_limiter.HOWLING_CALLCNT_FIELD            $hl_limiter_struct.HL_LIMITER_DATA_struct.HOWLING_CALLCNT_FIELD;
   .CONST   $cbops.howling_limiter.HL_ABS_MAX_FIELD                 $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_ABS_MAX_FIELD;
   .CONST   $cbops.howling_limiter.HL_ABS_PEAK_FIELD                $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_ABS_PEAK_FIELD;
   .CONST   $cbops.howling_limiter.HL_ABS_PEAK_SLOW_FIELD           $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_ABS_PEAK_SLOW_FIELD;
   .CONST   $cbops.howling_limiter.HL_ABS_RMS_FIELD                 $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_ABS_RMS_FIELD;
   .CONST   $cbops.howling_limiter.HL_ABS_RMS_FAST_FIELD            $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_ABS_RMS_FAST_FIELD;
   .CONST   $cbops.howling_limiter.HL_LIMITER_GAIN_FIELD            $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_LIMITER_GAIN_FIELD;
   .CONST   $cbops.howling_limiter.HL_DETECT_GAIN_FIELD             $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_DETECT_GAIN_FIELD;
   .CONST   $cbops.howling_limiter.HL_DETECT_CNT_FIELD              $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_DETECT_CNT_FIELD;
   .CONST   $cbops.howling_limiter.HL_DETECT_RELEASE_CNT_FIELD      $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_DETECT_RELEASE_CNT_FIELD;
   .CONST   $cbops.howling_limiter.HL_REF_LEVEL_RELEASE_FIELD       $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_REF_LEVEL_RELEASE_FIELD;
   .CONST   $cbops.howling_limiter.HL_SAMPLE_CNT_FIELD              $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_SAMPLE_CNT_FIELD;
   .CONST   $cbops.howling_limiter.HL_HP_Z_FIELD                    $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_HP_Z_FIELD;
   .CONST   $cbops.howling_limiter.HL_TC_DEC_ATTACK_FIELD           $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_TC_DEC_ATTACK_FIELD;
   .CONST   $cbops.howling_limiter.HL_TC_DEC_FIELD                  $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_TC_DEC_FIELD;
   .CONST   $cbops.howling_limiter.HL_TC_DEC_FAST_FIELD             $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_TC_DEC_FAST_FIELD;
   .CONST   $cbops.howling_limiter.HL_TC_DEC_SLOW_FIELD             $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_TC_DEC_SLOW_FIELD;
   .CONST   $cbops.howling_limiter.HL_REF_LEVEL_FIELD               $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_REF_LEVEL_FIELD;
   .CONST   $cbops.howling_limiter.HL_REF_LEVEL_FAST_FIELD          $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_REF_LEVEL_FAST_FIELD;
   .CONST   $cbops.howling_limiter.HL_DETECT_CNT_INIT_FIELD         $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_DETECT_CNT_INIT_FIELD;
   .CONST   $cbops.howling_limiter.HL_DETECT_RELEASE_CNT_INIT_FIELD $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_DETECT_RELEASE_CNT_INIT_FIELD;
   .CONST   $cbops.howling_limiter.HL_DETECT_FC_FIELD               $hl_limiter_struct.HL_LIMITER_DATA_struct.HL_DETECT_FC_FIELD;
   .CONST   $cbops.howling_limiter.STRUC_SIZE                       $hl_limiter_struct.HL_LIMITER_DATA_struct.STRUC_SIZE;

#endif /* INSTALL_AEC_REFERENCE_HOWL_LIMITER */
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#endif // CBOPS_HOWLING_LIMITER_HEADER_INCLUDED
