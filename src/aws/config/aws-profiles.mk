#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available AWSCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in AWSCFG_PROFILE. To pick-up the definitions, set AWSCFG_PROFILE to the
# desired profile and source/include this file. When AWSCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# AWSCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible bl-config profiles
##############################################################################

AWSCFG_PROFILES_ALL :=
AWSCFG_PROFILES_ALL += aws_ucode_gk10x
AWSCFG_PROFILES_ALL += aws_ucode_gk20x
AWSSW                = $(LW_SOURCE)/uproc/aws/

##############################################################################
# Profile-specific settings
#  - LS_FALCON setting is used to determine which (if any) tool should be
#    used to resolve signature requirements for ucode affected by the BL.
#    See ../build/gen_ls_sig.lwmk
##############################################################################

ifdef AWSCFG_PROFILE
  AWSSRC              = main.c
  LDSCRIPT_IN         = $(AWSSW)/build/link_script.ld.in
  LDSCRIPT            = $(OUTPUTDIR)/link_script.ld

  ifneq (,$(findstring aws_ucode_gk10x,  $(AWSCFG_PROFILE)))
      IMAGE_BASE      = 0x0
      MAX_CODE_SIZE   = 4K
      MAX_DATA_SIZE   = 4K
      FALCON_ARCH     = falcon4
  endif

  ifneq (,$(findstring aws_ucode_gk20x,  $(AWSCFG_PROFILE)))
      IMAGE_BASE      = 0x0
      MAX_CODE_SIZE   = 4K
      MAX_DATA_SIZE   = 4K
      FALCON_ARCH     = falcon5
  endif


  LD_DEFINES     := -DIMAGE_BASE=$(IMAGE_BASE) -DMAX_CODE_SIZE=$(MAX_CODE_SIZE) -DMAX_DATA_SIZE=$(MAX_DATA_SIZE) -Ufalcon

endif

