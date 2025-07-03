#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available ACRCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in ACRCFG_PROFILE. To pick-up the definitions, set ACRCFG_PROFILE to the
# desired profile and source/include this file. When ACRCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# ACRCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# This flag is set to "true" for AS2 build and it is checked in profiles
# which have prod signed binaries in chips_a to skip signing and submission.
# This is required to avoid overwriting prod binaries by debug binaries
# through AS2 submit.
# Defaulting to "false" for other scenarios like DVS or local build.
##############################################################################
AS2_ACR_SKIP_SIGN_RELEASE ?=false

##############################################################################
# Build a list of all possible acr_pmu-config profiles
##############################################################################

ACRCFG_PROFILES_ALL :=

#ACRCFG_PROFILES_ALL += acr_sec2-tu10x_ahesasc
#ACRCFG_PROFILES_ALL += acr_gsp-tu10x_asb
#ACRCFG_PROFILES_ALL += acr_sec2-tu10x_bsi_lockdown
#ACRCFG_PROFILES_ALL += acr_pmu-tu10x_unload

#ACRCFG_PROFILES_ALL += acr_sec2-tu10x_fmodel_ahesasc
#ACRCFG_PROFILES_ALL += acr_gsp-tu10x_fmodel_asb


# TU11X ACR profiles
#ACRCFG_PROFILES_ALL += acr_sec2-tu116_ahesasc
#ACRCFG_PROFILES_ALL += acr_gsp-tu116_asb
#ACRCFG_PROFILES_ALL += acr_sec2-tu116_bsi_lockdown
#ACRCFG_PROFILES_ALL += acr_pmu-tu116_unload

#ACRCFG_PROFILES_ALL += acr_sec2-tu116_fmodel_ahesasc
#ACRCFG_PROFILES_ALL += acr_gsp-tu116_fmodel_asb

# x86 ACR profiles
#ACRCFG_PROFILES_ALL += acr_sec2-tu10a_ahesasc_pkc
#ACRCFG_PROFILES_ALL += acr_gsp-tu10a_asb_pkc
#ACRCFG_PROFILES_ALL += acr_pmu-tu10a_unload_pkc

# Enchanced AES Turing ACR profiles
#ACRCFG_PROFILES_ALL += acr_sec2-tu10x_ahesasc_enhanced_aes
#ACRCFG_PROFILES_ALL += acr_gsp-tu10x_asb_enhanced_aes
#ACRCFG_PROFILES_ALL += acr_pmu-tu10x_unload_enhanced_aes

# GA100 ACR profiles
ACRCFG_PROFILES_ALL += acr_sec2-ga100_ahesasc
ACRCFG_PROFILES_ALL += acr_sec2-ga100_ahesasc_apm
ACRCFG_PROFILES_ALL += acr_sec2-ga100_ahesasc_gsp_rm
ACRCFG_PROFILES_ALL += acr_gsp-ga100_asb
ACRCFG_PROFILES_ALL += acr_sec2-ga100_fmodel_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-ga100_fmodel_asb
#ACRCFG_PROFILES_ALL += acr_pmu-ga100_unload
ACRCFG_PROFILES_ALL += acr_sec2-ga100_unload
ACRCFG_PROFILES_ALL += acr_sec2-ga100_unload_gsp_rm

# GA10X ACR profiles
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_ahesasc
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_ahesasc_apm
ACRCFG_PROFILES_ALL += acr_gsp-ga10x_asb
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_bsi_lockdown
#ACRCFG_PROFILES_ALL += acr_pmu-ga10x_unload
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_unload
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_fmodel_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-ga10x_fmodel_asb

ACRCFG_PROFILES_ALL += acr_sec2-ga10x_ahesasc_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_gsp-ga10x_asb_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_bsi_lockdown_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_unload_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_fmodel_ahesasc_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_gsp-ga10x_fmodel_asb_new_wpr_blobs

ACRCFG_PROFILES_ALL += acr_sec2-ga10x_ahesasc_boot_from_hs
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_ahesasc_boot_from_hs_gsp_rm
ACRCFG_PROFILES_ALL += acr_gsp-ga10x_asb_boot_from_hs
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_unload_boot_from_hs
ACRCFG_PROFILES_ALL += acr_sec2-ga10x_unload_boot_from_hs_gsp_rm

# AD10X ACR profiles
ACRCFG_PROFILES_ALL += acr_sec2-ad10x_ahesasc_boot_from_hs
ACRCFG_PROFILES_ALL += acr_gsp-ad10x_asb_boot_from_hs
ACRCFG_PROFILES_ALL += acr_sec2-ad10x_unload_boot_from_hs
ACRCFG_PROFILES_ALL += acr_sec2-ad10x_unload_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_sec2-ad10x_fmodel_ahesasc_new_wpr_blobs
ACRCFG_PROFILES_ALL += acr_gsp-ad10x_fmodel_asb_new_wpr_blobs

# GH100 ACR profiles
ACRCFG_PROFILES_ALL += acr_sec2-gh100_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-gh100_asb
ACRCFG_PROFILES_ALL += acr_sec2-gh100_unload
ACRCFG_PROFILES_ALL += acr_sec2-gh100_fmodel_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-gh100_fmodel_asb

