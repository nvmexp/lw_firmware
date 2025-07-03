#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available SEC2CFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SEC2CFG_PROFILE. To pick-up the definitions, set SEC2CFG_PROFILE to the
# desired profile and source/include this file. When SEC2CFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SEC2CFG_PROFILES. No default profile is assumed.
#

##############################################################################
# This flag is set to "true" for AS2 build and it is checked in profiles
# which have prod signed binaries in chips_a to skip signing and submission.
# This is required to avoid overwriting prod binaries by debug binaries
# through AS2 submit.
# Defaulting to "false" for other scenarios like DVS or local build.
##############################################################################
AS2_SEC2_SKIP_SIGN_RELEASE ?= false

##############################################################################
# Build a list of all possible sec2-config profiles
##############################################################################

SEC2CFG_PROFILES_ALL :=
SEC2CFG_PROFILES_ALL += sec2-gh100

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SEC2CFG_PROFILE
  MANUAL_PATHS                 =
  DMA_SUSPENSION               = false
  DMA_NACK_SUPPORTED           = true
  TASK_RESTART                 = true
  RELEASE_PATH                 = $(RESMAN_ROOT)/kernel/inc/sec2_riscv/bin
  LWOS_VERSION                 = dev
  SECFG_PROFILE                =
  LS_UCODE_VERSION             = 0
  SIGN_LICENSE                 = CODESIGN_LS
  LWRISCV_MPU_DUMP_ENABLED     = true
  LWRISCV_MPU_FBHUB_ALLOWED    = true
  # Flag to control legacy CSR timer behavior - before gh100 it counted time in units of 32 ns
  LWRISCV_MTIME_TICK_SHIFT     = 0
  LWRISCV_PARTITION_SWITCH     = true
  # TODO : sahilc to set to true for gh100 when adding channel mgmt tasks
  ELF_IN_PLACE                 = false
  ELF_IN_PLACE_FULL_IF_NOWPR   = false
  ELF_IN_PLACE_FULL_ODP_COW    = false
  # TODO: enable LWRISCV raw-mode prints for SEC2-RISCV when needed!
  LWRISCV_PRINT_RAW_MODE       = false
  NEW_WPR_BLOBS                = true
  IS_SSP_ENABLED               = false
  TOOLCHAIN_VERSION            = "8.2.0"
  IS_PROD_ENCRYPT_BIN_RELEASE  = false
  COT_ENABLED                  = false

  ifneq (,$(findstring sec2-gh100,  $(SEC2CFG_PROFILE)))
    PROJ                   = gh100_riscv
    ARCH                   = hopper
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    UPROC_ARCH             = lwriscv
    DEBUG_SIGN_ONLY        = false
    SECFG_PROFILE          = se-tu10x
    RTOS_VERSION           = SafeRTOSv8.4.0-lw1.0
    SIGN_LICENSE           = CODESIGN_LS_PKC
    ENGINE_ID              = $(ENGINE_ID_SEC2)

    TOOLCHAIN_VERSION            = "8.2.0"
    PARTITION_BOOT_ENABLED       = 1
    LWRISCV_PAGEFAULT_TYPE       = 2
    LWRISCV_SYMBOL_RESOLVER      = false
    LWRISCV_DEBUG_PRINT_LEVEL    = 5
    DMEM_END_CARVEOUT_SIZE       = 0x800
    LWRISCV_CORE_DUMP            = false
    LWRISCV_MPU_DIRTY_BIT        = false
    USE_CBB                      = false
    USE_CSB                      = true
    LIB_ACR                      = false
    ELF_IN_PLACE                 = false
    ELF_IN_PLACE_FULL_IF_NOWPR   = false
    ELF_IN_PLACE_FULL_ODP_COW    = false
    RVBL_PROFILE                 = gh100
    HS_OVERLAYS_ENABLED          = false
    DRIVERSCFG_PROFILE           = drivers-gh100
    IS_LS_ENCRYPTED              = 2
    LS_UCODE_ID                  = 1
    SYSLIBCFG_PROFILE            = syslib-gh100
    SHLIBCFG_PROFILE             = shlib-gh100
    SEPKERNCFG_PROFILE           = gh100
    LWRISCV_MPU_FBHUB_ALLOWED    = true
    IS_SSP_ENABLED               = true
    COT_ENABLED                  = true
    # not yet wired properly for those.
    SIGGEN_CHIPS                 = "tu116_aes"
    TASK_RESTART                 = false
  endif

endif
