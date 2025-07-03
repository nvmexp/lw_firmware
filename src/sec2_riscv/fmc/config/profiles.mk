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
# Build a list of all possible sec2-riscv fmc profiles
##############################################################################

PROFILES_ALL := gh100

##############################################################################
# Profile-specific settings
##############################################################################

ifdef PROFILE
  MANUAL_PATHS          =
  UPROC_ARCH            = lwriscv
  ENGINE                = sec
  USE_LIBCCC            = true
  PARTITIONS            = worklaunch shared rtos
  SK_PARTITION_COUNT    = 2
  LW_MANUALS_ROOT       = $(LW_SOURCE)/drivers/common/inc/hwref
  LWRISCV_SDK_VERSION   = lwriscv-sdk-3.0_dev
  LWRISCV_CONFIG_DEBUG_PRINT_LEVEL = 10 / Mask prints

  ifneq (,$(findstring gh100, $(PROFILE)))
    PROJ                = gh100
    ARCH                = hopper
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    SIGGEN_CHIP         = gh100_rsa3k_0
    LWRISCV_SDK_PROFILE = sec2-rtos-gh100
  endif
endif

