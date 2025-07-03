#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available BOOTERCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in BOOTERCFG_PROFILE. To pick-up the definitions, set BOOTERCFG_PROFILE to the
# desired profile and source/include this file. When BOOTERCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# BOOTERCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# This flag is set to "true" for AS2 build and it is checked in profiles
# which have prod signed binaries in chips_a to skip signing and submission.
# This is required to avoid overwriting prod binaries by debug binaries
# through AS2 submit.
# Defaulting to "false" for other scenarios like DVS or local build.
##############################################################################
AS2_BOOTER_SKIP_SIGN_RELEASE ?=false

##############################################################################
# Build a list of all possible Booter profiles
##############################################################################

BOOTERCFG_PROFILES_ALL :=

# TU10X Booter profiles
BOOTERCFG_PROFILES_ALL += booter-load-tu10x
BOOTERCFG_PROFILES_ALL += booter-reload-tu10x
BOOTERCFG_PROFILES_ALL += booter-unload-tu10x

# TU11X Booter profiles
BOOTERCFG_PROFILES_ALL += booter-load-tu11x
BOOTERCFG_PROFILES_ALL += booter-reload-tu11x
BOOTERCFG_PROFILES_ALL += booter-unload-tu11x

# GA100 Booter profiles
BOOTERCFG_PROFILES_ALL += booter-load-ga100
BOOTERCFG_PROFILES_ALL += booter-reload-ga100
BOOTERCFG_PROFILES_ALL += booter-unload-ga100

# GA10X Booter profiles
BOOTERCFG_PROFILES_ALL += booter-load-ga10x
BOOTERCFG_PROFILES_ALL += booter-reload-ga10x
BOOTERCFG_PROFILES_ALL += booter-unload-ga10x

BOOTERSW = $(LW_SOURCE)/uproc/booter/

SIGGEN_CSG_PROD_SIGNING  = false

##############################################################################
# Profile-specific settings
##############################################################################

ifdef BOOTERCFG_PROFILE

  # General options
  MANUAL_PATHS            += $(LW_SOURCE)/uproc/booter/inc
  DISABLE_SE_ACCESSES      = false
  BOOTER_SKIP_SIGN_RELEASE = false
  LDSCRIPT                 = link_script.ld
  BOOTER_LOAD              = false
  BOOTER_RELOAD            = false
  BOOTER_UNLOAD            = false
  PRI_SOURCE_ISOLATION_PLM = false

  # Security options
  SSP_ENABLED              = true

  # HS signing options
  IS_ENHANCED_AES          = false
  PKC_ENABLED              = true
  IS_HS_ENCRYPTED          = true
  SIGGEN_FUSE_VER          = -1        # -1 means not defined
  SIGGEN_ENGINE_ID         = -1
  SIGGEN_UCODE_ID          = -1
  NUM_SIG_PER_UCODE        = 1

  # Boot from HS options
  BOOT_FROM_HS_BUILD        = false
  BOOT_FROM_HS_FALCON_SEC2  = false
  BOOT_FROM_HS_FALCON_LWDEC = false

  # Ucode versions
  BOOTER_TU102_UCODE_BUILD_VERSION = 0
  BOOTER_TU104_UCODE_BUILD_VERSION = 0
  BOOTER_TU106_UCODE_BUILD_VERSION = 0
  BOOTER_TU116_UCODE_BUILD_VERSION = 0
  BOOTER_TU117_UCODE_BUILD_VERSION = 0
  BOOTER_TU10A_UCODE_BUILD_VERSION = 0
  BOOTER_GA100_UCODE_BUILD_VERSION = 0
  BOOTER_GA102_UCODE_BUILD_VERSION = 0

  ifneq (,$(findstring tu10x, $(BOOTERCFG_PROFILE)))
      BOOTER_TU102_UCODE_BUILD_VERSION = 0
      BOOTER_TU104_UCODE_BUILD_VERSION = 0
      BOOTER_TU106_UCODE_BUILD_VERSION = 0
  endif

  ifneq (,$(findstring tu116, $(BOOTERCFG_PROFILE)))
      BOOTER_TU116_UCODE_BUILD_VERSION = 0
      BOOTER_TU117_UCODE_BUILD_VERSION = 0
  endif

  ifneq (,$(findstring ga100, $(BOOTERCFG_PROFILE)))
      BOOTER_GA100_UCODE_BUILD_VERSION = 0
  endif

  ifneq (,$(findstring ga10x, $(BOOTERCFG_PROFILE)))
      BOOTER_GA102_UCODE_BUILD_VERSION = 0
  endif

####################################################################################

  # TU10X profiles
  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-load-tu10x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/tu10x/load/
      BOOTER_LOAD      = true

      IS_ENHANCED_AES    = true
      PKC_ENABLED        = false
      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_LOAD_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_LOAD_ENHANCED_AES_ID)
      SIGGEN_CHIPS      += "tu102_aes"
      SIGGEN_CFG         = booter_load_tu10x_siggen.cfg
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-reload-tu10x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/tu10x/reload/
      BOOTER_RELOAD    = true

      IS_ENHANCED_AES    = true
      PKC_ENABLED        = false
      SIGGEN_FUSE_VER    = $(LWDEC_BOOTER_RELOAD_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID    = $(LWDEC_BOOTER_RELOAD_ENHANCED_AES_ID)
      SIGGEN_CHIPS      += "tu102_aes"
      SIGGEN_CFG         = booter_reload_tu10x_siggen.cfg
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-unload-tu10x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/tu10x/unload/
      BOOTER_UNLOAD    = true

      IS_ENHANCED_AES    = true
      PKC_ENABLED        = false
      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_UNLOAD_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_UNLOAD_ENHANCED_AES_ID)
      SIGGEN_CHIPS      += "tu102_aes"
      SIGGEN_CFG         = booter_unload_tu10x_siggen.cfg
  endif

