#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available DRIVERSCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in DRIVERSCFG_PROFILE. To pick-up the definitions, set DRIVERSCFG_PROFILE to the
# desired profile and source/include this file. When DRIVERSCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# DRIVERSCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible drivers-config profiles
##############################################################################

DRIVERSCFG_PROFILES_ALL :=
DRIVERSCFG_PROFILES_ALL += drivers-ga10x
DRIVERSCFG_PROFILES_ALL += drivers-ga10b
DRIVERSCFG_PROFILES_ALL += drivers-ga10f
DRIVERSCFG_PROFILES_ALL += drivers-ga11b
DRIVERSCFG_PROFILES_ALL += drivers-t23xd
DRIVERSCFG_PROFILES_ALL += drivers-ad10x
DRIVERSCFG_PROFILES_ALL += drivers-ad10b
DRIVERSCFG_PROFILES_ALL += drivers-gh100
DRIVERSCFG_PROFILES_ALL += drivers-gh20x
DRIVERSCFG_PROFILES_ALL += drivers-gb100
DRIVERSCFG_PROFILES_ALL += drivers-g00x
DRIVERSCFG_PROFILES_ALL += drivers-ls10

##############################################################################
# Profile-specific settings
##############################################################################

ifdef DRIVERSCFG_PROFILE
  MANUAL_PATHS =

  ifneq (,$(findstring drivers-ga10x, $(DRIVERSCFG_PROFILE)))
    PROJ          = ga10x
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
  endif

  ifneq (,$(findstring drivers-ga10b, $(DRIVERSCFG_PROFILE)))
    PROJ          = ga10b
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
  endif

  ifneq (,$(findstring drivers-ga10f, $(DRIVERSCFG_PROFILE)))
    PROJ          = ga10f
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f
  endif

  ifneq (,$(findstring drivers-ga11b, $(DRIVERSCFG_PROFILE)))
    PROJ          = ga11b
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga11b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga11b
  endif

  ifneq (,$(findstring drivers-t23xd, $(DRIVERSCFG_PROFILE)))
    PROJ          = t23xd
    ARCH          = orin
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
  endif

  ifneq (,$(findstring drivers-ad10x, $(DRIVERSCFG_PROFILE)))
    PROJ          = ad10x
    ARCH          = ada
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
  endif

  ifneq (,$(findstring drivers-ad10b, $(DRIVERSCFG_PROFILE)))
    PROJ          = ad10b
    ARCH          = ada
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad10b
  endif

  ifneq (,$(findstring drivers-gh100, $(DRIVERSCFG_PROFILE)))
    PROJ          = gh100
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
  endif

  ifneq (,$(findstring drivers-gh20x, $(DRIVERSCFG_PROFILE)))
    PROJ          = gh20x
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
  endif

  ifneq (,$(findstring drivers-gb100, $(DRIVERSCFG_PROFILE)))
    PROJ          = gb100
    ARCH          = blackwell
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/blackwell/gb100
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/blackwell/gb100
  endif

  ifneq (,$(findstring drivers-g00x, $(DRIVERSCFG_PROFILE)))
    PROJ          = g00x
    ARCH          = g00x
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
  endif

  ifneq (,$(findstring drivers-ls10, $(DRIVERSCFG_PROFILE)))
    PROJ          = ls10
    ARCH          = lwswitch
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10
  endif

endif
