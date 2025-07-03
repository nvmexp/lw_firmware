#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available BLCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, uproc architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in BLCFG_PROFILE. To pick-up the definitions, set BLCFG_PROFILE to the
# desired profile and source/include this file. When BLCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# BLCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible bl-config profiles
##############################################################################

BLCFG_PROFILES_ALL :=
BLCFG_PROFILES_ALL += gsp_rvbl_ga10x
BLCFG_PROFILES_ALL += gsp_rvbl_t23xd
BLCFG_PROFILES_ALL += gsp_rvbl_gh100
BLCFG_PROFILES_ALL += sec2_rvbl_gh100
BLCFG_PROFILES_ALL += pmu_rvbl_ga10x
BLCFG_PROFILES_ALL += pmu_rvbl_ga10b
BLCFG_PROFILES_ALL += pmu_rvbl_ga10f
BLCFG_PROFILES_ALL += pmu_rvbl_ga11b
BLCFG_PROFILES_ALL += pmu_rvbl_ad10x
BLCFG_PROFILES_ALL += pmu_rvbl_ad10b
BLCFG_PROFILES_ALL += pmu_rvbl_gh100
BLCFG_PROFILES_ALL += pmu_rvbl_gh20x
BLCFG_PROFILES_ALL += pmu_rvbl_gb100
BLCFG_PROFILES_ALL += pmu_rvbl_g00x

BLSW                = $(LW_SOURCE)/uproc/lwriscv/bootloader


##############################################################################
# Profile-specific settings
#  - LS_UPROC setting is used to determine which (if any) tool should be
#    used to resolve signature requirements for ucode affected by the BL.
#    See ../build/gen_ls_sig.lwmk
##############################################################################

