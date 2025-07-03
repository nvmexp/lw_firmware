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
# 1. Define all available PMUCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in PMUCFG_PROFILE. To pick-up the definitions, set PMUCFG_PROFILE to the
# desired profile and source/include this file. When PMUCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# PMUCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible pmu-config profiles
##############################################################################

PMUCFG_PROFILES_ALL :=
PMUCFG_PROFILES_ALL += pmu-gm10x
PMUCFG_PROFILES_ALL += pmu-gm20x
PMUCFG_PROFILES_ALL += pmu-gp100
PMUCFG_PROFILES_ALL += pmu-gp10x
PMUCFG_PROFILES_ALL += pmu-gv10x
#PMUCFG_PROFILES_ALL += pmu-gv11b
PMUCFG_PROFILES_ALL += pmu-tu10x
PMUCFG_PROFILES_ALL += pmu-ga100
PMUCFG_PROFILES_ALL += pmu-ga10x_riscv
PMUCFG_PROFILES_ALL += pmu-ga10x_selfinit_riscv
PMUCFG_PROFILES_ALL += pmu-ga10b
PMUCFG_PROFILES_ALL += pmu-ga10b_riscv
PMUCFG_PROFILES_ALL += pmu-ga10f_riscv
PMUCFG_PROFILES_ALL += pmu-ga11b_riscv
PMUCFG_PROFILES_ALL += pmu-ad10x_riscv
PMUCFG_PROFILES_ALL += pmu-ad10b_riscv
PMUCFG_PROFILES_ALL += pmu-gh100
PMUCFG_PROFILES_ALL += pmu-gh100_riscv
PMUCFG_PROFILES_ALL += pmu-gh20x
PMUCFG_PROFILES_ALL += pmu-gh20x_riscv
PMUCFG_PROFILES_ALL += pmu-gb100_riscv
PMUCFG_PROFILES_ALL += pmu-g00x_riscv

##############################################################################
# Profile-specific settings
##############################################################################

ifdef PMUCFG_PROFILE
# Strip whitespace from profile name just in case
  PMUCFG_PROFILE := $(strip $(PMUCFG_PROFILE))

# Constant (always the same)
  FREEABLE_HEAP             = false
  LWOS_VERSION              = dev
  ON_DEMAND_PAGING_OVL_IMEM = false
  TASK_QUEUE_MAP            = true
  TASK_RESTART              = false
  BOOT_FROM_HS              = false
  SCHEDULER_ENABLED         = false
  CHIP_MANUAL_PATH          =
  LWRISCV_HAS_DCACHE        = true
  FPU_SUPPORTED             = true
  # This flag enables using RISCV PMU ELF-image-file sections in-place without copying them out.
  # By default this only applies to code and read-only data.
  ELF_IN_PLACE              = true
  # This flag indicates that when the RISCV PMU ELF image is not loaded into read-only WPR,
  # elf-in-place should be applied to all sections.
  ELF_IN_PLACE_FULL_IF_NOWPR= false
  # This flag enables elf-in-place ODP copy-on-write behavior, which means that
  # each data section will track an addr in the ELF (read-only subWPR) and in
  # the RW subWPR outside the ELF, and each ODP page will switch the backing storage
  # to the latter upon first flush-when-dirty.
  # Since GA10X now ships, we disable this flag on it just in case.
  ELF_IN_PLACE_FULL_ODP_COW = true
  # This flag enables token-based raw-mode prints on RISCV PMU profiles.
  LWRISCV_PRINT_RAW_MODE    = true
# Variable (overriden on some profiles)
  MANUAL_PATHS              =
  DMA_SUSPENSION            = true
  LIB_ACR                   = false
  LS_FALCON                 = true
  LS_FALCON_TEGRA           = false
  PA_47_BIT_SUPPORTED       = true
  FLCNDBG_ENABLED           = true
  OS_CALLBACKS              = true
  OS_CALLBACKS_WSTATS       = true
  OS_CALLBACKS_SINGLE_MODE  = false
  OS_CALLBACKS_RELAXED_MODE = false
  OS_TRACE                  = true
  MRU_OVERLAYS              = true
  DMEM_VA_SUPPORTED         = true
  DMREAD_WAR_200142015      = false
  DMTAG_WAR_1845883         = false
  LS_UCODE_VERSION          = 1
  IS_SSP_ENABLED            = true
  IS_SSP_ENABLED_PER_TASK   = true
  IS_SSP_ENABLED_WITH_SCP   = false
