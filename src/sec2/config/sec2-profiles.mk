#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
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
SEC2CFG_PROFILES_ALL += sec2-gp10x
SEC2CFG_PROFILES_ALL += sec2-gp10x_prmods
SEC2CFG_PROFILES_ALL += sec2-gv10x
#SEC2CFG_PROFILES_ALL += sec2-tu10x
#SEC2CFG_PROFILES_ALL += sec2-tu10x_dualcore         # Under implementation.  Disable this from DVS build
#SEC2CFG_PROFILES_ALL += sec2-tu10x_prmods
#SEC2CFG_PROFILES_ALL += sec2-tu116
#SEC2CFG_PROFILES_ALL += sec2-tu116_dualcore         # Under implementation.  Disable this from DVS build
#SEC2CFG_PROFILES_ALL += sec2-tu116_prmods
#SEC2CFG_PROFILES_ALL += sec2-tu10a                  # SEC2 profile for AUTO
SEC2CFG_PROFILES_ALL += sec2-ga100
SEC2CFG_PROFILES_ALL += sec2-ga100_apm               # Under implementation, only supports local signing.
SEC2CFG_PROFILES_ALL += sec2-ga100_riscv_ls          # Profile supporting RISC-V LS.
SEC2CFG_PROFILES_ALL += sec2-ga10x
SEC2CFG_PROFILES_ALL += sec2-ga10x_new_wpr_blob
#SEC2CFG_PROFILES_ALL += sec2-ga10x_boot_from_hs     # NOTE: All BFHS profiles intentionally disabled due to AS2 issue (Bug 3087201)
#SEC2CFG_PROFILES_ALL += sec2-ga10x_pr44_alt_img_boot_from_hs
#SEC2CFG_PROFILES_ALL += sec2-ga10x_apm_boot_from_hs # Under implementation, only supports local signing.
SEC2CFG_PROFILES_ALL += sec2-ga10x_prmods_boot_from_hs
SEC2CFG_PROFILES_ALL += sec2-ga10x_prmods
SEC2CFG_PROFILES_ALL += sec2-ad10x
SEC2CFG_PROFILES_ALL += sec2-ad10x_boot_from_hs
SEC2CFG_PROFILES_ALL += sec2-gh100
SEC2CFG_PROFILES_ALL += sec2-gh20x
SEC2CFG_PROFILES_ALL += sec2-g00x

