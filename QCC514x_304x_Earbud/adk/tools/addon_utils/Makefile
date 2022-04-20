# Copyright (c) 2019 Qualcomm Technologies International, Ltd.

# Name of addon dir must match name of project file, which should be the name of feature
ADDON_PROJECT_FILE:=$(wildcard *.x2p)
ADDON_NAME:=$(subst .x2p,,$(ADDON_PROJECT_FILE))
ADK_DIR:=../../../../../adk
ADK_TOOLS_DIR:=$(ADK_DIR)/tools
TYPEDEF_GEN:=$(ADK_TOOLS_DIR)/packages/typegen/build.py
ADDON_SPECIAL_TOOLS_DIR:=../../../tools
BUILDOUTPUT_PATH?=./build
INSTALL_DIR=../../../../../adk/src/installed_libs
COPYFILE=$(DEVKIT_ROOT)/tools/bin/copyfile.exe
REMOVEFILE=$(DEVKIT_ROOT)/tools/bin/remove.exe
KCC_DIR?=$(DEVKIT_ROOT)/tools/kcc
CC=$(KCC_DIR)/bin/kcc
AR=$(KCC_DIR)/bin/kar
LIB_DIR=$(BUILDOUTPUT_PATH)/lib
OBJECT_DIR=$(BUILDOUTPUT_PATH)/intermediate
BUILT_LIB=lib$(ADDON_NAME).a
MERGED_LIB=lib$(ADDON_NAME)_addon.a
GATTDBGEN = $(DEVKIT_ROOT)/tools/bin/gattdbgen.exe
MAKE_OUTPUT_DIR=$(call MKDIR,${@D})

ifeq ($(DEVKIT_ROOT),)
	PYTHON=python
else
	PYTHON=$(DEVKIT_ROOT)/tools/python27/python.exe
endif

ifeq ($(OS),Windows_NT)
	RMDIR=rmdir /S /Q
	FIXPATH=$(subst /,\,$1)
	MKDIR=-if not exist $(call FIXPATH,$1) mkdir $(call FIXPATH,$1)
else
	RMDIR=rm -rf
	FIXPATH=$1
	MKDIR=mkdir -p $1
endif

# Get dbi files
ALL_FILES=$(shell $(PYTHON) $(ADK_TOOLS_DIR)/addon_utils/addon_utils.py input $(ADDON_PROJECT_FILE) --file_paths)
DBI_FILES:=$(filter %.dbi, $(ALL_FILES))
VPATH = $(sort $(dir $(filter %.c,$(ALL_FILES))) $(dir $(filter %.h,$(ALL_FILES))))
C_FILES=$(notdir $(filter %.c,$(ALL_FILES)))
H_FILES=$(notdir $(filter %.h,$(ALL_FILES)))
C_OBJECTS=$(patsubst %.c,$(OBJECT_DIR)/%.o,$(C_FILES))

# Get common & addon specific defines
COMMON_DEFINES:=-D__KALIMBA__ -DHAVE_32BIT_DATA_WIDTH -DBLUELAB -DINSTALL_HYDRA_LOG -DHYDRACORE -DGATT_DBI_LIB -DHYDRA_LOG_MAX_LEVEL=5
C_DEFINES+=$(COMMON_DEFINES) $(shell $(PYTHON) $(ADK_TOOLS_DIR)/addon_utils/addon_utils.py input $(ADDON_PROJECT_FILE) --configs DEFS)

# Compiler warnings & warning suppression
WARNING_OPTS=-Wall -WAccuracyLoss -WnoConstantTest -WCharPointerUnaligned
ifneq "$(EXTRA_WARNINGS)" "TRUE"
	WARNING_OPTS += -WnoAccuracyLoss -WnoArgSizeLarger -WnoPointerUnaligned -WnoExplicitQualifier -WnoCharPointerUnaligned -WnoUnsignedCompare
endif

