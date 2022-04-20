// *****************************************************************************
// Copyright (c) 2007 - 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// $Change: 1141152 $  $DateTime: 2011/11/02 20:31:09 $
// *****************************************************************************

// *****************************************************************************
// NAME:
//    AEC reference latency
//
// DESCRIPTION:
//    Implementing the AEC latency sync logic
//
//
// *****************************************************************************

#include "stack.h"
#include "cbops/cbops.h"
#include "cbuffer_asm.h"
#include "aec_reference_latency_asm_defs.h"

#include "patch/patch_asm_macros.h"

/* For this op, operator specific data are right after the header since there is no buffer table. */
#define CBOPS_AEC_REFERENCE_LATENCY_FIELD(x) ($cbops.param_hdr.CHANNEL_INDEX_START_FIELD+(x))

.MODULE $M.cbops.aec_ref_latency;
   .CODESEGMENT PM;
   .DATASEGMENT DM;

   // ** function vector **
   .VAR $cbops.aec_ref_mic_latency_op[$cbops.function_vector.STRUC_SIZE] =
         &$cbops.function_vector.NO_FUNCTION,            // reset vector
      &$cbops.aec_ref_latency_mic.amount_to_use,      // amount to use function
      &$cbops.aec_ref_latency_mic.main;               // main function

    .VAR $cbops.aec_speaker_latency_op[$cbops.function_vector.STRUC_SIZE] =
         &$cbops.function_vector.NO_FUNCTION,            // reset vector
      &$cbops.aec_ref_latency_speaker.amount_to_use,  // amount to use function
      &$cbops.aec_ref_latency_speaker.main;           // main function

// Expose the location of this table to C
.set $_cbops_speaker_latency_table , $cbops.aec_speaker_latency_op
.set $_cbops_mic_latency_table , $cbops.aec_ref_mic_latency_op


// *****************************************************************************
// MODULE:
//   $cbops.aec_ref_latency_mic.amount_to_use
//
// DESCRIPTION:
//      makes sure lack of space in mic output path doesn't cause data accumulation
//      in mic buffers. If space not available mic input is read but any excess
//      is trashed by main function.
// INPUTS:
//    - r8 = pointer to operator structure
//    - r4 = cbops graph buffer table
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//    r0-r2, r6, r5
//
// *****************************************************************************

$cbops.aec_ref_latency_mic.amount_to_use:
    LIBS_SLOW_SW_ROM_PATCH_POINT($cbops.aec_ref_latency_mic.amount_to_use.PATCH_ID_0, r5)

   cbops_aec_ref_latency_amount_to_use:
   // Get I/O row entry for index (r5)
   r5 = M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.INDEX_FIELD)];
   r5 = r5 * $CBOP_BUFTAB_ENTRY_SIZE_IN_ADDR(int);
   r5 = r5 + r4;
   // At this point transfer amount is just the space available in the output buffer, store it for later use.
   r1 = M[r5 + $cbops_c.cbops_buffer_struct.TRANSFER_PTR_FIELD];
   r0 = M[r1];
   M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.AVAILABLE_FIELD)] = r0;
   // Normally there will space in the buffer for transferring all the inputs, however at times the
   // reader side might become busy. Force a minimum space available, this is to make sure all the
   // input is read into output buffer. Any extra written will be trashed in main function.
   r6 = M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.COMMON_FIELD)];
   r2 = M[r6 + $aec_reference_latency.aec_latency_common_struct.MIN_SPACE_FIELD];
   r0 = MAX r2;
   M[r1] = r0;
   rts;

