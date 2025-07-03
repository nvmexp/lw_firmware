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
# 1. Define all available FBFALCONCFG profiles
# 2. Define all profile-specific make-vars (such as the project name, linker-
#    script, falcon architecture, etc ...).
#
# The second item can only be accomplished when a specific profile is defined
# in FBFALCONCFG_PROFILE. To pick-up the definitions, set FBFALCONCFG_PROFILE to the
# desired profile and source/include this file. When FBFALCONCFG_PROFILE is
# undefined no make-vars are assigned aside from the profile-name list in
# FBFALCONCFG_PROFILES. No default profile is assumed.
#

##############################################################################
# Build a list of all possible fbfalcon-config profiles
##############################################################################

FBFALCONCFG_PROFILES_ALL :=
FBFALCONCFG_PROFILES_ALL += fbfalcon-v0101
FBFALCONCFG_PROFILES_ALL += fbfalcon-tu10x-gddr
FBFALCONCFG_PROFILES_ALL += fbfalcon-tu10x-gddr-rom
FBFALCONCFG_PROFILES_ALL += fbfalcon-tu10x-automotive-gddr-rom
FBFALCONCFG_PROFILES_ALL += fbfalcon-ga10x_pafalcon-verif
# the ga10x-gddr binary includes the pafalcon binary and has to be built behind
# the pafalcon binary, do not change build order
FBFALCONCFG_PROFILES_ALL += fbfalcon-ga10x_pafalcon
FBFALCONCFG_PROFILES_ALL += fbfalcon-ga10x_pafalcon-verif
FBFALCONCFG_PROFILES_ALL += fbfalcon-ga10x-gddr
FBFALCONCFG_PROFILES_ALL += fbfalcon-ga10x-gddr-rom
FBFALCONCFG_PROFILES_ALL += fbfalcon-ad10x_pafalcon
FBFALCONCFG_PROFILES_ALL += fbfalcon-ad10x-gddr
FBFALCONCFG_PROFILES_ALL += fbfalcon-ad10x-gddr-rom
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh100-hbm
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh100-hbm-rom
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh100-ate-hbm
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh100-ate-hbm-rom
# the gh20x-gddr binary includes the pafalcon binary and has to be built behind
# the pafalcon binary, do not change build order
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh20x_pafalcon
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh20x_pafalcon-verif
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh20x-gddr
FBFALCONCFG_PROFILES_ALL += fbfalcon-gh20x-gddr-rom

FBFALCONSW = $(LW_SOURCE)/uproc/fbflcn/

##############################################################################
# Profile-specific settings
##############################################################################

