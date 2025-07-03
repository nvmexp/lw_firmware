#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# File: vcast.mk
# Description: Vectorcast related macros and commands
#

# ==============================================================================
# Environment checks
# ==============================================================================
ifndef P4ROOT
    $(error P4ROOT is not defined)
endif

ifndef VECTORCAST_DIR
    $(error VECTORCAST_DIR is not defined)
    ifneq (,$(findstring Linux, $(LW_HOST_OS)))
        $(error "Please sync Vectorcast files and do:")
        $(error "source $(P4ROOT)/sw/tools/VectorCAST/linux/vectorcast_elw.sh")
    endif
endif

# ==============================================================================
# Paths
# ==============================================================================
VCEXTRACT_PATH           := $(P4ROOT)/sw/apps/gpu/drivers/resman/pmu/vcExtract/
VCAST_ELW_BASE_DIRECTORY ?= $(LW_SOURCE)/pmu_sw/prod_app

# ==============================================================================
# Macros need to generate CCAST_.CFG
# ==============================================================================
ifndef VCAST_COVERAGE_TYPE
    VCAST_COVERAGE_TYPE       = statement+branch
endif
ifneq (,$(filter $(VCAST_INSTRUMENT), instrument))
    CFLAGS               += -Wno-uninitialized
    CFLAGS               += -DVCAST_INSTRUMENT
    LW_INCLUDES          += $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw
endif
C_DEFINE_LIST            := $(subst -D,,$(filter -D%, $(CFLAGS)))
TESTABLE_SOURCE_DIRS     := $(subst -I,,$(filter -I%, $(CFLAGS)))
ifeq ($(LW_TARGET_OS),riscv)
    TESTABLE_SOURCE_DIRS     += $(LWRISCV_TOOLS)/lib/gcc/riscv64-elf/8.2.0/include
else
    TESTABLE_SOURCE_DIRS     += $(FALCON_TOOLS)/lib/gcc/falcon-elf/4.3.2/include
endif
export CC
export C_DEFINE_LIST
export TESTABLE_SOURCE_DIRS
export VCAST_COVERAGE_TYPE

# ==============================================================================
# Commands
# ==============================================================================
ifneq (,$(findstring Linux, $(LW_HOST_OS)))
    VCAST_ADD_SOURCE_CMD     := export PATH=/bin:/usr/bin &&
    VCAST_INSTRUMENT_CMD     := export PATH=/bin:/usr/bin &&
    VCAST_INSTRUMENT_ALL_CMD := export PATH=/bin:/usr/bin &&
    VCAST_CLOBBER_CMD        := export PATH=/bin:/usr/bin &&
endif

#
# generate CCAST_.CFG assuming necessary macros setup correctly
# usage: $(VCAST_GENERATE_CCAST_CMD)
#
ifneq (,$(findstring Linux, $(LW_HOST_OS)))
    VCAST_GENERATE_CCAST_CMD := export PATH=/bin:/usr/bin &&                   \
        cd $(VCAST_ELW_BASE_DIRECTORY) &&                                      \
        $(LW_SOURCE)/uproc/build/scripts/vcast/vectorcast_generate_ccast.sh $(LW_TARGET_OS)
else
    VCAST_GENERATE_CCAST_CMD := cd $(VCAST_ELW_BASE_DIRECTORY) &&              \
        $(LW_SOURCE)/uproc/build/scripts/vcast/vectorcast_generate_ccast.bat $(LW_TARGET_OS)
endif

VCAST_CREATE_PROJ_CMD    := cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast cover elw create vcast_elw

VCAST_INIT_PROJ_CMD      := cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover options in_place y

VCAST_ADD_SOURCE_CMD     += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover source add
#
# instrument sources given a list of files
# usage: $(VCAST_INSTRUMENT_CMD) $(SOURCES)
#
VCAST_INSTRUMENT_CMD     += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover instrument                    \
    $(VCAST_COVERAGE_TYPE) --c99 -u
#
# instrument all the sources already added to the Vectorcast project
# usage: $(VCAST_INSTRUMENT_ALL_CMD)
#
VCAST_INSTRUMENT_ALL_CMD += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover instrument                    \
    $(VCAST_COVERAGE_TYPE) --c99
VCAST_CLOBBER_CMD        += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover instrument none

ifdef VCAST_SRC_DIR
FILTERED_SOURCES    := $(filter-out $(VCAST_EXCLUDE_FILES), $(SOURCES))
ifdef VCAST_EXCLUDE
    VCAST_FILES := $(filter-out $(VCAST_EXCLUDE), $(FILTERED_SOURCES))
endif

ifneq (,$(filter $(VCAST_INSTRUMENT), add_source))
    $(OBJECTS): VC_ADD_FILES
else ifneq (,$(filter $(VCAST_INSTRUMENT), instrument))
    $(OBJECTS): VC_INSTRUMENT
endif

VC_ADD_FILES:
ifneq ($(LW_DVS_BLD),1)
	$(P4) edit                                                                 \
	    $(addprefix $(VCAST_SRC_DIR), $(FILTERED_SOURCES))
endif
	$(VCAST_ADD_SOURCE_CMD)                                                    \
	    $(addprefix $(VCAST_SRC_DIR), $(FILTERED_SOURCES))

VC_INSTRUMENT: VC_ADD_FILES
	$(VCAST_INSTRUMENT_CMD)                                                    \
	    "$(addprefix $(VCAST_SRC_DIR), $(FILTERED_SOURCES))"
endif
