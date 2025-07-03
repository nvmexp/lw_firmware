#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available HKDCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in HKDCFG_PROFILE. To pick-up the definitions, set HKDCFG_PROFILE to the
# desired profile and source/include this file. When HKDCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# HKDCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible HKD-config profiles
# lww - LwWatch binaries
##############################################################################

SELWRESCRUBCFG_PROFILES_ALL :=
#SELWRESCRUBCFG_PROFILES_ALL += selwrescrub-lwdec_gp10x
#SELWRESCRUBCFG_PROFILES_ALL += selwrescrub-lwdec_gv10x
SELWRESCRUBCFG_PROFILES_ALL += selwrescrub-lwdec_tu10x
#SELWRESCRUBCFG_PROFILES_ALL += selwrescrub-lwdec_tu116
SELWRESCRUBCFG_PROFILES_ALL += selwrescrub-lwdec_ga10x

SELWRESCRUBCFG_PROFILES_ALL += selwrescrub-lwdec_ga10x_boot_from_hs

SELWRESCRUBSW               = $(LW_SOURCE)/uproc/selwrescrub/

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SELWRESCRUBCFG_PROFILE
  MANUAL_PATHS         +=  $(LW_SOURCE)/uproc/selwrescrub/inc
  FALCON_ARCH           = falcon6
  LDSCRIPT              = link_script.ld
  IS_ENCRYPTED          = false

  SELWRESCRUB_FALCON_PMU     = false
  SELWRESCRUB_FALCON_LWDEC   = false
  SELWRESCRUB_FALCON_SEC2    = false
  SELWRESCRUB_FALCON_APERT   = false
  SIGGEN_FUSE_VER            = -1        # -1 means not defined
  SIGGEN_ENGINE_ID           = -1
  SIGGEN_UCODE_ID            = -1 
  SIGNATURE_SIZE             = 128       # Default AES signature size
  IS_ENHANCED_AES            = false
  PKC_ENABLED                = false
  NUM_SIG_PER_UCODE          = 1
  SSP_ENABLED                = false
  RTOS_VERSION               = OpenRTOSv4.1.3
  LWOS_VERSION               = dev
  BOOT_FROM_HS_BUILD         = false
  BOOT_FROM_HS_FALCON_LWDEC  = false


  ifneq (,$(findstring selwrescrub-lwdec_gp10x,  $(SELWRESCRUBCFG_PROFILE)))
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_08
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_08
      SELWRESCRUB_FALCON_LWDEC      = true
      SIGGEN_CFG                    = selwrescrub_siggen_gp10x.cfg
      SIGGEN_CHIPS                  += gp104 
      RELEASE_PATH                  = $(LW_SOURCE)/drivers/resman/kernel/inc/selwrescrub/bin/gp10x
      SELWRESCRUB_GP10X_UCODE_BUILD_VERSION  = 5
  endif
  ifneq (,$(findstring selwrescrub-lwdec_gv10x,  $(SELWRESCRUBCFG_PROFILE)))
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv100
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v03_00
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv100
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/disp/v03_00
      SELWRESCRUB_FALCON_LWDEC      = true
      SIGGEN_CFG                    = selwrescrub_siggen_gv10x.cfg
      SIGGEN_CHIPS                  += gp104
      RELEASE_PATH                  = $(LW_SOURCE)/drivers/resman/kernel/inc/selwrescrub/bin/gv10x
      SELWRESCRUB_GV10X_UCODE_BUILD_VERSION  = 2
  endif
  ifneq (,$(findstring selwrescrub-lwdec_tu10x,  $(SELWRESCRUBCFG_PROFILE)))
      CHIP_MANUAL_PATH  += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      SELWRESCRUB_FALCON_LWDEC              = true
      SIGGEN_CFG                            = selwrescrub_siggen_tu10x.cfg
      SIGGEN_CHIPS                         += tu102_aes
      RELEASE_PATH                          = $(LW_SOURCE)/drivers/resman/kernel/inc/selwrescrub/bin/tu10x
      SELWRESCRUB_TU102_UCODE_BUILD_VERSION = 5
      SELWRESCRUB_TU104_UCODE_BUILD_VERSION = 5
      SELWRESCRUB_TU106_UCODE_BUILD_VERSION = 5
      SIGGEN_FUSE_VER                       = $(LWDEC_SELWRESCRUB_ENHANCED_AES_VER)
      SIGGEN_ENGINE_ID                      = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID                       = $(LWDEC_SELWRESCRUB_ENHANCED_AES_ID)
      IS_ENHANCED_AES                       = true
      IS_ENCRYPTED                          = true
  endif
  ifneq (,$(findstring selwrescrub-lwdec_tu116,  $(SELWRESCRUBCFG_PROFILE)))
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS                      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      SELWRESCRUB_FALCON_LWDEC           = true
      SIGGEN_CFG                         = selwrescrub_siggen_tu116.cfg
      SIGGEN_CHIPS                      += tu116_aes
      RELEASE_PATH                       = $(LW_SOURCE)/drivers/resman/kernel/inc/selwrescrub/bin/tu116
      SELWRESCRUB_TU116_UCODE_BUILD_VERSION = 2
      SELWRESCRUB_TU117_UCODE_BUILD_VERSION = 2
      IS_ENCRYPTED                       = true
  endif
  ifneq (,$(findstring selwrescrub-lwdec_ga10x,  $(SELWRESCRUBCFG_PROFILE)))
      CHIP_MANUAL_PATH                      = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS                         += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      SELWRESCRUB_FALCON_LWDEC              = true
      SIGGEN_CFG                            = selwrescrub_siggen_ga10x.cfg
      SIGGEN_CHIPS                          = ga102_rsa3k_0
      RELEASE_PATH                          = $(LW_SOURCE)/drivers/resman/kernel/inc/selwrescrub/bin/ga10x
      SELWRESCRUB_GA102_UCODE_BUILD_VERSION = 1
      SIGGEN_FUSE_VER                       = $(LWDEC_SELWRESCRUB_PKC_VER)
      SIGGEN_ENGINE_ID                      = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID                       = $(LWDEC_SELWRESCRUB_PKC_ID)
      SIGNATURE_SIZE                        = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                           = true
      IS_ENCRYPTED                          = true
      NUM_SIG_PER_UCODE                     = 2
      SSP_ENABLED                           = true
  endif
  ifneq (,$(findstring selwrescrub-lwdec_ga10x_boot_from_hs,  $(SELWRESCRUBCFG_PROFILE)))
      CHIP_MANUAL_PATH                      = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS                         += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS                         += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      SELWRESCRUB_FALCON_LWDEC              = true
      SIGGEN_CFG                            = selwrescrub_siggen_ga10x.cfg
      SIGGEN_CHIPS                          = ga102_rsa3k_0
      RELEASE_PATH                          = $(LW_SOURCE)/drivers/resman/kernel/inc/selwrescrub/bin/ga10x
      SELWRESCRUB_GA102_UCODE_BUILD_VERSION = 1
      SIGGEN_FUSE_VER                       = $(LWDEC_SELWRESCRUB_PKC_VER)
      SIGGEN_ENGINE_ID                      = $(ENGINE_ID_LWDEC)
      SIGGEN_UCODE_ID                       = $(LWDEC_SELWRESCRUB_PKC_ID)
      SIGNATURE_SIZE                        = $(RSA3K_SIZE_IN_BITS)
      PKC_ENABLED                           = true
      IS_ENCRYPTED                          = true
      NUM_SIG_PER_UCODE                     = 3
      SSP_ENABLED                           = true
      BOOT_FROM_HS_BUILD                    = true
      BOOT_FROM_HS_FALCON_LWDEC             = true
      LDSCRIPT                              = link_script_boot_from_hs.ld
  endif

endif