# MMINTS-TODO: enable separate kernel SSP on RISCV
# profiles where we get proper user / kernel
# mode separation
  IS_KERNEL_SSP_SEPARATE    = false
  SANITIZER_COV_SUPPORTED   = false
  FB_QUEUES                 = true
  QUEUE_HALT_ON_FULL        = true
  TASK_SYNC                 = false
  SCHEDULER_2X              = false
  SCHEDULER_2X_AUTO         = false
  INSTRUMENTATION           = false
  INSTRUMENTATION_PTIMER    = true
  INSTRUMENTATION_PROTECT   = true
  USTREAMER                 = false
  USTREAMER_PTIMER          = true
  USTREAMER_PROTECT         = true
  HEAP_BLOCKING             = false
  WATCHDOG_ENABLE           = false
  ON_DEMAND_PAGING_BLK      = false
  EXCLUDE_LWOSDEBUG         = false
  LIB_MUTEX_SEMAPHORE_ONLY  = false
  PEDANTIC_BUILD            = false
  UINT_OVERFLOW_CHECK       = false
  QUEUE_MSG_BUF             = false
  QUEUE_MSG_BUF_SIZE        = 0
  DMEM_OVL_SIZE_LONG        = false
  FAULT_RECOVERY            = false
  LWRISCV_MPU_DUMP_ENABLED  = true
  LWRISCV_MPU_FBHUB_SUSPEND = false
  LWRISCV_MPU_FBHUB_ALLOWED = false
  LWRISCV_CORE_DUMP         = false
  LWRISCV_PARTITION_SWITCH  = false
  DMEM_END_CARVEOUT_SIZE    = 0
  DMA_NACK_SUPPORTED        = false
  SIGN_LICENSE              = CODESIGN_LS
  LS_PKC_PROD_ENC_ENABLED   = false
  LS_UCODE_ID               = 0
  LS_ENCRYPTION_MODE        = 0
  UCODE_HASH_BIN_RELEASED   = false
  USE_CSB                   = false
  USE_CBB                   = false
  USE_TRUE_BREAKPOINT       = false
  # Flag to control legacy CSR timer behavior - before gh100 it counted time in units of 32 ns.
  # This value tracks the shift of the mtime CSRs relative to ptimer (determined by HW):
  # it must always be the case that mtime == ptimer >> LWRISCV_MTIME_TICK_SHIFT
  LWRISCV_MTIME_TICK_SHIFT  = 0
  # This is enabled for profiles where we have the MMODE device map group set to read-only
  # for sub-m-mode code. Registers like FBIF_REGIONCFG, FBIF_TRANSCFG and TRACECTL become
  # accessible only through SBIs provided by MP-SK.
  LWRISCV_MMODE_REGS_RO     = false
  # Flag to enable a dry-run FMC build for a given profile
  LWRISCV_FMC_BUILD_DRY_RUN = false
  LIB_FSP_RPC               = false
  LWRISCV_SDK_VERSION       =
  LWRISCV_SDK_PROJ          =

  ifeq (pmu-gm10x, $(PMUCFG_PROFILE))
    PROJ                = gm10x
    ARCH                = maxwell
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/maxwell/gm108
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/swref/maxwell/gm108
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_04
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_04
    FALCON_ARCH         = falcon5
    RTOS_VERSION        = OpenRTOSv4.1.3
    FB_QUEUES           = false
    PA_47_BIT_SUPPORTED = false
    MRU_OVERLAYS        = false
    DMEM_VA_SUPPORTED   = false
    LS_FALCON           = false
    OS_CALLBACKS        = false
    OS_CALLBACKS_WSTATS = false
    IS_SSP_ENABLED      = false
    IS_SSP_ENABLED_PER_TASK = false
    QUEUE_HALT_ON_FULL  = false
  endif

  ifeq (pmu-gm20x, $(PMUCFG_PROFILE))
    PROJ                = gm20x
    ARCH                = maxwell
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/maxwell/gm200
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/swref/maxwell/gm200
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_05
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_05
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_05
    MANUAL_PATHS       += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_05
    MANUAL_PATHS       += $(RESMAN_ROOT)/kernel/inc/dpu/v02_05
    FALCON_ARCH         = falcon5
    RTOS_VERSION        = OpenRTOSv4.1.3
    FB_QUEUES           = false
    PA_47_BIT_SUPPORTED = false
    MRU_OVERLAYS        = false
    DMEM_VA_SUPPORTED   = false
    OS_CALLBACKS        = false
    OS_CALLBACKS_WSTATS = false
    IS_SSP_ENABLED      = false
    IS_SSP_ENABLED_PER_TASK = false
    QUEUE_HALT_ON_FULL  = false
  endif

  ifeq (pmu-gp100, $(PMUCFG_PROFILE))
    PROJ                 = gp100
    ARCH                 = pascal
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_07
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_07
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_07
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_07
    MANUAL_PATHS        += $(RESMAN_ROOT)/kernel/inc/dpu/v02_05
    FALCON_ARCH          = falcon6
    RTOS_VERSION         = OpenRTOSv4.1.3
    DMEM_VA_SUPPORTED   = false
    OS_CALLBACKS        = false
    OS_CALLBACKS_WSTATS = false
    IS_SSP_ENABLED      = false
    IS_SSP_ENABLED_PER_TASK = false
  endif

  ifeq (pmu-gp10x, $(PMUCFG_PROFILE))
    PROJ                 = gp10x
    ARCH                 = pascal
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    MANUAL_PATHS        += $(RESMAN_ROOT)/kernel/inc/dpu/v02_05
    FALCON_ARCH          = falcon6
    DMREAD_WAR_200142015 = true
    DMTAG_WAR_1845883    = true
    OS_CALLBACKS_WSTATS  = true
    LS_UCODE_VERSION     = 2
    RTOS_VERSION         = OpenRTOSv4.1.3
    FAULT_RECOVERY       = true
  endif

  ifeq (pmu-gv10x, $(PMUCFG_PROFILE))
    PROJ                 = gv10x
    ARCH                 = volta
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v03_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v03_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    MANUAL_PATHS        += $(RESMAN_ROOT)/kernel/inc/dpu/v02_07
    FALCON_ARCH          = falcon6
    DMREAD_WAR_200142015 = true
    DMTAG_WAR_1845883    = true
    OS_CALLBACKS_WSTATS  = true
    RTOS_VERSION         = OpenRTOSv4.1.3
    FAULT_RECOVERY       = true
    TASK_SYNC            = true
  endif

  ifeq (pmu-gv11b, $(PMUCFG_PROFILE))
    PROJ                 = gv11b
    ARCH                 = volta
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv11b
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv11b
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v03_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v03_00
    MANUAL_PATHS        += $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t194
    FALCON_ARCH          = falcon6
    LS_FALCON_TEGRA      = true
    LIB_ACR              = true
    DMEM_VA_SUPPORTED    = false
    RTOS_VERSION         = OpenRTOSv4.1.3
    OS_CALLBACKS        = false
    OS_CALLBACKS_WSTATS = false
    QUEUE_HALT_ON_FULL  = false
  endif

  ifeq (pmu-tu10x, $(PMUCFG_PROFILE))
    PROJ                      = tu10x
    ARCH                      = turing
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    FALCON_ARCH               = falcon6
    LS_UCODE_VERSION          = 2
    DMREAD_WAR_200142015      = true
    RTOS_VERSION              = SafeRTOSv5.16.0-lw1.3
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    FAULT_RECOVERY            = true
    INSTRUMENTATION           = true