$(info BUILDING FOR $(CHIP_TYPE))
KCC_OPTION_TARGET_CHIP_NAME =-k$(CHIP_NAME)
ADDON_MRI=addon_$(word 1,$(subst _, , $(CHIP_NAME))).mri

# Compiler flags
PREINCLUDES=-preinclude hydra_types.h -preinclude hydra_macros.h -preinclude hydra_dev.h
X2P_INCLUDE_PATHS=$(foreach inc,$(INCPATHS),-I$(inc))
COMPILE_FLAGS=-g -minim $(KCC_OPTION_TARGET_CHIP_NAME) -Xassembler -apply-b179745-workaround $(WARNING_OPTS)
COMPILE_FLAGS += $(PREINCLUDES) $(X2P_INCLUDE_PATHS) $(C_DEFINES)

# If the project property 'Build Type' is set to 'Optimised' use the space optimised compiler options, otherwise use debugging friendly options.
ifeq ("$(BUILD_TYPE)","RELEASE")
	COMPILE_FLAGS += -DRELEASE_BUILD -O2
else
	COMPILE_FLAGS += -DDEBUG_BUILD -O0
endif

all: $(LIB_DIR)/$(MERGED_LIB)

# Merged library
$(LIB_DIR)/$(MERGED_LIB): $(patsubst %.dbi,%.h,$(DBI_FILES)) $(LIB_DIR)/$(BUILT_LIB)
	echo Creating merged libs lib$(ADDON_NAME)_addon.a
	$(AR) -M < $(ADDON_SPECIAL_TOOLS_DIR)/ar_script/$(ADDON_MRI)
	$(COPYFILE) $(LIB_DIR)/$(MERGED_LIB) $(INSTALL_DIR)/lib/default_$(CHIP_TYPE)/native/$(MERGED_LIB)
	echo Exporting lib$(ADDON_NAME)_addon.a

# Typedef generated files
$(BUILDOUTPUT_PATH)\gen_typedefs.log:
	$(MAKE_OUTPUT_DIR)
	echo Generating typedefs > $@
	$(PYTHON) $(ADK_TOOLS_DIR)\packages\typegen\build.py --file  $(ADDON_PROJECT_FILE)	

# Gatt DB generated files
%.h: %.dbi
	echo Running gattdbgen on $<
	$(KCC_DIR)/bin/kas --preprocess-only -MD -MF $(<).d -MT $(<)_ $(<) -o $(<)_
	$(PYTHON) -c "with open('$(<)_'.strip()) as F: print(''.join([L for L in F.readlines() if 'linefile'.strip() not in L]))" > $(basename $(<))_stripped.dbi_
	$(GATTDBGEN) -i $(basename $(<))_stripped.dbi_ $(basename $(<)).h
	$(REMOVEFILE) $(<)_
	$(REMOVEFILE) $(basename $(<))_stripped.dbi_

# Addon source library
$(LIB_DIR)/$(BUILT_LIB): $(BUILDOUTPUT_PATH)\gen_typedefs.log $(C_OBJECTS)
	echo Building addon source library
	$(MAKE_OUTPUT_DIR)
	$(AR) ru $@ $(C_OBJECTS)

# Intermediate (object) files
$(OBJECT_DIR)/%.o: %.c $(BUILDOUTPUT_PATH)\gen_typedefs.log $(H_FILES) $(ADDON_PROJECT_FILE)
	echo Compiling $<
	$(MAKE_OUTPUT_DIR)
	$(CC) $(COMPILE_FLAGS) -o $@ -c $<

.PHONY: clean
clean:
	-$(RMDIR) $(call FIXPATH,$(BUILDOUTPUT_PATH))
	$(REMOVEFILE) $(INSTALL_DIR)/lib/default_$(CHIP_TYPE)/native/$(MERGED_LIB)
	$(foreach f, $(patsubst %.dbi,%.h,$(DBI_FILES)),$(REMOVEFILE) $(f))
	$(foreach f, $(patsubst %.dbi,%.dbi.d,$(DBI_FILES)),$(REMOVEFILE) $(f))
