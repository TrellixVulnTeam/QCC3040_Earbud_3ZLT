############################################################################
# CONFIDENTIAL
# Copyright (c) 2020 - 2020 Qualcomm Technologies International, Ltd.
#
############################################################################
#
# COMPONENT:    makefile
# MODULE:       support_lib/cbops
# DESCRIPTION:  Library makefile.
#
############################################################################


#########################################################################
# Define root directory (relative so we can be installed anywhere)
#########################################################################

BUILD_ROOT = ../../build
include $(BUILD_ROOT)/roots.mkf

#########################################################################
# Enter source files and directories and header directories here.
#
# Makerules will add the standard interface paths
#########################################################################

GEN_ASM_HDRS += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), cbops_sidetone_mix_op.h, )))
GEN_ASM_HDRS += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), cbops_howling_limiter_c.h, )))
GEN_ASM_DEFS += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), multichan_st_mix, )))
GEN_ASM_DEFS += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), cbops_howling_limiter, )))

C_SRC += cbops_support_lib_dummy.c
S_SRC += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), cbops_sidetone_mix_operator_extra.asm, )))
S_SRC += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), dsl_cbops_howling_limiter.asm, )))
C_SRC += cbops_sidetone_mix_op_extra.c
C_SRC += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), cbops_mute_op_extra.c, )))
C_SRC += $(if $(findstring $(SQIF_CONFIG), True), ,$(if $(findstring $(KALSIM_CONFIG), True), ,$(if $(findstring $(CAPABILITY_DOWNLOAD_BUILD), true), dsl_cbops_howling_limiter_op.c, )))

# Enter final target file here (only 1 target should be specified)
#########################################################################

TARGET_EXE_ROOT =
TARGET_LIB_ROOT = cbops 

#########################################################################
# Include the standard definitions and rules
#########################################################################

include $(BUILD_ROOT)/makerules.mkf
