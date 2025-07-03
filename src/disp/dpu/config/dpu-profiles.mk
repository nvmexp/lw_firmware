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
# 1. Define all available DPUCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in DPUCFG_PROFILE. To pick-up the definitions, set DPUCFG_PROFILE to the
# desired profile and source/include this file. When DPUCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# DPUCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# This flag is set to "true" for AS2 build and it is checked in profiles
# which have prod signed binaries in chips_a to skip signing and submission.
# This is required to avoid overwriting prod binaries by debug binaries
# through AS2 submit.
# Defaulting to "false" for other scenarios like DVS or local build.
##############################################################################
AS2_DPU_SKIP_SIGN_RELEASE ?= false

##############################################################################
# Build a list of all possible dpu-config profiles
##############################################################################

DPUCFG_PROFILES_ALL :=
DPUCFG_PROFILES_ALL += dpu-v0201
DPUCFG_PROFILES_ALL += dpu-v0205
DPUCFG_PROFILES_ALL += dpu-v0207
DPUCFG_PROFILES_ALL += dpu-gv100
DPUCFG_PROFILES_ALL += dpu-tu10x
DPUCFG_PROFILES_ALL += dpu-tu116
DPUCFG_PROFILES_ALL += dpu-ga10x
#DPUCFG_PROFILES_ALL += dpu-ga10x_boot_from_hs
DPUCFG_PROFILES_ALL += dpu-ad10x
DPUCFG_PROFILES_ALL += dpu-ad10x_boot_from_hs
#NOTE: intentionally disabled due to as2 seeing issues when enabled (see bug 3087201 as example)

##############################################################################
# Profile-specific settings
##############################################################################

