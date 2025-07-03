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
# 1. Define all available SYSLIBCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SYSLIBCFG_PROFILE. To pick-up the definitions, set SYSLIBCFG_PROFILE to the
# desired profile and source/include this file. When SYSLIBCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SYSLIBCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible syslib-config profiles
##############################################################################

SYSLIBCFG_PROFILES_ALL :=
SYSLIBCFG_PROFILES_ALL += syslib-ga10x
SYSLIBCFG_PROFILES_ALL += syslib-ga10b
SYSLIBCFG_PROFILES_ALL += syslib-ga10f
SYSLIBCFG_PROFILES_ALL += syslib-ga11b
SYSLIBCFG_PROFILES_ALL += syslib-ad10x
SYSLIBCFG_PROFILES_ALL += syslib-ad10b
SYSLIBCFG_PROFILES_ALL += syslib-gh100
SYSLIBCFG_PROFILES_ALL += syslib-gh20x
SYSLIBCFG_PROFILES_ALL += syslib-gb100
SYSLIBCFG_PROFILES_ALL += syslib-g00x
SYSLIBCFG_PROFILES_ALL += syslib-ls10

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SYSLIBCFG_PROFILE
  MANUAL_PATHS =

  ifneq (,$(findstring syslib-ga10x,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ga10x
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
  endif

  ifneq (,$(findstring syslib-ga10b,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ga10b
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
  endif

  ifneq (,$(findstring syslib-ga10f,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ga10f
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f
  endif

  ifneq (,$(findstring syslib-ga11b,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ga11b
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga11b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga11b
  endif

  ifneq (,$(findstring syslib-t23xd,  $(SYSLIBCFG_PROFILE)))
    PROJ          = t23xd
    ARCH          = orin
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
  endif

  ifneq (,$(findstring syslib-ad10x,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ad10x
    ARCH          = ada
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
  endif

  ifneq (,$(findstring syslib-ad10b,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ad10x
    ARCH          = ada
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad10b
  endif

  ifneq (,$(findstring syslib-gh100,  $(SYSLIBCFG_PROFILE)))
    PROJ          = gh100
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
  endif

  ifneq (,$(findstring syslib-gh20x,  $(SYSLIBCFG_PROFILE)))
    PROJ          = gh20x
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
  endif

  ifneq (,$(findstring syslib-gb100,  $(SYSLIBCFG_PROFILE)))
    PROJ          = gb100
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/blackwell/gb100
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/blackwell/gb100
  endif

  ifneq (,$(findstring syslib-g00x,  $(SYSLIBCFG_PROFILE)))
    PROJ          = g00x
    ARCH          = g00x
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
  endif

  ifneq (,$(findstring syslib-ls10,  $(SYSLIBCFG_PROFILE)))
    PROJ          = ls10
    ARCH          = lwswitch
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10
  endif

endif
