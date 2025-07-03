#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available HDCP22WIREDCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in HDCP22WIREDCFG_PROFILE. To pick-up the definitions, set HDCP22WIREDCFG_PROFILE to the
# desired profile and source/include this file. When HDCP22WIREDCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# HDCP22WIREDCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible hdcp22wired-config profiles
##############################################################################

HDCP22WIREDCFG_PROFILES_ALL :=
HDCP22WIREDCFG_PROFILES_ALL += hdcp22wired-v0205
HDCP22WIREDCFG_PROFILES_ALL += hdcp22wired-v0207
HDCP22WIREDCFG_PROFILES_ALL += hdcp22wired-v0300
HDCP22WIREDCFG_PROFILES_ALL += hdcp22wired-v0400
HDCP22WIREDCFG_PROFILES_ALL += hdcp22wired-v0401
HDCP22WIREDCFG_PROFILES_ALL += hdcp22wired-v0402

##############################################################################
# Profile-specific settings
##############################################################################

ifdef HDCP22WIREDCFG_PROFILE

  ifneq (,$(findstring hdcp22wired-v0205,  $(HDCP22WIREDCFG_PROFILE)))
    IPMAJORVER = 02
    IPMINORVER = 05
  endif

  ifneq (,$(findstring hdcp22wired-v0207,  $(HDCP22WIREDCFG_PROFILE)))
    IPMAJORVER = 02
    IPMINORVER = 07
  endif

  ifneq (,$(findstring hdcp22wired-v0300,  $(HDCP22WIREDCFG_PROFILE)))
    IPMAJORVER = 03
    IPMINORVER = 00
  endif

  ifneq (,$(findstring hdcp22wired-v0400,  $(HDCP22WIREDCFG_PROFILE)))
    IPMAJORVER = 04
    IPMINORVER = 00
  endif

  ifneq (,$(findstring hdcp22wired-v0401,  $(HDCP22WIREDCFG_PROFILE)))
    IPMAJORVER = 04
    IPMINORVER = 01
  endif

   ifneq (,$(findstring hdcp22wired-v0402,  $(HDCP22WIREDCFG_PROFILE)))
    IPMAJORVER = 04
    IPMINORVER = 02
   endif
endif
