// *****************************************************************************
// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

// *****************************************************************************
// NAME:
//    Howling Limiter operator
//
// DESCRIPTION:
//    This operator is used to reduce or eliminate "Howling" that can occur
//    due to audio feedback from a speaker output into an associated
//    microphone input.
//
// When using the operator the following data structure is used:
//    - operator structure + $cbops.param_hdr.CHANNEL_START_INDEX_FIELD = Channel index of the in-place buffer
//    - operator structure + $cbops.param_hdr.OPERATOR_DATA_PTR_FIELD = Pointer to the operator data structure
//
// *****************************************************************************

#include "cbops.h"
#include "stack.h"
#include "cbops_howling_limiter_c_asm_defs.h"

.MODULE $M.cbops.howling_limiter;
   .DATASEGMENT DM;

   // ** function vector **
   .VAR $cbops.howling_limiter[$cbops.function_vector.STRUC_SIZE] =
      $cbops.function_vector.NO_FUNCTION,          // reset function
      $cbops.function_vector.NO_FUNCTION,          // amount to use function
      $cbops.howling_limiter.main;                 // main function
.ENDMODULE;

// Expose the location of this table to C
.set $_cbops_howling_limiter_table, $cbops.howling_limiter

// *****************************************************************************
// MODULE:
//    $cbops.howling_limiter.main
//
// DESCRIPTION:
//    Operator that attempts to prevent "Howling" by means of a limiter (multi-channel version)
//
// INPUTS:
//    - r4 = pointer to internal framework object
//    - r5 = pointer to the list of input and output buffer start addresses (base_addr)
//    - r6 = pointer to the list of input and output buffer pointers
//    - r7 = pointer to the list of buffer lengths
//    - r8 = pointer to operator structure
//
// OUTPUTS:
//    - none
//
// TRASHED REGISTERS:
//    Assume all
//
// *****************************************************************************
.MODULE $M.cbops.howling_limiter.main;
   .CODESEGMENT CBOPS_HOWLING_LIMITER_MAIN_PM;
   .DATASEGMENT DM;

$cbops.howling_limiter.main:

   push rLink;

   // start profiling if enabled
   #ifdef ENABLE_PROFILER_MACROS
      .VAR/DM1 $cbops.profile_howling_limiter[$profiler.STRUC_SIZE] = $profiler.UNINITIALISED, 0 ...;
      r0 = &$cbops.profile_howling_limiter;
      call $profiler.start;
   #endif

   r0 = M[r8 + $cbops.param_hdr.CHANNEL_INDEX_START_FIELD];       // r0 = Buffer index for in-place I/O
   call $cbops.get_amount_ptr;                                    // Get the number of samples to process
   r2 = M[r0];                                                    // r2 = Number of samples to process

   r0 = M[r8 + $cbops.param_hdr.CHANNEL_INDEX_START_FIELD];       // r0 = Buffer index for in-place I/O
                                                                  // r4 = buffer table
   call $cbops.get_cbuffer;                                       // Get the cbuffer pointer (in r0)

                                                                  // r0 = cbuffer pointer for in-place I/O
   r1 = M[r8 + $cbops.param_hdr.OPERATOR_DATA_PTR_FIELD];         // r1 = Howling Limiter data structure pointer
                                                                  // r2 = Number of samples to process
   call $_hl_limiter_process;                                     // Call the private Howling Limiter process function

// stop profiling if enabled
#ifdef ENABLE_PROFILER_MACROS
   r0 = &$cbops.profile_howling_limiter;
   call $profiler.stop;
#endif

   pop rlink;
   rts;

.ENDMODULE;
