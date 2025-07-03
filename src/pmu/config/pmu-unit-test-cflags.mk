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

###############################################################################
# Target inspecific paths: these are paths used by the build but not dependent
# upon the build.
###############################################################################

PMU_SW             = $(LW_SOURCE)/pmu_sw
PMU_CFG            = $(LW_SOURCE)/pmu_sw/config
PMU_BUILD          = $(LW_SOURCE)/pmu_sw/build
LWUPROC_RISCV      = $(LW_SOURCE)/uproc/lwriscv
LWUPROC            = $(LW_SOURCE)/uproc
RESMAN_ROOT        = $(LW_SOURCE)/drivers/resman
BUILD_SCRIPTS      = $(LWUPROC)/build/scripts
MUTEX_SRC          = $(LWUPROC)/libs/mutex
LIB_UPROC_CMN_SRC  = $(LWUPROC)/libs/cmn
SCP_SRC            = $(LWUPROC)/libs/scp

###############################################################################
# Set profile-specific make vars. This includes things like the target falcon
# architecture (ex. falcon4), the linker-script to use, the manual directory,
# etc ...).
###############################################################################

PMUCFG_PROFILE ?= pmu-tu10x
include $(PMU_CFG)/pmu-profiles.mk

ifeq ($(LS_FALCON_TEGRA),true)
    ACR_CMN_SRC = $(LW_SOURCE)/uproc/tegra_acr
else ifeq ($(LS_FALCON),true)
    ACR_CMN_SRC = $(LW_SOURCE)/uproc/acr
endif

# ifneq (,$(findstring lwriscv,$(UPROC_ARCH)))
  # LDR_SRC            = $(LWUPROC_RISCV)/bootloader/src
  # LIB_UPROC_LWOS_SRC = $(LWUPROC_RISCV)/drivers

LDR_SRC            = $(LWUPROC)/bootloader/src
LIB_UPROC_LWOS_SRC = $(LWUPROC)/libs/lwos/$(LWOS_VERSION)

##############################################################################
# In PMU, the CSB access only takes the register offset as input (i.e. base
# address is not required), thus setting this flag informing the RTOS to do the
# CSB access with offset only
###############################################################################

BASEADDR_NEEDED_FOR_CSB_ACCESS = 0

###############################################################################
# Load common falcon/riscv make vars and host command definitions (ie. MKDIR,
# P4, COPY, etc...).
###############################################################################

# ifneq (,$(findstring lwriscv,$(UPROC_ARCH)))
  # include $(LWUPROC)/build/common/lwRiscvArch.lwmk
  # PMU_TARGET_ARCH = $(RISCV_TOOLS_BUILD)
  # PMU_PROJ        = $(PROJ)
  # PMU_TARGET_OS   = riscv
include $(LWUPROC)/build/common/lwFalconArch.lwmk
PMU_TARGET_ARCH = $(FALCON_TOOLS_BUILD)
PMU_PROJ        = $(PROJ)
PMU_TARGET_OS   = falcon

#
# Switching away from jump tables to reclaim precious DMEM. For now changing
# only falcon6 but after more testing will try to switch them all.
#
ifeq ($(FALCON_ARCH),falcon6)
    UNIT_CFLAGS += -fno-jump-tables
endif

ifeq ($(DMA_SUSPENSION),true)
    UNIT_CFLAGS += -DDMA_SUSPENSION
endif

ifeq ($(FLCNDBG_ENABLED),true)
    UNIT_CFLAGS += -DFLCNDBG_ENABLED
endif

ifeq ($(DMEM_VA_SUPPORTED),true)
    UNIT_CFLAGS += -DDMEM_VA_SUPPORTED
endif

ifeq ($(OS_CALLBACKS),true)
    UNIT_CFLAGS += -DOS_CALLBACKS
endif

ifeq ($(OS_CALLBACKS_WSTATS),true)
    UNIT_CFLAGS += -DOS_CALLBACKS_WSTATS
endif

ifeq ($(OS_CALLBACKS_SINGLE_MODE),true)
    UNIT_CFLAGS += -DOS_CALLBACKS_SINGLE_MODE
endif

ifeq ($(MRU_OVERLAYS),true)
    UNIT_CFLAGS += -DMRU_OVERLAYS
endif

ifeq ($(DMREAD_WAR_200142015),true)
    UNIT_CFLAGS += -DDMREAD_WAR_200142015
endif

ifeq ($(DMTAG_WAR_1845883),true)
    UNIT_CFLAGS += -DDMTAG_WAR_1845883
endif

ifdef BASEADDR_NEEDED_FOR_CSB_ACCESS
    UNIT_CFLAGS += -DBASEADDR_NEEDED=$(BASEADDR_NEEDED_FOR_CSB_ACCESS)
