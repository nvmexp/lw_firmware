#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017-2018,2021 by LWPU Corporation.  All rights reserved.  All
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
sm_70_kernels := $(LWBINS_DIR)/yield70.lwbin
sm_72_kernels := $(LWBINS_DIR)/yield72.lwbin
sm_75_kernels := $(LWBINS_DIR)/yield75.lwbin
sm_80_kernels := $(LWBINS_DIR)/yield80.lwbin
sm_86_kernels := $(LWBINS_DIR)/yield86.lwbin
sm_87_kernels := $(LWBINS_DIR)/yield87.lwbin
sm_89_kernels := $(LWBINS_DIR)/yield89.lwbin
sm_90_kernels := $(LWBINS_DIR)/yield90.lwbin

# Set LWDA_KERNEL_COMPILE to true for extra functionality from lwdaarchs.inc
LWDA_KERNEL_COMPILE := true
include ../lwdaarchs.inc

all: kernels
$(LWBIN_ARTIFACTS_DIR)/yield%.lwbin : yield.lw 
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l yield -s yield 
$(LWBIN_ARTIFACTS_DIR)/yield70.lwbin $(LWBIN_ARTIFACTS_DIR)/yield72.lwbin: make_opts += --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/yield80.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/yield86.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/yield87.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/yield89.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/yield90.lwbin: make_opts += --disasm-options=-ndf

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,sass,ptx,report}

.PHONY: clean