ifdef DPUCFG_PROFILE
  MANUAL_PATHS                  =
  DMA_SUSPENSION                = false
  TASK_RESTART                  = false
  LS_FALCON                     = false
  FLCNDBG_ENABLED               = true
  ON_DEMAND_PAGING_OVL_IMEM     = false
  MRU_OVERLAYS                  = true
  LS_UCODE_VERSION              = 0
  GSPLITE                       = false
  EMEM_SUPPORTED                = false
  DMEM_VA_SUPPORTED             = false
  SECFG_PROFILE                 = se-gp10x
  SELWRITY_ENGINE               = false
  HS_OVERLAYS_ENABLED           = false
  ENABLE_HS_SIGN                = true
  OS_CALLBACKS                  = false
  HDCP22_USE_SCP_ENCRYPT_SECRET = false
  HDCP22_CHECK_STATE_INTEGRITY  = false
  HDCP22_LC_RETRY_WAR_ENABLED   = false
  HDCP22_WAR_3051763_ENABLED    = false
  HDCP22_WAR_ECF_SELWRE_ENABLED = false
  GSPLITE_RTTIMER_WAR_ENABLED   = false
  HDCP22_USE_SCP_GEN_DKEY       = false
  LWOS_VERSION                  = dev
  SSP_ENABLED                   = false
  RTTIMER_TEST_LIB              = false
  HS_UCODE_ENCRYPTION           = false
  EXCLUDE_LWOSDEBUG             = false
  DMREAD_WAR_200142015          = false
  DMTAG_WAR_1845883             = false
  ENGINE_ID                     = -1      # -1 means not defined
  IS_PKC_ENABLED                = false
  SIG_SIZE_IN_BITS              = 128     # default size for AES
  NUM_SIG_PER_UCODE             = 1
  SIGN_LICENSE                  = CODESIGN_LS
  LS_UCODE_ID                   = 0
  IS_LS_ENCRYPTED               = 0
  RELEASE_PATH                  = $(RESMAN_ROOT)/kernel/inc/dpu/bin
  BOOT_FROM_HS                  = false
  DEBUG_SIGN_ONLY               = false
  STEADY_STATE_BUILD            = true
  HDCP22_USE_HW_RSA             = false
  HDCP22_DEBUG_MODE             = false
  RUNTIME_HS_OVL_SIG_PATCHING   = false
  LS_PKC_PROD_ENC_ENABLED       = false
  LIB_CCC_PRESENT               = false
  HDCP22_KMEM_ENABLED           = false
  HDCP22_USE_HW_SHA             = false

  ifneq (,$(findstring dpu-v0201,  $(DPUCFG_PROFILE)))
      IPMAJORVER      = 02
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/kepler/gk104
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/kepler/gk104
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_01
      FALCON_ARCH     = falcon4
      RTOS_VERSION    = OpenRTOSv4.1.3
      ENABLE_HS_SIGN  = false
  endif

  ifneq (,$(findstring dpu-v0205,  $(DPUCFG_PROFILE)))
      IPMAJORVER             = 02
      IPMINORVER             = 05
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/maxwell/gm200
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_05
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_05
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/maxwell/gm200
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_05
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_05
      HDCPCFG_PROFILE        = hdcp-v0205
      FALCON_ARCH            = falcon5
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5800
      I2CCFG_PROFILE         = i2c-gm20x
      DPAUXCFG_PROFILE       = dpaux-v0205
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0205
      RTOS_VERSION           = OpenRTOSv4.1.3
      ENABLE_HS_SIGN         = false
  endif

  ifneq (,$(findstring dpu-v0207,  $(DPUCFG_PROFILE)))
      IPMAJORVER             = 02
      IPMINORVER             = 07
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp100
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_07
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/dpu/v02_07
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp100
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_07
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/dpu/v02_07
      HDCPCFG_PROFILE        = hdcp-v0207
      FALCON_ARCH            = falcon6
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5800
      SELWRITY_ENGINE        = true
      I2CCFG_PROFILE         = i2c-gp10x
      COMPILE_SWI2C          = true
      DPAUXCFG_PROFILE       = dpaux-v0207
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0207
      LS_UCODE_VERSION       = 1
      RTOS_VERSION           = OpenRTOSv4.1.3
      ENABLE_HS_SIGN         = false
  endif

  ifneq (,$(findstring dpu-gv100,  $(DPUCFG_PROFILE)))
      PROJ                   = gv100
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv100
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v03_00
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv100
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v03_00
      HDCPCFG_PROFILE        = hdcp-v0300
      FALCON_ARCH            = falcon6
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5800
      SELWRITY_ENGINE        = true
      I2CCFG_PROFILE         = i2c-gv10x
      DPAUXCFG_PROFILE       = dpaux-v0300
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0300
      LS_UCODE_VERSION       = 3
      GSPLITE                = true
      EMEM_SUPPORTED         = true
      DMEM_VA_SUPPORTED      = true
      SECFG_PROFILE          = se-gv10x
      SIGN_CHIP              = 0300
      SIG_GEN_DBG_CFG        = dispflcn-siggen-gv100-debug.cfg
      SIG_GEN_PROD_CFG       = dispflcn-siggen-gv100-prod.cfg
      SIGGEN_CHIPS           = "gp104"
      HS_OVERLAYS_ENABLED    = true
      OS_CALLBACKS           = true
      RTOS_VERSION           = OpenRTOSv4.1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET = true
      HDCP22_CHECK_STATE_INTEGRITY  = true
      GSPLITE_RTTIMER_WAR_ENABLED   = true
      HDCP22_USE_SCP_GEN_DKEY       = true
      SSP_ENABLED            = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN      = false
         NO_RELEASE          = true
      endif
      DMREAD_WAR_200142015   = true
  endif

  # GSP ucode is lwrrently not loaded on displayless chip since it only supports display feature now.
  # RM code will need to be adjusted if we are going to support any feature on displayless chip.
  ifneq (,$(findstring dpu-tu10x,  $(DPUCFG_PROFILE)))
      PROJ                   = tu10x
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      HDCPCFG_PROFILE        = hdcp-v0300
      FALCON_ARCH            = falcon6
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5800
      SELWRITY_ENGINE        = true
      I2CCFG_PROFILE         = i2c-gv10x
      DPAUXCFG_PROFILE       = dpaux-v0300
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0400
      LS_UCODE_VERSION       = 4
      GSPLITE                = true
      EMEM_SUPPORTED         = true
      DMEM_VA_SUPPORTED      = true
      SECFG_PROFILE          = se-tu10x
      SIGN_CHIP              = 0400
      SIG_GEN_DBG_CFG        = dispflcn-siggen-tu10x-debug.cfg
      SIG_GEN_PROD_CFG       = dispflcn-siggen-tu10x-prod.cfg
      SIGGEN_CHIPS           = "tu102_aes"
      HS_OVERLAYS_ENABLED    = true
      OS_CALLBACKS           = true
      RTOS_VERSION           = SafeRTOSv5.16.0-lw1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET = false
      HDCP22_CHECK_STATE_INTEGRITY  = false
      HDCP22_USE_SCP_GEN_DKEY       = true
      SSP_ENABLED            = true
      HS_UCODE_ENCRYPTION    = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN      = false
         NO_RELEASE          = true
      endif
      # WAR only required for Turing, fixed in Ampere.
      DMREAD_WAR_200142015   = true
      HDCP22_KMEM_ENABLED    = true
      HDCP22_USE_HW_SHA      = true
  endif

  # GSP ucode is lwrrently not loaded on displayless chip since it only supports display feature now.
  # RM code will need to be adjusted if we are going to support any feature on displayless chip.
  ifneq (,$(findstring dpu-tu116,  $(DPUCFG_PROFILE)))
      PROJ                   = tu116
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      HDCPCFG_PROFILE        = hdcp-v0300
      FALCON_ARCH            = falcon6
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5800
      SELWRITY_ENGINE        = true
      I2CCFG_PROFILE         = i2c-gv10x
      DPAUXCFG_PROFILE       = dpaux-v0300
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0400
      LS_UCODE_VERSION       = 1
      GSPLITE                = true
      EMEM_SUPPORTED         = true
      DMEM_VA_SUPPORTED      = true
      SECFG_PROFILE          = se-tu10x
      SIGN_CHIP              = tu116
      SIG_GEN_DBG_CFG        = dispflcn-siggen-tu116-debug.cfg
      SIG_GEN_PROD_CFG       = dispflcn-siggen-tu116-prod.cfg
      SIGGEN_CHIPS           = "tu116_aes"
      HS_OVERLAYS_ENABLED    = true
      OS_CALLBACKS           = true
      RTOS_VERSION           = SafeRTOSv5.16.0-lw1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET = false
      HDCP22_CHECK_STATE_INTEGRITY  = false
      HDCP22_USE_SCP_GEN_DKEY       = true
      SSP_ENABLED            = true
      HS_UCODE_ENCRYPTION    = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN      = false
         NO_RELEASE          = true
      endif
      # WAR only required for Turing, fixed in Ampere.
      DMREAD_WAR_200142015   = true
      HDCP22_KMEM_ENABLED    = true
      HDCP22_USE_HW_SHA      = true
  endif

  ifneq (,$(filter $(DPUCFG_PROFILE), dpu-ga10x))
      # We are using this profile for running on ga10x DFPGA.
      # HDCP2.2 sanity test is run on binaries generated by this profile
      # on VDVS on GA10x DFPGA
      PROJ                   = ga10x
      CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      HDCPCFG_PROFILE        = hdcp-v0300
      FALCON_ARCH            = falcon6
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5b00
      SELWRITY_ENGINE        = true
      I2CCFG_PROFILE         = i2c-gv10x
      DPAUXCFG_PROFILE       = dpaux-v0300
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0401
      LS_UCODE_VERSION       = 1
      GSPLITE                = true
      EMEM_SUPPORTED         = true
      DMEM_VA_SUPPORTED      = true
      SECFG_PROFILE          = se-tu10x
      SIGN_CHIP              = ga10x
      SIG_GEN_DBG_CFG        = dispflcn-siggen-ga10x-debug.cfg
      SIG_GEN_PROD_CFG       = dispflcn-siggen-ga10x-prod.cfg
      SIGGEN_CHIPS           = "ga102_rsa3k_0"
      HS_OVERLAYS_ENABLED    = true
      OS_CALLBACKS           = true
      RTOS_VERSION           = SafeRTOSv5.16.0-lw1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET   = false
      HDCP22_CHECK_STATE_INTEGRITY    = false
      HDCP22_USE_SCP_GEN_DKEY         = true
      HDCP22_LC_RETRY_WAR_ENABLED     = true
      HDCP22_WAR_3051763_ENABLED      = true
      HDCP22_WAR_ECF_SELWRE_ENABLED   = true
      SSP_ENABLED            = true
      HS_UCODE_ENCRYPTION    = true
      # Not Skipping signing and submission for this Non-BFHS GA10x profile
      # We are using this binary for HDCP Sanity on GA10x DFPGA on VDVS
      # This profile is not used for production.
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN      = true
         NO_RELEASE          = false
      endif
      ENGINE_ID              = $(ENGINE_ID_GSP)
      IS_PKC_ENABLED         = true
      SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
      # Temporary making Number of signature per HS overlay to 1 for DFPGA profile
      # Todo :- to make number of signature to 3 i.e same as bfhs profile after
      # implementing runtime patching of HS overlay for non bfhs profile as well
      NUM_SIG_PER_UCODE      = 1
      SIGN_LICENSE           = CODESIGN_LS_PKC
      LS_UCODE_ID            = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED        = 0
      # If switch to use SW RSA, needs to add libBigIntHs, SwRsaModularExpHs HAL support for GA10X.
      HDCP22_USE_HW_RSA      = true
      HDCP22_DEBUG_MODE      = true
      HDCP22_KMEM_ENABLED    = true
      HDCP22_USE_HW_SHA      = true
  endif

  ifneq (,$(filter $(DPUCFG_PROFILE), dpu-ga10x_boot_from_hs))
      PROJ                   = ga10x_boot_from_hs
      PROJ1                  = ga102
      CHIP_MANUAL_PATH       = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS          += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS          += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      HDCPCFG_PROFILE        = hdcp-v0300
      FALCON_ARCH            = falcon6
      LS_FALCON              = true
      LDR_STACK_OFFS         = 0x5b00
      SELWRITY_ENGINE        = true
      I2CCFG_PROFILE         = i2c-gv10x
      DPAUXCFG_PROFILE       = dpaux-v0300
      HDCP22WIREDCFG_PROFILE = hdcp22wired-v0401
      LS_UCODE_VERSION       = 1
      GSPLITE                = true
      EMEM_SUPPORTED         = true
      DMEM_VA_SUPPORTED      = true
      SECFG_PROFILE          = se-tu10x
      SIGN_CHIP              = ga10x_boot_from_hs
      SIG_GEN_DBG_CFG        = dispflcn-siggen-ga10x-boot-from-hs-debug.cfg
      SIG_GEN_PROD_CFG       = dispflcn-siggen-ga10x-boot-from-hs-prod.cfg
      SIGGEN_CHIPS           = "ga102_rsa3k_0"
      HS_OVERLAYS_ENABLED    = true
      OS_CALLBACKS           = true
      RTOS_VERSION           = SafeRTOSv5.16.0-lw1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET = false
      HDCP22_CHECK_STATE_INTEGRITY  = false
      HDCP22_USE_SCP_GEN_DKEY       = true
      HDCP22_LC_RETRY_WAR_ENABLED   = true
      HDCP22_WAR_3051763_ENABLED    = true
      HDCP22_WAR_ECF_SELWRE_ENABLED = true
      SSP_ENABLED            = true
      HS_UCODE_ENCRYPTION    = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN      = false
         NO_RELEASE          = true
      endif
      ENGINE_ID              = $(ENGINE_ID_GSP)
      IS_PKC_ENABLED         = true
      SIG_SIZE_IN_BITS       = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE      = 3
      SIGN_LICENSE           = CODESIGN_LS_PKC
      LS_UCODE_ID            = 1
      LS_PKC_PROD_ENC_ENABLED = true
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED        = 2
      BOOT_FROM_HS           = true
      # If switch to use SW RSA, needs to add libBigIntHs, SwRsaModularExpHs HAL support for GA10X.
      HDCP22_USE_HW_RSA      = true
      RUNTIME_HS_OVL_SIG_PATCHING  = true
      HDCP22_KMEM_ENABLED    = true
      HDCP22_USE_HW_SHA      = true
  endif

  ifneq (,$(filter $(DPUCFG_PROFILE), dpu-ad10x))
      # We are using this profile for running on ad10x DFPGA.
      # HDCP2.2 sanity test is run on binaries generated by this profile
      # on VDVS on AD10x DFPGA
      PROJ                          = ad10x
      CHIP_MANUAL_PATH              = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS                 += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS                 += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                 += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_04
      MANUAL_PATHS                 += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_04
      HDCPCFG_PROFILE               = hdcp-v0300
      FALCON_ARCH                   = falcon6
      LS_FALCON                     = true
      LDR_STACK_OFFS                = 0x6400
      SELWRITY_ENGINE               = true
      I2CCFG_PROFILE                = i2c-gv10x
      DPAUXCFG_PROFILE              = dpaux-v0300
      HDCP22WIREDCFG_PROFILE        = hdcp22wired-v0401
      LS_UCODE_VERSION              = 1
      GSPLITE                       = true
      EMEM_SUPPORTED                = true
      DMEM_VA_SUPPORTED             = true
      SECFG_PROFILE                 = se-ad10x
      SIGN_CHIP                     = ad10x
      SIG_GEN_DBG_CFG               = dispflcn-siggen-ad10x-debug.cfg
      SIG_GEN_PROD_CFG              = dispflcn-siggen-ad10x-prod.cfg
      SIGGEN_CHIPS                  = "ga102_rsa3k_0"
      HS_OVERLAYS_ENABLED           = true
      OS_CALLBACKS                  = true
      RTOS_VERSION                  = SafeRTOSv5.16.0-lw1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET = false
      HDCP22_CHECK_STATE_INTEGRITY  = false
      HDCP22_USE_SCP_GEN_DKEY       = true
      HDCP22_LC_RETRY_WAR_ENABLED   = true
      HDCP22_WAR_3051763_ENABLED    = true
      HDCP22_WAR_ECF_SELWRE_ENABLED = true
      SSP_ENABLED                   = true
      HS_UCODE_ENCRYPTION           = true
      # Not Skipping signing and submission for this Non-BFHS GA10x profile
      # We are using this binary for HDCP Sanity on GA10x DFPGA on VDVS
      # This profile is not used for production.
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN             = true
         NO_RELEASE                 = false
      endif
      ENGINE_ID                     = $(ENGINE_ID_GSP)
      IS_PKC_ENABLED                = true
      SIG_SIZE_IN_BITS              = $(RSA3K_SIZE_IN_BITS)
      # Temporary making Number of signature per HS overlay to 1 for DFPGA profile
      # Todo :- to make number of signature to 3 i.e same as bfhs profile after
      # implementing runtime patching of HS overlay for non bfhs profile as well
      NUM_SIG_PER_UCODE             = 1
      SIGN_LICENSE                  = CODESIGN_LS_PKC
      LS_UCODE_ID                   = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED               = 0
      # If switch to use SW RSA, needs to add libBigIntHs, SwRsaModularExpHs HAL support for AD10X.
      HDCP22_USE_HW_RSA             = true
      HDCP22_DEBUG_MODE             = true
      LIB_CCC_PRESENT               = true
      HDCP22_KMEM_ENABLED           = true
      HDCP22_USE_HW_SHA             = true
  endif

  ifneq (,$(filter $(DPUCFG_PROFILE), dpu-ad10x_boot_from_hs))
      PROJ                          = ad10x_boot_from_hs
      PROJ1                         = ad102
      CHIP_MANUAL_PATH              = $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS                 += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS                 += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                 += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_04
      MANUAL_PATHS                 += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_04
      HDCPCFG_PROFILE               = hdcp-v0300
      FALCON_ARCH                   = falcon6
      LS_FALCON                     = true
      LDR_STACK_OFFS                = 0x6400
      SELWRITY_ENGINE               = true
      I2CCFG_PROFILE                = i2c-gv10x
      DPAUXCFG_PROFILE              = dpaux-v0300
      HDCP22WIREDCFG_PROFILE        = hdcp22wired-v0401
      LS_UCODE_VERSION              = 1
      GSPLITE                       = true
      EMEM_SUPPORTED                = true
      DMEM_VA_SUPPORTED             = true
      SECFG_PROFILE                 = se-ad10x
      SIGN_CHIP                     = ad10x_boot_from_hs
      SIG_GEN_DBG_CFG               = dispflcn-siggen-ad10x-boot-from-hs-debug.cfg
      SIG_GEN_PROD_CFG              = dispflcn-siggen-ad10x-boot-from-hs-prod.cfg
      SIGGEN_CHIPS                  = "ga102_rsa3k_0" # TODO: Update to ad10x
      HS_OVERLAYS_ENABLED           = true
      OS_CALLBACKS                  = true
      RTOS_VERSION                  = SafeRTOSv5.16.0-lw1.3
      HDCP22_USE_SCP_ENCRYPT_SECRET = false
      HDCP22_CHECK_STATE_INTEGRITY  = false
      HDCP22_USE_SCP_GEN_DKEY       = true
      HDCP22_LC_RETRY_WAR_ENABLED   = true
      HDCP22_WAR_3051763_ENABLED    = true
      HDCP22_WAR_ECF_SELWRE_ENABLED = true
      SSP_ENABLED                   = true
      HS_UCODE_ENCRYPTION           = true
      # Skip signing and submission of prod-signed binary for AS2 build
      ifeq ("$(AS2_DPU_SKIP_SIGN_RELEASE)","true")
         ENABLE_HS_SIGN             = true
         NO_RELEASE                 = false
      endif
      ENGINE_ID                     = $(ENGINE_ID_GSP)
      IS_PKC_ENABLED                = true
      SIG_SIZE_IN_BITS              = $(RSA3K_SIZE_IN_BITS)
      NUM_SIG_PER_UCODE             = 1
      SIGN_LICENSE                  = CODESIGN_LS_PKC
      LS_UCODE_ID                   = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED               = 2
      BOOT_FROM_HS                  = true
      # If switch to use SW RSA, needs to add libBigIntHs, SwRsaModularExpHs HAL support for AD10X.
      HDCP22_USE_HW_RSA             = true
      RUNTIME_HS_OVL_SIG_PATCHING   = true
      LIB_CCC_PRESENT               = true
      HDCP22_KMEM_ENABLED           = true
      HDCP22_USE_HW_SHA             = true
  endif

endif

