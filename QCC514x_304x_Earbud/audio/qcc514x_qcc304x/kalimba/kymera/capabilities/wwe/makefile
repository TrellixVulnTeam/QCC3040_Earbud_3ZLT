#****************************************************************************
#* Copyright (c) 2019 Qualcomm Technologies International, Ltd.
#****************************************************************************

############################################################################
#
# COMPONENT:    makefile
# MODULE:       wwe
# DESCRIPTION:  Capability library makefile.
#
############################################################################


#########################################################################
# Define root directory (relative so we can be installed anywhere)
#########################################################################

BUILD_ROOT = ../../build
include $(BUILD_ROOT)/roots.mkf

#CPS XML TODO: Re-enable after adding XML
# CPS_XML = wwe.xml

# Convert C Header to assembly
GEN_ASM_HDRS += wwe_struct.h
GEN_ASM_DEFS += WWE_OP_DATA

#########################################################################
# Enter source files and directories and header directories here.
#
# Makerules will add the standard interface paths
#########################################################################

H_PATH +=       $(call ospath,../../../lib/malloc)
C_SRC +=         wwe.c
S_SRC +=         wwe_proc.asm

PATCH_SRC = wwe_proc.asm

GEN_PIDS = $(PATCH_DIR)/capability_wwe_ids.txt

#########################################################################
# Enter final target file here (only 1 target should be specified)
#########################################################################

TARGET_EXE_ROOT =
TARGET_LIB_ROOT = wwe

#########################################################################
# Include the standard definitions and rules
#########################################################################
#CPS xml TODO: Re-enable after adding XML
# include $(BUILD_ROOT)/makerules_cps.mkf
include $(BUILD_ROOT)/makerules.mkf