ifdef BLCFG_PROFILE
  MANUAL_PATHS         =
  BLSRC                = main.c debug.c elf.c load.S start.S
  BLSRC_PARSED         =
  LDSCRIPT_IN          = $(BLSW)/build/link_script.ld.in
  LDSCRIPT             = $(OUTPUTDIR)/link_script.ld

  EMEM_SIZE            = 0x02000
  IMEM_SIZE            = 0x10000
  DMEM_SIZE            = 0x10000
  STACK_SIZE           = 0x300
  DMEM_BASE            = 0x180000
  RISCV_ARCH           = lwriscv

  IS_FUSE_CHECK_ENABLED     = true
  IS_PRESILICON             = false
  IS_SSP_ENABLED            = true

  ELF_LOAD_USE_DMA          = true
  DMEM_DMA_BUFFER_SIZE      = 0x1000
  DMEM_DMA_ZERO_BUFFER_SIZE = 0x1000

  # This can be used as an override to disable bootloader prints
  # even if the main ucode profile actually has prints enabled.
  # This works since bootloader build doesn't use SUBMAKE_CFLAGS.
  LWRISCV_BL_PRINT_ENABLED  = true

  ifneq (,$(findstring gsp_rvbl_ga10x, $(BLCFG_PROFILE)))
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/gsp/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102

    # HDCP22 needs high DMEM usage, so we want these buffers a bit smaller.
    DMEM_DMA_BUFFER_SIZE      = 0x200
    DMEM_DMA_ZERO_BUFFER_SIZE = 0x200
  endif

  ifneq (,$(findstring gsp_rvbl_gh100, $(BLCFG_PROFILE)))
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/gsp/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    IS_PRESILICON             = true
  endif

  ifneq (,$(findstring sec2_rvbl_gh100, $(BLCFG_PROFILE)))
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/sec2/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    IS_PRESILICON             = true
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    ELF_LOAD_USE_DMA          = false
    DMEM_DMA_BUFFER_SIZE      = 0
    DMEM_DMA_ZERO_BUFFER_SIZE = 0
    IMEM_SIZE                 = 0x1e000
  endif

  # This profile runs on t234 tsec
  # todo: bug 200586859 move to tsec profile
  ifneq (,$(findstring gsp_rvbl_t23xd, $(BLCFG_PROFILE)))
    EMEM_SIZE         = 0x2000
    IMEM_SIZE         = 0x26000#0x21000 # 0x26000 on net 24
    DMEM_SIZE         = 0x15000#0x15000 # 0x10000 on net 24
    DEBUG_ENABLED     = true
    RELEASE_PATH      = $(LW_SOURCE)/drivers/resman/kernel/inc/gsp/bin
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    IS_FUSE_CHECK_ENABLED     = false
    IS_PRESILICON             = true
    ELF_LOAD_USE_DMA          = false
    DMEM_DMA_BUFFER_SIZE      = 0
    DMEM_DMA_ZERO_BUFFER_SIZE = 0
  endif

  ifneq (,$(findstring pmu_rvbl_ga10x, $(BLCFG_PROFILE)))
    EMEM_SIZE           = 0x0
    # MMINTS-TODO: find a way to sync these values up with Profiles.pm!
    IMEM_SIZE           = 0x24000
    DMEM_SIZE           = 0x2C000
    RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS        = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
  endif

  ifneq (,$(findstring pmu_rvbl_ga10b, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x10000
    DMEM_SIZE                 = 0x10000
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b

    # GA10B PMU has a lot of resident data in DMEM due to partition requirements,
    # so we want these buffers a bit smaller.
    DMEM_DMA_BUFFER_SIZE      = 0x400
    DMEM_DMA_ZERO_BUFFER_SIZE = 0x400

    # On GA10B, PMU will have its bootloader execute under lockdown, so prints have to be disabled.
    LWRISCV_BL_PRINT_ENABLED  = false
  endif

  ifneq (,$(findstring pmu_rvbl_ga10f, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x10000
    DMEM_SIZE                 = 0x10000
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f

    # GA10F PMU has a lot of resident data in DMEM due to partition requirements,
    # so we want these buffers a bit smaller.
    DMEM_DMA_BUFFER_SIZE      = 0x400
    DMEM_DMA_ZERO_BUFFER_SIZE = 0x400

    # On GA10F, PMU will have its bootloader execute under lockdown, so prints have to be disabled.
    LWRISCV_BL_PRINT_ENABLED  = false
  endif

  ifneq (,$(findstring pmu_rvbl_ga11b, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x10000
    DMEM_SIZE                 = 0x10000
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga11b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga11b

    # GA11B PMU has a lot of resident data in DMEM due to partition requirements,
    # so we want these buffers a bit smaller.
    DMEM_DMA_BUFFER_SIZE      = 0x400
    DMEM_DMA_ZERO_BUFFER_SIZE = 0x400

    # On GA11B, PMU will have its bootloader execute under lockdown, so prints have to be disabled.
    LWRISCV_BL_PRINT_ENABLED  = false
  endif

  ifneq (,$(findstring pmu_rvbl_ad10x, $(BLCFG_PROFILE)))
    EMEM_SIZE           = 0x0
    IMEM_SIZE           = 0x24000
    DMEM_SIZE           = 0x2C000
    RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS        = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
  endif

  ifneq (,$(findstring pmu_rvbl_ad10b, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x2A000
    DMEM_SIZE                 = 0x2C800
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad10b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad10b
    IS_PRESILICON             = true
  endif

  ifneq (,$(findstring pmu_rvbl_gh100, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x2A000
    DMEM_SIZE                 = 0x2C800
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100

    # On GH100, PMU will have its bootloader execute under lockdown, so prints have to be disabled.
    LWRISCV_BL_PRINT_ENABLED  = false
  endif

  ifneq (,$(findstring pmu_rvbl_gh20x, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x2A000
    DMEM_SIZE                 = 0x2C800
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
    IS_PRESILICON             = true
  endif

  ifneq (,$(findstring pmu_rvbl_gb100, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x2A000
    DMEM_SIZE                 = 0x2C800
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/blackwell/gb100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/blackwell/gb100
    IS_PRESILICON             = true
  endif

  ifneq (,$(findstring pmu_rvbl_g00x, $(BLCFG_PROFILE)))
    EMEM_SIZE                 = 0x0
    IMEM_SIZE                 = 0x2A000
    DMEM_SIZE                 = 0x2C800
    RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
    MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
    IS_PRESILICON             = true
  endif

  LD_DEFINES         := -DEMEM_SIZE=$(EMEM_SIZE) -DIMEM_SIZE=$(IMEM_SIZE) -DDMEM_SIZE=$(DMEM_SIZE)

  ifeq ($(IS_SSP_ENABLED), true)
    BLSRC += ssp.c
  endif

endif