endif

ifeq ($(ON_DEMAND_PAGING_BLK),true)
    UNIT_CFLAGS += -DON_DEMAND_PAGING_BLK
endif

ifeq ($(IS_SSP_ENABLED), true)
#   leave -fstack-protector-strong disabled for now. Enabling it causes UTF
#   to hang. TODO: investigate this.  
#   UNIT_CFLAGS += -fstack-protector-strong
    UNIT_CFLAGS += -DIS_SSP_ENABLED
endif

ifeq ($(FB_QUEUES), true)
    UNIT_CFLAGS += -DFB_QUEUES
endif

ifneq (,$(findstring SafeRTOS, $(RTOS_VERSION)))
    UNIT_CFLAGS += -DSAFERTOS
endif

ifeq ($(QUEUE_HALT_ON_FULL), true)
    UNIT_CFLAGS += -DQUEUE_HALT_ON_FULL
endif

ifeq ($(SCHEDULER_2X), true)
    UNIT_CFLAGS += -DSCHEDULER_2X
endif

ifeq ($(HEAP_BLOCKING), true)
    UNIT_CFLAGS += -DHEAP_BLOCKING
endif

ifeq ($(WATCHDOG_ENABLE), true)
    UNIT_CFLAGS += -DWATCHDOG_ENABLE
endif

ifeq ($(PMU_TARGET_OS),falcon)
    UNIT_CFLAGS += -DUPROC_FALCON
else ifeq ($(PMU_TARGET_OS),riscv)
    UNIT_CFLAGS += -DUPROC_RISCV
endif

ifeq ($(TASK_QUEUE_MAP), true)
    UNIT_CFLAGS += -DTASK_QUEUE_MAP
endif

ifeq ($(UINT_OVERFLOW_CHECK), true)
    UNIT_CFLAGS += -DUINT_OVERFLOW_CHECK
endif

ifeq ($(PA_47_BIT_SUPPORTED), true)
    UNIT_CFLAGS += -DPA_47_BIT_SUPPORTED
endif

ifeq ($(EXCLUDE_LWOSDEBUG), true)
    UNIT_CFLAGS += -DEXCLUDE_LWOSDEBUG
endif

UNIT_CFLAGS += -DLW_MISRA_COMPLIANCE_REQUIRED

# ###############################################################################
# # Additional defines to provide to the compiler and assembler.
# ###############################################################################

UNIT_DEFINES += __$(FALCON_ARCH)__
UNIT_DEFINES += PMU_RTOS

###############################################################################
# Define the output directory paths for the PMU build and all sub-make builds.
###############################################################################

PMUOUTPUTDIR := _out/$(subst pmu-,,$(PMUCFG_PROFILE))
PMUCMNOUTDIR := _out/cmn

###############################################################################
# RTOS Configuration
###############################################################################
include $(LWUPROC)/build/common/lwRtosVersion.lwmk

##############################################################################
# Additional include paths required
##############################################################################

UNIT_INCLUDES += $(LIB_UPROC_LWOS_SRC)/inc
UNIT_INCLUDES += $(PMU_SW)/inc
UNIT_INCLUDES += $(LW_SOURCE)/uproc/libs/cmn/inc
UNIT_INCLUDES += $(LW_SOURCE)/drivers/common/inc
UNIT_INCLUDES += $(LW_SOURCE)/drivers/common/inc/hwref
UNIT_INCLUDES += $(RESMAN_ROOT)/arch/lwalloc/common/inc
UNIT_INCLUDES += $(LW_RTOS_INCLUDES)
UNIT_INCLUDES += $(FALCON_TOOLS)/include
UNIT_INCLUDES += $(MANUAL_PATHS)
UNIT_INCLUDES += $(PMU_SW)/prod_app/$(PMUOUTPUTDIR)
UNIT_INCLUDES += $(PMU_SW)/prod_app/$(PMUOUTPUTDIR)/rpcgen
UNIT_INCLUDES += $(PMU_SW)/prod_app/$(PMUCMNOUTDIR)
UNIT_INCLUDES += $(MUTEX_SRC)/inc
UNIT_INCLUDES += $(LIB_UPROC_CMN_SRC)/inc
UNIT_INCLUDES += $(SCP_SRC)/inc
UNIT_INCLUDES += $(LW_SOURCE)/uproc/libs/vbios/inc

ifeq ($(LS_FALCON),true)
    UNIT_INCLUDES += $(LW_SOURCE)/uproc/acr/inc
else ifeq ($(LS_FALCON_TEGRA),true)
    UNIT_INCLUDES += $(LW_SOURCE)/uproc/tegra_acr/inc
endif
