#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available SEPKERNCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SEPKERNCFG_PROFILE. To pick-up the definitions, set SEPKERNCFG_PROFILE to the
# desired profile and source/include this file. When SEPKERNCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SEPKERNCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible sepkern-config profiles
##############################################################################

SEPKERNCFG_PROFILES_ALL :=
SEPKERNCFG_PROFILES_ALL += gsp_sepkern-ga10x
SEPKERNCFG_PROFILES_ALL += gsp_sepkern-t23xd
SEPKERNCFG_PROFILES_ALL += gsp_sepkern-gh100
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-ga10x
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-ad10x
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-ad10b
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-gh100
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-gh20x
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-gb100
SEPKERNCFG_PROFILES_ALL += pmu_sepkern-g00x
SEPKERNCFG_PROFILES_ALL += sec_sepkern-gh100

SIGGEN_CSG_PROD_SIGNING = false
SIGGEN_PROD_SIGNING_OPTIONS = -mode 0

##############################################################################
# Profile-specific settings
##############################################################################
ifdef SEPKERNCFG_PROFILE
  MANUAL_PATHS         =
  BOOT_PROFILING      ?= false
  DEBUG_BUILD         ?= true
  USE_CSB              = true
  USE_CBB              = false
  RUN_ON_SEC           = false
  IMEM_LIMIT           = 0x800
  DMEM_LIMIT           = 0x400
  USE_GSCID            = false
  SK_MF_INST_IMG       = false

  # Common chip settings
  ifneq (,$(findstring -ga10x, $(SEPKERNCFG_PROFILE)))
    PROJ               = ga10x
    ARCH               = ampere
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    SIGGEN_CHIPS       = "ga102_rsa3k_0"
  endif

  ifneq (,$(findstring -t23xd, $(SEPKERNCFG_PROFILE)))
    PROJ               = t23xd
    ARCH               = orin
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    SIGGEN_CHIPS       = "ga10b_rsa3k_0"
  endif

  ifneq (,$(findstring -ad10x, $(SEPKERNCFG_PROFILE)))
    PROJ               = ad10x
    ARCH               = ada
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    SIGGEN_CHIPS       = "ga102_rsa3k_0"
  endif

  ifneq (,$(findstring -ad10b, $(SEPKERNCFG_PROFILE)))
    PROJ               = ad10b
    ARCH               = ada
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad10b
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad10b
    SIGGEN_CHIPS       = "ga10b_rsa3k_0"
  endif

  ifneq (,$(findstring -gh100, $(SEPKERNCFG_PROFILE)))
    PROJ               = gh100
    ARCH               = hopper
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    SIGGEN_CHIPS       = "gh100_rsa3k_0"
  endif

  ifneq (,$(findstring -gh20x, $(SEPKERNCFG_PROFILE)))
    PROJ               = gh20x
    ARCH               = hopper
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    SIGGEN_CHIPS       = "gh100_rsa3k_0"
  endif

  ifneq (,$(findstring -gb100, $(SEPKERNCFG_PROFILE)))
    PROJ               = gh100
    ARCH               = hopper
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/blackwell/gb100
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/blackwell/gb100
    SIGGEN_CHIPS       = "gh100_rsa3k_0"
  endif

  ifneq (,$(findstring -g00x, $(SEPKERNCFG_PROFILE)))
    PROJ               = g00x
    ARCH               = g00x
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
    MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    SIGGEN_CHIPS       = "gh100_rsa3k_0"
  endif

  # Common engine settings
  ifneq (,$(findstring gsp_, $(SEPKERNCFG_PROFILE)))
    ENGINE             = gsp
    GSP_RTOS           = 1
  endif
  ifneq (,$(findstring pmu_, $(SEPKERNCFG_PROFILE)))
    ENGINE             = pmu
    PMU_RTOS           = 1
  endif
  ifneq (,$(findstring sec_, $(SEPKERNCFG_PROFILE)))
    ENGINE             = sec
    SEC2_RTOS          = 1
  endif

  # Per profile settings
  ifneq (,$(findstring gsp_sepkern-t23xd, $(SEPKERNCFG_PROFILE)))
    USE_CBB            = true # todo need to be use DIO actually for tsec
    RUN_ON_SEC         = true
    USE_GSCID          = true
  endif

  ifneq (,$(findstring pmu_sepkern-gh100, $(SEPKERNCFG_PROFILE)))
    SK_MF_INST_IMG = true
  endif

  ifneq (,$(findstring pmu_sepkern-gh20x, $(SEPKERNCFG_PROFILE)))
    SK_MF_INST_IMG = true
  endif

  ifneq (,$(findstring pmu_sepkern-g00x, $(SEPKERNCFG_PROFILE)))
    SK_MF_INST_IMG = true
  endif

endif
