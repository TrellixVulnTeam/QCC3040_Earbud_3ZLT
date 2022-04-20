// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __AANC_LITE_GEN_C_H__
#define __AANC_LITE_GEN_C_H__

#ifndef ParamType
typedef unsigned ParamType;
#endif

// CodeBase IDs
#define AANC_LITE_AANC_LITE_16K_CAP_ID	0x00C9
#define AANC_LITE_AANC_LITE_16K_ALT_CAP_ID_0	0x40A1
#define AANC_LITE_AANC_LITE_16K_SAMPLE_RATE	16000
#define AANC_LITE_AANC_LITE_16K_VERSION_MAJOR	1

// Constant Definitions


// Runtime Config Parameter Bitfields
// AANC_LITE_CONFIG bits
#define AANC_LITE_CONFIG_AANC_LITE_CONFIG_DISABLE_GAIN_RAMP		0x00000001


// System Mode
#define AANC_LITE_SYSMODE_STANDBY		0
#define AANC_LITE_SYSMODE_FULL   		1
#define AANC_LITE_SYSMODE_STATIC 		2
#define AANC_LITE_SYSMODE_MAX_MODES		3

// ANC FF Fine Gain

// Statistics Block
typedef struct _tag_AANC_LITE_STATISTICS
{
	ParamType OFFSET_CUR_MODE;
	ParamType OFFSET_FF_FINE_GAIN_CTRL;
}AANC_LITE_STATISTICS;

typedef AANC_LITE_STATISTICS* LP_AANC_LITE_STATISTICS;

// Parameters Block
typedef struct _tag_AANC_LITE_PARAMETERS
{
	ParamType OFFSET_AANC_LITE_CONFIG;
	ParamType OFFSET_GAIN_RAMP_TIMER;
}AANC_LITE_PARAMETERS;

typedef AANC_LITE_PARAMETERS* LP_AANC_LITE_PARAMETERS;

unsigned *AANC_LITE_GetDefaults(unsigned capid);

#endif // __AANC_LITE_GEN_C_H__
