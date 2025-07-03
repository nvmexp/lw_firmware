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
# 1. Define all available GSPCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, uproc architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in GSPCFG_PROFILE. To pick-up the definitions, set GSPCFG_PROFILE to the
# desired profile and source/include this file. When GSPCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# GSPCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible gsp-config profiles
##############################################################################

GSPCFG_PROFILES_ALL :=
GSPCFG_PROFILES_ALL += gsp-ga10x
GSPCFG_PROFILES_ALL += gsp-t23xd
GSPCFG_PROFILES_ALL += gsp-gh100

##############################################################################
# Profile-specific settings
##############################################################################

ifdef GSPCFG_PROFILE
  MANUAL_PATHS                  =
  LWOS_VERSION                  = dev
  LS_UCODE_VERSION              = 0
  UPROC_ARCH                    = lwriscv
  RTOS_VERSION                  = SafeRTOSv8.4.0-lw1.0
  IS_SSP_ENABLED                = true
  HS_OVERLAYS_ENABLED           = false
  DMEM_VA_SUPPORTED             = false
  MRU_OVERLAYS                  = false
  ODP_ENABLED                   = true
  FPU_SUPPORTED                 = true
  LWRISCV_MPU_DUMP_ENABLED      = true
  LWRISCV_CORE_DUMP             = true
  LWRISCV_PARTITION_SWITCH      = false
  # TODO: clean up partition switch test code completely.
  PARTITION_SWITCH_TEST         = false
  LWRISCV_DEBUG_PRINT_LEVEL     = 3
  LWRISCV_SYMBOL_RESOLVER       = true
  LWRISCV_MPU_FBHUB_ALLOWED     = true
  SCHEDULER_ENABLED             = false
  HDCP22_SUPPORT_MST            = true
  HDCP22_FORCE_TYPE1_ONLY       = false
  HDCP_TEGRA_BUILD              = false
  USE_GSCID                     = false
  USE_SCPLIB                    = false
  DMEM_END_CARVEOUT_SIZE        = 0x1000
  USE_CSB                       = true
  USE_CBB                       = false
  RUN_ON_SEC                    = false
  LWRISCV_HAS_DCACHE            = true
  ELF_IN_PLACE                  = false
  ELF_IN_PLACE_FULL_IF_NOWPR    = false
  SELWRITY_ENGINE               = false
  ELF_IN_PLACE_FULL_ODP_COW     = false
  # Flag to control legacy CSR timer behavior - before gh100 it counted time in units of 32 ns
  LWRISCV_MTIME_TICK_SHIFT      = 0
  # MMINTS-TODO: enable LWRISCV raw-mode prints for GSP-RTOS!
  LWRISCV_PRINT_RAW_MODE        = false
  HDCP22_USE_SCP_ENCRYPT_SECRET = false
  HDCP22_CHECK_STATE_INTEGRITY  = false
  HDCP22_LC_RETRY_WAR_ENABLED   = false
  HDCP22_WAR_3051763_ENABLED    = false
  HDCP22_WAR_ECF_SELWRE_ENABLED = false
  HDCP22_USE_SCP_GEN_DKEY       = false
  HDCP22_USE_SE_RSA_HW          = false
  HDCP22_DEBUG_MODE             = false
  LWRISCV_SDK_VERSION           = lwriscv-sdk-3.0_dev
  HDCP22_KMEM_ENABLED           = false

  ifneq (,$(findstring gsp-ga10x, $(GSPCFG_PROFILE)))
    PROJ                        = ga10x
    ARCH                        = ampere
    RVBL_PROFILE                = ga10x
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
    SELWRITY_ENGINE             = true
    SECFG_PROFILE               = se-tu10x

    DISPLAY_ENABLED             = true
    I2CCFG_PROFILE              = i2c-gv10x
    DPAUXCFG_PROFILE            = dpaux-v0300
    HDCPCFG_PROFILE             = hdcp-v0300
    HDCP22WIREDCFG_PROFILE      = hdcp22wired-v0401
    DRIVERSCFG_PROFILE          = drivers-ga10x
    SYSLIBCFG_PROFILE           = syslib-ga10x
    SHLIBCFG_PROFILE            = shlib-ga10x
    SEPKERNCFG_PROFILE          = ga10x_hdcp
    LWRISCV_PARTITION_SWITCH    = true
    PARTITION_SWITCH_TEST       = true

    USE_SCPLIB                  = true
    IS_SSP_ENABLED              = false
    OS_CALLBACKS                = true
    USE_CSB                     = false

    ELF_IN_PLACE                = true
    ELF_IN_PLACE_FULL_IF_NOWPR  = true
    ELF_IN_PLACE_FULL_ODP_COW   = true

    LWRISCV_MTIME_TICK_SHIFT    = 5
  endif

  ifneq (,$(findstring gsp-t23xd, $(GSPCFG_PROFILE)))
    PROJ                        = t23xd
    ARCH                        = orin
    RVBL_PROFILE                = t23xd
    RUN_ON_SEC                  = true
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/t23x/t234/
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/t23x/t234/
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga10b/
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga10b/
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_02
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_02
    SELWRITY_ENGINE             = false
    #SECFG_PROFILE              = se-tu10x #TODO 200586859: Need to compile CHEETAH SE library

    DISPLAY_ENABLED             = true
    #I2CCFG_PROFILE             = i2c-gv10x #TODO 200586859: Need to compile CHEETAH I2C FSP
    DPAUXCFG_PROFILE            = dpaux-v0402
    HDCPCFG_PROFILE             = hdcp-v0402
    HDCP22WIREDCFG_PROFILE      = hdcp22wired-v0402
    HDCP22_SUPPORT_MST          = false
    HDCP22_FORCE_TYPE1_ONLY     = true
    HDCP_TEGRA_BUILD            = true
    USE_GSCID                   = true
    DRIVERSCFG_PROFILE          = drivers-t23xd
    SYSLIBCFG_PROFILE           = syslib-t23xd
    SHLIBCFG_PROFILE            = shlib-t23xd
    SEPKERNCFG_PROFILE          = t23xd_hdcp
    LWRISCV_PARTITION_SWITCH    = true
    ODP_ENABLED                 = false
    LWRISCV_CORE_DUMP           = false
    LWRISCV_SYMBOL_RESOLVER     = false
    LWRISCV_SDK_PROJ            = t23x-tsec

    IS_SSP_ENABLED              = false
    OS_CALLBACKS                = true
    USE_CBB                     = true
    SIGN_LOCAL                  = 1  #TODO 200586859: Need to remove these after updating LS server
    SIGN_SERVER                 = 0
    LWRISCV_HAS_DCACHE          = false

    LWRISCV_MTIME_TICK_SHIFT    = 5
  endif

  ifneq (,$(findstring gsp-gh100, $(GSPCFG_PROFILE)))
    PROJ                        = gh100
    ARCH                        = hopper
    RVBL_PROFILE                = gh100
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
    MANUAL_PATHS               += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
    SELWRITY_ENGINE              = false
    #SECFG_PROFILE               = se-tu10x  #TODO need to use sechub SE

    DISPLAY_ENABLED             = false
    I2CCFG_PROFILE              = i2c-gv10x
    DPAUXCFG_PROFILE            = dpaux-v0300
    HDCPCFG_PROFILE             = hdcp-v0300
    HDCP22WIREDCFG_PROFILE      = hdcp22wired-v0300
    DRIVERSCFG_PROFILE          = drivers-gh100
    SYSLIBCFG_PROFILE           = syslib-gh100
    SHLIBCFG_PROFILE            = shlib-gh100
    SEPKERNCFG_PROFILE          = gh100_hdcp
    USE_SCPLIB                  = true
    IS_SSP_ENABLED              = false
    OS_CALLBACKS                = true

    ELF_IN_PLACE                = true
    ELF_IN_PLACE_FULL_IF_NOWPR  = true
    ELF_IN_PLACE_FULL_ODP_COW   = true

    SIGN_LOCAL                  = 1  #TODO 200586859: Need to remove these after updating LS server
    SIGN_SERVER                 = 0
  endif

endif
