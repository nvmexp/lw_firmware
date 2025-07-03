#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available I2CCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in I2CCFG_PROFILE. To pick-up the definitions, set I2CCFG_PROFILE to the
# desired profile and source/include this file. When I2CCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# I2CCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible i2c-config profiles
##############################################################################

I2CCFG_PROFILES_ALL :=
I2CCFG_PROFILES_ALL += i2c-gm20x
I2CCFG_PROFILES_ALL += i2c-gp10x
I2CCFG_PROFILES_ALL += i2c-gv10x
I2CCFG_PROFILES_ALL += i2c-t23xd

##############################################################################
# Profile-specific settings
##############################################################################

ifdef I2CCFG_PROFILE

  ifneq (,$(findstring i2c-gm20x,  $(I2CCFG_PROFILE)))
    PROJ = gm20x
    ARCH = maxwell
  endif

  ifneq (,$(findstring i2c-gp10x,  $(I2CCFG_PROFILE)))
    PROJ = gp10x
    ARCH = pascal
  endif

  ifneq (,$(findstring i2c-gv10x,  $(I2CCFG_PROFILE)))
    PROJ = gv10x
    ARCH = volta
  endif

  ifneq (,$(findstring i2c-t23xd,  $(I2CCFG_PROFILE)))
    PROJ = t234d
    ARCH = orin
  endif

endif
