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

#
# This makefile aims to accomplish two things:
# 1. Define all available SOECFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SOECFG_PROFILE. To pick-up the definitions, set SOECFG_PROFILE to the
# desired profile and source/include this file. When SOECFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SOECFG_PROFILES. No default profile is assumed.
#

##############################################################################
# This flag is set to "true" for AS2 build and it is checked in profiles
# which have prod signed binaries in chips_a to skip signing and submission.
# This is required to avoid overwriting prod binaries by debug binaries
# through AS2 submit.
# Defaulting to "false" for other scenarios like DVS or local build.
##############################################################################
AS2_SOE_SKIP_SIGN_RELEASE ?= false

##############################################################################
# Build a list of all possible soe-config profiles
##############################################################################

SOECFG_PROFILES_ALL :=
SOECFG_PROFILES_ALL += soe-ls10

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SOECFG_PROFILE
  MANUAL_PATHS                 =
  HS_OVERLAYS_ENABLED          = false
  DMA_SUSPENSION               = false
  DMA_NACK_SUPPORTED           = true
  TASK_RESTART                 = false
  RELEASE_PATH                 = $(LW_SOURCE)/drivers/lwswitch/kernel/inc/soe/bin
  LWOS_VERSION                 = dev
  SECFG_PROFILE                =
  LS_UCODE_VERSION             = 0
  LWRISCV_MPU_DUMP_ENABLED     = true
  LWRISCV_MPU_FBHUB_ALLOWED    = true
  # Flag to control legacy CSR timer behavior - on older chips it counted time in units of 32 ns
  LWRISCV_MTIME_TICK_SHIFT     = 0
  LWRISCV_PARTITION_SWITCH     = false
  # These must always be false since we don't have the bootloader
  ELF_IN_PLACE                 = false
  ELF_IN_PLACE_FULL_IF_NOWPR   = false
  ELF_IN_PLACE_FULL_ODP_COW    = false
  # TODO: enable LWRISCV raw-mode prints for SOE-RISCV when needed!
  LWRISCV_PRINT_RAW_MODE       = false
  IS_SSP_ENABLED               = false
  TOOLCHAIN_VERSION            = "8.2.0"

  ifneq (,$(findstring soe-ls10,  $(SOECFG_PROFILE)))
    PROJ                         = ls10
    ARCH                         = lwswitch
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    UPROC_ARCH                   = lwriscv
    DEBUG_SIGN_ONLY              = false
    SECFG_PROFILE                = se-ls10
    RTOS_VERSION                 = SafeRTOSv8.4.0-lw1.0
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    ENGINE_ID                    = $(ENGINE_ID_SOE)

    TOOLCHAIN_VERSION            = "8.2.0"
    LWRISCV_SYMBOL_RESOLVER      = false
    LWRISCV_DEBUG_PRINT_LEVEL    = 5
    DMEM_END_CARVEOUT_SIZE       = 0x1400
    DMESG_BUFFER_SIZE            = 0x1000
    LWRISCV_CORE_DUMP            = false
    LWRISCV_MPU_DIRTY_BIT        = false
    USE_CBB                      = false
    USE_CSB                      = true
    LIB_ACR                      = false
    ELF_IN_PLACE                 = false
    ELF_IN_PLACE_FULL_IF_NOWPR   = false
    ELF_IN_PLACE_FULL_ODP_COW    = false
    DMA_SUSPENSION               = false
    # Required for offline-extracted SafeRTOS.
    ODP_ENABLED                  = false
    IS_SSP_ENABLED               = false
    USE_SCPLIB                   = false
    RVBS_PROFILE                 = ls10
    DRIVERSCFG_PROFILE           = drivers-ls10
    SYSLIBCFG_PROFILE            = syslib-ls10
    SHLIBCFG_PROFILE             = shlib-ls10
    SEPKERNCFG_PROFILE           = ls10
    LWRISCV_MPU_FBHUB_ALLOWED    = false
    IS_SSP_ENABLED               = true

    SIGGEN_CHIP                  = gh100_rsa3k_0
  endif

endif
