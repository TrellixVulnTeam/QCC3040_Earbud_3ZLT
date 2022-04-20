/****************************************************************************
 * Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
****************************************************************************/
#include "portability_macros.h"
#include "stack.h"
#include "aanc_gen_asm.h"
#include "aanc_proc_asm_defs.h"

#define $aanc_proc.LOG2_TO_DB_CONV_FACTOR           Qfmt_(6.020599913279624, 5)
#define $aanc_proc.COARSE_GAIN_TO_DB                6
#define $aanc_proc.FINE_GAIN_LOG2_SHIFT_AMT         24
#define $aanc_proc.COARSE_GAIN_DB_SHIFT_AMT         20
#define $aanc_proc.FRAME_SIZE                       64

// *****************************************************************************
// MODULE:
//   $aanc_proc.clipping_detect
//
// DESCRIPTION:
//    AANC PROC clipping and peak detection on input buffers
//
// INPUTS:
//    - r0 = pointer to adaptive gain structure
//
// OUTPUTS:
//    - r0 = success/failure
//
// TRASHED REGISTERS:
//    C compliant
//
// NOTES:
//
// *****************************************************************************

.CONST $aanc_proc.PEAK_INT_FIELD ($aanc_proc._ADAPTIVE_GAIN_struct.CLIP_INT_FIELD + \
                                  $aanc_proc._AANC_CLIP_DETECT_struct.PEAK_VALUE_FIELD);
.CONST $aanc_proc.PEAK_EXT_FIELD ($aanc_proc._ADAPTIVE_GAIN_struct.CLIP_EXT_FIELD + \
                                  $aanc_proc._AANC_CLIP_DETECT_struct.PEAK_VALUE_FIELD);
.CONST $aanc_proc.PEAK_PB_FIELD  ($aanc_proc._ADAPTIVE_GAIN_struct.CLIP_PB_FIELD + \
                                  $aanc_proc._AANC_CLIP_DETECT_struct.PEAK_VALUE_FIELD);
.CONST $aanc_proc.CLIP_INT_FIELD ($aanc_proc._ADAPTIVE_GAIN_struct.CLIP_INT_FIELD + \
                                  $aanc_proc._AANC_CLIP_DETECT_struct.FRAME_DETECT_FIELD);
.CONST $aanc_proc.CLIP_EXT_FIELD ($aanc_proc._ADAPTIVE_GAIN_struct.CLIP_EXT_FIELD + \
                                  $aanc_proc._AANC_CLIP_DETECT_struct.FRAME_DETECT_FIELD);
.CONST $aanc_proc.CLIP_PB_FIELD ($aanc_proc._ADAPTIVE_GAIN_struct.CLIP_PB_FIELD + \
                                 $aanc_proc._AANC_CLIP_DETECT_struct.FRAME_DETECT_FIELD);

.MODULE $M.aanc_proc.clipping_peak_detect;
    .CODESEGMENT PM;

