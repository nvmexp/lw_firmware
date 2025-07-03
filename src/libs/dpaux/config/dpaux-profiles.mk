#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available DPAUXCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in DPAUXCFG_PROFILE. To pick-up the definitions, set DPAUXCFG_PROFILE to the
# desired profile and source/include this file. When DPAUXCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# DPAUXCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible dpaux-config profiles
##############################################################################

DPAUXCFG_PROFILES_ALL :=
DPAUXCFG_PROFILES_ALL += dpaux-v0205
DPAUXCFG_PROFILES_ALL += dpaux-v0207
DPAUXCFG_PROFILES_ALL += dpaux-v0300
DPAUXCFG_PROFILES_ALL += dpaux-v0402

##############################################################################
# Profile-specific settings
##############################################################################

ifdef DPAUXCFG_PROFILE

  ifneq (,$(findstring dpaux-v0205,  $(DPAUXCFG_PROFILE)))
    IPMAJORVER = 02
    IPMINORVER = 05
  endif

  ifneq (,$(findstring dpaux-v0207,  $(DPAUXCFG_PROFILE)))
    IPMAJORVER = 02
    IPMINORVER = 07
  endif

  ifneq (,$(findstring dpaux-v0300,  $(DPAUXCFG_PROFILE)))
    IPMAJORVER = 03
    IPMINORVER = 00
  endif

  ifneq (,$(findstring dpaux-v0402,  $(DPAUXCFG_PROFILE)))
    IPMAJORVER = 04
    IPMINORVER = 02
  endif

endif
