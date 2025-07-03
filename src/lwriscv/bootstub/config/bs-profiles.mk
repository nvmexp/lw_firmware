#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to define all profile-specific make-vars. To pick-up the
# definitions, set BSCFG_PROFILE to the desired profile and source/include this
# file.
#

##############################################################################
# Default settings - can be overridden if needed
##############################################################################

BSSRC                     = main.c debug.c mpu.c load.S start.S
LDSCRIPT_IN               = $(BSSW)/build/link_script.ld.in
LDSCRIPT                  = $(OUTPUTDIR)/link_script.ld

STACK_SIZE                = 0x180

IS_FUSE_CHECK_ENABLED     = true
IS_SSP_ENABLED            = true

##############################################################################
# Profile-specific settings
##############################################################################

ifneq (,$(findstring gsp_rvbs_ga10x, $(BSCFG_PROFILE)))
  MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
  MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102

  DMEM_BASE                 = 0x180000
  DMEM_SIZE                 = 0x10000
endif

ifneq (,$(findstring soe_rvbs_ls10, $(BSCFG_PROFILE)))
  MANUAL_PATHS              = $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
  MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10

  DMEM_BASE                 = 0x180000
  DMEM_SIZE                 = 0x20000
endif

ifeq ($(IS_SSP_ENABLED), true)
  BSSRC += ssp.c
endif
