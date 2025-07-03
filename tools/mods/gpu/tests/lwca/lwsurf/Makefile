#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2018,2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
MODS_DIR ?= ../../../..
LWBINS_DIR ?= ../../traces


# List kernels by SM and then include lwdaarchs.inc to include the kernels 
# supported in the current branch in the "kernels" rule
sm_50_kernels := $(LWBINS_DIR)/lwsurf50.lwbin
sm_52_kernels := $(LWBINS_DIR)/lwsurf52.lwbin
sm_53_kernels := $(LWBINS_DIR)/lwsurf53.lwbin
sm_60_kernels := $(LWBINS_DIR)/lwsurf60.lwbin
sm_62_kernels := $(LWBINS_DIR)/lwsurf62.lwbin
sm_70_kernels := $(LWBINS_DIR)/lwsurf70.lwbin
sm_72_kernels := $(LWBINS_DIR)/lwsurf72.lwbin
sm_75_kernels := $(LWBINS_DIR)/lwsurf75.lwbin
sm_80_kernels := $(LWBINS_DIR)/lwsurf80.lwbin
sm_86_kernels := $(LWBINS_DIR)/lwsurf86.lwbin
sm_87_kernels := $(LWBINS_DIR)/lwsurf87.lwbin
sm_89_kernels := $(LWBINS_DIR)/lwsurf89.lwbin
sm_90_kernels := $(LWBINS_DIR)/lwsurf90.lwbin

# Set LWDA_KERNEL_COMPILE to true for extra functionality from lwdaarchs.inc
LWDA_KERNEL_COMPILE := true
include ../lwdaarchs.inc

all: kernels
$(LWBIN_ARTIFACTS_DIR)/lwsurf%.lwbin : lwsurf.lw 
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l lwsurf -s lwsurf 

$(LWBIN_ARTIFACTS_DIR)/lwsurf50.lwbin: make_opts := --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/lwsurf52.lwbin: make_opts := --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/lwsurf53.lwbin: make_opts := --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/lwsurf60.lwbin: make_opts := --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/lwsurf62.lwbin: make_opts := --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/lwsurf70.lwbin: make_opts := --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/lwsurf72.lwbin: make_opts := --disasm-options=-ndf

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,sass,report,ptx}

.PHONY: clean