ifdef FBFALCONCFG_PROFILE
  MANUAL_PATHS         =
  LS_FALCON            = true
  FLCNDBG_ENABLED      = true
  SIGGEN_CFG           =
  SIGGEN_CHIPS         =
  RELEASE_PATH         = $(RESMAN_ROOT)/kernel/inc/fbflcn/bin
  LS_UCODE_VERSION     = 0
  BOOTLOADER_INCLUDED  = 1
  UCODE_SEGMENT_ALIGNMENT_4096 = 0
  LWOS_VERSION         = dev
  RTOS_VERSION         = OpenRTOSv4.1.3
  SHA_ENGINE           = false
  SIGN_LICENSE         = CODESIGN_LS
  LS_UCODE_ID          = 0
  IS_LS_ENCRYPTED      = 0

  CFG_BIOS_TABLE_ALLOCATION     = 0x4000
  CFG_TRAINING_TABLE_ALLOCATION = 0x2000

  LINKER_SCRIPT_TEMPLATE = gt_link_script.ld
  LINKER_SCRIPT_TEMPLATE_DEFAULT = gt_link_script.ld  
  ADDITIONAL_LD_INCLUDES =
  
  LS_PKC_PROD_ENC_ENABLED = false
  
  GFW_ENCRYPTION = false
  GFW_SIGN_TYPE = NA

  ifneq (,$(findstring fbfalcon-v0101,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 01
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/volta/gv100
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/volta/gv100
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/volta/gv100
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0x3e00
      FBFLCN_LDR_STACK_OFFS = 0x3fc0
      UCODE_SEGMENT_ALIGNMENT_4096 = 1

      CFG_BIOS_TABLE_ALLOCATION = 0x1200
  endif

  ifneq (,$(findstring fbfalcon-tu10x-hbm,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 02
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/turing/tu102
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      UCODE_SEGMENT_ALIGNMENT_4096 = 1
  endif

  ifneq (,$(findstring fbfalcon-tu10x-hbm-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 02
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/turing/tu102
      FALCON_ARCH     = falcon6
      BOOTLOADER_INCLUDED = 0
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LDFLAGS += -u __ifdata_header
      UCODE_SEGMENT_ALIGNMENT_4096 = 1
  endif

  ifneq (,$(findstring fbfalcon-tu10x-gddr,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 02
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 1
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/turing/tu102
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      UCODE_SEGMENT_ALIGNMENT_4096 = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED              = 2
      SFK_CHIP                     = tu104
      SFK_CHIP_1                   = tu116
      LS_PKC_PROD_ENC_ENABLED      = true
  endif

  ifneq (,$(findstring fbfalcon-tu10x-gddr-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 02
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/turing/tu102
      FALCON_ARCH     = falcon6
      BOOTLOADER_INCLUDED = 0
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LDFLAGS += -u __ifdata_header
      UCODE_SEGMENT_ALIGNMENT_4096 = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED              = 0
      LS_PKC_PROD_ENC_ENABLED      = false
  endif

  ifneq (,$(findstring fbfalcon-tu10x-automotive-gddr-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 04
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/turing/tu102
      FALCON_ARCH     = falcon6
      BOOTLOADER_INCLUDED = 0
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LDFLAGS += -u __ifdata_header
      UCODE_SEGMENT_ALIGNMENT_4096 = 1
  endif

  ifneq (,$(findstring fbfalcon-ga10x-gddr,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 06
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 4
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ampere/ga102
      FALCON_ARCH = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_overlay_link_script.ld
      SHA_ENGINE = true
      CFG_BIOS_TABLE_ALLOCATION = 0x5000
      SIGN_LICENSE = CODESIGN_LS_PKC
      LS_UCODE_ID = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED = 2
      SFK_CHIP = ga102
      LS_PKC_PROD_ENC_ENABLED = true
  endif

  ifneq (,$(findstring fbfalcon-ga10x-gddr-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 08
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ampere/ga102
      FALCON_ARCH     = falcon6
      BOOTLOADER_INCLUDED = 0
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LDFLAGS += -u __ifdata_header
      CFG_BIOS_TABLE_ALLOCATION = 0x5000
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_boot_link_script.ld
      SHA_ENGINE = true
      SIGN_LICENSE = CODESIGN_LS
      IS_LS_ENCRYPTED = 0
      LS_PKC_PROD_ENC_ENABLED = false
  endif

  ifneq (,$(findstring fbfalcon-ad10x-gddr,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 0a
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 1
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ada/ad102
      FALCON_ARCH = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_overlay_link_script.ld
      SHA_ENGINE = true
      CFG_BIOS_TABLE_ALLOCATION = 0x5000
      SIGN_LICENSE = CODESIGN_LS_PKC
      LS_UCODE_ID = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED = 2
      SFK_CHIP = ad102
      LS_PKC_PROD_ENC_ENABLED = false
  endif

  ifneq (,$(findstring fbfalcon-ad10x-gddr-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 0b
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ampere/ga102
      FALCON_ARCH     = falcon6
      BOOTLOADER_INCLUDED = 0
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LDFLAGS += -u __ifdata_header
      CFG_BIOS_TABLE_ALLOCATION = 0x5000
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_boot_link_script.ld
      SHA_ENGINE = true
      SIGN_LICENSE = CODESIGN_LS
      IS_LS_ENCRYPTED = 0
      LS_PKC_PROD_ENC_ENABLED = false
  endif

  ifneq (,$(findstring fbfalcon-ga10x_pafalcon, $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 07
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 1
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ampere/ga102
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0x1e00
      FBFLCN_LDR_STACK_OFFS = 0x1fc0
      BOOTLOADER_INCLUDED = 0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_paflcn_link_script.ld
  endif

  ifneq (,$(findstring fbfalcon-ga10x_pafalcon-verif,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 07
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 1
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ampere/ga102
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0x1e00
      FBFLCN_LDR_STACK_OFFS = 0x1fc0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_paflcn_link_script.ld
  endif

  ifneq (,$(findstring fbfalcon-ad10x_pafalcon, $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 0c
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 1
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/ada/ad102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/ada/ad102
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/ada/ad102
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0x1e00
      FBFLCN_LDR_STACK_OFFS = 0x1fc0
      BOOTLOADER_INCLUDED = 0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_paflcn_link_script.ld
  endif

  ifneq (,$(findstring fbfalcon-gh100-hbm,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 09
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100/
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100/
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh100
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      SIGN_LICENSE    = CODESIGN_LS_PKC
      LS_UCODE_ID     = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED = 2
      SFK_CHIP = gh100
      LS_PKC_PROD_ENC_ENABLED = false 
  endif

  ifneq (,$(findstring fbfalcon-gh100-hbm-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 10
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100/
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100/
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh100
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      SIGN_LICENSE = CODESIGN_LS
      LS_UCODE_ID = 1
      BOOTLOADER_INCLUDED  = 0
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED = 0
      LS_PKC_PROD_ENC_ENABLED = false
      GFW_ENCRYPTION = true
      GFW_SIGN_TYPE = DEBUG
  endif
  
  
  # the ATE target is a self contained binary that can run stand-alone on the 
  # tester and does neet rm or memory based vbios/frts copies. It will be loaded
  # through jtag into imem/dmem

  ifneq (,$(findstring fbfalcon-gh100-ate-hbm,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 09
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100/
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100/
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh100
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LS_UCODE_ID     = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED = 0
      LS_FALCON       = false
      LINKER_SCRIPT_TEMPLATE = gt_gh100_ate_link_script.ld
      ADDITIONAL_LD_INCLUDES = gh100_ate_bios.ld gh100_ate_bios_references.ld
  endif
  
  ifneq (,$(findstring fbfalcon-gh100-ate-hbm-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 10
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100/
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100/
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh100
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      # signing is not required
      BOOTLOADER_INCLUDED  = 0
      LS_FALCON = false
      LINKER_SCRIPT_TEMPLATE = gt_gh100_ate_link_script.ld
      ADDITIONAL_LD_INCLUDES = gh100_ate_bios.ld gh100_ate_bios_references.ld
  endif

  #
  # Hopper GDDR Targets
  #

  ifneq (,$(findstring fbfalcon-gh20x-gddr,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 11
      IPMINORVER      = 01
      LS_UCODE_VERSION  = 1
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh202
      FALCON_ARCH = falcon6
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_overlay_link_script.ld
      SHA_ENGINE = true
      CFG_BIOS_TABLE_ALLOCATION = 0x5000
      SIGN_LICENSE = CODESIGN_LS_PKC
      LS_UCODE_ID = 1
      # This flag will be used for AES-CBC LS Encryption
      IS_LS_ENCRYPTED = 2
      SFK_CHIP = gh202
      LS_PKC_PROD_ENC_ENABLED = false
  endif

  ifneq (,$(findstring fbfalcon-gh20x_pafalcon, $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 12
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh202
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0x1e00
      FBFLCN_LDR_STACK_OFFS = 0x1fc0
      BOOTLOADER_INCLUDED = 0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_paflcn_link_script.ld
      LS_FALCON = false
  endif

  ifneq (,$(findstring fbfalcon-gh20x_pafalcon-verif,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 00
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh202
      FALCON_ARCH     = falcon6
      FBFLCN_LDR_OFFS = 0x1e00
      FBFLCN_LDR_STACK_OFFS = 0x1fc0
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_paflcn_link_script.ld
      LS_FALCON = false
  endif

  ifneq (,$(findstring fbfalcon-gh20x-gddr-rom,  $(FBFALCONCFG_PROFILE)))
      IPMAJORVER      = 13
      IPMINORVER      = 01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh202
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_01
      MANUAL_PATHS   += $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_01
      MANUAL_PATHS   += $(RESMAN_ROOT)/kernel/inc/hopper/gh202
      FALCON_ARCH     = falcon6
      BOOTLOADER_INCLUDED = 0
      FBFLCN_LDR_OFFS = 0xfe00
      FBFLCN_LDR_STACK_OFFS = 0xffc0
      LDFLAGS += -u __ifdata_header
      CFG_BIOS_TABLE_ALLOCATION = 0x5000
      LINKER_SCRIPT_TEMPLATE = gt_ga1x_boot_link_script.ld
      SHA_ENGINE = true
      SIGN_LICENSE = CODESIGN_LS
      IS_LS_ENCRYPTED = 0
      LS_UCODE_ID = 1
      SFK_CHIP = gh202
  endif

endif