####################################################################################

  # TU11X profiles
  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-load-tu11x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/tu11x/load/
      BOOTER_LOAD      = true

      IS_ENHANCED_AES    = true
      PKC_ENABLED        = false
      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_LOAD_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_LOAD_ENHANCED_AES_ID)
      SIGGEN_CHIPS      += "tu116_aes"
      SIGGEN_CFG         = booter_load_tu11x_siggen.cfg
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-reload-tu11x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/tu11x/reload/
      BOOTER_RELOAD    = true

      IS_ENHANCED_AES    = true
      PKC_ENABLED        = false
      SIGGEN_FUSE_VER    = $(LWDEC_BOOTER_RELOAD_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID    = $(LWDEC_BOOTER_RELOAD_ENHANCED_AES_ID)
      SIGGEN_CHIPS      += "tu116_aes"
      SIGGEN_CFG         = booter_reload_tu11x_siggen.cfg
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-unload-tu11x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/tu11x/unload/
      BOOTER_UNLOAD    = true

      IS_ENHANCED_AES    = true
      PKC_ENABLED        = false
      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_UNLOAD_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_UNLOAD_ENHANCED_AES_ID)
      SIGGEN_CHIPS      += "tu116_aes"
      SIGGEN_CFG         = booter_unload_tu11x_siggen.cfg
  endif

####################################################################################

  # GA100 profiles
  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-load-ga100))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/ga100/load/
      BOOTER_LOAD      = true

      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_LOAD_PKC_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_LOAD_PKC_ID)
      SIGGEN_CHIPS      += "ga100_rsa3k_1"
      SIGGEN_CFG         = booter_load_ga100_siggen.cfg
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-reload-ga100))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/ga100/reload/
      BOOTER_RELOAD    = true

      SIGGEN_FUSE_VER    = $(LWDEC_BOOTER_RELOAD_PKC_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID    = $(LWDEC_BOOTER_RELOAD_PKC_ID)
      SIGGEN_CHIPS      += "ga100_rsa3k_1"
      SIGGEN_CFG         = booter_reload_ga100_siggen.cfg
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-unload-ga100))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/ga100/unload/
      BOOTER_UNLOAD    = true

      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_UNLOAD_PKC_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_UNLOAD_PKC_ID)
      SIGGEN_CHIPS      += "ga100_rsa3k_1"
      SIGGEN_CFG         = booter_unload_ga100_siggen.cfg
  endif

####################################################################################

  # GA10X profiles
  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-load-ga10x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/ga10x/load/
      BOOTER_LOAD      = true

      PRI_SOURCE_ISOLATION_PLM = true

      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_LOAD_PKC_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_LOAD_PKC_ID)
      SIGGEN_CHIPS      += "ga102_rsa3k_0"
      SIGGEN_CFG         = booter_load_ga10x_siggen.cfg

      LDSCRIPT                  = link_script_boot_from_hs.ld
      BOOT_FROM_HS_BUILD        = true
      BOOT_FROM_HS_FALCON_SEC2  = true
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-reload-ga10x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/ga10x/reload/
      BOOTER_RELOAD    = true

      PRI_SOURCE_ISOLATION_PLM = true

      SIGGEN_FUSE_VER    = $(LWDEC_BOOTER_RELOAD_PKC_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID    = $(LWDEC_BOOTER_RELOAD_PKC_ID)
      SIGGEN_CHIPS      += "ga102_rsa3k_0"
      SIGGEN_CFG         = booter_reload_ga10x_siggen.cfg

      LDSCRIPT                  = link_script_boot_from_hs.ld
      BOOT_FROM_HS_BUILD        = true
      BOOT_FROM_HS_FALCON_LWDEC  = true
  endif

  ifneq (,$(filter $(BOOTERCFG_PROFILE), booter-unload-ga10x))
      CHIP_MANUAL_PATH = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      RELEASE_PATH     = $(RESMAN_ROOT)/kernel/inc/gsprm/bin/booter/ga10x/unload/
      BOOTER_UNLOAD      = true

      PRI_SOURCE_ISOLATION_PLM = true

      SIGGEN_FUSE_VER    = $(SEC2_BOOTER_UNLOAD_PKC_VER)
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = $(SEC2_BOOTER_UNLOAD_PKC_ID)
      SIGGEN_CHIPS      += "ga102_rsa3k_0"
      SIGGEN_CFG         = booter_unload_ga10x_siggen.cfg

      LDSCRIPT                  = link_script_boot_from_hs.ld
      BOOT_FROM_HS_BUILD        = true
      BOOT_FROM_HS_FALCON_SEC2  = true
  endif

####################################################################################

endif
