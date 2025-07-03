#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available HKDCFG profiles
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
# lww - LwWatch binaries
##############################################################################

BSIGC6SETUP_PROFILES_ALL :=
BSIGC6SETUP_PROFILES_ALL += bsigc6setup_tu10x
#BSIGC6SETUPSW             = $(LW_SOURCE)/uproc/bsigc6setup/

##############################################################################
# Profile-specific settings
##############################################################################

ifdef BSIGC6SETUP_PROFILE
  BSIGC6SETUPSRC_ALL    = $(LW_SOURCE)/uproc/bsigc6setup/src/lw/bsigc6setup.c
#  BSIGC6SETUPSRC_ALL    += $(LW_SOURCE)/uproc/libs/rtos/src/osptimer.c
  FALCON_ARCH           = falcon6
  LDSCRIPT              = link_script.ld

  BSIGC6SETUP_FALCON_PMU   = false
  BSIGC6SETUP_FALCON_LWDEC = false
  BSIGC6SETUP_FALCON_SEC2  = false
  BSIGC6SETUP_FALCON_APERT = false
  LWOS_VERSION             = dev
  RTOS_VERSION             = OpenRTOSv4.1.3

  ifneq (,$(findstring bsigc6setup_tu10x,  $(BSIGC6SETUP_PROFILE)))
      MANUAL_PATHS              += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS              += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      BSIGC6SETUP_FALCON_LWDEC  = true
      RELEASE_PATH              = $(LW_SOURCE)/drivers/resman/kernel/inc/bsigc6setup/bin/tu10x
  endif
endif

