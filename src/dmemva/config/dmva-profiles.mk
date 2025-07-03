#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available DMEMVACFG profiles
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
##############################################################################

DMVACFG_PROFILES_ALL :=
DMVACFG_PROFILES_ALL += dmva-pmu_gp10x
DMVACFG_PROFILES_ALL += dmva-sec2_gp10x
DMVACFG_PROFILES_ALL += dmva-sec2_tu10x
DMVACFG_PROFILES_ALL += dmva-gsp_tu10x
DMVACFG_PROFILES_ALL += dmva-lwdec_gp10x
DMVASW               = $(LW_SOURCE)/uproc/dmemva/

##############################################################################
# Profile-specific settings
##############################################################################

ifdef DMVACFG_PROFILE
  MANUAL_PATHS      = $(LW_SOURCE)/diag/mods/gpu/tests/rm/dmemva/ucode
  DMVASRC_ALL       = lw/dmvatests.c lw/dmvadma.c lw/dmvautils.c lw/dmvaselwrity.c lw/dmvarw.c lw/dmvarand.c lw/dmvainstrs.c lw/dmvaassert.c lw/dmvamem.c lw/dmvainterrupts.c
  FALCON_ARCH       = falcon6
  LDSCRIPT          = link_script.ld
  RELEASE_PATH      = $(LW_SOURCE)/diag/mods/gpu/tests/rm/dmemva/ucode

  #TODO: add MAX_CODE_SIZE, MAX_DATA_SIZE

  DMVA_FALCON_PMU   = false
  DMVA_FALCON_LWDEC = false
  DMVA_FALCON_SEC2  = false
  DMVA_FALCON_GSP   = false
  DMVA_FALCON_APERT = false #TODO: find out what this does
  IMAGE_BASE        = 0x0000
  LWOS_VERSION      = dev
  RTOS_VERSION      = OpenRTOSv4.1.3

  ifneq (,$(findstring dmva-pmu_gp10x,  $(DMVACFG_PROFILE)))
      DMVA_FALCON_PMU   = true
      SIG_GEN_CFG       = dmva_pmu_siggen.cfg
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
      SIGGEN_CHIP       = gp102
  endif

  ifneq (,$(findstring dmva-lwdec_gp10x,  $(DMVACFG_PROFILE)))
      DMVA_FALCON_LWDEC = true
      SIG_GEN_CFG       = dmva_lwdec_siggen.cfg
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
      SIGGEN_CHIP       = gp102
  endif

  ifneq (,$(findstring dmva-sec2_gp10x,  $(DMVACFG_PROFILE)))
      DMVA_FALCON_SEC2  = true
      SIG_GEN_CFG       = dmva_sec2_siggen.cfg
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/pascal/gp102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/pascal/gp102
      SIGGEN_CHIP       = gp102
  endif

  ifneq (,$(findstring dmva-sec2_tu10x,  $(DMVACFG_PROFILE)))
      DMVA_FALCON_SEC2  = true
      SIG_GEN_CFG       = dmva_sec2_siggen.cfg
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      SIGGEN_CHIP       = gp102
  endif

  ifneq (,$(findstring dmva-gsp_tu10x,  $(DMVACFG_PROFILE)))
      DMVA_FALCON_GSP  = true
      SIG_GEN_CFG       = dmva_gsp_siggen.cfg
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      SIGGEN_CHIP       = gp102
  endif

endif

