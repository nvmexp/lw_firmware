#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to Define all profile-specific make-vars
# (such as the project name, linker-script, falcon architecture, etc ...).
#

##############################################################################
#  Build a list of all possible fub-config profiles
##############################################################################

FUBCFG_PROFILES_ALL :=
FUBCFG_PROFILES_ALL += fub-sec2_tu10x_pkc

MANUAL_PATHS       =
LDSCRIPT           = link_script.ld
LWOS_VERSION       = dev
RTOS_VERSION       = OpenRTOSv4.1.3
SIGGEN_FUSE_VER    = -1        # -1 means not defined
SIGGEN_ENGINE_ID   = -1
SIGGEN_UCODE_ID    = -1 
SIGNATURE_SIZE     = 128       # Default - AES Signature Size
FUB_TU104_UCODE_BUILD_VERSION = 0
FUB_TU104_FPF_UCODE_BUILD_VERSION = 0
SSP_ENABLED        = true
IS_HS_ENCRYPTED    = true
PKC_ENABLED        = true
FUBSW              = $(LW_SOURCE)/uproc/spark/ucode/fub/
# ADA SCENARIO VARIABLES
ARCHITECTURE       = Tu10x
FALCON             = Sec2
BOARD              = DGPU
BUILD_UCODE        = fub

##############################################################################
# Profile-specific settings
##############################################################################

ifdef FUBCFG_PROFILE
  MANUAL_PATHS                  += $(LW_SOURCE)/drivers/resman/interface
  MANUAL_PATHS                  += $(LW_SOURCE)/drivers/resman/arch/lwalloc/common/inc/
  FALCON_ARCH                    = falcon6
  LDSCRIPT                       = link_script.ld
  IS_HS_ENCRYPTED                = true
  SSP_ENABLED                    = true
  SIGGEN_FUSE_VER                = -1        # -1 means not defined
  SIGGEN_ENGINE_ID               = -1
  SIGGEN_UCODE_ID                = -1 
  SIGNATURE_SIZE                 = 128       # Default AES signature size
  IS_ENHANCED_AES                = false
  FUB_ON_LWDEC                   = false
  FUB_ON_SEC2                    = false
  AUTO_FUB                       = false
  PKC_ENABLED                    = false
  FUB_TU102_UCODE_BUILD_VERSION  = 0
  FUB_TU104_UCODE_BUILD_VERSION  = 0 
  FUB_TU106_UCODE_BUILD_VERSION  = 0
  FUB_TU116_UCODE_BUILD_VERSION  = 0
  FUB_TU117_UCODE_BUILD_VERSION  = 0
  FUB_TU10A_UCODE_BUILD_VERSION  = 0

  ifneq (,$(findstring fub-sec2_tu10x_pkc,  $(FUBCFG_PROFILE)))
      CHIP_MANUAL_PATH              += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS                  += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      SIGGEN_CFG                     = fub_siggen_tu10x.cfg
      SIGGEN_CHIPS                  += tu102_aes
      RELEASE_PATH                   = $(RESMAN_ROOT)/kernel/inc/spark/bin/fub/sec2/tu10x/
      FUB_TU102_UCODE_BUILD_VERSION  = 3
      FUB_TU104_UCODE_BUILD_VERSION  = 3
      FUB_TU106_UCODE_BUILD_VERSION  = 3
      SIGGEN_FUSE_VER                = $(SEC2_FUB_VER)
      SIGGEN_ENGINE_ID               = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID                = $(SEC2_FUB_ID)
      IS_ENHANCED_AES                = true
      FUB_ON_SEC2                    = true
      PKC_ENABLED                    = true
      SIGNATURE_SIZE                 = $(RSA3K_SIZE_IN_BITS)
  endif
 
endif
