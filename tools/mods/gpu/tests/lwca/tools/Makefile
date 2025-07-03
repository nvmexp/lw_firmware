#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012-2018,2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# List kernels by SM and then include lwdaarchs.inc to include the kernels 
# supported in the current branch in the "kernels" rule
#
# Note that this list must be established before including lwdaarchs.inc
MODS_DIR ?= ../../../..
LWBINS_DIR ?= ../../traces
sm_50_kernels := $(LWBINS_DIR)/gpusync50.lwbin
sm_52_kernels := $(LWBINS_DIR)/gpusync52.lwbin
sm_53_kernels := $(LWBINS_DIR)/gpusync53.lwbin
sm_60_kernels := $(LWBINS_DIR)/gpusync60.lwbin
sm_61_kernels := $(LWBINS_DIR)/gpusync61.lwbin
sm_62_kernels := $(LWBINS_DIR)/gpusync62.lwbin
sm_72_kernels := $(LWBINS_DIR)/gpusync72.lwbin
sm_70_kernels := $(LWBINS_DIR)/gpusync70.lwbin \
                 $(LWBINS_DIR)/instrpulse70.lwbin
sm_75_kernels := $(LWBINS_DIR)/gpusync75.lwbin \
		 $(LWBINS_DIR)/instrpulse75.lwbin
sm_80_kernels := $(LWBINS_DIR)/gpusync80.lwbin \
		 $(LWBINS_DIR)/instrpulse80.lwbin
sm_86_kernels := $(LWBINS_DIR)/gpusync86.lwbin \
		 $(LWBINS_DIR)/instrpulse86.lwbin
sm_87_kernels := $(LWBINS_DIR)/gpusync87.lwbin \
		 $(LWBINS_DIR)/instrpulse87.lwbin
sm_89_kernels := $(LWBINS_DIR)/gpusync89.lwbin \
		 $(LWBINS_DIR)/instrpulse89.lwbin
sm_90_kernels := $(LWBINS_DIR)/gpusync90.lwbin \
		 $(LWBINS_DIR)/instrpulse90.lwbin

# Set LWDA_KERNEL_COMPILE to true for extra functionality from lwdaarchs.inc
LWDA_KERNEL_COMPILE := true

# Including lwdaarchs.inc creates default rules for building and copying lwbins
# as well as a default call for the make kernel bin command
#
# Variables used by lwdaarchs.inc that can be lwstomized by setting explicit 
# values for them before including lwdaarchs.inc:
#
#     LWBINS_DIR: Directory where lwbin files are copied to
#                 (default = ../../traces)
#     make_dir  : Directory where make_kernel_bin.py resides
#                 (default = ../)
#     mach      : Default machine type used in compiles (32 or 64), can be 
#                 overridden on a per-target basis
#                 (default = 64)
#     make_opts : Default make options used when calling make_kernel_bin.py
#                 (default = "")
include ../lwdaarchs.inc

all: kernels

gpusync%.lwbin: gpusync.lw gpusync.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l gpusync -s gpusync 

# Minimize register use (the compiler rounds up to the minimum
# This is redundant, but saveguards against changes that affect oclwpancy
instrpulse%.lwbin: make_opts := --lwcc-options="-maxrregcount 1"
instrpulse%.lwbin: instrpulse.lw instrpulse.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l instrpulse -s instrpulse 

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,ptx,sass,report}

.PHONY: clean