// *****************************************************************************
// MODULE:
//   $cbops.aec_ref_latency_mic.main
//
// DESCRIPTION:
//      makes sure lack of space in mic output path doesn't cause data accumulation
//      in mic buffers. If space not available mic input is read but any excess
//      is trashed.
// INPUTS:
//    - r8 = pointer to operator structure
//    - r4 = cbops graph buffer table
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//    r0-r2, r5-r7
//
// *****************************************************************************
$cbops.aec_ref_latency_mic.main:
    LIBS_SLOW_SW_ROM_PATCH_POINT($cbops.aec_ref_latency_mic.main.PATCH_ID_0, r6)

   cbops_aec_ref_latency_main:
   // Get I/O row entry for index (r5)
   r5 = M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.INDEX_FIELD)];
   r5 = r5 * $CBOP_BUFTAB_ENTRY_SIZE_IN_ADDR(int);
   r5 = r5 + r4;

   // get transfer amount (r0) and pointer (r7)
   r7 = M[r5 + $cbops_c.cbops_buffer_struct.TRANSFER_PTR_FIELD];
   r0 = M[r7];
   r1 = M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.AVAILABLE_FIELD)];

   // r0 = new amount written
   // r1 = space was available
   r2 = r0 - r1;
   if LE jump drop_check_done;
      // transfer amount can't be more than space available. So some or all of data has to be dropped.
      // This can happen if the output buffer gets full because reader hasn't read the data.
      r0 = r1;

      // log the amount dropped so far.
      r1 = M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.TRANSFER_DROPS_FIELD)];
      r1 = r1 + r2;
      M[r8 + CBOPS_AEC_REFERENCE_LATENCY_FIELD($aec_reference_latency._latency_op_struct.TRANSFER_DROPS_FIELD)] = r1;
   drop_check_done:
   M[r7] = r0;

   rts;
// *****************************************************************************
// MODULE:
//   $cbops.aec_ref_latency_speaker.amount_to_use
//
// DESCRIPTION:
//      makes sure lack of space in reference path doesn't block speaker path,
//      if space not available input of ref path is read but excess is trashed by
//      main function.
//
// INPUTS:
//    - r8 = pointer to operator structure
//    - r4 = cbops graph buffer table
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//    r0-r2, r6, r5
//
// *****************************************************************************
$cbops.aec_ref_latency_speaker.amount_to_use:
    LIBS_SLOW_SW_ROM_PATCH_POINT($cbops.aec_ref_latency_speaker.amount_to_use.PATCH_ID_0, r5)

    /* All is the same as amount_to_use function for mic, just here for adding a separate patch point */
    jump cbops_aec_ref_latency_amount_to_use;

// *****************************************************************************
// MODULE:
//   $cbops.aec_ref_latency_speaker.main
//
// DESCRIPTION:
//      makes sure lack of space in reference path doesn't block speaker path,
//      if space not available input of ref path is read but excess is trashed.
//
// INPUTS:
//    - r8 = pointer to operator structure
//    - r4 = cbops graph buffer table
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//    r0-r2, r5-r6
//
// *****************************************************************************
$cbops.aec_ref_latency_speaker.main:
    LIBS_SLOW_SW_ROM_PATCH_POINT($cbops.aec_ref_latency_speaker.main.PATCH_ID_0, r5)

    /* All is the same as main function for mic, just here for adding a separate patch point */
    jump cbops_aec_ref_latency_main;
.ENDMODULE;
// *****************************************************************************
// MODULE:
//   $_aec_ref_purge_mics
//
// DESCRIPTION:
//
// INPUTS:
//    - r0 = pointer to cbops_graph to process
//
// OUTPUTS:
//
// TRASHED REGISTERS:
//
// *****************************************************************************
.MODULE $M.cbops.aec_ref_purge_mics;
   .CODESEGMENT PM;

// void aec_ref_purge_mics(cbops_graph *mic_graph,unsigned num_mics);
$_aec_ref_purge_mics:
   LIBS_PUSH_REGS_SLOW_SW_ROM_PATCH_POINT($_aec_ref_purge_mics.PATCH_ID_0)
  // Save registers
   push rLink;
   pushm <r5,r6,r7,r8,r10>;
   pushm <M0,L0>;
   push I0;
   push B0;

  // Force cbops to update buffers
  M[r0 + $cbops_c.cbops_graph_struct.FORCE_UPDATE_FIELD]=r0;

   // Extra parameters
   r7 = r0 + $cbops_c.cbops_graph_struct.BUFFERS_FIELD;
   r8 = r1;
   r6 = MAXINT;

   // Get minimum data in mics
   r10 = r8;
   r5  = r7;
   do aec_ref_purge_mics.data_loop;
      // Get amount of data in mic
      r0 = M[r5 + $cbops_c.cbops_buffer_struct.BUFFER_FIELD];
      call $cbuffer.calc_amount_data_in_words;
      // update minimum
      r6 = MIN r0;
      // Go to next mic
      r5 = r5 + $CBOP_BUFTAB_ENTRY_SIZE_IN_ADDR;
   aec_ref_purge_mics.data_loop:

  r0 = r6;
   Words2Addr(r0);
   M0 = r0;
   if Z jump aec_ref_purge_mics_done;

   // Read Data
   r5  = r7;
