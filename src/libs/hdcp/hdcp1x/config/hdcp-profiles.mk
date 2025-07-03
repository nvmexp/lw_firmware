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
# 1. Define all available HDCPCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in HDCPCFG_PROFILE. To pick-up the definitions, set HDCPCFG_PROFILE to the
# desired profile and source/include this file. When HDCPCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# HDCPCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible hdcp-config profiles
##############################################################################

HDCPCFG_PROFILES_ALL :=
HDCPCFG_PROFILES_ALL += hdcp-v0205
HDCPCFG_PROFILES_ALL += hdcp-v0207
HDCPCFG_PROFILES_ALL += hdcp-v0300
HDCPCFG_PROFILES_ALL += hdcp-v0402

##############################################################################
# Profile-specific settings
##############################################################################

ifdef HDCPCFG_PROFILE

  ifneq (,$(findstring hdcp-v0205,  $(HDCPCFG_PROFILE)))
    IPMAJORVER = 02
    IPMINORVER = 05
  endif

  ifneq (,$(findstring hdcp-v0207,  $(HDCPCFG_PROFILE)))
    IPMAJORVER = 02
    IPMINORVER = 07
  endif

  ifneq (,$(findstring hdcp-v0300,  $(HDCPCFG_PROFILE)))
    IPMAJORVER = 03
    IPMINORVER = 00
  endif

  ifneq (,$(findstring hdcp-v0402,  $(HDCPCFG_PROFILE)))
    IPMAJORVER = 04
    IPMINORVER = 02
  endif

endif
