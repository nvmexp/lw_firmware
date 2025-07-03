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
# 1. Define all available SECFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SECFG_PROFILE. To pick-up the definitions, set SECFG_PROFILE to the
# desired profile and source/include this file. When SECFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SECFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible se-config profiles
##############################################################################

SECFG_PROFILES_ALL :=
SECFG_PROFILES_ALL += se-gp10x
SECFG_PROFILES_ALL += se-gv10x
SECFG_PROFILES_ALL += se-tu10x
SECFG_PROFILES_ALL += se-ad10x

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SECFG_PROFILE
    SELWRE_BUS_ONLY_SUPPORTED = false

  ifneq (,$(findstring se-gp10x,  $(SECFG_PROFILE)))
    PROJ = gp10x
    ARCH = pascal
  endif

  ifneq (,$(findstring se-gv10x,  $(SECFG_PROFILE)))
    PROJ = gv10x
    ARCH = volta
  endif

  ifneq (,$(findstring se-tu10x,  $(SECFG_PROFILE)))
    PROJ = tu10x
    ARCH = turing
  endif

  ifneq (,$(findstring se-ad10x,  $(SECFG_PROFILE)))
    PROJ = ad10x
    ARCH = ada
    SELWRE_BUS_ONLY_SUPPORTED = true
  endif

endif
