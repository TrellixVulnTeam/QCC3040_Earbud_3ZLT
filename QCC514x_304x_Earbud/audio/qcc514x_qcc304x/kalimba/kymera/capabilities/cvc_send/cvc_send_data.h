// *****************************************************************************
// Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

#ifndef _CVC_SEND_DATA_ASM_H
#define _CVC_SEND_DATA_ASM_H

#include "cvc_modules.h"
#include "cvc_send_cap_asm.h"

#if defined(CVC_INEAR_STREPLUS_ROM_HARDWARE_BUILD) || defined(LEGACY_CVC)
   #define CVC3MIC_INEAR_ROM_HW_DOWNLOAD_BUILD
#endif

#ifndef CVC3MIC_INEAR_ROM_HW_DOWNLOAD_BUILD
 #include "cvc_send_gen_asm.h"
#else
 #include "cvc_send_gen_asm_dl.h"
#endif

// -----------------------------------------------------------------------------
// CVC SEND DATA ROOT STRUCTURE
// -----------------------------------------------------------------------------
.CONST $cvc_send.data.config_flag      0*ADDR_PER_WORD;
.CONST $cvc_send.data.cap_root_ptr     1*ADDR_PER_WORD;
.CONST $cvc_send.data.param            2*ADDR_PER_WORD;
.CONST $cvc_send.data.harm_obj         3*ADDR_PER_WORD;
.CONST $cvc_send.data.dms200_obj       4*ADDR_PER_WORD;
.CONST $cvc_send.data.one              5*ADDR_PER_WORD;
.CONST $cvc_send.data.zero             6*ADDR_PER_WORD;
.CONST $cvc_send.data.use              ADDR_PER_WORD + $cvc_send.data.zero;
.CONST $cvc_send.data.mic_mode         ADDR_PER_WORD + $cvc_send.data.use;
.CONST $cvc_send.data.end_fire         ADDR_PER_WORD + $cvc_send.data.mic_mode;
.CONST $cvc_send.data.hfk_config       ADDR_PER_WORD + $cvc_send.data.end_fire;
.CONST $cvc_send.data.dmss_config      ADDR_PER_WORD + $cvc_send.data.hfk_config;
.CONST $cvc_send.data.wind_flag        ADDR_PER_WORD + $cvc_send.data.dmss_config;
.CONST $cvc_send.data.echo_flag        ADDR_PER_WORD + $cvc_send.data.wind_flag;
.CONST $cvc_send.data.vad_flag         ADDR_PER_WORD + $cvc_send.data.echo_flag;
.CONST $cvc_send.data.TP_mode          ADDR_PER_WORD + $cvc_send.data.vad_flag;
.CONST $cvc_send.data.aec_inactive     ADDR_PER_WORD + $cvc_send.data.TP_mode;
.CONST $cvc_send.data.fftwin_power     ADDR_PER_WORD + $cvc_send.data.aec_inactive;
.CONST $cvc_send.data.power_adjust     ADDR_PER_WORD + $cvc_send.data.fftwin_power;
.CONST $cvc_send.data.dmss_obj         ADDR_PER_WORD + $cvc_send.data.power_adjust;
.CONST $cvc_send.data.reserved_0       ADDR_PER_WORD + $cvc_send.data.dmss_obj;
.CONST $cvc_send.data.reserved_1       ADDR_PER_WORD + $cvc_send.data.reserved_0;
.CONST $cvc_send.data.low_snr_dobj     ADDR_PER_WORD + $cvc_send.data.reserved_1;
.CONST $cvc_send.data.mic_malfunc_dobj ADDR_PER_WORD + $cvc_send.data.low_snr_dobj;
.CONST $cvc_send.data.mic_fiteval_dobj ADDR_PER_WORD + $cvc_send.data.mic_malfunc_dobj;
.CONST $cvc_send.data.eq_map_hold_flag  ADDR_PER_WORD + $cvc_send.data.mic_fiteval_dobj;
.CONST $cvc_send.data.ref_power_flag   ADDR_PER_WORD + $cvc_send.data.eq_map_hold_flag;
.CONST $cvc_send.data.selfclean100_dobj ADDR_PER_WORD + $cvc_send.data.ref_power_flag;
.CONST $cvc_send.data.highnoise_hyst_flag  ADDR_PER_WORD + $cvc_send.data.selfclean100_dobj;


.CONST $cvc_send.data.STRUC_SIZE       1 + ($cvc_send.data.reserved_1  >> LOG2_ADDR_PER_WORD);
.CONST $cvc_send.data.STRUC_SIZE_DL    1 + ($cvc_send.data.highnoise_hyst_flag >> LOG2_ADDR_PER_WORD);

.CONST $cvc_send.stream.refin          MK1 * 0;
.CONST $cvc_send.stream.sndin_left     MK1 * 1;
.CONST $cvc_send.stream.sndin_right    MK1 * 2;
.CONST $cvc_send.stream.sndin_mic3     MK1 * 3;
.CONST $cvc_send.stream.sndin_mic4     MK1 * 4;
.CONST $cvc_send.stream.sndout         MK1 * 5;


// reference power object
.CONST $refpwr.AEC_REF_PTR_FIELD                    0*ADDR_PER_WORD;
.CONST $refpwr.PARAM_PTR_FIELD                      1*ADDR_PER_WORD;
.CONST $refpwr.PTR_VARIANT_FIELD                    2*ADDR_PER_WORD;
.CONST $refpwr.SCRATCH_PTR                          3*ADDR_PER_WORD;
// =========================== START OF INTERNAL FIELDS ============================================
.CONST $refpwr.FFTLEN_FIELD                         4*ADDR_PER_WORD;
.CONST $refpwr.PEAK_FIELD                           5*ADDR_PER_WORD;
.CONST $refpwr.STRUCT_SIZE                          6;
// =========================== PARAMETERS ============================================
.CONST $refpwr.param.REF_THRESH                     0*ADDR_PER_WORD;





#endif // _CVC_SEND_DATA_ASM_H
