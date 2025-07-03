#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available CFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, uproc architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in CFG_PROFILE. To pick-up the definitions, set CFG_PROFILE to the
# desired profile and source/include this file. When CFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# CFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible config profiles
##############################################################################

CFG_PROFILES_ALL :=
CFG_PROFILES_ALL += gsp_riscv_tu10x
CFG_PROFILES_ALL += sec2_riscv_tu10x
CFG_PROFILES_ALL += pmu_riscv_ga100

##############################################################################
# Profile-specific settings
#  - LS_UPROC setting is used to determine which (if any) tool should be
#    used to resolve signature requirements for ucode affected by the BL.
#    See ../build/gen_ls_sig.lwmk
##############################################################################

ifdef CFG_PROFILE
  MANUAL_PATHS        =
  LDSCRIPT_IN         = $(APP_SW)/build/link_script.ld.in
  LDSCRIPT            = $(OUTPUTDIR)/link_script.ld
  HS_UPROC            = false
  LS_UPROC            = false

  # Lwrrently the same for all target configs
  IMEM_SIZE           = 0x10000
  DMEM_SIZE           = 0x10000
  RISCV_ARCH          = lwriscv

  ifneq (,$(findstring gsp_riscv_tu10x, $(CFG_PROFILE)))
    SAVE_PERF_CTRS      = true
  endif

  ifneq (,$(findstring sec2_riscv_tu10x, $(CFG_PROFILE)))
    SAVE_PERF_CTRS      = true
  endif

  ifneq (,$(findstring pmu_riscv_ga100, $(CFG_PROFILE)))
    IMEM_SIZE           = 0x20000
    DMEM_SIZE           = 0x20000
    SAVE_PERF_CTRS      = true
  endif

  LD_DEFINES         := -DIMEM_SIZE=$(IMEM_SIZE) -DDMEM_SIZE=$(DMEM_SIZE)

endif
