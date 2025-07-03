#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile aims to accomplish two things:
# 1. Define all available SELWREBLCFG_PROFILE profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in SELWREBLCFG_PROFILE. To pick-up the definitions, set SELWREBLCFG_PROFILE to the
# desired profile and source/include this file. When SELWREBLCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# SELWREBLCFG_PROFILES_ALL. No default profile is assumed.
#

##############################################################################
# Build a list of all possible selwrebl-config profiles
##############################################################################

SELWREBLCFG_PROFILES      :=
SELWREBLCFG_PROFILES_ALL  += selwrebl-soe-lr10
SELWREBLCFG_PROFILES_ALL  += selwrebl-soe-ls10
BOOTSW                    = $(LW_SOURCE)/uproc/soe/src/boot

##############################################################################
# Profile-specific settings
##############################################################################

ifdef SELWREBLCFG_PROFILE
  SELWREBLSRC_ORIGINAL := boot.c
  ifneq (,$(findstring selwrebl-soe-ls10,  $(SELWREBLCFG_PROFILE)))
	SELWREBLSRC_ORIGINAL += boot_selwre.c
	SELWREBLSRC_ORIGINAL += boot_util.c
	SELWREBLSRC_ORIGINAL += start.S
	SELWREBLSRC_ORIGINAL += platform/lr10.c
  else
	SELWREBLSRC_ORIGINAL += boot_selwre_old.c
	SELWREBLSRC_ORIGINAL += boot_util.c
	SELWREBLSRC_ORIGINAL += start.S
	SELWREBLSRC_ORIGINAL += platform/lr10_old.c
  endif

  LDSCRIPT_IN           = $(BOOTSW)/build/boot_link_script.ld.in
  LDSCRIPT              = $(OUTPUTDIR)/boot_link_script.ld
  HS_UCODE_ENCRYPTION   = false
  ENABLE_HS_SIGN        = false
  
  ifneq (,$(findstring selwrebl-soe-ls10,  $(SELWREBLCFG_PROFILE)))

      ifeq ($(and $(IMEM_OFFS),$(DMEM_OFFS)),)
          $(error IMEM_OFFS or DMEM_OFFS not set)
      endif

      IMAGE_BASE                      = $(IMEM_OFFS)
      DATA_BASE                       = $(DMEM_OFFS)
      MAX_DATASECTION_SIZE_IN_BYTES   = 7328
      MAX_CODE_SIZE_IN_BYTES          = 64K
      MAX_DATA_SIZE_IN_BYTES          = 64K
      SIG_GEN_DBG_CFG                 = soe-siggen-ls10-debug.cfg
      SIG_GEN_PROD_CFG                = soe-siggen-ls10-prod.cfg
      ENABLE_HS_SIGN                  = true
      HS_UCODE_ENCRYPTION             = true
      SIGGEN_CHIPS                    = "ls10"
      CHIP_MANUAL_SW_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/ls10

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif

   else

      ifeq ($(and $(IMEM_OFFS),$(DMEM_OFFS)),)
          $(error IMEM_OFFS or DMEM_OFFS not set)
      endif

      IMAGE_BASE                      = $(IMEM_OFFS)
      DATA_BASE                       = $(DMEM_OFFS)
      MAX_DATASECTION_SIZE_IN_BYTES   = 7328
      MAX_CODE_SIZE_IN_BYTES          = 64K
      MAX_DATA_SIZE_IN_BYTES          = 64K
      SIG_GEN_DBG_CFG                 = soe-siggen-lr10-debug.cfg
      SIG_GEN_PROD_CFG                = soe-siggen-lr10-prod.cfg
      ENABLE_HS_SIGN                  = true
      HS_UCODE_ENCRYPTION             = true
      SIGGEN_CHIPS                    = "lr10"
      CHIP_MANUAL_SW_PATH             = $(LW_SOURCE)/drivers/common/inc/swref/lwswitch/lr10

      ifneq ($(NO_RELEASE), true)
          $(error NO_RELEASE should be "true")
      endif
  endif


  LD_DEFINES     := -DIMAGE_BASE=$(IMAGE_BASE)
  LD_DEFINES     += -DDATA_BASE=$(DATA_BASE)
  LD_DEFINES     += -DMAX_CODE_SIZE_IN_BYTES=$(MAX_CODE_SIZE_IN_BYTES)
  LD_DEFINES     += -DMAX_DATA_SIZE_IN_BYTES=$(MAX_DATA_SIZE_IN_BYTES)
  LD_DEFINES     += -DMAX_DATASECTION_SIZE_IN_BYTES=$(MAX_DATASECTION_SIZE_IN_BYTES)
  LD_DEFINES     += -Ufalcon

endif
