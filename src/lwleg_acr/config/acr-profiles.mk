#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
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
ACRCFG_PROFILES_ALL += acr_pmu-t210_load
ACRCFG_PROFILES_ALL += acr_pmu-t186_load
ACRCFG_PROFILES_ALL += acr_pmu-t194_load
ACRCFG_PROFILES_ALL += acr_pmu-t194_load_safety
ACRCFG_PROFILES_ALL += acr_pmu-t234_load

ACRSW               = $(LW_SOURCE)/uproc/tegra_acr/

##############################################################################
# Profile-specific settings
##############################################################################

ifdef ACRCFG_PROFILE
  MANUAL_PATHS     += $(LW_SOURCE)/uproc/tegra_acr/inc
  SEC2_PRESENT                     = true
  LWENC_PRESENT                    = true
  LWDEC_PRESENT                    = true
  DPU_PRESENT                      = false
  ACR_ONLY_BO                      = true
  ACR_UNLOAD                       = false
  ACR_LOAD                         = false
  SSP_ENABLED                      = true
  CAPTURE_VCAST                    = false
  ACR_BSI_LOCK                     = false
  ACR_BSI_BOOT                     = false
  ACR_SIG_VERIF                    = true
  ACR_SAFE_BUILD                   = false
  ACR_SKIP_SCRUB                   = false
  ACR_BUILD_ONLY                   = true
  LDSCRIPT                         = link_script.ld
  ACR_FALCON_PMU                   = false
  ACR_FALCON_SEC2                  = false
  ACR_FIXED_TRNG                   = false
  ACR_FMODEL_BUILD                 = false
  ACR_MMU_INDEXED                  = true
  ACR_PRI_SOURCE_ISOLATION_ENABLED = false
  ACR_SUB_WPR                      = false
  IS_HS_ENCRYPTED                  = false
  LWOS_VERSION                     = dev
  RTOS_VERSION                     = OpenRTOSv4.1.3

  ifneq (,$(findstring _pmu, $(ACRCFG_PROFILE)))
      ACR_FALCON_PMU = true
  endif

  ifneq (,$(findstring _sec2, $(ACRCFG_PROFILE)))
      ACR_FALCON_SEC2 = true
      ACR_ONLY_BO     = false
  endif

  ifneq (,$(findstring acr_pmu-t210_load,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/maxwell/gm20b/
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/maxwell/gm20b/
      SEC2_PRESENT       = false
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/t210
      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t210
      FALCON_ARCH        = falcon5
      ACR_LOAD           = true
      ACR_ONLY_BO        = false
      ACR_SIG_VERIF      = true
      SIGGEN_CFG         = acr_siggen_t210.cfg
      SIGGEN_CHIPS      += "gm204"
  endif

  ifneq (,$(findstring acr_pmu-t186_load,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp10b/
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp10b/
      SEC2_PRESENT       = false
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/t186
      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t186
      FALCON_ARCH        = falcon6
      ACR_LOAD           = true
      ACR_ONLY_BO        = false
      ACR_SIG_VERIF      = true
      SIGGEN_CFG         = acr_siggen_t186.cfg
      SIGGEN_CHIPS      += "gm206"
  endif

  ifneq (,$(findstring acr_pmu-t194_load,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/misra/volta/gv11b/
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/misra/volta/gv11b/
      SEC2_PRESENT       = false
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/t194
      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t194
      FALCON_ARCH        = falcon6
      ACR_LOAD           = true
      ACR_ONLY_BO        = false
      ACR_SIG_VERIF      = true
      IS_HS_ENCRYPTED    = true
      SIGGEN_CFG         = acr_siggen_t194.cfg
      SIGGEN_CHIPS      += "t194"
  endif

  ifneq (,$(findstring acr_pmu-t194_load_safety,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/misra/volta/gv11b/
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/misra/volta/gv11b/
      SEC2_PRESENT       = false
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/t194
      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t194
      FALCON_ARCH        = falcon6
      ACR_LOAD           = true
      ACR_ONLY_BO        = false
      ACR_SIG_VERIF      = true
      ACR_SAFE_BUILD     = true
      IS_HS_ENCRYPTED    = true
      SIGGEN_CFG         = acr_siggen_t194.cfg
      SIGGEN_CHIPS      += "t194"
  endif

#  ifneq (,$(findstring acr_pmu-gm20b_load,  $(ACRCFG_PROFILE)))
#      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/maxwell/gm20b
#      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/maxwell/gm20b
#      SEC2_PRESENT       = false
#      LWENC_PRESENT      = false
#      LWDEC_PRESENT      = false
#      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/gm20x
#      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/gm20b
#      FALCON_ARCH        = falcon5
#      ACR_LOAD           = true
#      ACR_ONLY_BO        = false
#      ACR_SIG_VERIF      = false
#      SIGGEN_CFG         = acr_siggen_gm20b.cfg
#      SIGGEN_CHIPS      += "gm204"
#  endif

  ifneq (,$(findstring acr_pmu-gp10b_load,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp10b
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp10b
      SEC2_PRESENT       = false
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/gp10x
      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/gp10b
      FALCON_ARCH        = falcon6
      ACR_LOAD           = true
      ACR_ONLY_BO        = false
      ACR_SIG_VERIF      = false
      SIGGEN_CFG         = acr_siggen_gp10b.cfg
      SIGGEN_CHIPS      += "gm206"
  endif

  ifneq (,$(findstring acr_pmu-gv11b_load,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/misra/volta/gv11b
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/misra/volta/gv11b
      SEC2_PRESENT       = false
      LWENC_PRESENT      = false
      LWDEC_PRESENT      = false
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/gv11b
      RELEASE_PATH_HAL   = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/gv11b
      FALCON_ARCH        = falcon6
      ACR_LOAD           = true
      ACR_ONLY_BO        = false
      ACR_SIG_VERIF      = false
      SIGGEN_CFG         = acr_siggen_gv11b.cfg
      SIGGEN_CHIPS      += "gm206"
  endif

  ifneq (,$(findstring acr_pmu-t234_load,  $(ACRCFG_PROFILE)))
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f/
      MANUAL_PATHS                    += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f/
      SEC2_PRESENT                     = false
      LWENC_PRESENT                    = false
      LWDEC_PRESENT                    = false
      RELEASE_PATH                     = $(RESMAN_ROOT)/kernel/inc/acr/bin/pmu/t234
      RELEASE_PATH_HAL                 = $(LW_SOURCE)/uproc/tegra_acr/halgen/pmu/t234
      FALCON_ARCH                      = falcon6
      ACR_LOAD                         = true
      ACR_ONLY_BO                      = false
      ACR_SIG_VERIF                    = false
      ACR_FMODEL_BUILD                 = true
      ACR_SAFE_BUILD                   = false
      ACR_MMU_INDEXED                  = false
      IS_HS_ENCRYPTED                  = false
      ACR_PRI_SOURCE_ISOLATION_ENABLED = true
      ACR_SUB_WPR                      = true
      SIGGEN_CFG                       = acr_siggen_t234.cfg
      SIGGEN_CHIPS                     = "gm206"
  endif

endif


