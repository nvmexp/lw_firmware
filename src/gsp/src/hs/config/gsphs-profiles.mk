#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
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
# in GSPHSCFG_PROFILE. To pick-up the definitions, set GSPHSCFG_PROFILE to the
# desired profile and source/include this file. When GSPHSCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# GSPHSCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible HKD-config profiles
##############################################################################

GSPHSCFG_PROFILES_ALL := gsphs-tu10x
GSPHSSW               = $(LW_SOURCE)/uproc/gsp/src/hs

##############################################################################
# Profile-specific settings
##############################################################################

ifdef GSPHSCFG_PROFILE
  MANUAL_PATHS          :=
  LDSCRIPT              := link_script.ld
  LWOS_VERSION          := dev
  IS_SSP_ENABLED        := false
  IMEM_ON_DEMAND_PAGING := false
  RTOS_VERSION          := SafeRTOSv5.16.0-lw1.3
  DMEM_VA_SUPPORTED     := true
  MRU_OVERLAYS          := true

  ifneq (,$(findstring gsphs-tu10x,  $(GSPHSCFG_PROFILE)))
      PROJ                          := tu10x
      ARCH                          := turing
      GSPHSSRC_ALL                  :=  entry.c testApplet.c gsp_selwreaction_hs.c
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102/
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102/
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00/
      MANUAL_PATHS                  += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00/
      FALCON_ARCH                   := falcon6
      SELWRITY_ENGINE               := true
      SECFG_PROFILE                 := se-tu10x
      HDCPCFG_PROFILE               := hdcp-v0300
      HDCP22WIREDCFG_PROFILE        := hdcp22wired-v0300
      SIGGEN_CHIPS                  := "tu102_aes"
      OS_CALLBACKS                  := true
      HDCP22_USE_SCP_ENCRYPT_SECRET := true
      HDCP22_CHECK_STATE_INTEGRITY  := true
      HDCP22_USE_SCP_GEN_DKEY       := true
      HS_OVERLAYS_ENABLED           := true
  endif


endif

