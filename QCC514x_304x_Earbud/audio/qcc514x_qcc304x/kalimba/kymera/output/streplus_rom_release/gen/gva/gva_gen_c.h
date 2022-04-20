// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __GVA_GEN_C_H__
#define __GVA_GEN_C_H__

#ifndef ParamType
typedef unsigned ParamType;
#endif

// CodeBase IDs
#define GVA_GVA_CAP_ID	0x00C5
#define GVA_GVA_ALT_CAP_ID_0	0x409B
#define GVA_GVA_SAMPLE_RATE	16000
#define GVA_GVA_VERSION_MAJOR	1

// Constant Definitions


// Runtime Config Parameter Bitfields


// System Mode
#define GVA_SYSMODE_STATIC   		0
#define GVA_SYSMODE_FULL_PROC		1
#define GVA_SYSMODE_PASS_THRU		2
#define GVA_SYSMODE_MAX_MODES		3

// System Control
#define GVA_CONTROL_MODE_OVERRIDE		0x2000

// Statistics Block
typedef struct _tag_GVA_STATISTICS
{
	ParamType OFFSET_CUR_MODE;
	ParamType OFFSET_OVR_CONTROL;
	ParamType OFFSET_GVA_DETECTED;
	ParamType OFFSET_GVA_KEYWORD_LEN;
}GVA_STATISTICS;

typedef GVA_STATISTICS* LP_GVA_STATISTICS;

unsigned *GVA_GetDefaults(unsigned capid);

#endif // __GVA_GEN_C_H__
