// -----------------------------------------------------------------------------
// Copyright (c) 2021                  Qualcomm Technologies International, Ltd.
//
#ifndef __XOVER_GEN_ASM_H__
#define __XOVER_GEN_ASM_H__

// CodeBase IDs
.CONST $M.XOVER_XOVER_CAP_ID       	0x000033;
.CONST $M.XOVER_XOVER_ALT_CAP_ID_0       	0x406B;
.CONST $M.XOVER_XOVER_SAMPLE_RATE       	0;
.CONST $M.XOVER_XOVER_VERSION_MAJOR       	1;

// Constant Values
.CONST $M.XOVER.CONSTANT.XOVER_FILTER_TYPE_Single_precision		0x00000000;
.CONST $M.XOVER.CONSTANT.XOVER_FILTER_TYPE_First_order     		0x00000001;
.CONST $M.XOVER.CONSTANT.XOVER_FILTER_TYPE_Double_precision		0x00000002;
.CONST $M.XOVER.CONSTANT.LP_TYPE_Butterworth               		0x00000001;
.CONST $M.XOVER.CONSTANT.LP_TYPE_Linkwitz_Riley            		0x00000002;
.CONST $M.XOVER.CONSTANT.LP_TYPE_APC                       		0x00000003;
.CONST $M.XOVER.CONSTANT.HP_TYPE_Butterworth               		0x00000001;
.CONST $M.XOVER.CONSTANT.HP_TYPE_Linkwitz_Riley            		0x00000002;
.CONST $M.XOVER.CONSTANT.HP_TYPE_APC                       		0x00000003;


// Piecewise Disables
.CONST $M.XOVER.CONFIG.BYPXFAD  		0x00000001;
.CONST $M.XOVER.CONFIG.INV_BAND1		0x00000002;
.CONST $M.XOVER.CONFIG.INV_BAND2		0x00000004;


// Statistic Block
.CONST $M.XOVER.STATUS.CUR_MODE         		0*ADDR_PER_WORD;
.CONST $M.XOVER.STATUS.OVR_CONTROL      		1*ADDR_PER_WORD;
.CONST $M.XOVER.STATUS.COMPILED_CONFIG  		2*ADDR_PER_WORD;
.CONST $M.XOVER.STATUS.OP_STATE         		3*ADDR_PER_WORD;
.CONST $M.XOVER.STATUS.OP_INTERNAL_STATE		4*ADDR_PER_WORD;
.CONST $M.XOVER.STATUS.BLOCK_SIZE            	5;

// System Mode
.CONST $M.XOVER.SYSMODE.STATIC   		0;
.CONST $M.XOVER.SYSMODE.MUTE     		1;
.CONST $M.XOVER.SYSMODE.FULL     		2;
.CONST $M.XOVER.SYSMODE.PASS_THRU		3;
.CONST $M.XOVER.SYSMODE.MAX_MODES		4;

// System Control
.CONST $M.XOVER.CONTROL.MODE_OVERRIDE		0x2000;

// CompCfg

// Operator state

// Operator internal state

// Parameter Block
.CONST $M.XOVER.PARAMETERS.OFFSET_XOVER_CONFIG     		0*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_NUM_BANDS        		1*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_XOVER_FILTER_TYPE		2*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_LP_TYPE          		3*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_LP_FC            		4*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_LP_ORDER         		5*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_HP_TYPE          		6*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_HP_FC            		7*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.OFFSET_HP_ORDER         		8*ADDR_PER_WORD;
.CONST $M.XOVER.PARAMETERS.STRUCT_SIZE            		9;


#endif // __XOVER_GEN_ASM_H__
