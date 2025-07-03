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

##############################################################################
# Build a list of all possible soe-riscv fmc profiles
##############################################################################

PROFILES_ALL := ls10

##############################################################################
# Profile-specific settings
##############################################################################

ifdef PROFILE
  MANUAL_PATHS          =
  UPROC_ARCH            = lwriscv
  ENGINE                = soe
  USE_LIBCCC            = true
  PARTITIONS            =
  SK_PARTITION_COUNT    = 1
  LW_MANUALS_ROOT       = $(LW_SOURCE)/drivers/common/inc/hwref
  LWRISCV_SDK_VERSION   = lwriscv-sdk-3.0_dev
  LWRISCV_CONFIG_DEBUG_PRINT_LEVEL = 10 / Mask prints

  ifneq (,$(findstring ls10, $(PROFILE)))
    PROJ                = ls10
    ARCH                = lwswitch
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/lwswitch/ls10
    SIGGEN_CHIP         = gh100_rsa3k_0
    LWRISCV_SDK_PROFILE = soe-rtos-ls10
  endif
endif

