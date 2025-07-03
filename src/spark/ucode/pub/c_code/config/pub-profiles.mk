#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available PUBCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in PUBCFG_PROFILE. To pick-up the definitions, set PUBCFG_PROFILE to the
# desired profile and source/include this file. When PUBCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# PUBCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible pub-config profiles
##############################################################################

PUBCFG_PROFILES_ALL :=

MANUAL_PATHS       =
PUBCFG_PROFILES_ALL += pub_sec2-tu10a
LDSCRIPT           = link_script.ld
LWOS_VERSION       = dev
RTOS_VERSION       = OpenRTOSv4.1.3
SIGGEN_FUSE_VER    = -1        # -1 means not defined
SIGGEN_ENGINE_ID   = -1
SIGGEN_UCODE_ID    = -1 
SIGNATURE_SIZE     = 128       # Default - AES Signature Size
PUB_TU104_UCODE_BUILD_VERSION = 0
SEC2_PUB_UCODE_ID  = 5         # TODO : Remove this once added in lwFalconSignParams.mk, by ongoing RM change
SSP_ENABLED        = true
IS_HS_ENCRYPTED    = true
PKC_ENABLED        = true
# ADA SCENARIO VARIABLES
ARCHITECTURE       = tu10x
FALCON             = sec2
BOARD              = auto
BUILD_UCODE        = pub

##############################################################################
# Profile-specific settings
##############################################################################

  ifneq (,$(findstring pub_sec2-tu10a,  $(PUBCFG_PROFILE)))
      PUB_TU104_UCODE_BUILD_VERSION = 0
      CHIP_MANUAL_PATH  += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu104
      MANUAL_PATHS      += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu104
#      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/acr/bin/sec2/tu10a/pub
      RELEASE_PATH       = $(RESMAN_ROOT)/kernel/inc/spark/bin/pub/sec2/tu10a/
      LWENC_PRESENT      = false
      FALCON_ARCH        = falcon6
      SIGGEN_CFG         = pub_siggen_tu10a.cfg
      SIGGEN_CHIPS      += "tu102_rsa3k_3"
      SIGGEN_FUSE_VER    = 0
      SIGGEN_ENGINE_ID   = $(ENGINE_ID_SEC2)
      SIGGEN_UCODE_ID    = 5
      SIGNATURE_SIZE     = $(RSA3K_SIZE_IN_BITS)
  endif
