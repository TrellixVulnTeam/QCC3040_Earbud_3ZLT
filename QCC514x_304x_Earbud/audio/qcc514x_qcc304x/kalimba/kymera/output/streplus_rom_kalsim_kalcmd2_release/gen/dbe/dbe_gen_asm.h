// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __DBE_GEN_ASM_H__
#define __DBE_GEN_ASM_H__

// CodeBase IDs
.CONST $M.DBE_DBE_CAP_ID       	0x002f;
.CONST $M.DBE_DBE_ALT_CAP_ID_0       	0x4058;
.CONST $M.DBE_DBE_SAMPLE_RATE       	0;
.CONST $M.DBE_DBE_VERSION_MAJOR       	1;

.CONST $M.DBE_DBE_FULLBAND_CAP_ID       	0x0090;
.CONST $M.DBE_DBE_FULLBAND_ALT_CAP_ID_0       	0x4059;
.CONST $M.DBE_DBE_FULLBAND_SAMPLE_RATE       	0;
.CONST $M.DBE_DBE_FULLBAND_VERSION_MAJOR       	1;

.CONST $M.DBE_DBE_FULLBAND_BASSOUT_CAP_ID       	0x0091;
.CONST $M.DBE_DBE_FULLBAND_BASSOUT_ALT_CAP_ID_0       	0x405A;
.CONST $M.DBE_DBE_FULLBAND_BASSOUT_SAMPLE_RATE       	0;
.CONST $M.DBE_DBE_FULLBAND_BASSOUT_VERSION_MAJOR       	1;

// Constant Values


// Piecewise Disables
.CONST $M.DBE.CONFIG.BYPASS                		0x00000001;
.CONST $M.DBE.CONFIG.BYPASS_XOVER          		0x00000002;
.CONST $M.DBE.CONFIG.BYPASS_BASS_OUTPUT_MIX		0x00000004;


// Statistic Block
.CONST $M.DBE.STATUS.CUR_MODE         		0*ADDR_PER_WORD;
.CONST $M.DBE.STATUS.OVR_CONTROL      		1*ADDR_PER_WORD;
.CONST $M.DBE.STATUS.COMPILED_CONFIG  		2*ADDR_PER_WORD;
.CONST $M.DBE.STATUS.OP_STATE         		3*ADDR_PER_WORD;
.CONST $M.DBE.STATUS.OP_INTERNAL_STATE		4*ADDR_PER_WORD;
.CONST $M.DBE.STATUS.BLOCK_SIZE            	5;

// System Mode
.CONST $M.DBE.SYSMODE.STATIC   		0;
.CONST $M.DBE.SYSMODE.MUTE     		1;
.CONST $M.DBE.SYSMODE.FULL     		2;
.CONST $M.DBE.SYSMODE.PASS_THRU		3;
.CONST $M.DBE.SYSMODE.MAX_MODES		4;

// System Control
.CONST $M.DBE.CONTROL.MODE_OVERRIDE		0x2000;

// CompCfg

// Operator state

// Operator internal state

// Parameter Block
.CONST $M.DBE.PARAMETERS.OFFSET_DBE_CONFIG     		0*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_EFFECT_STRENGTH		1*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_AMP_LIMIT      		2*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_FC_LP          		3*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_FC_HP          		4*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_HARM_CONTENT   		5*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_XOVER_FC       		6*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.OFFSET_MIX_BALANCE    		7*ADDR_PER_WORD;
.CONST $M.DBE.PARAMETERS.STRUCT_SIZE          		8;


#endif // __DBE_GEN_ASM_H__
