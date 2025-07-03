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
# 1. Define all available SHLIBCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SHLIBCFG_PROFILE. To pick-up the definitions, set SHLIBCFG_PROFILE to the
# desired profile and source/include this file. When SHLIBCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SHLIBCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible shlib-config profiles
##############################################################################

SHLIBCFG_PROFILES_ALL :=
# PMU does not have RISCV on TU10X.
SHLIBCFG_PROFILES_ALL += shlib-ga10x
SHLIBCFG_PROFILES_ALL += shlib-ga10b
SHLIBCFG_PROFILES_ALL += shlib-ga10f
SHLIBCFG_PROFILES_ALL += shlib-ga11b
SHLIBCFG_PROFILES_ALL += shlib-t23xd
SHLIBCFG_PROFILES_ALL += shlib-ad10x
SHLIBCFG_PROFILES_ALL += shlib-ad10b
SHLIBCFG_PROFILES_ALL += shlib-gh100
SHLIBCFG_PROFILES_ALL += shlib-gh20x
SHLIBCFG_PROFILES_ALL += shlib-gb100
SHLIBCFG_PROFILES_ALL += shlib-g00x
SHLIBCFG_PROFILES_ALL += shlib-ls10

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SHLIBCFG_PROFILE
  MANUAL_PATHS =

  ifneq (,$(findstring shlib-ga10x, $(SHLIBCFG_PROFILE)))
    PROJ          = ga10x
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
  endif

  ifneq (,$(findstring shlib-ga10b, $(SHLIBCFG_PROFILE)))
    PROJ          = ga10b
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
  endif

  ifneq (,$(findstring shlib-ga10f, $(SHLIBCFG_PROFILE)))
    PROJ          = ga10f
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10f
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10f
  endif

  ifneq (,$(findstring shlib-ga11b, $(SHLIBCFG_PROFILE)))
    PROJ          = ga11b
    ARCH          = ampere
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga11b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga11b
  endif

  ifneq (,$(findstring shlib-t23xd, $(SHLIBCFG_PROFILE)))
    PROJ          = t23xd
    ARCH          = orin
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b
  endif

  ifneq (,$(findstring shlib-ad10x, $(SHLIBCFG_PROFILE)))
    PROJ          = ad10x
    ARCH          = ada
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
  endif

  ifneq (,$(findstring shlib-ad10b, $(SHLIBCFG_PROFILE)))
    PROJ          = ad10b
    ARCH          = ada
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad10b
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad10b
  endif

  ifneq (,$(findstring shlib-gh100, $(SHLIBCFG_PROFILE)))
    PROJ          = gh100
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
  endif

  ifneq (,$(findstring shlib-gh20x, $(SHLIBCFG_PROFILE)))
    PROJ          = gh20x
    ARCH          = hopper
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
  endif

  ifneq (,$(findstring shlib-gb100, $(SHLIBCFG_PROFILE)))
    PROJ          = gb100
    ARCH          = blackwell
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/blackwell/gb100
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/blackwell/gb100
  endif

  ifneq (,$(findstring shlib-g00x, $(SHLIBCFG_PROFILE)))
    PROJ          = g00x
    ARCH          = g00x
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/g00x/g000
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/g00x/g000
  endif

  ifneq (,$(findstring shlib-ls10, $(SHLIBCFG_PROFILE)))
    PROJ          = ls10
    ARCH          = lwswitch
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
    MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10
  endif

endif