# Not enabling default building of Nouveau profile.
#SEC2CFG_PROFILES_ALL += sec2-gp10x_nouveau
#SEC2CFG_PROFILES_ALL += sec2-ga100_nouveau
#SEC2CFG_PROFILES_ALL += sec2-ga10x_nouveau_boot_from_hs

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SEC2CFG_PROFILE
  MANUAL_PATHS                              =
  DMA_SUSPENSION                            = false
  LS_FALCON                                 = true
  PA_47_BIT_SUPPORTED                       = true
  DMA_NACK_SUPPORTED                        = true
  SELWRITY_ENGINE                           = true
  EMEM_SUPPORTED                            = true
  MRU_OVERLAYS                              = true
  DMEM_VA_SUPPORTED                         = true
  FREEABLE_HEAP                             = true
  DMREAD_WAR_200142015                      = false
  DMTAG_WAR_1845883                         = false
  PR_MPKE_ENABLED                           = true
  SINGLE_METHOD_ID_REPLAY                   = false
  ON_DEMAND_PAGING_OVL_IMEM                 = true
  TASK_RESTART                              = true
  LS_UCODE_VERSION                          = 0
  LS_DEPENDENCY_MAP                         =
  NOUVEAU_SUPPORTED                         = false
  RELEASE_PATH                              = $(RESMAN_ROOT)/kernel/inc/sec2/bin
  ENABLE_HS_SIGN                            = true
  HS_OVERLAYS_ENABLED                       = true
  PR_SL3000_ENABLED                         = false
  LWOS_VERSION                              = dev
  HS_UCODE_ENCRYPTION                       = false
  SECFG_PROFILE                             =
  SCP_ENGINE                                = true
  SHA_ENGINE                                = true
  SHAHW_LIB                                 = false
  BIGINT_LIB                                = true
  MUTEX_LIB                                 = true
  SCHEDULER_2X                              = false
  SCHEDULER_2X_AUTO                         = false
  LIB_ACR                                   = true
  EXCLUDE_LWOSDEBUG                         = false
  ENGINE_ID                                 = -1      # -1 means not defined
  IS_PKC_ENABLED                            = false
  SIG_SIZE_IN_BITS                          = 128     # default size for AES
  ACR_RISCV_LS                              = false
  SSP_ENABLED                               = false
  NUM_SIG_PER_UCODE                         = 1
  NEW_WPR_BLOBS                             = false
  SIGN_LICENSE                              = CODESIGN_LS
  LS_UCODE_ID                               = 0
  IS_LS_ENCRYPTED                           = 0
  IS_PROD_ENCRYPT_BIN_RELEASE               = false
  BOOT_FROM_HS                              = false
  BOOT_FROM_HS_BUILD                        = $(BOOT_FROM_HS)
  STEADY_STATE_BUILD                        = true
  SSP_ENABLE_STRONG_FLAG                    = false
  RUNTIME_HS_OVL_SIG_PATCHING               = false
  WAR_BUG_200670718                         = false
  WAR_DISABLE_HUB_ENC_CHECK_BUG_3126208     = false
  PR_V0404                                  = false
  WAR_SELWRE_THROTTLE_MINING_BUG_3263169    = false
  LIBSPDM_RESPONDER                         = false
  WAR_DISABLE_HASH_SAVING_BUG_3335627       = false
  IS_UCODE_MEASUREMENT_ENABLED              = false

  #
  # WARNING : Make sure to update GP10X_PRMODS profile if you are updating GP10X profile
  #
  ifeq ("$(SEC2CFG_PROFILE)","sec2-gp10x")
    PROJ                 = gp10x
    ARCH                 = pascal
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    MANUAL_PATHS        += $(LW_SOURCE)/uproc/acr/src/_out/sec2/gp10x_load/config
    FALCON_ARCH          = falcon6
    SIG_GEN_DBG_CFG      = sec2-siggen-gp10x-debug.cfg
    SIG_GEN_PROD_CFG     = sec2-siggen-gp10x-prod.cfg
    DEBUG_SIGN_ONLY      = false
    LS_UCODE_VERSION     = 1
    LS_DEPENDENCY_MAP    = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_gp10x.cfg
    LIB_ACR              = false

    # WARNING: Multiple chips for the same binary is not yet supported.
    SIGGEN_CHIPS         = "gp104"
    RTOS_VERSION         = OpenRTOSv4.1.3
    DMREAD_WAR_200142015 = true
    DMTAG_WAR_1845883    = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-gp10x_prmods")
    PROJ                    = gp10x_prmods
    ARCH                    = pascal
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_08
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_08
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    MANUAL_PATHS           += $(LW_SOURCE)/uproc/acr/src/_out/sec2/gp10x_load/config
    FALCON_ARCH             = falcon6
    SIG_GEN_DBG_CFG         = sec2-siggen-gp10x-debug.cfg
    DEBUG_SIGN_ONLY         = true
    SINGLE_METHOD_ID_REPLAY = true
    LS_UCODE_VERSION        = 1
    LS_DEPENDENCY_MAP       = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_gp10x.cfg
    LIB_ACR                 = false

    # WARNING: Multiple chips for the same binary is not yet supported.
    SIGGEN_CHIPS         = "gp104"
    RTOS_VERSION         = OpenRTOSv4.1.3
    DMREAD_WAR_200142015 = true
    DMTAG_WAR_1845883    = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-gp10x_nouveau")
    PROJ                  = gp10x_nouveau
    ARCH                  = pascal
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_08
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_08
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_08
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_08
    MANUAL_PATHS         += $(LW_SOURCE)/uproc/nouveau_acr/halgen/sec2/gp10x
    FALCON_ARCH           = falcon6
    SIG_GEN_DBG_CFG       = sec2-siggen-gp10x-debug.cfg
    SIG_GEN_PROD_CFG      = sec2-siggen-gp10x-prod.cfg
    DEBUG_SIGN_ONLY       = false
    LS_UCODE_VERSION      = 1
    LS_DEPENDENCY_MAP     = $(SEC2_SW)/build/sign/nouveau_sec2-ls-version_dependency_map_gp10x.cfg
    RELEASE_PATH          = $(RESMAN_ROOT)/kernel/inc/nouveau_sec2/bin
    ENABLE_HS_SIGN        = false
    NOUVEAU_SUPPORTED     = true
    HS_OVERLAYS_ENABLED   = false
    SECFG_PROFILE         = se-gp10x
    LIB_ACR               = false

    # WARNING: Multiple chips for the same binary is not yet supported.
    SIGGEN_CHIPS         = "gp104"
    RTOS_VERSION         = OpenRTOSv4.1.3
    DMREAD_WAR_200142015 = true
    DMTAG_WAR_1845883    = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-gv10x")
    PROJ                 = gv10x
    ARCH                 = volta
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v03_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v03_00
    MANUAL_PATHS        += $(LW_SOURCE)/uproc/acr/src/_out/sec2/gv100_load/config
    FALCON_ARCH          = falcon6
    SIG_GEN_DBG_CFG      = sec2-siggen-gv10x-debug.cfg
    SIG_GEN_PROD_CFG     = sec2-siggen-gv10x-prod.cfg
    LS_UCODE_VERSION     = 3
    DEBUG_SIGN_ONLY      = false
    LS_DEPENDENCY_MAP    = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_gv10x.cfg
    LIB_ACR              = false

    # WARNING: Multiple chips for the same binary is not yet supported.

    # Using GP104 siggen configs, MPKE and siggen chips now because GV100 is
    # not yet wired properly for those.
    SIGGEN_CHIPS         = "gp104"
    SECFG_PROFILE        = se-gp10x
    PR_MPKE_ENABLED      = false
    RTOS_VERSION         = OpenRTOSv4.1.3
    DMREAD_WAR_200142015 = true
    DMTAG_WAR_1845883    = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
  endif

  # Shared configuration for risk mitigation (pure falcon) and dual-core builds
  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-tu10x sec2-tu10x_dualcore))
    PROJ                 = tu10x
    ARCH                 = turing
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/uproc/acr/src/_out/gsp/tu10x_asb/config
    FALCON_ARCH          = falcon6
    SIG_GEN_DBG_CFG      = sec2-siggen-tu10x-debug.cfg
    SIG_GEN_PROD_CFG     = sec2-siggen-tu10x-prod.cfg
    LS_UCODE_VERSION     = 3
    DEBUG_SIGN_ONLY      = false
    HS_UCODE_ENCRYPTION  = true
    LS_DEPENDENCY_MAP    = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_tu10x.cfg
    SIGGEN_CHIPS         = "tu102_aes"
    SECFG_PROFILE        = se-tu10x
    ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-tu10x ))
      PR_MPKE_ENABLED    = true
      PR_SL3000_ENABLED  = true
      ACRCFG_PROFILE     = acr_gsp-tu10x_asb

      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
        ENABLE_HS_SIGN     = false
        NO_RELEASE         = true
      endif
    endif
    RTOS_VERSION         = OpenRTOSv4.1.3
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015 = true
  endif

  # Configuration of 'sec2-tu10x_dualcore'
  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-tu10x_dualcore))
    DUAL_CORE            = true
    RISCV_CORE           = true
    RISCV_ARCH           = lwriscv

    # disabled in the initial version
    DUAL_CORE_W_FALCON   = false
    RTOS_VERSION         = OpenRTOSv4.1.3
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015 = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-tu10x_prmods")
    PROJ                    = tu10x_prmods
    ARCH                    = turing
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS           += $(LW_SOURCE)/uproc/acr/src/_out/gsp/tu10x_asb/config
    FALCON_ARCH             = falcon6
    SIG_GEN_DBG_CFG         = sec2-siggen-tu10x-debug.cfg
    DEBUG_SIGN_ONLY         = true
    HS_UCODE_ENCRYPTION     = true
    SINGLE_METHOD_ID_REPLAY = true
    LS_DEPENDENCY_MAP       = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_tu10x.cfg
    SIGGEN_CHIPS            = "tu102_aes"
    SECFG_PROFILE           = se-tu10x
    RTOS_VERSION            = OpenRTOSv4.1.3
    ACRCFG_PROFILE          = acr_gsp-tu10x_asb
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015 = true
  endif

  # Shared configuration for risk mitigation (pure falcon) and dual-core builds
  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-tu116 sec2-tu116_dualcore))
    PROJ                 = tu116
    ARCH                 = turing
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS        += $(LW_SOURCE)/uproc/acr/src/_out/gsp/tu116_asb/config
    FALCON_ARCH          = falcon6
    SIG_GEN_DBG_CFG      = sec2-siggen-tu116-debug.cfg
    SIG_GEN_PROD_CFG     = sec2-siggen-tu116-prod.cfg
    LS_UCODE_VERSION     = 2
    DEBUG_SIGN_ONLY      = false
    HS_UCODE_ENCRYPTION  = true
    LS_DEPENDENCY_MAP    = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_tu116.cfg
    SIGGEN_CHIPS         = "tu116_aes"
    SECFG_PROFILE        = se-tu10x
    ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-tu116 ))
        PR_MPKE_ENABLED  = true
        PR_SL3000_ENABLED= true
        ACRCFG_PROFILE   = acr_gsp-tu116_asb

        # Skip signing and submission of prod-signed binary for AS2 build
        ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
            ENABLE_HS_SIGN     = false
            NO_RELEASE         = true
        endif
    endif
    RTOS_VERSION         = OpenRTOSv4.1.3
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015 = true
  endif

  # Configuration of 'sec2-tu116_dualcore'
  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-tu116_dualcore))
    DUAL_CORE            = true
    RISCV_CORE           = true
    RISCV_ARCH           = lwriscv

    # disabled in the initial version
    DUAL_CORE_W_FALCON   = false
    RTOS_VERSION         = OpenRTOSv4.1.3
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015 = true
  endif

  ifneq (,$(findstring sec2-tu116_prmods, $(SEC2CFG_PROFILE)))
    PROJ                    = tu116_prmods
    ARCH                    = turing
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS           += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS           += $(LW_SOURCE)/uproc/acr/src/_out/gsp/tu116_asb/config
    FALCON_ARCH             = falcon6
    SIG_GEN_DBG_CFG         = sec2-siggen-tu116-debug.cfg
    DEBUG_SIGN_ONLY         = true
    HS_UCODE_ENCRYPTION     = true
    SINGLE_METHOD_ID_REPLAY = true
    LS_DEPENDENCY_MAP       = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_tu116.cfg
    SIGGEN_CHIPS            = "tu116_aes"
    SECFG_PROFILE           = se-tu10x
    RTOS_VERSION            = OpenRTOSv4.1.3
    ACRCFG_PROFILE          = acr_gsp-tu116_asb
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015 = true
  endif

  ifneq (,$(findstring sec2-tu10a, $(SEC2CFG_PROFILE)))
    PROJ                      = tu10a
    ARCH                      = turing
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu104
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu104
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/uproc/acr/src/_out/gsp/tu10x_asb/config
    FALCON_ARCH               = falcon6
    PR_MPKE_ENABLED           = false
    LS_UCODE_VERSION          = 1
    # AUTO profile does not have HS overlays for now. Uncomment when HS ovelays are to be added.
    # HS_UCODE_ENCRYPTION     = true
    ENABLE_HS_SIGN            = false
    DEBUG_SIGN_ONLY           = true
    SIGGEN_CHIPS              = "tu102_aes"
    RTOS_VERSION              = SafeRTOSv5.16.0-lw1.3
    # WAR only required for Turing, fixed in Ampere.
    DMREAD_WAR_200142015      = true
    FREEABLE_HEAP             = false
    ON_DEMAND_PAGING_OVL_IMEM = false
    TASK_RESTART              = false
    SCHEDULER_2X              = true
    SCHEDULER_2X_AUTO         = true
    HEAP_BLOCKING             = true
    SELWRITY_ENGINE           = false
    SCP_ENGINE                = false
    SHA_ENGINE                = false
    BIGINT_LIB                = false
    MUTEX_LIB                 = false
    ACRCFG_PROFILE            = acr_gsp-tu10x_asb
  endif

  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-ga100))
    PROJ                   = ga100
    ARCH                   = ampere
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga100_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-ga100-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-ga100-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_UCODE_VERSION       = 1
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga100.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-ga100_asb
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS           = "ga100_rsa3k_1"
    SSP_ENABLED            = true
    NUM_SIG_PER_UCODE      = 2

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
        ENABLE_HS_SIGN     = false
        NO_RELEASE         = true
    endif
  endif

  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-ga100_apm))
    PROJ                         = ga100_apm
    PROJ1                        = ga100
    ARCH                         = ampere
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga100_asb/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ga100-apm-debug.cfg
    SIG_GEN_PROD_CFG             = sec2-siggen-ga100-apm-prod.cfg
    LS_UCODE_VERSION             = 1
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga100.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = false
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ga100_asb
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga100_rsa3k_1"
    SSP_ENABLED                  = true
    NUM_SIG_PER_UCODE            = 2
    SHAHW_LIB                    = true
    LIBSPDM_RESPONDER            = true
    DEBUG_SIGN_ONLY              = false
    IS_UCODE_MEASUREMENT_ENABLED = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
        ENABLE_HS_SIGN           = false
        NO_RELEASE               = true
    endif
  endif

  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-ga100_nouveau))
    PROJ                   = ga100_nouveau
    ARCH                   = ampere
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga100_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = nouveau_sec2-siggen-ga100-debug.cfg
    SIG_GEN_PROD_CFG       = nouveau_sec2-siggen-ga100-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_UCODE_VERSION       = 1
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/nouveau_sec2-ls-version_dependency_map_ga100.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-ga100_asb
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS           = "ga100_rsa3k_1"
    SSP_ENABLED            = true
    NUM_SIG_PER_UCODE      = 2
    RELEASE_PATH           = $(RESMAN_ROOT)/kernel/inc/nouveau_sec2/bin
    NOUVEAU_SUPPORTED      = true
  endif

  ifneq (,$(filter $(SEC2CFG_PROFILE), sec2-ga100_riscv_ls))
    PROJ                   = ga100_riscv_ls
    ARCH                   = ampere
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga100_fmodel_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-ga100-riscv-ls-debug.cfg
    DEBUG_SIGN_ONLY        = true
    LS_UCODE_VERSION       = 1
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga100.cfg
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    SELWRITY_ENGINE        = false
    ACRCFG_PROFILE         = acr_gsp-ga100_fmodel_asb
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS           = "ga100_rsa3k_1"
    ACR_RISCV_LS           = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x")
    PROJ                   = ga10x
    ARCH                   = ampere
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-ga10x-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-ga10x-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-ga10x_asb
    ACR_RISCV_LS           = true
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS           = "ga100_rsa3k_1"
    SSP_ENABLED            = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_new_wpr_blob")
    PROJ                   = ga10x_new_wpr_blob
    PROJ1                  = ga100
    ARCH                   = ampere
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_new_wpr_blobs/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-ga10x-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-ga10x-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-ga10x_asb_new_wpr_blobs
    ACR_RISCV_LS           = true
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS           = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE      = 2
    NEW_WPR_BLOBS          = true
    SIGN_LICENSE           = CODESIGN_LS_PKC
    LS_UCODE_ID            = 1
    LS_UCODE_VERSION       = 1
    SSP_ENABLED            = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED             = 2
    IS_PROD_ENCRYPT_BIN_RELEASE = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_boot_from_hs")
    PROJ                         = ga10x_boot_from_hs
    PROJ1                        = ga102
    ARCH                         = ampere
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ga10x-boot-from-hs-debug.cfg
    SIG_GEN_PROD_CFG             = sec2-siggen-ga10x-boot-from-hs-prod.cfg
    DEBUG_SIGN_ONLY              = false
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = true
    PR_SL3000_ENABLED            = true
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ga10x_asb_boot_from_hs
    ACR_RISCV_LS                 = true
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 5
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    BL_PROFILE                   = sec2_hs_bl_rtos
    BOOT_FROM_HS                 = true
    BOOT_FROM_HS_BUILD           = $(BOOT_FROM_HS)
    SSP_ENABLED                  = true
    SSP_ENABLE_STRONG_FLAG       = true
    RUNTIME_HS_OVL_SIG_PATCHING  = true
    PR_V0404                     = true

    WAR_SELWRE_THROTTLE_MINING_BUG_3263169 = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED             = 2
    IS_PROD_ENCRYPT_BIN_RELEASE = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_pr44_alt_img_boot_from_hs")
    PROJ                         = ga10x_pr44_alt_img_boot_from_hs
    PROJ1                        = ga102
    ARCH                         = ampere
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ga10x-pr44-boot-from-hs-debug.cfg
    SIG_GEN_PROD_CFG             = sec2-siggen-ga10x-pr44-boot-from-hs-prod.cfg
    DEBUG_SIGN_ONLY              = false
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = true
    PR_SL3000_ENABLED            = true
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ga10x_asb_boot_from_hs
    ACR_RISCV_LS                 = true
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 4
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    BOOT_FROM_HS                 = true
    BL_PROFILE                   = sec2_hs_bl_rtos
    BOOT_FROM_HS_BUILD           = $(BOOT_FROM_HS)
    SSP_ENABLED                  = true
    SSP_ENABLE_STRONG_FLAG       = true
    RUNTIME_HS_OVL_SIG_PATCHING  = true
    PR_V0404                     = true

    WAR_SELWRE_THROTTLE_MINING_BUG_3263169 = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED             = 2
    IS_PROD_ENCRYPT_BIN_RELEASE = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_prmods_boot_from_hs")
    PROJ                         = ga10x_prmods_boot_from_hs
    PROJ1                        = ga102
    ARCH                         = ampere
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ga10x-prmods-boot-from-hs-debug.cfg
    DEBUG_SIGN_ONLY              = true
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = true
    PR_SL3000_ENABLED            = true
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ga10x_asb_boot_from_hs
    ACR_RISCV_LS                 = true
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 4
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    BOOT_FROM_HS                 = true
    BL_PROFILE                   = sec2_hs_bl_rtos
    BOOT_FROM_HS_BUILD           = $(BOOT_FROM_HS)
    SSP_ENABLED                  = true
    SSP_ENABLE_STRONG_FLAG       = true
    RUNTIME_HS_OVL_SIG_PATCHING  = true
    PR_V0404                     = true
    SINGLE_METHOD_ID_REPLAY      = true

    WAR_SELWRE_THROTTLE_MINING_BUG_3263169 = true

    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED             = 2
    IS_PROD_ENCRYPT_BIN_RELEASE = false
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_apm_boot_from_hs")
    PROJ                         = ga10x_apm_boot_from_hs
    PROJ1                        = ga102
    ARCH                         = ampere
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ga10x-apm-boot-from-hs-debug.cfg
    SIG_GEN_PROD_CFG             = sec2-siggen-ga10x-apm-boot-from-hs-prod.cfg
    DEBUG_SIGN_ONLY              = true
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = false
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ga10x_asb_boot_from_hs
    ACR_RISCV_LS                 = true
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 3
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    BOOT_FROM_HS                 = true
    BL_PROFILE                   = sec2_hs_bl_rtos
    BOOT_FROM_HS_BUILD           = $(BOOT_FROM_HS)
    SSP_ENABLED                  = true
    SSP_ENABLE_STRONG_FLAG       = true
    RUNTIME_HS_OVL_SIG_PATCHING  = true
    SIGN_LOCAL                   = 1
    SIGN_SERVER                  = 0

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED        = 2
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_nouveau_boot_from_hs")
    PROJ                         = ga10x_nouveau_boot_from_hs
    PROJ1                        = ga102
    ARCH                         = ampere
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = nouveau_sec2-siggen-ga10x-boot-from-hs-debug.cfg
    SIG_GEN_PROD_CFG             = nouveau_sec2-siggen-ga10x-boot-from-hs-prod.cfg
    DEBUG_SIGN_ONLY              = false
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/nouveau_sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = false
    PR_SL3000_ENABLED            = false
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ga10x_asb_boot_from_hs
    ACR_RISCV_LS                 = false
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 4
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    BOOT_FROM_HS                 = true
    BL_PROFILE                   = sec2_hs_bl_rtos
    BOOT_FROM_HS_BUILD           = $(BOOT_FROM_HS)
    SSP_ENABLED                  = true
    SSP_ENABLE_STRONG_FLAG       = true
    RELEASE_PATH                 = $(RESMAN_ROOT)/kernel/inc/nouveau_sec2/bin
    NOUVEAU_SUPPORTED            = true
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED              = 2
    WAR_SELWRE_THROTTLE_MINING_BUG_3263169 = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ga10x_prmods")
    PROJ                   = ga10x_prmods
    PROJ1                  = ga100
    ARCH                   = ampere
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ga10x_asb_new_wpr_blobs/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-ga10x-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-ga10x-prod.cfg
    DEBUG_SIGN_ONLY        = true
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ga10x.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = true
    PR_SL3000_ENABLED      = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-ga10x_asb_new_wpr_blobs
    ACR_RISCV_LS           = true
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS           = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE      = 1
    NEW_WPR_BLOBS          = true
    SIGN_LICENSE           = CODESIGN_LS_PKC
    LS_UCODE_ID            = 1
    LS_UCODE_VERSION       = 1
    SSP_ENABLED            = false # Not enabling SSP for prmods profile yet.
    SINGLE_METHOD_ID_REPLAY  = true
    PR_V0404                 = true

    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED        = 0
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ad10x")
    PROJ                         = ad10x
    PROJ1                        = ad102
    ARCH                         = ada
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ad10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ad10x-debug.cfg
    SIG_GEN_PROD_CFG             = sec2-siggen-ad10x-prod.cfg
    DEBUG_SIGN_ONLY              = true
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ad10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = false
    PR_SL3000_ENABLED            = false
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ad10x_asb_boot_from_hs
    ACR_RISCV_LS                 = true
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 1
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    SIGN_LOCAL                   = 1
    SIGN_SERVER                  = 0

  # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
    SELWRITY_ENGINE              = false

    WAR_BUG_200670718            = true
    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED        = 0

  # Hash-saving feature will be disabled on Ada
    WAR_DISABLE_HASH_SAVING_BUG_3335627   = true
  endif

  ifeq ("$(SEC2CFG_PROFILE)","sec2-ad10x_boot_from_hs")
    PROJ                         = ad10x_boot_from_hs
    PROJ1                        = ad102
    ARCH                         = ada
    CHIP_MANUAL_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
    MANUAL_PATHS                += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS                += $(LW_SOURCE)/uproc/acr/src/_out/gsp/ad10x_asb_boot_from_hs/config
    FALCON_ARCH                  = falcon6
    SIG_GEN_DBG_CFG              = sec2-siggen-ad10x-boot-from-hs-debug.cfg
    SIG_GEN_PROD_CFG             = sec2-siggen-ad10x-boot-from-hs-prod.cfg
    DEBUG_SIGN_ONLY              = false
    LS_DEPENDENCY_MAP            = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_ad10x.cfg
    SECFG_PROFILE                = se-tu10x
    PR_MPKE_ENABLED              = true
    PR_SL3000_ENABLED            = true
    RTOS_VERSION                 = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION          = true
    DMREAD_WAR_200142015         = true
    ACRCFG_PROFILE               = acr_gsp-ad10x_asb_boot_from_hs
    ACR_RISCV_LS                 = true
    ENGINE_ID                    = $(ENGINE_ID_SEC2)
    SEC2_RTOS_COMMON_ID          = $(SEC2_RTOS_COMMON_ID)
    IS_PKC_ENABLED               = true
    SIG_SIZE_IN_BITS             = $(RSA3K_SIZE_IN_BITS)
    SIGGEN_CHIPS                 = "ga102_rsa3k_0"
    NUM_SIG_PER_UCODE            = 1
    NEW_WPR_BLOBS                = true
    SIGN_LICENSE                 = CODESIGN_LS_PKC
    LS_UCODE_ID                  = 1
    LS_UCODE_VERSION             = 1
    BL_PROFILE                   = sec2_hs_bl_rtos_ad10x
  # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
    SELWRITY_ENGINE              = false

    BOOT_FROM_HS                 = true
    BOOT_FROM_HS_BUILD           = $(BOOT_FROM_HS)
    RUNTIME_HS_OVL_SIG_PATCHING  = true
  # SSP_ENABLED                  = true
  # SSP_ENABLE_STRONG_FLAG       = true
    WAR_BUG_200670718            = true
    # Skip signing and submission of prod-signed binary for AS2 build
    ifeq ("$(AS2_SEC2_SKIP_SIGN_RELEASE)","true")
      ENABLE_HS_SIGN     = false
      NO_RELEASE         = true
    endif
    # This flag will be used for AES-CBC LS Encryption
    IS_LS_ENCRYPTED        = 2

  # Hash-saving feature will be disabled on Ada
    WAR_DISABLE_HASH_SAVING_BUG_3335627   = true
  endif

  ifneq (,$(findstring sec2-gh100,  $(SEC2CFG_PROFILE)))
    PROJ                   = gh100
    ARCH                   = hopper
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/gh100_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-gh100-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-gh100-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_gh100.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-gh100_asb
    ACR_RISCV_LS           = true
    SELWRITY_ENGINE        = false # Unlink libSE
    NEW_WPR_BLOBS          = true
    SIGN_LICENSE           = CODESIGN_LS_PKC
    LS_UCODE_ID            = 1
    LS_UCODE_VERSION       = 0
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    WAR_BUG_200670718      = true

    # Disabling SSP for FNL as SE TRNG is being removed in Hopper (Bug 3078167)
    SSP_ENABLED            = false
    # Using GA100 siggen configs, MPKE and siggen chips now because GH100 is
    # not yet wired properly for those.
    SIGGEN_CHIPS               = "ga100_rsa3k_1"

    # WAR to disable hub encryption check since hub encryption is moving to FSP
    WAR_DISABLE_HUB_ENC_CHECK_BUG_3126208 = true
  endif

  ifneq (,$(findstring sec2-gh20x,  $(SEC2CFG_PROFILE)))
    PROJ                   = gh20x
    ARCH                   = hopper
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/gh20x_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-gh20x-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-gh20x-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_gh20x.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-gh20x_asb
    ACR_RISCV_LS           = true
    SELWRITY_ENGINE        = false # Unlink libSE

    # Disabling SSP for FNL as SE TRNG is being removed in Hopper (Bug 3078167)
    SSP_ENABLED            = false

    # Using TU102 siggen configs, MPKE and siggen chips now because GH20X is
    # not yet wired properly for those.
    SIGGEN_CHIPS           = "tu116_aes"

    # WAR to disable hub encryption check since hub encryption is moving to FSP
    WAR_DISABLE_HUB_ENC_CHECK_BUG_3126208 = true
  endif

  ifneq (,$(findstring sec2-g00x,  $(SEC2CFG_PROFILE)))
    PROJ                   = g00x
    ARCH                   = hopper
    CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
    MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS          += $(LW_SOURCE)/uproc/acr/src/_out/gsp/g000_asb/config
    FALCON_ARCH            = falcon6
    SIG_GEN_DBG_CFG        = sec2-siggen-g000-debug.cfg
    SIG_GEN_PROD_CFG       = sec2-siggen-g000-prod.cfg
    DEBUG_SIGN_ONLY        = false
    LS_DEPENDENCY_MAP      = $(SEC2_SW)/build/sign/sec2-ls-version_dependency_map_g000.cfg
    SECFG_PROFILE          = se-tu10x
    PR_MPKE_ENABLED        = false
    RTOS_VERSION           = OpenRTOSv4.1.3
    HS_UCODE_ENCRYPTION    = true
    DMREAD_WAR_200142015   = true
    ACRCFG_PROFILE         = acr_gsp-g000_asb
    ACR_RISCV_LS           = true
    SELWRITY_ENGINE        = false # Unlink libSE
    NEW_WPR_BLOBS          = true
    SIGN_LICENSE           = CODESIGN_LS_PKC
    LS_UCODE_ID            = 1
    LS_UCODE_VERSION       = 0
    ENGINE_ID              = $(ENGINE_ID_SEC2)
    IS_PKC_ENABLED         = true
    SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
    WAR_BUG_200670718      = true

    # Disabling SSP for FNL as SE TRNG is being removed in Hopper (Bug 3078167)
    SSP_ENABLED            = false

    # Using TU102 siggen configs, MPKE and siggen chips now because G000 is
    # not yet wired properly for those.
    SIGGEN_CHIPS           = "ga100_rsa3k_1"

    # WAR to disable hub encryption check since hub encryption is moving to FSP
    WAR_DISABLE_HUB_ENC_CHECK_BUG_3126208 = true
  endif
endif
