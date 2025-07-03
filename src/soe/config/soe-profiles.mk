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
SOECFG_PROFILES_ALL += soe-lr10
SOECFG_PROFILES_ALL += soe-ls10

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SOECFG_PROFILE
  MANUAL_PATHS             =
  DMA_SUSPENSION           = false
  LS_FALCON                = true
  PA_47_BIT_SUPPORTED      = true
  DMA_NACK_SUPPORTED       = true
  SELWRITY_ENGINE          = true
  EMEM_SUPPORTED           = true
  MRU_OVERLAYS             = true
  DMEM_VA_SUPPORTED        = false
  FREEABLE_HEAP            = false
  DMREAD_WAR_200142015     = false
  DMTAG_WAR_1845883        = false
  IMEM_ON_DEMAND_PAGING    = false
  TASK_RESTART             = true
  LS_UCODE_VERSION         = 0
  LS_DEPENDENCY_MAP        =
  RELEASE_PATH             = $(LW_SOURCE)/drivers/lwswitch/kernel/inc/soe/bin
  RELEASE_PATH_HEADER      = $(LW_SOURCE)/drivers/lwswitch/kernel/inc/$(subst soe-,,$(SOECFG_PROFILE))
  HS_OVERLAYS_ENABLED      = false
  LWOS_VERSION             = dev
  EXCLUDE_LWOSDEBUG        = false

  ifneq (,$(filter $(SOECFG_PROFILE), soe-lr10))
    PROJ                 = lr10
    ARCH                 = limerock
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/lr10
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/lr10
    FALCON_ARCH          = falcon6
    DEBUG_SIGN_ONLY      = false
    RTOS_VERSION         = SafeRTOSv5.16.0-lw1.3
    LS_UCODE_VERSION     = 2
  endif

  ifneq (,$(filter $(SOECFG_PROFILE), soe-ls10))
    PROJ                 = ls10
    ARCH                 = lagunaseca
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
    MANUAL_PATHS         += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10
    FALCON_ARCH          = falcon6
    DEBUG_SIGN_ONLY      = false
    RTOS_VERSION         = SafeRTOSv5.16.0-lw1.3
    LS_UCODE_VERSION     = 2
# TODO : Remove local signing once LS Signing is enabled for soe-ls10.
# Tracked in bug 200634104
    SIGN_LOCAL           = 1
    SIGN_SERVER          = 0
  endif

endif