# PMU turing profile have multiple SFK support so as to run same binary on both TU10X as well as on TU116
# Hence LS server releases encrypted images for both SFK
    SFK_CHIP                  = tu104
    SFK_CHIP_1                = tu116
    LS_ENCRYPTION_MODE        = 2
    LS_PKC_PROD_ENC_ENABLED   = true
  endif

  ifeq (pmu-ga100, $(PMUCFG_PROFILE))
    PROJ                    = ga100
    ARCH                    = ampere
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
    FALCON_ARCH             = falcon6
    DMREAD_WAR_200142015    = true
    IS_SSP_ENABLED_WITH_SCP = true
    RTOS_VERSION            = SafeRTOSv5.16.0-lw1.3
    TASK_SYNC               = true
    SCHEDULER_2X            = true
    HEAP_BLOCKING           = true
    FAULT_RECOVERY          = true
    INSTRUMENTATION         = true
    UCODE_HASH_BIN_RELEASED = true
  endif

  ifeq (pmu-ga10x_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ga10x_riscv
    SFK_CHIP                  = ga102
    ARCH                      = ampere
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    SANITIZER_COV_SUPPORTED   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true
    OS_CALLBACKS_RELAXED_MODE = true
    LWRISCV_MTIME_TICK_SHIFT  = 5
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev

    # LS Signing settings
    LS_UCODE_VERSION          = 4
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = true
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 2
  endif

  ifeq (pmu-ga10x_selfinit_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ga10x_selfinit_riscv
    SFK_CHIP                  = ga102
    ARCH                      = ampere
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    SANITIZER_COV_SUPPORTED   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true
    OS_CALLBACKS_RELAXED_MODE = true
    LWRISCV_MTIME_TICK_SHIFT  = 5
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev
    LWRISCV_SDK_PROJ          = basic-sepkern-ga10x-pmu

    # LS Signing settings
    LS_UCODE_VERSION          = 4
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = true
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 2
  endif

  # MMINTS-TODO: this profile is kept for CheetAh team's colwenience and should be removed ASAP!
  ifeq (pmu-ga10b, $(PMUCFG_PROFILE))
    PROJ                 = ga10b
    ARCH                 = ampere
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t234
    LS_FALCON_TEGRA      = true
    LIB_ACR              = true
    FALCON_ARCH          = falcon6
    DMREAD_WAR_200142015 = true
    IS_SSP_ENABLED_WITH_SCP = false
    RTOS_VERSION         = SafeRTOSv5.16.0-lw1.3
    TASK_SYNC            = true
    SCHEDULER_2X         = true
    HEAP_BLOCKING        = true
    ON_DEMAND_PAGING_BLK = true
  endif

  ifeq (pmu-ga10b_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ga10b_riscv
    SFK_CHIP                  = ga10b
    ARCH                      = ampere
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t234
    UPROC_ARCH                = lwriscv
    LS_FALCON_TEGRA           = true
    LIB_ACR                   = true
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = false
    OS_CALLBACKS_RELAXED_MODE = true
    LWRISCV_MTIME_TICK_SHIFT  = 5
    LWRISCV_PARTITION_SWITCH  = true
    # Registers like FBIF_TRANSCFG, FBIF_REGIONCFG and TRACECTL are only writable via SBI
    LWRISCV_MMODE_REGS_RO     = true
    # Dry-run-build the FMC every time this profile gets built
    LWRISCV_FMC_BUILD_DRY_RUN = true
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev
    LWRISCV_SDK_PROJ          = pmu-rtos-cheetah-ga10b

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
  endif

  ifeq (pmu-ga10f_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ga10f_riscv
    SFK_CHIP                  = ga10f
    ARCH                      = ampere
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t234
    # MMINTS-TODO: t239 path needed for dev_hdacodec.h, remove once it's added to the ga10f tree
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t239
    UPROC_ARCH                = lwriscv
    LS_FALCON_TEGRA           = true
    LIB_ACR                   = true
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = false
    OS_CALLBACKS_RELAXED_MODE = true
    LWRISCV_MTIME_TICK_SHIFT  = 5
    LWRISCV_PARTITION_SWITCH  = true
    # Registers like FBIF_TRANSCFG, FBIF_REGIONCFG and TRACECTL are only writable via SBI
    LWRISCV_MMODE_REGS_RO     = true
    # Dry-run-build the FMC every time this profile gets built
    LWRISCV_FMC_BUILD_DRY_RUN = true
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev
    LWRISCV_SDK_PROJ          = pmu-rtos-cheetah-ga10b

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
  endif

  ifeq (pmu-ga11b_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ga11b_riscv
    SFK_CHIP                  = ga11b
    ARCH                      = ampere
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga11b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga11b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t234
    # MMINTS-TODO: ga10b path needed for dev_hdacodec.h, remove once it's added to the ga11b tree
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
    UPROC_ARCH                = lwriscv
    LS_FALCON_TEGRA           = true
    LIB_ACR                   = true
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = false
    OS_CALLBACKS_RELAXED_MODE = true
    LWRISCV_MTIME_TICK_SHIFT  = 5
    LWRISCV_PARTITION_SWITCH  = true
    # Registers like FBIF_TRANSCFG, FBIF_REGIONCFG and TRACECTL are only writable via SBI
    LWRISCV_MMODE_REGS_RO     = true
    # Dry-run-build the FMC every time this profile gets built
    LWRISCV_FMC_BUILD_DRY_RUN = true

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev
    LWRISCV_SDK_PROJ          = pmu-rtos-cheetah-ga10b
  endif

  ifeq (pmu-ad10x_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ad10x_riscv
    SFK_CHIP                  = ad102
    ARCH                      = ada
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true
    OS_CALLBACKS_RELAXED_MODE = true
    LWRISCV_MTIME_TICK_SHIFT  = 5
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev
    LWRISCV_SDK_PROJ          = pmu-rtos-ad10x

    # LS Signing settings
    LS_UCODE_VERSION          = 1
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = true
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 2
  endif

  ifeq (pmu-ad10b_riscv, $(PMUCFG_PROFILE))
    PROJ                      = ad10b_riscv
    SFK_CHIP                  = ad10b
    ARCH                      = ada
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad10b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad10b
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t254
    UPROC_ARCH                = lwriscv
    LS_FALCON_TEGRA           = true
    LIB_ACR                   = true
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = false
    OS_CALLBACKS_RELAXED_MODE = true

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
  endif

  # MMINTS-TODO: this profile is kept for gh100 bringup colwenience and should be removed ASAP!
  ifeq (pmu-gh100, $(PMUCFG_PROFILE))
    PROJ                 = gh100
    ARCH                 = hopper
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    FALCON_ARCH          = falcon6
    DMREAD_WAR_200142015 = true
    IS_SSP_ENABLED_WITH_SCP = true
    RTOS_VERSION         = SafeRTOSv5.16.0-lw1.3
    TASK_SYNC            = true
    SCHEDULER_2X         = true
    HEAP_BLOCKING        = true
    ON_DEMAND_PAGING_BLK = true
    DMEM_OVL_SIZE_LONG   = true
    FAULT_RECOVERY       = true
    INSTRUMENTATION      = true
  endif

  ifeq (pmu-gh100_riscv, $(PMUCFG_PROFILE))
    PROJ                      = gh100_riscv
    ARCH                      = hopper
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    SANITIZER_COV_SUPPORTED   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true
    # Registers like FBIF_TRANSCFG, FBIF_REGIONCFG and TRACECTL are only writable via SBI
    LWRISCV_MMODE_REGS_RO     = true
    # Dry-run-build the FMC every time this profile gets built
    LWRISCV_FMC_BUILD_DRY_RUN = true
    LIB_FSP_RPC               = true
    LWRISCV_SDK_VERSION       = lwriscv-sdk-3.1_dev
    LWRISCV_SDK_PROJ          = pmu-rtos-gh100

    # LS Signing settings
    LS_UCODE_VERSION          = 1
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = true
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 2
  endif

  # MMINTS-TODO: this profile is kept for gh20x GC6 testing colwenience and should be removed ASAP!
  ifeq (pmu-gh20x, $(PMUCFG_PROFILE))
    PROJ                 = gh20x
    ARCH                 = hopper
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    FALCON_ARCH          = falcon6
    DMREAD_WAR_200142015 = true
    IS_SSP_ENABLED       = false
    IS_SSP_ENABLED_WITH_SCP = false
    IS_SSP_ENABLED_PER_TASK = false
    RTOS_VERSION         = SafeRTOSv5.16.0-lw1.3
    TASK_SYNC            = true
    SCHEDULER_2X         = true
    HEAP_BLOCKING        = true
    ON_DEMAND_PAGING_BLK = true
    DMEM_OVL_SIZE_LONG   = true
    FAULT_RECOVERY       = true
    INSTRUMENTATION      = true
  endif

  ifeq (pmu-gh20x_riscv, $(PMUCFG_PROFILE))
    PROJ                      = gh20x_riscv
    ARCH                      = hopper
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    SANITIZER_COV_SUPPORTED   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
  endif

  ifeq (pmu-gb100_riscv, $(PMUCFG_PROFILE))
    PROJ                      = gb100_riscv
    ARCH                      = blackwell
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/blackwell/gb100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/blackwell/gb100
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    SANITIZER_COV_SUPPORTED   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
  endif

  ifeq (pmu-g00x_riscv, $(PMUCFG_PROFILE))
    PROJ                      = g00x_riscv
    ARCH                      = g00x
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    UPROC_ARCH                = lwriscv
    FLCNDBG_ENABLED           = false
    DMREAD_WAR_200142015      = true
    IS_SSP_ENABLED_WITH_SCP   = true
    RTOS_VERSION              = SafeRTOSv8.4.0-lw1.0
    TASK_SYNC                 = true
    SCHEDULER_2X              = true
    HEAP_BLOCKING             = true
    QUEUE_MSG_BUF             = true
    QUEUE_MSG_BUF_SIZE        = 64
    ON_DEMAND_PAGING_BLK      = true
    LWRISCV_MPU_DUMP_ENABLED  = false
    LWRISCV_SYMBOL_RESOLVER   = false
    LWRISCV_DEBUG_PRINT_LEVEL = 3
    DMEM_END_CARVEOUT_SIZE    = 0x400
    # Enabled by sepkern
    DMA_NACK_SUPPORTED        = true
    INSTRUMENTATION           = true

    # LS Signing settings
    SIGN_LICENSE              = CODESIGN_LS_PKC
    LS_UCODE_ID               = 1
    LS_PKC_PROD_ENC_ENABLED   = false
    # This flag will be used for AES-CBC LS Encryption
    LS_ENCRYPTION_MODE        = 0
  endif

endif
