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
# Build a list of all possible acr_pmu-config profiles
##############################################################################

ACRCFG_PROFILES_ALL :=

# GH100 ACR RISCV profiles
ACRCFG_PROFILES_ALL += acr_rv_gsp-gh100
ACRCFG_PROFILES_ALL += acr_rv_gsp-gh100_fmodel

ACRSW               = $(LW_SOURCE)/uproc/acr_riscv/

##############################################################################
# Profile-specific settings
##############################################################################

ifdef ACRCFG_PROFILE
  MANUAL_PATHS             += $(LW_SOURCE)/uproc/acr_riscv/inc
  SEC2_PRESENT             = true
  GSP_PRESENT              = true
  FBFALCON_PRESENT         = true
  LWENC_PRESENT            = true
  LWDEC_PRESENT            = false
  LWDEC1_PRESENT           = false
  LWDEC_RISCV_PRESENT      = true
  LWDEC_RISCV_EB_PRESENT   = true
  OFA_PRESENT              = false
  LWJPG_PRESENT            = false
  ACR_SIG_VERIF            = true
  ACR_SKIP_SCRUB           = false
  LDSCRIPT                 = link_script_rv.ld
  ACR_FMODEL_BUILD         = false
  ACR_GSPRM_BUILD          = false
  ACR_RISCV_LS             = false
  FB_FRTS                  = true
  SSP_ENABLED              = false
  PRI_SOURCE_ISOLATION_PLM = false
  BOOT_FROM_HS_BUILD       = false
  EMEM_SIZE                = 0x02000
  IMEM_SIZE                = 0x10000
  DMEM_SIZE                = 0x10000
  STACK_SIZE               = 0x300
  DMEM_BASE                = 0x180000
  UPROC_ARCH               = lwriscv
  PARTITION_BOOT_ENABLED   = 1
  USE_CSB                  = false
  USE_CBB                  = false
  FPU_SUPPORTED            = true
  WAR_BUG_200670718        = false
  ACR_ENGINE               = GSP_RISCV
  ALLOW_FSP_TO_RESET_GSP   = true
  IS_LASSAHS_REQUIRED      = false
  TOOLCHAIN_VERSION        = "8.2.0"
  LS_UCODE_MEASUREMENT     = false
  ACR_CMAC_KDF_ENABLED     = false

  ACR_GH100_UCODE_BUILD_VERSION = 0

  ifeq ("$(ACRCFG_PROFILE)","acr_rv_gsp-gh100_fmodel")
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh100/riscv
      ACR_RISCV_LS             = true
      LWDEC_PRESENT            = true
      ACR_SIG_VERIF            = true
      FB_FRTS                  = false
      ACR_SIG_VERIF            = true
      PKC_LS_RSA3K_KEYS_GH100  = true
      LWENC_PRESENT            = false
      OFA_PRESENT              = true
      LWJPG_PRESENT            = true
      PRI_SOURCE_ISOLATION_PLM = true
      SSP_ENABLED              = true
      DMEM_END_CARVEOUT_SIZE   = 0x400
      ACR_FMODEL_BUILD         = true
      ACR_GSPRM_BUILD          = false
      WAR_BUG_200670718        = true

      # TODO: Need to give GSP reset control to FSP based on COT .
      ALLOW_FSP_TO_RESET_GSP   = false
      TOOLCHAIN_VERSION        = "8.2.0"
  endif

  ifeq ("$(ACRCFG_PROFILE)","acr_rv_gsp-gh100")
      CHIP_MANUAL_PATH               = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS                  += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH                   = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh100/riscv
      ACR_RISCV_LS                   = true
      FB_FRTS                        = false
      PKC_LS_RSA3K_KEYS_GH100        = true
      LWENC_PRESENT                  = false
      OFA_PRESENT                    = true
      LWJPG_PRESENT                  = true
      PRI_SOURCE_ISOLATION_PLM       = true
      SSP_ENABLED                    = true
      DMEM_END_CARVEOUT_SIZE         = 0x400
      ACR_FMODEL_BUILD               = false
      ACR_GSPRM_BUILD                = false
      WAR_BUG_200670718              = true
      ACR_GH100_UCODE_BUILD_VERSION  = 1 #ES version

      # TODO: Need to give GSP reset control to FSP based on COT .
      ALLOW_FSP_TO_RESET_GSP         = false
      TOOLCHAIN_VERSION              = "8.2.0"
  endif

  ifeq ("$(ACRCFG_PROFILE)","acr_rv_gsp-gh100_gsprm")
      CHIP_MANUAL_PATH         = $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
      MANUAL_PATHS            += $(CHIP_MANUAL_PATH)
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS            += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/acr/bin/gsp/gh100/riscv
      ACR_RISCV_LS             = true
      FB_FRTS                  = false
      PKC_LS_RSA3K_KEYS_GH100  = true
      LWENC_PRESENT            = false
      OFA_PRESENT              = true
      LWJPG_PRESENT            = true
      PRI_SOURCE_ISOLATION_PLM = true
      SSP_ENABLED              = true
      DMEM_END_CARVEOUT_SIZE   = 0x400
      ACR_FMODEL_BUILD         = false
      ACR_GSPRM_BUILD          = true
      WAR_BUG_200670718        = true

      # TODO: Need to give GSP reset control to FSP based on COT .
      ALLOW_FSP_TO_RESET_GSP   = false
      TOOLCHAIN_VERSION        = "8.2.0"
  endif
endif

