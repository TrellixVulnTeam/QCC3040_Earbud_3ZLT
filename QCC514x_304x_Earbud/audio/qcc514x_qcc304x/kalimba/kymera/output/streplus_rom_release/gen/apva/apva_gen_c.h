// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __APVA_GEN_C_H__
#define __APVA_GEN_C_H__

#ifndef ParamType
typedef unsigned ParamType;
#endif

// CodeBase IDs
#define APVA_APVA_CAP_ID	0x00C6
#define APVA_APVA_ALT_CAP_ID_0	0x409C
#define APVA_APVA_SAMPLE_RATE	16000
#define APVA_APVA_VERSION_MAJOR	1

// Constant Definitions


// Runtime Config Parameter Bitfields


// System Mode
#define APVA_SYSMODE_STATIC   		0
#define APVA_SYSMODE_FULL_PROC		1
#define APVA_SYSMODE_PASS_THRU		2
#define APVA_SYSMODE_MAX_MODES		3

// System Control
#define APVA_CONTROL_MODE_OVERRIDE		0x2000

// Statistics Block
typedef struct _tag_APVA_STATISTICS
{
	ParamType OFFSET_CUR_MODE;
	ParamType OFFSET_OVR_CONTROL;
	ParamType OFFSET_APVA_DETECTED;
	ParamType OFFSET_APVA_RESULT_CONFIDENCE;
	ParamType OFFSET_APVA_KEYWORD_LEN;
}APVA_STATISTICS;

typedef APVA_STATISTICS* LP_APVA_STATISTICS;

// Parameters Block
typedef struct _tag_APVA_PARAMETERS
{
	ParamType OFFSET_APVA_THRESHOLD;
}APVA_PARAMETERS;

typedef APVA_PARAMETERS* LP_APVA_PARAMETERS;

unsigned *APVA_GetDefaults(unsigned capid);

#endif // __APVA_GEN_C_H__
