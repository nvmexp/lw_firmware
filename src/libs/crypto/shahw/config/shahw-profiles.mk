#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available SHAHWCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SHAHWCFG_PROFILE. To pick-up the definitions, set SHAHWCFG_PROFILE to the
# desired profile and source/include this file. When SHAHWCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SHAHWCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible shahw-config profiles
##############################################################################

SHAHWCFG_PROFILES_ALL :=
SHAHWCFG_PROFILES_ALL += shahw-tu10x

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SHAHWCFG_PROFILE

  ifneq (,$(findstring shahw-tu10x,  $(SHAHWCFG_PROFILE)))
    PROJ = tu10x
    ARCH = turing
  endif

endif