$_aanc_proc_clipping_peak_detect:

    PUSH_ALL_C;

    r9 = r0;
    r4 = M[r9 + $aanc_proc._ADAPTIVE_GAIN_struct.CLIP_THRESHOLD_FIELD];

    // Get cbuffer details for DM1 input
    r0 = M[r9 + $aanc_proc._ADAPTIVE_GAIN_struct.P_TMP_INT_IP_FIELD];
    call $cbuffer.get_read_address_and_size_and_start_address;
    push r2;
    pop B0;
    I0 = r0;
    L0 = r1;

    // Get cbuffer details for DM2 input
    r0 = M[r9 + $aanc_proc._ADAPTIVE_GAIN_struct.P_TMP_EXT_IP_FIELD];
    call $cbuffer.get_read_address_and_size_and_start_address;
    push r2;
    pop B4;
    I4 = r0;
    L4 = r1;

    r10 = $aanc_proc.FRAME_SIZE;

    r3 = 0; // Internal mic clipping detection
    r5 = 0; // Internal mic clipping detection

    r7 = M[r9 + $aanc_proc.PEAK_INT_FIELD];
    r8 = M[r9 + $aanc_proc.PEAK_EXT_FIELD];

    do detect_mic_clipping;
        r0 = M[I0, MK1], r2 = M[I4, MK1];
        r0 = ABS r0; // r0 = Int Mic (I0)
        r7 = MAX r0; // Peak detect int mic
        r2 = ABS r2; // r2 = Ext Mic (I4)
        r8 = MAX r2; // Peak detect ext mic
        Null = r0 - r4;
        if GE r3 = 1;
        Null = r2 - r4;
        if GE r5 = 1;
    detect_mic_clipping:

    MB[r9 + $aanc_proc.CLIP_INT_FIELD] = r3;
    MB[r9 + $aanc_proc.CLIP_EXT_FIELD] = r5;
    M[r9 + $aanc_proc.PEAK_INT_FIELD] = r7;
    M[r9 + $aanc_proc.PEAK_EXT_FIELD] = r8;

    // Do clipping and peak detect on the playback channel
    r0 = M[r9 + $aanc_proc._ADAPTIVE_GAIN_struct.P_PLAYBACK_IP_FIELD];
    if Z jump done_detection;

    r0 = M[r9 + $aanc_proc._ADAPTIVE_GAIN_struct.P_TMP_PB_IP_FIELD];
    call $cbuffer.get_read_address_and_size_and_start_address;
    push r2;
    pop B0;
    I0 = r0;
    L0 = r1;

    r3 = 0;
    r10 = $aanc_proc.FRAME_SIZE;
    r5 = $M.AANC.FLAGS.CLIPPING_PLAYBACK;
    r7 = M[r9 + $aanc_proc.PEAK_PB_FIELD];
    do detect_pb_clipping;
        r0 = M[I0, MK1];
        r0 = ABS r0; // r0 = Playback (I0)
        r7 = MAX r0; // Peak detect playback signal
        Null = r0 - r4;
        if GE r3 = 1;
    detect_pb_clipping:

    MB[r9 + $aanc_proc.CLIP_PB_FIELD] = r3;
    M[r9 + $aanc_proc.PEAK_PB_FIELD] = r7;

done_detection:
    POP_ALL_C; // POP_ALL_C won't touch r0-r3
    r0 = 1;

    rts;

.ENDMODULE;

// *****************************************************************************
// MODULE:
//   $aanc_proc.calc_gain_db
//
// DESCRIPTION:
//    AANC PROC calculate dB representation of gain to be reported as statistic
//    Formula used : Gain (dB) = 6*coarse_gain + 20*log10(fine_gain/128)
//
// INPUTS:
//    - r0 = fine gain (uint16)
//    - r1 = coarse gain (int16)
//
// OUTPUTS:
//    - r0 = gain value in dB in Q12.20
//
// TRASHED REGISTERS:
//    C compliant
//
// NOTES: If fine gain is 0, gain value returned is INT_MIN
//
// *****************************************************************************

.MODULE $M.aanc_proc.calc_gain_db;
    .CODESEGMENT PM;

$_aanc_proc_calc_gain_db:

    pushm <r6, rLink>;

    Null = r0;
    if Z jump return_early;

calc_fine_gain_db:
    rMAC = r0; // Copy fine gain to rMAC, in Q40.32
    r3 = r1 * $aanc_proc.COARSE_GAIN_TO_DB (int); // Store coarse gain dB value in r3

    // Fine gain measured relative to 128, so needs to be scaled by 2^-7
    // log2_table takes input (rMAC) in Q9.63, so shift rMAC by 63-32-7=24
    rMAC = rMAC ASHIFT $aanc_proc.FINE_GAIN_LOG2_SHIFT_AMT (72bit);
    r3 = r3 ASHIFT $aanc_proc.COARSE_GAIN_DB_SHIFT_AMT; // Coarse gain (dB) in Q12.20

    call $math.log2_table;

    // Convert to dB by using multiplying factor of 20/log2(10)
    rMAC = r0 * $aanc_proc.LOG2_TO_DB_CONV_FACTOR;
    r0 = rMAC + r3; // Add coarse gain (dB) and fine gain (dB)
    jump return_db_gain;

return_early:
    r0 = MININT;

return_db_gain:
    popm <r6, rLink>;

    rts;

.ENDMODULE;