aec_ref_purge_mics.advance_loop:
      // Get Input buffer (source)
      r0 = M[r5 + $cbops_c.cbops_buffer_struct.BUFFER_FIELD];
    call $cbuffer.get_read_address_and_size_and_start_address;
      L0 = r1;
      push r2;
      pop B0;
     I0 = r0;
      // Advance buffer
      NULL = r1 - MK1;
      if NZ jump aec_ref_purge_mics.sw_buf;
         // MMU buffer must be read
         r10 = r6;
         do aec_ref_purge_mics.mmu_buf;
            r0 = M[I0,MK1];
         aec_ref_purge_mics.mmu_buf:

         jump aec_ref_purge_mics.next;

aec_ref_purge_mics.sw_buf:
         r0 = M[I0,M0];
aec_ref_purge_mics.next:
      // Update buffer
      r0 = M[r5 + $cbops_c.cbops_buffer_struct.BUFFER_FIELD];
      r1 = I0;
      call $cbuffer.set_read_address;
      // Go to next mic
      r5 = r5 + $CBOP_BUFTAB_ENTRY_SIZE_IN_ADDR;
      r8 = r8 - 1;
      if GT jump aec_ref_purge_mics.advance_loop;

aec_ref_purge_mics_done:
  // Restore registers
   pop B0;
   pop I0;
   popm <M0,L0>;
   popm <r5,r6,r7,r8,r10>;
  pop rLink;
   rts;


.ENDMODULE;

// *****************************************************************************
// MODULE:
//   $$_aecref_calc_ref_rate
//   int aecref_calc_ref_rate(unsigned mic_rt,int mic_Ta,unsigned spic_rt,int spkr_ra);
//
// DESCRIPTION:
//    Compute reference rate adjustment from MIC & SPKR
//
//  rate_adjust = (rate_sink / rate_src) – 1.0           > 0 to speed up source
//
//  RateMIC = Expected/accumulated (QN.22)   (source)       >1.0 if slower than expected
//  RateOUT = RateMIC x ( RateAdjMIC + 1.0)     (sink)
//
//  RateSPKR = Expected/accumulated (QN.22)  (sink)                >1.0 if slower than expected
//  RateIN   = RateSPKR / (RateAdjSPKR + 1.0)      (source)
//
//  RateOUT =  RateIN  x (RateAdjREF + 1.0)
//
//  RateAdjREF = (RateOUT / RateIN) – 1.0
//  RateAdjREF = ([RateMIC x ( RateAdjMIC + 1.0)] / [RateSPKR / (RateAdjSPKR + 1.0)]) – 1.0
//  RateAdjREF = ([( RateAdjMIC + 1.0) x (RateAdjSPKR + 1.0)] x RateMIC / RateSPKR) – 1.0
//  RateAdjREF = [[( RateAdjMIC + 1.0) x (RateAdjSPKR + 1.0) x RateMIC ] / RateSPKR ] – 1.0
//
// INPUTS:
//    - r0 = MIC rate measurement (QN.22)
//    - r1 = MIC rate adjustment   (-1.0 ... 1.0)
//    - r2 = SPKR rate measurement (QN.22)
//    - r3 = SPKR rate adjustment  (-1.0 ... 1.0)
//
// OUTPUTS:
//    - r0 = reference rate adjustment
//
// TRASHED REGISTERS:
//
// *****************************************************************************
.MODULE $M.aecref_calc_ref_rate;
   .CODESEGMENT PM;
