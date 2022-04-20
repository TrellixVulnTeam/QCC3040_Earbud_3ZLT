// *****************************************************************************
// Copyright (c) 2007 - 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

#ifndef MASK100_LIB_H
#define MASK100_LIB_H

#include "portability_macros.h"

// -----------------------------------------------------------------------------
// mask100 constants
// -----------------------------------------------------------------------------
#define $mask100.gainLBWeight                      Qfmt_(1.0, 4)
#define $mask100.gainHBWeight                      Qfmt_(1.0, 4)
#define $mask100.attenuationGain                   Qfmt_(0.5, 4)
#define $mask100.lowerThreshold                    0.6
#define $mask100.upperThreshold                    0.92

#define $mask100.npc.DMS_Ltilt                     Qfmt_(1.0, 8)
#define $mask100.npc.upperThreshold                0.75
#define $mask100.npc.gainHBWeight                  Qfmt_(1.0, 4)

// -----------------------------------------------------------------------------
// mask100 data structure
// -----------------------------------------------------------------------------
.CONST $mask100.Y_FIELD                            MK1 * 0;
.CONST $mask100.X_FIELD                            MK1 * 1;
.CONST $mask100.PTR_G_FIELD                        MK1 * 2;
.CONST $mask100.PTR_VARIANT_FIELD                  MK1 * 3;
.CONST $mask100.PTR_SCRATCH_FIELD                  MK1 * 4;
.CONST $mask100.GAIN_HB_FIELD                      MK1 * 5;
.CONST $mask100.LOWER_THRES_FIELD                  MK1 * 6;
.CONST $mask100.UPPER_THRES_FIELD                  MK1 * 7;
.CONST $mask100.ATT_GAIN_FIELD                     MK1 * 8;
.CONST $mask100.PARAM_CFREQ_FIELD                  MK1 * 9;
.CONST $mask100.PARAM_YWEI_FIELD                   MK1 * 10;
.CONST $mask100.PTR_MASK_FIELD                     MK1 * 11;
.CONST $mask100.MASK_GAIN_SHARED_FIELD             MK1 * 12;
.CONST $mask100.SCTRACH_G_DMS_FIELD                MK1 * 13;
.CONST $mask100.SCTRACH_GAIN_FIELD                 MK1 * 14;
.CONST $mask100.FFTLEN_FIELD                       MK1 * 15;
.CONST $mask100.UPPER_BAND_START_FIELD             MK1 * 16;
.CONST $mask100.STRUC_SIZE                               17;

#endif // MASK100_LIB_H
