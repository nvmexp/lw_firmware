#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

##############################################################################
# Build a list of all possible pmu-riscv fmc profiles
##############################################################################

PROFILES_ALL :=
PROFILES_ALL += pmu_ga10b_riscv pmu_ga10b_riscv_inst
PROFILES_ALL += pmu_ga10f_riscv pmu_ga10f_riscv_inst
PROFILES_ALL += pmu_ga11b_riscv pmu_ga11b_riscv_inst
PROFILES_ALL += pmu_gh100_riscv pmu_gh100_riscv_inst

##############################################################################
# Profile-specific settings
##############################################################################

ifdef PROFILE
  MANUAL_PATHS              =
  UPROC_ARCH                = lwriscv
  ENGINE                    = pmu
  USE_LIBCCC                = false
  SK_PARTITION_COUNT        = 1
  PARTITIONS                = rtos
  LW_MANUALS_ROOT           = $(LW_SOURCE)/drivers/common/inc/hwref
  LWRISCV_SDK_VERSION       =
  USE_PL0_CONFIG            = false
  PLM_LIST_FILE             =
  LIB_TEGRA_ACR             = false
  TEGRA_ACR_IMEM_SIZE       = 0
  TEGRA_ACR_DMEM_SIZE       = 0
  TEGRA_ACR_DMEM_RO_SIZE    = 0
  LIBLWRISCV_IMEM_SIZE      = 0
  LIBLWRISCV_DMEM_RO_SIZE   = 0
  MANIFEST_PROFILE_OVERRIDE =
  POLICIES_PROFILE_OVERRIDE =
  # MMINTS-TODO: unify buffer size with rest of PMU builds somehow
  DEBUG_BUFFER_SIZE         = 0x400

  ifneq (,$(findstring inst, $(PROFILE)))
    USE_PL0_CONFIG          = true
  endif

  ifneq (,$(findstring ga10b_riscv, $(PROFILE)))
    PROJ                    = orin
    ARCH                    = ampere
    LWRISCV_SDK_VERSION     = lwriscv-sdk-2.1
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
    SIGGEN_CHIP             = ga10b_rsa3k_0
    CSG_SIGNING_PROFILE     = pmu_sepkern-ga10b
    LWRISCV_SDK_PROFILE     = pmu-rtos-cheetah-ga10b
    SEPKERNCFG_PROFILE      = pmu_sepkern-ga10b
  endif

  ifneq (,$(findstring ga10f_riscv, $(PROFILE)))
    # MMINTS-TODO: modify with ga10f-specific values as necessary!
    PROJ                      = orin
    ARCH                      = ampere
    # MMINTS-TODO: figure out if it's worth it to fix all the -Werror=unused-parameter warnings and try 2.2_dev
    LWRISCV_SDK_VERSION       = lwriscv-sdk-2.1
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f
    SIGGEN_CHIP               = ga10b_rsa3k_0
    CSG_SIGNING_PROFILE       = pmu_sepkern-ga10b
    LWRISCV_SDK_PROFILE       = pmu-rtos-cheetah-ga10b
    SEPKERNCFG_PROFILE        = pmu_sepkern-ga10b
    MANIFEST_PROFILE_OVERRIDE = pmu_ga10b_riscv
    POLICIES_PROFILE_OVERRIDE = pmu_ga10b_riscv
  endif

  ifneq (,$(findstring ga11b_riscv, $(PROFILE)))
    # MMINTS-TODO: modify with ga11b-specific values as necessary!
    PROJ                      = orin
    ARCH                      = ampere
    # MMINTS-TODO: figure out if it's worth it to fix all the -Werror=unused-parameter warnings and try 2.2_dev
    LWRISCV_SDK_VERSION       = lwriscv-sdk-2.1
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga11b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga11b
    SIGGEN_CHIP               = ga10b_rsa3k_0
    CSG_SIGNING_PROFILE       = pmu_sepkern-ga10b
    LWRISCV_SDK_PROFILE       = pmu-rtos-cheetah-ga10b
    SEPKERNCFG_PROFILE        = pmu_sepkern-ga10b
    MANIFEST_PROFILE_OVERRIDE = pmu_ga10b_riscv
    POLICIES_PROFILE_OVERRIDE = pmu_ga10b_riscv
  endif

  ifneq (,$(findstring ga10b_riscv,$(PROFILE))$(findstring ga10f_riscv,$(PROFILE))$(findstring ga11b_riscv,$(PROFILE)))
    PLM_LIST_FILE           = pmu_plm_list.ads
    SK_PARTITION_COUNT      = 2
    PARTITIONS              = rtos tegra_acr shared
    LIB_TEGRA_ACR           = true
    # Total fmc-package steady-state size (counting 0x1000 SK imem and 0x1000 SK dmem):
    # IMEM - 0x4000
    # DMEM - 0x2000
    #
    # IMEM layout (LW_RISCV_AMAP_IMEM_START = 0x100000):
    # 0x100000:0x101000 - SK code
    # 0x101000:0x102C00 - CheetAh-ACR code
    # 0x102C00:0x104000 - Shared LIBLWRISCV code
    # 0x104000:0x110000 - RTOS code
    #
    # DMEM layout (LW_RISCV_AMAP_DMEM_START = 0x180000):
    # 0x180000:0x181000 - SK data
    # 0x181000:0x181400 - CheetAh-ACR RO data
    # 0x181400:0x182000 - CheetAh-ACR RW data
    # 0x182000:0x182000 - Shared RO data + RTOS RO data (lwrrently empty)
    # 0x182000:0x190000 - RTOS data (includes print buffer)
    # 0x18fc00:0x190000 - RTOS print buffer
    TEGRA_ACR_IMEM_SIZE     = 0x1C00
    TEGRA_ACR_DMEM_RO_SIZE  = 0x400
    TEGRA_ACR_DMEM_SIZE     = 0xC00
    LIBLWRISCV_IMEM_SIZE    = 0x1400
    LIBLWRISCV_DMEM_RO_SIZE = 0x0
  endif

  ifneq (,$(findstring gh100_riscv, $(PROFILE)))
    PROJ                    = gh100
    ARCH                    = hopper
    LWRISCV_SDK_VERSION     = lwriscv-sdk-3.0_dev
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    SIGGEN_CHIP             = gh100_rsa3k_0
    ifneq (,$(findstring inst, $(PROFILE)))
      CSG_SIGNING_PROFILE   = GH100_PMU_PMURTOS_INST_IN_SYS
    else
      CSG_SIGNING_PROFILE   = GH100_PMU_PMURTOS
    endif
    SK_PARTITION_COUNT      = 1
    PARTITIONS              = rtos shared
    LWRISCV_SDK_PROFILE     = pmu-rtos-gh100
    SEPKERNCFG_PROFILE      = pmu_sepkern-gh100
    # Total fmc-package steady-state size (counting 0x1000 SK imem and 0x1000 SK dmem):
    # IMEM - 0x1000
    # DMEM - 0x1000
    #
    # IMEM layout (LW_RISCV_AMAP_IMEM_START = 0x100000):
    # 0x100000:0x101000 - SK code
    # 0x101000:0x12A000 - RTOS code
    #
    # DMEM layout (LW_RISCV_AMAP_DMEM_START = 0x180000):
    # 0x180000:0x181000 - SK data
    # 0x181000:0x1AC800 - RTOS data (includes print buffer)
    # 0x1AC400:0x1AC800 - RTOS print buffer
    LIBLWRISCV_IMEM_SIZE    = 0x100
    LIBLWRISCV_DMEM_RO_SIZE = 0x0
  endif
endif

