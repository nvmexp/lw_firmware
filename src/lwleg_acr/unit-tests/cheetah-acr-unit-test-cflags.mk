#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

###############################################################################
# Target inspecific paths: these are paths used by the build but not dependent
# upon the build.
###############################################################################

ACR_SW             = $(LW_SOURCE)/uproc/tegra_acr
PMU_SW             = $(LW_SOURCE)/pmu_sw
PMU_CFG            = $(LW_SOURCE)/pmu_sw/config
PMU_BUILD          = $(LW_SOURCE)/pmu_sw/build
LWUPROC_RISCV      = $(LW_SOURCE)/uproc/lwriscv
LWUPROC            = $(LW_SOURCE)/uproc
RESMAN_ROOT        = $(LW_SOURCE)/drivers/resman
BUILD_SCRIPTS      = $(LWUPROC)/build/scripts
LIB_UPROC_CMN_SRC  = $(LWUPROC)/libs/cmn

###############################################################################
# Set profile-specific make vars. This includes things like the target falcon
# architecture (ex. falcon4), the linker-script to use, the manual directory,
# etc ...).
###############################################################################

ACRCFG_PROFILE = acr_pmu-t234_load
ACR_CMN_SRC = $(LW_SOURCE)/uproc/tegra_acr
include $(ACR_SW)/config/acr-profiles.mk

# ifneq (,$(findstring lwriscv,$(UPROC_ARCH)))
  # LDR_SRC            = $(LWUPROC_RISCV)/bootloader/src
  # LIB_UPROC_LWOS_SRC = $(LWUPROC_RISCV)/drivers

LDR_SRC            = $(LWUPROC)/bootloader/src
LIB_UPROC_LWOS_SRC = $(LWUPROC)/libs/lwos/$(LWOS_VERSION)

###############################################################################
# Load common falcon/riscv make vars and host command definitions (ie. MKDIR,
# P4, COPY, etc...).
###############################################################################
include $(LWUPROC)/build/common/lwFalconArch.lwmk
PMU_TARGET_ARCH = $(FALCON_TOOLS_BUILD)
PMU_PROJ        = $(PROJ)
PMU_TARGET_OS   = falcon
#include $(LW_SOURCE)/drivers/common/build/lwCommon.lwmk

#
# Switching away from jump tables to reclaim precious DMEM. For now changing
# only falcon6 but after more testing will try to switch them all.
#
UNIT_CFLAGS += -fno-jump-tables

# Set macro to identify which profile
ifeq ($(ACR_FALCON_PMU), true)
    UNIT_CFLAGS += -DACR_FALCON_PMU
    UNIT_CFLAGS += -DDMEM_APERT_ENABLED
    UNIT_CFLAGS += -DLW_MISRA_COMPLIANCE_REQUIRED
endif

ifeq ($(ACR_LOAD), true)
	UNIT_CFLAGS += -DACR_LOAD_PATH
endif

ifeq ($(ACR_ONLY_BO), true)
    UNIT_CFLAGS += -DACR_ONLY_BO
endif

ifeq ($(ACR_SIG_VERIF), true)
	UNIT_CFLAGS += -DACR_SIG_VERIF
endif

ifeq ($(ACR_SAFE_BUILD), true)
	UNIT_CFLAGS += -DACR_SAFE_BUILD
endif

ifeq ($(ACR_SKIP_SCRUB), true)
	UNIT_CFLAGS += -DACR_SKIP_SCRUB
endif

ifeq ($(ACR_FIXED_TRNG), true)
	UNIT_CFLAGS += -DACR_FIXED_TRNG
endif

ifeq ($(ACR_BUILD_ONLY), true)
    UNIT_CFLAGS += -DACR_BUILD_ONLY
endif

ifeq ($(ACR_FMODEL_BUILD), true)
    UNIT_CFLAGS += -DACR_FMODEL_BUILD
endif

ifeq ($(ACR_SUB_WPR), true)
    UNIT_CFLAGS += -DACR_SUB_WPR
endif

ifeq ($(ACR_MMU_INDEXED), true)
    UNIT_CFLAGS += -DACR_MMU_INDEXED
endif

ifeq ($(LW_TARGET_OS),falcon)
  UNIT_CFLAGS += -DUPROC_FALCON
endif

ifeq ($(SSP_ENABLED), true)
   #RANDOM_CANARY_NS := $(shell $(PYTHON) -c 'from random import randint; print(randint(65535, 1048576))')
   #UNIT_CFLAGS += -DIS_SSP_ENABLED
   #UNIT_CFLAGS += -fstack-protector-all
   #UNIT_CFLAGS += -DRANDOM_CANARY_NS=$(RANDOM_CANARY_NS)
endif

ifeq ($(FALCON_ARCH),falcon6)
    UNIT_CFLAGS += -fno-jump-tables
endif

ifeq ($(LS_FALCON),true)
    UNIT_CFLAGS += -DDMA_REGION_CHECK
endif

ifeq ($(PMU_TARGET_OS),falcon)
    UNIT_CFLAGS += -DUPROC_FALCON
endif

UNIT_CFLAGS += -DLW_MISRA_COMPLIANCE_REQUIRED

# ###############################################################################
# # Additional defines to provide to the compiler and assembler.
# ###############################################################################

UNIT_DEFINES += __$(FALCON_ARCH)__

###############################################################################
# Define the output directory paths for the ACR build and all sub-make builds.
###############################################################################

CHIPNAME  := $(subst acr_,,$(ACRCFG_PROFILE))
CHIPNAME  := $(subst pmu-,,$(CHIPNAME))
CHIPNAME  := $(subst sec2-,,$(CHIPNAME))
FALCONNAME := pmu

###############################################################################
# Define the output directory paths for the CHEETAH-ACR build and all sub-make builds.
###############################################################################

OUTPUTDIR := _out/$(FALCONNAME)/$(CHIPNAME)
###############################################################################
# RTOS Configuration
###############################################################################
include $(LWUPROC)/build/common/lwRtosVersion.lwmk

##############################################################################
# Additional include paths required
##############################################################################

UNIT_INCLUDES += $(LIB_UPROC_LWOS_SRC)/inc
UNIT_INCLUDES += $(LW_SOURCE)/uproc/libs/cmn/inc
UNIT_INCLUDES += $(LW_SOURCE)/drivers/common/inc
UNIT_INCLUDES += $(LW_SOURCE)/drivers/common/inc/swref
UNIT_INCLUDES += $(LW_SOURCE)/drivers/common/inc/hwref
UNIT_INCLUDES += $(RESMAN_ROOT)/kernel/inc
UNIT_INCLUDES += $(RESMAN_ROOT)/arch/lwalloc/common/inc
UNIT_INCLUDES += $(LW_RTOS_INCLUDES)
UNIT_INCLUDES += $(FALCON_TOOLS)/include
UNIT_INCLUDES += $(MANUAL_PATHS)
UNIT_INCLUDES += $(ACR_SW)/src/$(OUTPUTDIR)
UNIT_INCLUDES += $(ACR_SW)/src/$(OUTPUTDIR)/config
UNIT_INCLUDES += $(MUTEX_SRC)/inc
UNIT_INCLUDES += $(LIB_UPROC_CMN_SRC)/inc
UNIT_INCLUDES += $(LW_SOURCE)/uproc/tegra_acr/inc
UNIT_INCLUDES += $(LW_SOURCE)/uproc/tegra_acr/inc/external
