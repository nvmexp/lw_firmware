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
# Build a list of all possible pmu-riscv fmc profiles
##############################################################################

PROFILES_ALL :=
PROFILES_ALL += gsp_ga10x_hdcp_riscv
PROFILES_ALL += gsp_t23xd_hdcp_riscv

##############################################################################
# Profile-specific settings
##############################################################################

ifdef PROFILE
  MANUAL_PATHS                =
  UPROC_ARCH                  = lwriscv
  ENGINE                      = gsp
  USE_LIBCCC                  = false
  SK_PARTITION_COUNT          = 2
  PARTITIONS                  = rtos selwreaction
  LW_MANUALS_ROOT             = $(LW_SOURCE)/drivers/common/inc/hwref
  # TODO: update to 3.0_dev for non-orin chips.
  LWRISCV_SDK_VERSION         = lwriscv-sdk-2.1
  PROFILE_BASE_NAME           = $(PROFILE)
  LWRISCV_SDK_PROFILE         =
  PLM_LIST_FILE               =
  SEPKERN_IMEM_LIMIT          = 0x1000
  SEPKERN_DMEM_LIMIT          = 0x1000
  LWRISCV_APP_IMEM_LIMIT      = 0x8000
  LWRISCV_APP_DMEM_LIMIT      = 0x4000
  HDCP22WIREDHSCFG_PROFILE    =
  PARTITION_SWITCH_TEST       = false

  ifneq (,$(findstring gsp_ga10x_hdcp_riscv, $(PROFILE)))
    PROJ                      = ga10x
    SK_CHIP                   = ga10x
    ENGINE                    = gsp
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    SIGGEN_CHIP               = ga102_rsa3k_0
    SEPKERNCFG_PROFILE        = gsp_ga10x
    PLM_LIST_FILE             =
    PARTITION_SWITCH_TEST     = true
  endif

  ifneq (,$(findstring gsp_t23xd_hdcp_riscv, $(PROFILE)))
    PROJ                      = t23x
    SK_CHIP                   = orin
    ENGINE                    = tsec
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234/
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234/
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b/
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b/
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_02
    MANUAL_PATHS             += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_02
    SIGGEN_CHIP               = ga10b_rsa3k_0
    SEPKERNCFG_PROFILE        = gsp_t23xd
    PLM_LIST_FILE             =
  endif
endif