# GH20X ACR profiles
ACRCFG_PROFILES_ALL += acr_sec2-gh20x_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-gh20x_asb
ACRCFG_PROFILES_ALL += acr_sec2-gh20x_unload
ACRCFG_PROFILES_ALL += acr_sec2-gh20x_fmodel_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-gh20x_fmodel_asb

# G000 ACR profiles
ACRCFG_PROFILES_ALL += acr_sec2-g000_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-g000_asb
ACRCFG_PROFILES_ALL += acr_sec2-g000_unload
ACRCFG_PROFILES_ALL += acr_sec2-g000_fmodel_ahesasc
ACRCFG_PROFILES_ALL += acr_gsp-g000_fmodel_asb

ACRSW               = $(LW_SOURCE)/uproc/acr/

SIGGEN_CSG_PROD_SIGNING	 = false
##############################################################################
# Profile-specific settings
##############################################################################

ifdef ACRCFG_PROFILE
  MANUAL_PATHS                              += $(LW_SOURCE)/uproc/acr/inc
  SEC2_PRESENT                              = true
  GSP_PRESENT                               = true
  FBFALCON_PRESENT                          = true
  LWENC_PRESENT                             = true
  LWDEC_PRESENT                             = true
  LWDEC1_PRESENT                            = false
  OFA_PRESENT                               = false
  DPU_PRESENT                               = false
  ACR_UNLOAD                                = false        #This means ACR_UNLOAD_ON_PMU
  ACR_SIG_VERIF                             = true
  ACR_SKIP_MEMRANGE                         = false
  ACR_SKIP_SCRUB                            = false
  ACR_BUILD_ONLY                            = true
  LDSCRIPT                                  = link_script.ld
  ACR_FALCON_PMU                            = false
  ACR_FALCON_SEC2                           = false
  ACR_FIXED_TRNG                            = false
  ACR_FMODEL_BUILD                          = false
  ACR_RISCV_LS                              = false
  NOUVEAU_ACR                               = false
  UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE = false
  LWOS_VERSION                              = dev
  RTOS_VERSION                              = OpenRTOSv4.1.3
  IS_HS_ENCRYPTED                           = true
  SSP_ENABLED                               = true
  ASB                                       = false
  AHESASC                                   = false
  AMPERE_PROTECTED_MEMORY                   = false
  BSI_LOCK                                  = false
  ACR_UNLOAD_ON_SEC2                        = false
  PKC_ENABLED                               = false
  SIGGEN_FUSE_VER                           = -1        # -1 means not defined
  SIGGEN_ENGINE_ID                          = -1
  SIGGEN_UCODE_ID                           = -1
  SIGNATURE_SIZE                            = 128       # Default - AES Signature Size
  PRI_SOURCE_ISOLATION_PLM                  = false
  IS_ENHANCED_AES                           = false
  NEW_WPR_BLOBS                             = false
  PKC_LS_RSA3K_KEYS_GA10X                   = false
  NUM_SIG_PER_UCODE                         = 1
  BOOT_FROM_HS_BUILD                        = false
  BOOT_FROM_HS_FALCON_SEC2                  = false
  BOOT_FROM_HS_FALCON_GSP                   = false
  DISABLE_SE_ACCESSES                       = false
  ACR_ENFORCE_LS_UCODE_ENCRYPTION           = true
  LWDEC_RISCV_PRESENT                       = false
  LWDEC_RISCV_EB_PRESENT                    = false
  LWJPG_PRESENT                             = false
  ACR_SKIP_SIGN_RELEASE                     = false
  LIB_CCC_PRESENT                           = false
  ACR_MEMSETBYTE_ENABLED                    = true
  ACR_GSP_RM_BUILD                          = false

  ACR_GM200_UCODE_BUILD_VERSION = 0
  ACR_GP100_UCODE_BUILD_VERSION = 0
  ACR_GP104_UCODE_BUILD_VERSION = 0
  ACR_GP106_UCODE_BUILD_VERSION = 0
  ACR_GP107_UCODE_BUILD_VERSION = 0
  ACR_GP108_UCODE_BUILD_VERSION = 0
  ACR_GV100_UCODE_BUILD_VERSION = 0
  ACR_TU102_UCODE_BUILD_VERSION = 0
  ACR_TU104_UCODE_BUILD_VERSION = 0
  ACR_TU106_UCODE_BUILD_VERSION = 0
  ACR_TU116_UCODE_BUILD_VERSION = 0
  ACR_TU117_UCODE_BUILD_VERSION = 0
  ACR_TU10A_UCODE_BUILD_VERSION = 0
  ACR_GA100_UCODE_BUILD_VERSION = 0
  ACR_GA102_UCODE_BUILD_VERSION = 0
  ACR_AD102_UCODE_BUILD_VERSION = 0
  ACR_GH100_UCODE_BUILD_VERSION = 0
  ACR_GH202_UCODE_BUILD_VERSION = 0
  ACR_G000_UCODE_BUILD_VERSION  = 0

  ifneq (,$(findstring tu10x, $(ACRCFG_PROFILE)))
      ACR_TU102_UCODE_BUILD_VERSION = 4
      ACR_TU104_UCODE_BUILD_VERSION = 4
      ACR_TU106_UCODE_BUILD_VERSION = 4

  endif

  ifneq (,$(findstring tu116, $(ACRCFG_PROFILE)))
      ACR_TU116_UCODE_BUILD_VERSION = 3
      ACR_TU117_UCODE_BUILD_VERSION = 3
  endif

  ifneq (,$(findstring tu10a, $(ACRCFG_PROFILE)))
      ACR_TU10A_UCODE_BUILD_VERSION = 0
  endif

  ifneq (,$(findstring ga100, $(ACRCFG_PROFILE)))
      ACR_GA100_UCODE_BUILD_VERSION = 1
  endif

  ifneq (,$(findstring ga10x, $(ACRCFG_PROFILE)))
      ACR_GA102_UCODE_BUILD_VERSION = 1
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_pmu-tu10x_unload acr_pmu-tu10x_unload_enhanced_aes))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/tu10x
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      ACR_UNLOAD         = true
      ACR_ONLY_BO        = false
      SIGGEN_CFG         = acrul_siggen_tu10x.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      ifneq (,$(filter $(ACRCFG_PROFILE),  acr_pmu-tu10x_unload_enhanced_aes))
          CHIP_MANUAL_PATH  += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
          SIGGEN_FUSE_VER    = $(PMU_ACR_UNLOAD_ENHANCED_AES_VER)
          SIGGEN_ENGINE_ID   = $(ENGINE_ID_PMU)
          SIGGEN_UCODE_ID    = $(PMU_ACR_UNLOAD_ENHANCED_AES_ID)
          IS_ENHANCED_AES    = true
      endif
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_pmu-tu116_unload))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/tu116
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      ACR_UNLOAD         = true
      ACR_ONLY_BO        = false
      SIGGEN_CFG         = acrul_siggen_tu116.cfg
      SIGGEN_CHIPS      += "tu116_aes"
      ACR_SKIP_MEMRANGE  = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_pmu-ga100_unload))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v03_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v03_00
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/ga100
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD               = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ga100.cfg
      SIGGEN_CHIPS            += "gp104"
      ACR_SKIP_MEMRANGE        = true
      ACR_MEMSETBYTE_ENABLED   = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga100_unload))
      CHIP_MANUAL_PATH            = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS               += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH                = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga100/unload/
      LWENC_PRESENT               = false
      GSP_PRESENT                 = false
      FBFALCON_PRESENT            = false
      FALCON_ARCH                 = falcon6
      ACR_UNLOAD_ON_SEC2          = true
      ACR_ONLY_BO                 = false
      SIGGEN_CFG                  = acrul_siggen_ga100.cfg
      SIGGEN_CHIPS               += "ga100_rsa3k_1"
      ACR_SKIP_MEMRANGE           = true
      SIGGEN_FUSE_VER             = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID            = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID             = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE              = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                 = true
      NUM_SIG_PER_UCODE           = 2
      ACR_MEMSETBYTE_ENABLED      = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga100_unload_gsp_rm))
      CHIP_MANUAL_PATH            = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS               += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH                = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga100/unload/
      LWENC_PRESENT               = false
      GSP_PRESENT                 = false
      FBFALCON_PRESENT            = false
      FALCON_ARCH                 = falcon6
      ACR_UNLOAD_ON_SEC2          = true
      ACR_ONLY_BO                 = false
      SIGGEN_CFG                  = acrul_gsp_rm_siggen_ga100.cfg
      SIGGEN_CHIPS               += "ga100_rsa3k_1"
      ACR_SKIP_MEMRANGE           = true
      SIGGEN_FUSE_VER             = $(SEC2_ACR_UNLOAD_GSP_RM_VER)
      SIGGEN_ENGINE_ID            = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID             = $(SEC2_ACR_UNLOAD_GSP_RM_ID)
      SIGNATURE_SIZE              = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                 = true
      NUM_SIG_PER_UCODE           = 2
      ACR_MEMSETBYTE_ENABLED      = false
      ACR_GSP_RM_BUILD            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_unload))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/unload/
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ga10x.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_unload_new_wpr_blobs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/unload/
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ga10x.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 2
      PKC_ENABLED              = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_unload_boot_from_hs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/unload/
      LWDEC1_PRESENT           = true
      LWDEC_PRESENT            = true
      OFA_PRESENT              = true
      LWENC_PRESENT            = true
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ga10x.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 3
      PKC_ENABLED              = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_unload_boot_from_hs_gsp_rm))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/unload/
      LWDEC1_PRESENT           = true
      LWDEC_PRESENT            = true
      OFA_PRESENT              = true
      LWENC_PRESENT            = true
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_gsp_rm_siggen_ga10x.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_GSP_RM_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_GSP_RM_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 2
      PKC_ENABLED              = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      ACR_GSP_RM_BUILD         = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_pmu-ga10x_unload))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/ga10x
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD               = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ga10x.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu10x_ahesasc acr_sec2-tu10x_ahesasc_enhanced_aes))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu10x/ahesasc
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = acr_siggen_tu10x_ahesasc.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      AHESASC            = true
      ifneq (,$(filter $(ACRCFG_PROFILE),  acr_sec2-tu10x_ahesasc_enhanced_aes))
          CHIP_MANUAL_PATH  += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
          SIGGEN_FUSE_VER    = $(SEC2_ACR_AHESASC_ENHANCED_AES_VER)
          SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
          SIGGEN_UCODE_ID    = $(SEC2_ACR_AHESASC_ENHANCED_AES_ID)
          IS_ENHANCED_AES    = true
      endif
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-tu10x_asb acr_gsp-tu10x_asb_enhanced_aes))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/tu10x/asb
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = acr_siggen_tu10x_asb.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SIG_VERIF      = false
      ASB                = true
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
       ifneq (,$(filter $(ACRCFG_PROFILE),  acr_gsp-tu10x_asb_enhanced_aes))
          CHIP_MANUAL_PATH  += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
          SIGGEN_FUSE_VER    = $(GSP_ACR_ASB_ENHANCED_AES_VER)
          SIGGEN_ENGINE_ID   = $(ENGINE_ID_GSP)
          SIGGEN_UCODE_ID    = $(GSP_ACR_ASB_ENHANCED_AES_ID)
          IS_ENHANCED_AES    = true
      endif
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu10x_fmodel_ahesasc))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu10x/ahesasc
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      ACR_SIG_VERIF      = false
      SIGGEN_CFG         = acrfmodel_siggen_tu10x_ahesasc.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SKIP_SCRUB     = true
      ACR_FIXED_TRNG     = true
      ACR_FMODEL_BUILD   = true
      AHESASC            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-tu10x_fmodel_asb))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/tu10x/asb
      FALCON_ARCH        = falcon6
      ACR_SIG_VERIF      = false
      SIGGEN_CFG         = acrfmodel_siggen_tu10x_asb.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SKIP_SCRUB     = true
      ACR_FIXED_TRNG     = true
      ACR_FMODEL_BUILD   = true
      ASB                = true
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu116_ahesasc))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu116/ahesasc
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = acr_siggen_tu116_ahesasc.cfg
      SIGGEN_CHIPS      += "tu116_aes"
      ACR_SKIP_MEMRANGE  = true
      AHESASC            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-tu116_asb))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/tu116/asb
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = acr_siggen_tu116_asb.cfg
      SIGGEN_CHIPS      += "tu116_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SIG_VERIF      = false
      ASB                = true
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu116_fmodel_ahesasc))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu116/ahesasc
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      ACR_SIG_VERIF      = false
      SIGGEN_CFG         = acrfmodel_siggen_tu116_ahesasc.cfg
      SIGGEN_CHIPS      += "tu116_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SKIP_SCRUB     = true
      ACR_FIXED_TRNG     = true
      ACR_FMODEL_BUILD   = true
      AHESASC            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-tu116_fmodel_asb))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/tu116/asb
      FALCON_ARCH        = falcon6
      ACR_SIG_VERIF      = false
      SIGGEN_CFG         = acrfmodel_siggen_tu116_asb.cfg
      SIGGEN_CHIPS      += "tu116_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SKIP_SCRUB     = true
      ACR_FIXED_TRNG     = true
      ACR_FMODEL_BUILD   = true
      ASB                = true
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu10a_ahesasc_pkc))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu10a/ahesasc
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = acr_siggen_tu10a_ahesasc.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      IS_HS_ENCRYPTED    = true
      AHESASC            = true
      SSP_ENABLED        = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-tu10a_asb_pkc))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/tu10a/asb
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = acr_siggen_tu10a_asb.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      ACR_SIG_VERIF      = false
      IS_HS_ENCRYPTED    = true
      ASB                = true
      SSP_ENABLED        = true
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_pmu-tu10a_unload_pkc))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/tu10a
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      ACR_UNLOAD         = true
      ACR_ONLY_BO        = false
      SIGGEN_CFG         = acrul_siggen_tu10a.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      ACR_SKIP_MEMRANGE  = true
      IS_HS_ENCRYPTED    = true
      SSP_ENABLED        = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga100_ahesasc))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga100/ahesasc
      LWENC_PRESENT            = false
      FBFALCON_PRESENT         = false
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga100_ahesasc.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      NUM_SIG_PER_UCODE        = 2
      ACR_MEMSETBYTE_ENABLED   = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga100_ahesasc_apm))
      CHIP_MANUAL_PATH           = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS              += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS              += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH               = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga100/ahesasc_apm
      LWENC_PRESENT              = false
      FBFALCON_PRESENT           = false
      FALCON_ARCH                = falcon6
      SIGGEN_CFG                 = acr_siggen_ga100_ahesasc_apm.cfg
      SIGGEN_CHIPS              += "ga100_rsa3k_1"
      SIGGEN_FUSE_VER            = $(SEC2_ACR_AHESASC_APM_VER)
      SIGGEN_ENGINE_ID           = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID            = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE             = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                = true
      ACR_SKIP_MEMRANGE          = true
      AHESASC                    = true
      AMPERE_PROTECTED_MEMORY    = true
      NUM_SIG_PER_UCODE          = 2
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga100_ahesasc_gsp_rm))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga100/ahesasc_gsp_rm
      LWENC_PRESENT            = false
      FBFALCON_PRESENT         = false
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga100_ahesasc_gsp_rm.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_GSP_RM_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_GSP_RM_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      NUM_SIG_PER_UCODE        = 2
      ACR_MEMSETBYTE_ENABLED   = false
      ACR_GSP_RM_BUILD         = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga100_asb))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga100/asb
      LWENC_PRESENT            = false
      FBFALCON_PRESENT         = false
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga100_asb.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      NUM_SIG_PER_UCODE        = 2
      ACR_MEMSETBYTE_ENABLED   = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga100_fmodel_ahesasc))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v03_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v03_00
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga100/ahesasc
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_ga100_ahesasc.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_0"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ACR_RISCV_LS             = true
      AHESASC                  = true
      ACR_MEMSETBYTE_ENABLED   = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga100_fmodel_asb))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v03_00
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v03_00
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga100/asb
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_ga100_asb.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_0"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ACR_RISCV_LS             = true
      ASB                      = true
      ACR_MEMSETBYTE_ENABLED   = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_ahesasc))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_ahesasc.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      PRI_SOURCE_ISOLATION_PLM = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ad10x_ahesasc_boot_from_hs))
      CHIP_MANUAL_PATH                 = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS                    += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                     = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ad10x/ahesasc
      LWDEC_PRESENT                    = true
      LWDEC1_PRESENT                   = true
      OFA_PRESENT                      = true
      LWJPG_PRESENT                    = true
      LWENC_PRESENT                    = true
      FALCON_ARCH                      = falcon6
      SIGGEN_CFG                       = acr_siggen_ad10x_ahesasc.cfg
      SIGGEN_CHIPS                     = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER                  = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                 = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                  = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                   = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE                = 1
      PKC_ENABLED                      = true
      ACR_SIG_VERIF                    = false
      ACR_SKIP_MEMRANGE                = true
      AHESASC                          = true
      PRI_SOURCE_ISOLATION_PLM         = true
      ACR_RISCV_LS                     = true
      NEW_WPR_BLOBS                    = true
      PKC_LS_RSA3K_KEYS_GA10X          = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION  = false

    # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
      ACR_FIXED_TRNG           = true
      LIB_CCC_PRESENT          = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = false
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ad10x_asb_boot_from_hs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ad10x/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ad10x_asb.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 1
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = true
      LWDEC_PRESENT            = true
      LWDEC1_PRESENT           = true
      LWJPG_PRESENT            = true
      LWENC_PRESENT            = true
      OFA_PRESENT              = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true

    # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_GSP  = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ad10x_unload_boot_from_hs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ad10x/unload/
      LWDEC1_PRESENT           = true
      LWDEC_PRESENT            = true
      OFA_PRESENT              = true
      LWENC_PRESENT            = true
      LWJPG_PRESENT            = true
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ad10x.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 1
      PKC_ENABLED              = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true

    # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ad10x_fmodel_ahesasc_new_wpr_blobs))
      CHIP_MANUAL_PATH                 = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS                    += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                     = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ad10x/ahesasc
      LWENC_PRESENT                    = false
      FALCON_ARCH                      = falcon6
      ACR_SIG_VERIF                    = false
      SIGGEN_CFG                       = acrfmodel_siggen_ad10x_ahesasc.cfg
      SIGGEN_CHIPS                     = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                  = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                 = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                  = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                   = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE                = 1
      PKC_ENABLED                      = true
      ACR_SKIP_MEMRANGE                = true
      ACR_SKIP_SCRUB                   = true
      ACR_FIXED_TRNG                   = true
      ACR_FMODEL_BUILD                 = true
      AHESASC                          = true
      PRI_SOURCE_ISOLATION_PLM         = true
      NEW_WPR_BLOBS                    = true
      PKC_LS_RSA3K_KEYS_GA10X          = true
      ACR_RISCV_LS                     = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION  = false

    # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
      SSP_ENABLED                      = false
      DISABLE_SE_ACCESSES              = true
      LIB_CCC_PRESENT                  = true

      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE        = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ad10x_fmodel_asb_new_wpr_blobs))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ad10x/asb
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_ad10x_asb.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      NUM_SIG_PER_UCODE        = 1
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true

    # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true

      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ad10x_unload_new_wpr_blobs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ad10x/unload/
      LWDEC1_PRESENT           = true
      LWDEC_PRESENT            = true
      OFA_PRESENT              = true
      LWENC_PRESENT            = true
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_ad10x.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      ACR_SKIP_MEMRANGE        = true
      PRI_SOURCE_ISOLATION_PLM = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 1
      PKC_ENABLED              = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true

    # TODO: Disabling SNPSPKA access and switch to LWPKA (Bug 200729855)
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-gh100_ahesasc))
      CHIP_MANUAL_PATH                    = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS                        += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                        = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/gh100/ahesasc
      FALCON_ARCH                         = falcon6
      SIGGEN_CFG                          = acr_siggen_gh100_ahesasc.cfg
      SIGGEN_CHIPS                        = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                     = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                    = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                     = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                      = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                         = true
      ACR_SKIP_MEMRANGE                   = true
      AHESASC                             = true
      PRI_SOURCE_ISOLATION_PLM            = true
      ACR_FIXED_TRNG                      = true
      SSP_ENABLED                         = false
      DISABLE_SE_ACCESSES                 = true
      NEW_WPR_BLOBS                       = true
      ACR_SIG_VERIF                       = true
      PKC_LS_RSA3K_KEYS_GA10X             = true
      ACR_RISCV_LS                        = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION     = false
      OFA_PRESENT                         = true
      LWENC_PRESENT                       = false
      OFA_PRESENT                         = true
      LWDEC_RISCV_PRESENT                 = true
      LWDEC_RISCV_EB_PRESENT              = true
      LWJPG_PRESENT                       = true
      LIB_CCC_PRESENT                     = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-gh100_unload))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/gh100/unload/
      LWENC_PRESENT            = false
      GSP_PRESENT              = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_gh100.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_1"
      ACR_SKIP_MEMRANGE        = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      NEW_WPR_BLOBS            = true
      NUM_SIG_PER_UCODE        = 1   # Changed to 1 oherwise there are error seen while generating  signature
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
      ACR_RISCV_LS             = true
      OFA_PRESENT              = true
      LWENC_PRESENT            = false
      LWDEC_RISCV_PRESENT      = true
      LWDEC_RISCV_EB_PRESENT   = true
      LWJPG_PRESENT            = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-gh100_asb))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh100/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_gh100_asb.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_FIXED_TRNG           = true
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-gh100_fmodel_ahesasc))
      CHIP_MANUAL_PATH                    = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS                        += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                        = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/gh100/ahesasc
      FALCON_ARCH                         = falcon6
      ACR_SIG_VERIF                       = false
      SIGGEN_CFG                          = acrfmodel_siggen_gh100_ahesasc.cfg
      SIGGEN_CHIPS                        = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                     = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                    = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                     = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                      = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                         = true
      ACR_SKIP_MEMRANGE                   = true
      ACR_SKIP_SCRUB                      = true
      ACR_FIXED_TRNG                      = true
      ACR_FMODEL_BUILD                    = true
      AHESASC                             = true
      PRI_SOURCE_ISOLATION_PLM            = true
      SSP_ENABLED                         = false
      ACR_RISCV_LS                        = true
      DISABLE_SE_ACCESSES                 = true
      NEW_WPR_BLOBS                       = true
      PKC_LS_RSA3K_KEYS_GA10X             = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION     = false
      OFA_PRESENT                         = true
      LWENC_PRESENT                       = false
      LWDEC_RISCV_PRESENT                 = true
      LWDEC_RISCV_EB_PRESENT              = true
      LWJPG_PRESENT                       = true
      LIB_CCC_PRESENT                     = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-gh100_fmodel_asb))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh100/asb
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_gh100_asb.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ASB                      = true
      LWENC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      SSP_ENABLED              = false
      ACR_RISCV_LS             = true
      DISABLE_SE_ACCESSES      = true
      NEW_WPR_BLOBS            = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-gh20x_ahesasc))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/gh20x/ahesasc
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_gh20x_ahesasc.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_FIXED_TRNG           = true
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-gh20x_unload))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/gh20x/unload/
      LWENC_PRESENT            = false
      GSP_PRESENT              = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_gh20x.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_1"
      ACR_SKIP_MEMRANGE        = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      NUM_SIG_PER_UCODE        = 1   # Changed to 1 oherwise there are error seen while generating  signature
      DISABLE_SE_ACCESSES      = true
      ACR_FIXED_TRNG           = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-gh20x_asb))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh20x/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_gh20x_asb.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_FIXED_TRNG           = true
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-gh20x_fmodel_ahesasc))
      CHIP_MANUAL_PATH                    = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS                        += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                        += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                        = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/gh20x/ahesasc
      LWENC_PRESENT                       = false
      FALCON_ARCH                         = falcon6
      ACR_SIG_VERIF                       = false
      SIGGEN_CFG                          = acrfmodel_siggen_gh20x_ahesasc.cfg
      SIGGEN_CHIPS                        = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                     = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                    = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                     = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                      = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                         = true
      ACR_SKIP_MEMRANGE                   = true
      ACR_SKIP_SCRUB                      = true
      ACR_FIXED_TRNG                      = true
      ACR_FMODEL_BUILD                    = true
      AHESASC                             = true
      PRI_SOURCE_ISOLATION_PLM            = true
      SSP_ENABLED                         = false
      ACR_RISCV_LS                        = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION     = false
      DISABLE_SE_ACCESSES                 = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-gh20x_fmodel_asb))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh20x/asb
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_gh20x_asb.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ASB                      = true
      LWENC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      SSP_ENABLED              = false
      DISABLE_SE_ACCESSES      = true
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-g000_ahesasc))
      CHIP_MANUAL_PATH                   = $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
      MANUAL_PATHS                      += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/g000/ahesasc
      LWENC_PRESENT                      = false
      FALCON_ARCH                        = falcon6
      SIGGEN_CFG                         = acr_siggen_g000_ahesasc.cfg
      SIGGEN_CHIPS                       = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                    = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                    = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                     = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                        = true
      ACR_SKIP_MEMRANGE                  = true
      AHESASC                            = true
      PRI_SOURCE_ISOLATION_PLM           = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION    = false
      NEW_WPR_BLOBS                      = true
      PKC_LS_RSA3K_KEYS_GA10X            = true
      ACR_FIXED_TRNG                     = true
      DISABLE_SE_ACCESSES                = true
      ACR_SIG_VERIF                      = false
      ACR_RISCV_LS                       = true
      LIB_CCC_PRESENT                    = true
      SSP_ENABLED                        = false
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-g000_unload))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/g000/unload/
      LWENC_PRESENT            = false
      GSP_PRESENT              = false
      FALCON_ARCH              = falcon6
      ACR_UNLOAD_ON_SEC2       = true
      ACR_ONLY_BO              = false
      SIGGEN_CFG               = acrul_siggen_g000.cfg
      SIGGEN_CHIPS            += "ga100_rsa3k_1"
      ACR_SKIP_MEMRANGE        = true
      SIGGEN_FUSE_VER          = $(SEC2_ACR_UNLOAD_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_UNLOAD_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      NUM_SIG_PER_UCODE        = 1   # Changed to 1 oherwise there are error seen while generating  signature
      NEW_WPR_BLOBS            = true
      ACR_FIXED_TRNG           = true
      DISABLE_SE_ACCESSES      = true
      ACR_RISCV_LS             = true
      SSP_ENABLED              = false
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-g000_asb))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/g000/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_g000_asb.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      NEW_WPR_BLOBS            = true
      ACR_FIXED_TRNG           = true
      DISABLE_SE_ACCESSES      = true
      ACR_SIG_VERIF            = false
      ACR_RISCV_LS             = true
      SSP_ENABLED              = false
  endif
       
  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-g000_fmodel_ahesasc))
      CHIP_MANUAL_PATH                 = $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
      MANUAL_PATHS                    += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                     = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/g000/ahesasc
      LWENC_PRESENT                    = false
      FALCON_ARCH                      = falcon6
      ACR_SIG_VERIF                    = false
      SIGGEN_CFG                       = acrfmodel_siggen_g000_ahesasc.cfg
      SIGGEN_CHIPS                     = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                  = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                 = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                  = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                   = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                      = true
      ACR_SKIP_MEMRANGE                = true
      ACR_SKIP_SCRUB                   = true
      ACR_FIXED_TRNG                   = true
      ACR_FMODEL_BUILD                 = true
      AHESASC                          = true
      PRI_SOURCE_ISOLATION_PLM         = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION  = false
      NEW_WPR_BLOBS                    = true
      PKC_LS_RSA3K_KEYS_GA10X          = true
      SSP_ENABLED                      = false
      ACR_RISCV_LS                     = true
      DISABLE_SE_ACCESSES              = true
      ACR_FIXED_TRNG                   = true
      DISABLE_SE_ACCESSES              = true
      ACR_SIG_VERIF                    = false
      LIB_CCC_PRESENT                  = true
  endif
     
  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-g000_fmodel_asb))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/g000/asb
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_g000_asb.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      NEW_WPR_BLOBS            = true
      ACR_FIXED_TRNG           = true
      DISABLE_SE_ACCESSES      = true
      ACR_SIG_VERIF            = false
      ACR_RISCV_LS             = true
      SSP_ENABLED              = false
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_ahesasc_new_wpr_blobs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_ahesasc.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 2
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true
      PKC_LS_RSA3K_KEYS_GA10X  = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_ahesasc_boot_from_hs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc
      LWENC_PRESENT            = true
      LWDEC_PRESENT            = true
      LWDEC1_PRESENT           = true
      OFA_PRESENT              = true
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_ahesasc.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 3
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true
      PKC_LS_RSA3K_KEYS_GA10X  = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_ahesasc_boot_from_hs_gsp_rm))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc_gsp_rm
      LWENC_PRESENT            = true
      LWDEC_PRESENT            = true
      LWDEC1_PRESENT           = true
      OFA_PRESENT              = true
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_ahesasc_gsp_rm.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_GSP_RM_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_GSP_RM_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 2
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true
      PKC_LS_RSA3K_KEYS_GA10X  = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      ACR_GSP_RM_BUILD         = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_ahesasc_apm))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc_apm
      LWENC_PRESENT            = true
      LWDEC_PRESENT            = true
      LWDEC1_PRESENT           = true
      OFA_PRESENT              = true
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_ahesasc_apm.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(SEC2_ACR_AHESASC_APM_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID          = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 1
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      AHESASC                  = true
      AMPERE_PROTECTED_MEMORY  = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true
      PKC_LS_RSA3K_KEYS_GA10X  = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_SEC2 = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_bsi_lockdown_new_wpr_blobs))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      BSI_LOCK                 = true
      SIGGEN_CFG               = acrbsilock_siggen_ga10x.cfg
      SIGGEN_CHIPS             += "tu116_aes"
      SSP_ENABLED              = false
      PRI_SOURCE_ISOLATION_PLM = true
      NEW_WPR_BLOBS            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_fmodel_ahesasc_new_wpr_blobs))
      CHIP_MANUAL_PATH                 = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS                    += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                     = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc
      LWENC_PRESENT                    = false
      FALCON_ARCH                      = falcon6
      ACR_SIG_VERIF                    = false
      SIGGEN_CFG                       = acrfmodel_siggen_ga10x_ahesasc.cfg
      SIGGEN_CHIPS                     = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                  = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                 = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                  = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                   = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE                = 3
      PKC_ENABLED                      = true
      ACR_SKIP_MEMRANGE                = true
      ACR_SKIP_SCRUB                   = true
      ACR_FIXED_TRNG                   = true
      ACR_FMODEL_BUILD                 = true
      AHESASC                          = true
      PRI_SOURCE_ISOLATION_PLM         = true
      NEW_WPR_BLOBS                    = true
      PKC_LS_RSA3K_KEYS_GA10X          = true
      ACR_RISCV_LS                     = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION  = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE        = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga10x_fmodel_asb_new_wpr_blobs))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga10x/asb
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_ga10x_asb.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      NUM_SIG_PER_UCODE        = 3
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      NEW_WPR_BLOBS            = true
      ACR_RISCV_LS             = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga10x_asb))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga10x/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_asb.cfg
      SIGGEN_CHIPS             = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga10x_asb_new_wpr_blobs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga10x/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_asb.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 2
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga10x_asb_boot_from_hs))
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga10x/asb
      FALCON_ARCH              = falcon6
      SIGGEN_CFG               = acr_siggen_ga10x_asb.cfg
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      SIGGEN_FUSE_VER          = $(GSP_ACR_ASB_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_UCODE_ID          = $(GSP_ACR_ASB_ID)
      SIGNATURE_SIZE           = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE        = 3
      PKC_ENABLED              = true
      ACR_SKIP_MEMRANGE        = true
      ACR_SIG_VERIF            = false
      ASB                      = true
      LWENC_PRESENT            = true
      LWDEC_PRESENT            = true
      LWDEC1_PRESENT           = true
      OFA_PRESENT              = true
      PRI_SOURCE_ISOLATION_PLM = true
      ACR_RISCV_LS             = true
      NEW_WPR_BLOBS            = true
      BOOT_FROM_HS_BUILD       = true
      BOOT_FROM_HS_FALCON_GSP  = true
      LDSCRIPT                 = link_script_boot_from_hs.ld
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-ga10x_fmodel_ahesasc))
      CHIP_MANUAL_PATH                 = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS                    += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                     = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x/ahesasc
      LWENC_PRESENT                    = false
      FALCON_ARCH                      = falcon6
      ACR_SIG_VERIF                    = false
      SIGGEN_CFG                       = acrfmodel_siggen_ga10x_ahesasc.cfg
      SIGGEN_CHIPS                     = "ga100_rsa3k_1"
      SIGGEN_FUSE_VER                  = $(SEC2_ACR_AHESASC_VER)
      SIGGEN_ENGINE_ID                 = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                  = $(SEC2_ACR_AHESASC_ID)
      SIGNATURE_SIZE                   = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                      = true
      ACR_SKIP_MEMRANGE                = true
      ACR_SKIP_SCRUB                   = true
      ACR_FIXED_TRNG                   = true
      ACR_FMODEL_BUILD                 = true
      AHESASC                          = true
      PRI_SOURCE_ISOLATION_PLM         = true
      ACR_ENFORCE_LS_UCODE_ENCRYPTION  = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE        = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_gsp-ga10x_fmodel_asb))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/ga10x/asb
      FALCON_ARCH              = falcon6
      ACR_SIG_VERIF            = false
      SIGGEN_CFG               = acrfmodel_siggen_ga10x_asb.cfg
      SIGGEN_CHIPS            += "tu116_aes"
      ACR_SKIP_MEMRANGE        = true
      ACR_SKIP_SCRUB           = true
      ACR_FIXED_TRNG           = true
      ACR_FMODEL_BUILD         = true
      ASB                      = true
      LWENC_PRESENT            = false
      LWDEC_PRESENT            = false
      PRI_SOURCE_ISOLATION_PLM = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu10x_bsi_lockdown))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu10x
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      BSI_LOCK           = true
      SIGGEN_CFG         = acrbsilock_siggen_tu10x.cfg
      SIGGEN_CHIPS      += "tu102_aes"
      SSP_ENABLED        = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE), acr_sec2-tu116_bsi_lockdown))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu116
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      BSI_LOCK           = true
      SIGGEN_CFG         = acrbsilock_siggen_tu116.cfg
      SIGGEN_CHIPS      += "tu116_aes"
      SSP_ENABLED        = false
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

  ifneq (,$(filter $(ACRCFG_PROFILE),  acr_sec2-ga10x_bsi_lockdown))
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/ga10x
      LWENC_PRESENT            = false
      FALCON_ARCH              = falcon6
      BSI_LOCK                 = true
      SIGGEN_CFG               = acrbsilock_siggen_ga10x.cfg
      SIGGEN_CHIPS             += "tu116_aes"
      SSP_ENABLED              = false
      PRI_SOURCE_ISOLATION_PLM = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_ACR_SKIP_SIGN_RELEASE)","true")
          ACR_SKIP_SIGN_RELEASE    = true
      endif
  endif

endif
