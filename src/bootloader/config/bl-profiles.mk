#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available BLCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in BLCFG_PROFILE. To pick-up the definitions, set BLCFG_PROFILE to the
# desired profile and source/include this file. When BLCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# BLCFG_PROFILES. No default profile is assumed.
#
# Some usage notes to build bootloader locally:
# - export BLCFG_PROFILE=<the profile you want>
# - Go to the /src directory and run `make` or `lwmake`
#
# This should do the following automatically:
# - Compile the bootloader ucode, header etc
# - Use the syncctx script to update all impacted ucode files by contacting
#   the signing server
# - p4 add the files automatically
#
# Note that this script is not protected by any testing systems. So if the
# above steps are broken, you will need to debug locally.
#

##############################################################################
# Build a list of all possible bl-config profiles
##############################################################################

BLCFG_PROFILES_ALL :=
BLCFG_PROFILES_ALL += pmu_bl_gm10x
BLCFG_PROFILES_ALL += pmu_bl_gm20x
BLCFG_PROFILES_ALL += pmu_bl_gp10x
BLCFG_PROFILES_ALL += fecs_bl_gm10x
BLCFG_PROFILES_ALL += gpccs_bl_gm10x
BLCFG_PROFILES_ALL += fecs_bl_gm20x
BLCFG_PROFILES_ALL += fecs_bl_gv10x
BLCFG_PROFILES_ALL += gpccs_bl_gm20x
BLCFG_PROFILES_ALL += sec2_bl_gm20x
BLCFG_PROFILES_ALL += sec2_bl_gp10x
BLCFG_PROFILES_ALL += lwdec_bl_ga10x
BLCFG_PROFILES_ALL += lwdec_bl_gh100
BLCFG_PROFILES_ALL += lwdec_bl_ad10x
BLCFG_PROFILES_ALL += fecs_bl_gh10x
BLCFG_PROFILES_ALL += gpccs_bl_gh10x
BLCFG_PROFILES_ALL += fecs_bl_ad10x
BLCFG_PROFILES_ALL += gpccs_bl_ad10x
BLCFG_PROFILES_ALL += fecs_bl_ad10b
BLCFG_PROFILES_ALL += gpccs_bl_ad10b
BLSW                = $(LW_SOURCE)/uproc/bootloader/

##############################################################################
# Profile-specific settings
#  - LS_FALCON setting is used to determine which (if any) tool should be
#    used to resolve signature requirements for ucode affected by the BL.
#    See ../build/gen_ls_sig.lwmk
##############################################################################

