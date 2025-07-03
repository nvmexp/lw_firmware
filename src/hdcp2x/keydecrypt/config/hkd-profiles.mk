#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2018 by LWPU Corporation.  All rights reserved.  All
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
##############################################################################

HKDCFG_PROFILES_ALL :=

HKDCFG_PROFILES_ALL += hkd-gm20x
HKDCFG_PROFILES_ALL += hkd-gv10xf
HKDCFG_PROFILES_ALL += hkd-tu10x
HKDCFG_PROFILES_ALL += hkd-tu116
HKDSW               = $(LW_SOURCE)/uproc/hdcp2x/keydecrypt

##############################################################################
# Profile-specific settings
##############################################################################

ifdef HKDCFG_PROFILE

  HKDSRC_CMN        = ./lw/hkd.c
  HKDSRC_ALL        = $(HKDSRC_CMN)
  UCODE_ENCRYPTED   = 0
  HKD_PROFILE_TU10X = false
  HKD_PROFILE_TU11X = false
  IS_HS_ENCRYPTION  = false
  MANUAL_PATHS      =
  LDSCRIPT          := link_script.ld

  ifneq (,$(findstring hkd-gm20x,  $(HKDCFG_PROFILE)))
      HKDSRC_ALL   =  $(HKDSRC_CMN)
      HKDSRC_ALL   += $(HKDSW)/src/maxwell/hkdgm200.c
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/maxwell/gm200
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/maxwell/gm200
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/disp/v02_05
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v02_05
      MANUAL_PATHS += $(RESMAN_ROOT)/kernel/inc/fermi/gf110
      MANUAL_PATHS += $(LW_SOURCE)/uproc/dmemva/inc
      RELEASE_PATH  = $(RESMAN_ROOT)/kernel/cipher/v01
      FALCON_ARCH  := falcon5
      SIGGEN_CHIPS := "gm206" "gp104"
  endif

  ifneq (,$(findstring hkd-gv10xf,  $(HKDCFG_PROFILE)))
      HKDSRC_ALL    =  $(HKDSRC_CMN)
      HKDSRC_ALL   += $(HKDSW)/src/volta/hkdgv10xf.c
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv100/
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv100/
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/swref/disp/v03_00
      MANUAL_PATHS += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v03_00
      RELEASE_PATH  = $(RESMAN_ROOT)/kernel/cipher/v01
      FALCON_ARCH  := falcon6
      SIGGEN_CHIPS := "gv100"
  endif

  ifneq (,$(findstring hkd-tu10x,  $(HKDCFG_PROFILE)))
      HKDSRC_ALL        += $(HKDSW)/src/maxwell/hkdgm200.c
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/cipher/v04
      FALCON_ARCH       := falcon6
      UCODE_ENCRYPTED   := 1
      IS_HS_ENCRYPTION  := true
      SIGGEN_CHIPS      := "tu102_aes"
  endif

  ifneq (,$(findstring hkd-tu116,  $(HKDCFG_PROFILE)))
      HKDSRC_ALL        += $(HKDSW)/src/maxwell/hkdgm200.c
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu116
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/cipher/v04
      FALCON_ARCH       := falcon6
      UCODE_ENCRYPTED   := 1
      IS_HS_ENCRYPTION  := true
      HKD_PROFILE_TU11X := true
      SIGGEN_CHIPS      := "tu116_aes"
  endif
endif
