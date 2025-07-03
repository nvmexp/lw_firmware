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
sm_50_kernels := $(LWBINS_DIR)/cstrsf50.lwbin
sm_52_kernels := $(LWBINS_DIR)/cstrsf52.lwbin
sm_53_kernels := $(LWBINS_DIR)/cstrsf53.lwbin
sm_60_kernels := $(LWBINS_DIR)/cstrsf60.lwbin
sm_62_kernels := $(LWBINS_DIR)/cstrsf62.lwbin
sm_70_kernels := $(LWBINS_DIR)/cstrsf70.lwbin
sm_75_kernels := $(LWBINS_DIR)/cstrsf75.lwbin
sm_80_kernels := $(LWBINS_DIR)/cstrsf80.lwbin
sm_86_kernels := $(LWBINS_DIR)/cstrsf86.lwbin
sm_87_kernels := $(LWBINS_DIR)/cstrsf87.lwbin
sm_89_kernels := $(LWBINS_DIR)/cstrsf89.lwbin
sm_90_kernels := $(LWBINS_DIR)/cstrsf90.lwbin

# Set LWDA_KERNEL_COMPILE to true for extra functionality from lwdaarchs.inc
LWDA_KERNEL_COMPILE := true

# Including lwdaarchs.inc creates default rules for building and copying lwbins
# as well as a default call for the make kernel bin command
include ../lwdaarchs.inc

all: kernels

$(LWBIN_ARTIFACTS_DIR)/cstrsf%.lwbin: lwda_stress_fermi.lw lwda_stress_fermi.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l lwda_stress_fermi -s cstrsf
$(LWBIN_ARTIFACTS_DIR)/cstrsf70.lwbin: make_opts := --no-coverage-report

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,ptx,sass,report,linkinfo,pdb}

.PHONY: clean