ifdef BLCFG_PROFILE
  BLSRC_ORIGINAL             = main.c
  BLSRC_PARSED               = $(OUTPUTDIR)/g_main_cciparsed.c
  LDSCRIPT_IN                = $(BLSW)/build/link_script.ld.in
  LDSCRIPT                   = $(OUTPUTDIR)/link_script.ld
  HS_FALCON                  = false
  LS_FALCON                  = false
  VIRTUAL_DMA                = false
  PA_47_BIT_SUPPORTED        = false
  CLEAN_IMEM                 = false
  LWOS_VERSION               = dev
  RTOS_VERSION               = OpenRTOSv4.1.3
  BOOT_FROM_HS_BUILD         = false
  BOOT_FROM_HS_FALCON_SEC2   = false
  BOOT_FROM_HS_FALCON_GSP    = false
  BOOT_FROM_HS_FALCON_LWDEC  = false
  NUM_SIG_PER_UCODE          = 1

  ifneq (,$(findstring pmu_bl_gm10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x10000
      MAX_CODE_SIZE   = 64K
      MAX_DATA_SIZE   = 24K
      FALCON_ARCH     = falcon5
      HS_FALCON       = true
      VIRTUAL_DMA     = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/maxwell/gm107
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm107
  endif

  ifneq (,$(findstring pmu_bl_gm20x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x10000
      MAX_CODE_SIZE   = 64K
      MAX_DATA_SIZE   = 40K
      FALCON_ARCH     = falcon5
      HS_FALCON       = true
      VIRTUAL_DMA     = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/maxwell/gm200
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm200
  endif

  ifneq (,$(findstring pmu_bl_gp10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE          = 0x10000
      MAX_CODE_SIZE       = 64K
      MAX_DATA_SIZE       = 40K
      FALCON_ARCH         = falcon6
      HS_FALCON           = true
      VIRTUAL_DMA         = true
      PA_47_BIT_SUPPORTED = true
      RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/kernel/inc/pmu/bin
      MANUAL_PATH         = $(BLSW)/../../drivers/common/inc/swref/pascal/gp100
      MANUAL_PATH        += $(BLSW)/../../drivers/common/inc/hwref/pascal/gp100
  endif

  ifneq (,$(findstring fecs_bl_gm10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x5C00
      MAX_CODE_SIZE   = 24K
      MAX_DATA_SIZE   = 4K
      FALCON_ARCH     = falcon5
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/maxwell
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/maxwell/gm107
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm107
  endif

  ifneq (,$(findstring gpccs_bl_gm10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x3400
      MAX_CODE_SIZE   = 16K
      MAX_DATA_SIZE   = 1K
      FALCON_ARCH     = falcon5
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/maxwell
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/maxwell/gm107
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm107
  endif

  ifneq (,$(findstring fecs_bl_gm20x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x5E00
      MAX_CODE_SIZE   = 24K
      MAX_DATA_SIZE   = 4K
      FALCON_ARCH     = falcon5
      LS_FALCON       = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/maxwell
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/maxwell/gm200
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm200
  endif

  ifneq (,$(findstring gpccs_bl_gm20x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x3400
      MAX_CODE_SIZE   = 16K
      MAX_DATA_SIZE   = 1K
      FALCON_ARCH     = falcon5
      LS_FALCON       = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/maxwell
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm200
  endif

  ifneq (,$(findstring gpccs_bl_gh10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x3400
      MAX_CODE_SIZE   = 20K
      MAX_DATA_SIZE   = 8K
      FALCON_ARCH     = falcon5
      LS_FALCON       = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/hopper
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/hwref/hopper/gh100
  endif

  ifneq (,$(findstring fecs_bl_gv10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x7E00
      MAX_CODE_SIZE   = 32K
      MAX_DATA_SIZE   = 8K
      FALCON_ARCH     = falcon5
      LS_FALCON       = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/volta
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/volta/gv100
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/volta/gv100
  endif

  ifneq (,$(findstring fecs_bl_gh10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE          = 0x8E00
      MAX_CODE_SIZE       = 40K
      MAX_DATA_SIZE       = 10K
      FALCON_ARCH         = falcon6
      PA_47_BIT_SUPPORTED = true
      LS_FALCON           = true
      RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/hopper
      MANUAL_PATH         = $(BLSW)/../../drivers/common/inc/swref/hopper/gh100
      MANUAL_PATH        += $(BLSW)/../../drivers/common/inc/hwref/hopper/gh100
  endif
  
  ifneq (,$(findstring gpccs_bl_ad10x,  $(BLCFG_PROFILE)))
     IMAGE_BASE      = 0x3400
     MAX_CODE_SIZE   = 20K
     MAX_DATA_SIZE   = 8K
     FALCON_ARCH     = falcon5
     LS_FALCON       = true
     RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/ada
     MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/hwref/ada/ad102
  endif

  ifneq (,$(findstring fecs_bl_ad10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE          = 0x8E00
      MAX_CODE_SIZE       = 40K
      MAX_DATA_SIZE       = 10K
      FALCON_ARCH         = falcon6
      LS_FALCON           = true
      PA_47_BIT_SUPPORTED = true
      RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/ada
      MANUAL_PATH         = $(BLSW)/../../drivers/common/inc/swref/ada/ad102
      MANUAL_PATH        += $(BLSW)/../../drivers/common/inc/hwref/ada/ad102
  endif

  # ToDo : set LS_FALCON = true for gpccs_bl_ad10b and fecs_bl_ad10b when emulation starts for AD10B
  ifneq (,$(findstring gpccs_bl_ad10b,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0x3400
      MAX_CODE_SIZE   = 24K
      MAX_DATA_SIZE   = 10K
      FALCON_ARCH     = falcon5
      LS_FALCON       = false
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/ada
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/hwref/ada/ad10b
  endif

  ifneq (,$(findstring fecs_bl_ad10b,  $(BLCFG_PROFILE)))
      IMAGE_BASE          = 0x8E00
      MAX_CODE_SIZE       = 44K
      MAX_DATA_SIZE       = 12K
      FALCON_ARCH         = falcon6
      PA_47_BIT_SUPPORTED = true
      LS_FALCON           = false
      RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/src/physical/gpu/gr/arch/ada
      MANUAL_PATH         = $(BLSW)/../../drivers/common/inc/swref/ada/ad10b
      MANUAL_PATH        += $(BLSW)/../../drivers/common/inc/hwref/ada/ad10b
  endif

  ifneq (,$(findstring sec2_bl_gm20x,  $(BLCFG_PROFILE)))
      IMAGE_BASE      = 0xFD00
      MAX_CODE_SIZE   = 64K
      MAX_DATA_SIZE   = 16K
      FALCON_ARCH     = falcon5
      HS_FALCON       = true
      VIRTUAL_DMA     = true
      RELEASE_PATH    = $(LW_SOURCE)/drivers/resman/kernel/inc/sec2/bin
      MANUAL_PATH     = $(BLSW)/../../drivers/common/inc/swref/maxwell/gm200
      MANUAL_PATH    += $(BLSW)/../../drivers/common/inc/hwref/maxwell/gm200
  endif

  ifneq (,$(findstring sec2_bl_gp10x,  $(BLCFG_PROFILE)))
      IMAGE_BASE          = 0xFD00
      MAX_CODE_SIZE       = 64K
      MAX_DATA_SIZE       = 16K
      FALCON_ARCH         = falcon6
      HS_FALCON           = true
      VIRTUAL_DMA         = true
      PA_47_BIT_SUPPORTED = true
      RELEASE_PATH        = $(LW_SOURCE)/drivers/resman/kernel/inc/sec2/bin
      MANUAL_PATH         = $(BLSW)/../../drivers/common/inc/swref/pascal/gp100
      MANUAL_PATH        += $(BLSW)/../../drivers/common/inc/hwref/pascal/gp100
  endif

  #
  # Generic profiles for loading RTOS on falcons.
  # This allows RTOS-based falcons to utilize the generic bootloader.
  #
  # This should only be called from a parent makefile, and as such
  # isn't included in BLCFG_PROFILES_ALL
  #
  # The stack and imem offsets and FALCON_ARCH must be set.
  # NO_RELEASE should be true, as the loader gets copied to the top of
  # IMEM (highest addresses) specified in the parent makefile.
  # The bootloader is copied along with the RTOS image as a "blob"
  # by the mkimg script.
  #
  # Also, CODE/DATA size is set to 8KB, as in the previous bootloader
  #
  ifneq (,$(findstring sec2_bl_rtos,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH or MANUAL_PATH not set)
      endif

      IMAGE_BASE      = $(IMEM_OFFS)
      LDFLAGS        += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS        += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE   = 8K
      MAX_DATA_SIZE   = 8K
      FALCON_ARCH     = $(FALCON_ARCH)
      HS_FALCON       = true
      CLEAN_IMEM      = true
      MANUAL_PATH     = $(MANUAL_PATH)

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  ifneq (,$(filter sec2_hs_bl_rtos,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif

      BLSRC_ORIGINAL            = hs_main.c hs_entry_checks.c
      BLSRC_PARSED              =
      LDSCRIPT_IN               = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                  = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE                = $(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE             = 64K
      MAX_DATA_SIZE             = 8K
      FALCON_ARCH               = $(FALCON_ARCH)
      HS_FALCON                 = true
      CLEAN_IMEM                = true
      MANUAL_PATH               = $(MANUAL_PATH)
      BOOT_FROM_HS_BUILD        = true
      IS_HS_ENCRYPTED           = true
      SIGGEN_CFG                = bl-siggen-ga10x.cfg
      SIGGEN_UCODE_ID           = $(SEC2_RTOS_HS_BL_ID)
      SIGGEN_FUSE_VER           = $(SEC2_RTOS_HS_BL_VER)
      SIGGEN_ENGINE_ID          = $(ENGINE_ID_SEC2)
      SIGGEN_CHIPS              = "ga102_rsa3k_0"
      NUM_SIG_PER_UCODE         = 2
      BOOT_FROM_HS_FALCON_SEC2  = true
      RELEASE_PATH              = $(RESMAN_ROOT)/kernel/inc/sec2/bin

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  ifneq (,$(findstring lwdec_bl_ga10x,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif

      BLSRC_ORIGINAL            = hs_main.c hs_entry_checks.c
      BLSRC_PARSED              =
      LDSCRIPT_IN               = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                  = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE                = $(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE             = 64K
      MAX_DATA_SIZE             = 16K
      FALCON_ARCH               = $(FALCON_ARCH)
      HS_FALCON                 = true
      CLEAN_IMEM                = true
      MANUAL_PATH               = $(MANUAL_PATH)
      CHIP_MANUAL_PATH          = $(CHIP_MANUAL_PATH)
      BOOT_FROM_HS_BUILD        = true
      BOOT_FROM_HS_FALCON_LWDEC = true
      IS_HS_ENCRYPTED           = true
      SIGGEN_CFG                = bl-siggen-ga10x.cfg
      SIGGEN_UCODE_ID           =  $(UCODE_ID_LWDEC_VIDEO_LEGACY_HS)
      SIGGEN_FUSE_VER           = 1
      SIGGEN_ENGINE_ID          = $(ENGINE_ID_LWDEC)
      SIGGEN_CHIPS              = "ga102_rsa3k_0"
      RELEASE_PATH              = $(RESMAN_ROOT)/kernel/inc/video/bin/bsp/v05

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif

  endif

  ifneq (,$(findstring lwdec_bl_gh100,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif

      BLSRC_ORIGINAL            = hs_main.c hs_entry_checks.c
      BLSRC_PARSED              =
      LDSCRIPT_IN               = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                  = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE                = $(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE             = 64K
      MAX_DATA_SIZE             = 16K
      FALCON_ARCH               = $(FALCON_ARCH)
      HS_FALCON                 = true
      CLEAN_IMEM                = true
      MANUAL_PATH               = $(MANUAL_PATH)
      CHIP_MANUAL_PATH          = $(CHIP_MANUAL_PATH)
      BOOT_FROM_HS_BUILD        = true
      BOOT_FROM_HS_FALCON_LWDEC = true
      IS_HS_ENCRYPTED           = true
      SIGGEN_CFG                = bl-siggen-gh100.cfg
      SIGGEN_UCODE_ID           =  $(UCODE_ID_LWDEC_VIDEO_LEGACY_HS)
      SIGGEN_FUSE_VER           = 1
      SIGGEN_ENGINE_ID          = $(ENGINE_ID_LWDEC)
      SIGGEN_CHIPS              = "ga102_rsa3k_0"
      RELEASE_PATH              = $(RESMAN_ROOT)/kernel/inc/video/bin/bsp/v04

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif

  endif

  ifneq (,$(findstring lwdec_bl_ad10x,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif

      BLSRC_ORIGINAL            = hs_main.c hs_entry_checks.c
      BLSRC_PARSED              =
      LDSCRIPT_IN               = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                  = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE                = $(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE             = 64K
      MAX_DATA_SIZE             = 16K
      FALCON_ARCH               = $(FALCON_ARCH)
      HS_FALCON                 = true
      CLEAN_IMEM                = true
      MANUAL_PATH               = $(MANUAL_PATH)
      CHIP_MANUAL_PATH          = $(CHIP_MANUAL_PATH)
      BOOT_FROM_HS_BUILD        = true
      BOOT_FROM_HS_FALCON_LWDEC = true
      IS_HS_ENCRYPTED           = true
      SIGGEN_CFG                = bl-siggen-ad10x.cfg
      SIGGEN_UCODE_ID           =  $(UCODE_ID_LWDEC_VIDEO_LEGACY_HS)
      SIGGEN_FUSE_VER           = 1
      SIGGEN_ENGINE_ID          = $(ENGINE_ID_LWDEC)
      SIGGEN_CHIPS              = "ga102_rsa3k_0"
      RELEASE_PATH              = $(RESMAN_ROOT)/kernel/inc/video/bin/bsp/v05

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif

  endif

  ifneq (,$(findstring pmu_bl_rtos,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH or MANUAL_PATH not set)
      endif

      IMAGE_BASE      = $(IMEM_OFFS)
      LDFLAGS        += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS        += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE   = 8K
      MAX_DATA_SIZE   = 8K
      FALCON_ARCH     = $(FALCON_ARCH)
      HS_FALCON       = false
      CLEAN_IMEM      = true
      MANUAL_PATH     = $(MANUAL_PATH)

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  ifneq (,$(findstring dpu_bl_rtos,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH or MANUAL_PATH not set)
      endif

      IMAGE_BASE      = $(IMEM_OFFS)
      LDFLAGS        += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS        += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE   = 8K
      MAX_DATA_SIZE   = 8K
      FALCON_ARCH     = $(FALCON_ARCH)
      MANUAL_PATH     = $(MANUAL_PATH)
ifeq ($(GSPLITE_RTOS),true)
      HS_FALCON       = true
else
      HS_FALCON       = false
endif
      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  ifneq (,$(findstring gsp_hs_bl_rtos,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif

      BLSRC_ORIGINAL           = hs_main.c hs_entry_checks.c
      BLSRC_PARSED             =
      LDSCRIPT_IN              = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                 = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE               = $(IMEM_OFFS)
      LDFLAGS                 += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                 += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE            = 64K
      MAX_DATA_SIZE            = 8K
      FALCON_ARCH              = $(FALCON_ARCH)
      HS_FALCON                = true
      CLEAN_IMEM               = true
      MANUAL_PATH              = $(MANUAL_PATH)
      BOOT_FROM_HS_BUILD       = true
      IS_HS_ENCRYPTED          = true
      SIGGEN_CFG               = bl-siggen-ga10x.cfg
      SIGGEN_UCODE_ID          = $(GSP_RTOS_HS_BL_UCODE_ID)
      SIGGEN_FUSE_VER          = $(GSP_RTOS_HS_BL_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_CHIPS             = "ga102_rsa3k_0"
      NUM_SIG_PER_UCODE        = 2
      BOOT_FROM_HS_FALCON_GSP  = true
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/dpu/bin

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  ifneq (,$(findstring gsp_hs_bl_rtos_ad10x,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif

      BLSRC_ORIGINAL           = hs_main.c hs_entry_checks.c
      BLSRC_PARSED             =
      LDSCRIPT_IN              = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                 = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE               = $(IMEM_OFFS)
      LDFLAGS                 += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                 += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE            = 64K
      MAX_DATA_SIZE            = 8K
      FALCON_ARCH              = $(FALCON_ARCH)
      HS_FALCON                = true
      CLEAN_IMEM               = true
      MANUAL_PATH              = $(MANUAL_PATH)
      BOOT_FROM_HS_BUILD       = true
      IS_HS_ENCRYPTED          = true
      SIGGEN_CFG               = bl-siggen-ad10x.cfg
      SIGGEN_UCODE_ID          = $(GSP_RTOS_HS_BL_UCODE_ID)
      SIGGEN_FUSE_VER          = $(GSP_RTOS_HS_BL_VER)
      SIGGEN_ENGINE_ID         = $(ENGINE_ID_GSP)
      SIGGEN_CHIPS             = "ga102_rsa3k_0" # TODO: Needs to be updated with ad10x keys
      NUM_SIG_PER_UCODE        = 1
      BOOT_FROM_HS_FALCON_GSP  = true
      RELEASE_PATH             = $(RESMAN_ROOT)/kernel/inc/dpu/bin

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  ifneq (,$(filter sec2_hs_bl_rtos_ad10x,  $(BLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(STACK_OFFS),$(FALCON_ARCH),$(MANUAL_PATH),$(CHIP_MANUAL_PATH)),)
          $(error IMEM_OFFS, STACK_OFFS, FALCON_ARCH, MANUAL_PATH or CHIP_MANUAL_PATH not set)
      endif
      BLSRC_ORIGINAL            = hs_main.c hs_entry_checks.c
      BLSRC_PARSED              =
      LDSCRIPT_IN               = $(BLSW)/build/link_script_hsbl.ld.in
      LDSCRIPT                  = $(OUTPUTDIR)/link_script.ld
      IMAGE_BASE                = $(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__imem_offs=$(IMEM_OFFS)
      LDFLAGS                  += -Wl,--defsym=__stack=$(STACK_OFFS)
      MAX_CODE_SIZE             = 64K
      MAX_DATA_SIZE             = 8K
      FALCON_ARCH               = $(FALCON_ARCH)
      HS_FALCON                 = true
      CLEAN_IMEM                = true
      MANUAL_PATH               = $(MANUAL_PATH)
      BOOT_FROM_HS_BUILD        = true
      IS_HS_ENCRYPTED           = true
      SIGGEN_CFG                = bl-siggen-ad10x.cfg
      SIGGEN_UCODE_ID           = $(SEC2_RTOS_HS_BL_ID)
      SIGGEN_FUSE_VER           = $(SEC2_RTOS_HS_BL_VER)
      SIGGEN_ENGINE_ID          = $(ENGINE_ID_SEC2)
      SIGGEN_CHIPS              = "ga102_rsa3k_0"
      NUM_SIG_PER_UCODE         = 1
      BOOT_FROM_HS_FALCON_SEC2  = true
      RELEASE_PATH              = $(RESMAN_ROOT)/kernel/inc/sec2/bin

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif

  LD_DEFINES     := -DIMAGE_BASE=$(IMAGE_BASE) -DMAX_CODE_SIZE=$(MAX_CODE_SIZE) -DMAX_DATA_SIZE=$(MAX_DATA_SIZE) -Ufalcon

endif