$_aecref_calc_ref_rate:
    LIBS_PUSH_REGS_SLOW_SW_ROM_PATCH_POINT($_aecref_calc_ref_rate.PATCH_ID_0)
    push r4;
    r4 = 0.5;
    // RateAdjREF = ([( mic_ra + 1.0) x (spkr_ra + 1.0) x mic_rt ] / spkr_rt) - 1.0

    // (mic_ra+1) x (spkr_ra+1.0) x 0.25 =
    //       (mic_ra*0.5 + 0.5) x (spkr_ra*0.5 + 0.5)
    r1 = r1 ASHIFT -1;
    r1 = r1 + r4;
    r3 = r3 ASHIFT -1;
    r3 = r3 + r4;
    rMAC = r1*r3;

    // (mic_ra+1) x (spkr_ra+1.0) x mic_rt x 0.25
    rMAC = rMAC * r0;

    //  Note:  Multiplier is 0.25 instead of 0.5 because it is going
    //         into a fractional divide

    // [(mic_ra+1) x (spkr_ra+1.0) x mic_rt x 0.25]/spkr_rt
    DIV = rMAC / r2;
    r0 = DivResult;

    // Result of the divide is [0.0 ... 1.0], 0.5 is unity
    //   Convert to [-1.0 ... +1.0]
    r0 = r0 - r4;
    r0 = r0 ASHIFT 1;
    pop r4;
    rts;
.ENDMODULE;

// *****************************************************************************
// MODULE:
//   $_aecref_calc_sync_mic_rate
//   int aecref_calc_sync_mic_rate(int spkr_ra, unsigned spkr_rt, unsigned spkr_rt);
//
// DESCRIPTION:
//   computes suitable rate to syncronise mic to speaker input, suitable backend input and
//   output have same clock source.
//
// INPUTS:
//   r0 = spkr_ra - rate that is applied to speaker (fractional, expected to be in +-0.03 raneg)
//   r1 = spkr_rt - measured rate for spkr - (QN.22)
//   r2 = mic_rt  - measured rate for mic  - (QN.22)
//
// OUTPUTS:
//  r0 : rate to be used for mic = spkr_rt/(mic_rt*(1+spkr_ra))
//
// TRASHED REGISTERS:
//  r0-r3 and rMAC - C callable
/////////////////////////////////////////////////////////////////////
.MODULE $M.aecref_calc_sync_mic_rate;
   .CODESEGMENT PM;
$_aecref_calc_sync_mic_rate:
    LIBS_PUSH_REGS_SLOW_SW_ROM_PATCH_POINT($_aecref_calc_sync_mic_rate.PATCH_ID_0)
    // r0 = spkr_ra
    // r1 = spkr_rt
    // r2 = mic_rt
    // return spkr_rt/(mic_rt*(1+spkr_ra))
    rMAC = -r0;
    r3 = r0 * r0 (frac);
    rMAC = rMAC + r0 * r0;
    rMAC = rMAC - r0*r3;   // rMAC = -spkr_ra + spkr_ra^2 - spkr_ra^3
                           // good enough estimation of 1.0/(1.0+spkr_ra) -1.0
    r0 = rMAC;
    rMAC = r1 - r2;
    if Z rts;              // all done if mic_rt==spkr_rt
    // calculate (spkr_rt/mic_rt)-1.0
#if DAWTH>24
   // division & rounding
   rMAC0 = r2;
   r2 = r2 + r2;
   Div = rMAC / r2;
   r1 = DivResult;       // r1 = (spkr_rt/mic_rt)-1.0
#else
   // K24, positive division
   rMAC = rMAC + r2;
   rMAC = rMAC ASHIFT -2 (56bit);
   Div = rMAC / r2;
   r1 = DivResult;   // r1 = (0.5*spkr_rt/mic_rt)
   r1 = r1 - 0.5;    // r1 = 0.5(spkr_rt/mic_rt-1.0)
   r1 = r1 + r1;     // r1 = (spkr_rt/mic_rt)-1.0
#endif
    // r1 = (spkr_rt/mic_rt)-1.0
    // r0 = 1.0/(1.0+spkr_ra)-1.0
    r2 = r1 * r0 (frac);
    r0 = r1 + r0;
    r0 = r0 + r2;
    // r0 = spkr_rt/mic_rt/(1+spkr_ra) - 1.0
    rts;
.ENDMODULE;
