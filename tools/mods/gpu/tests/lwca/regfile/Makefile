#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
sm_90_kernels := $(LWBINS_DIR)/rfstress90.lwbin

# Set LWDA_KERNEL_COMPILE to true for extra functionality from lwdaarchs.inc
LWDA_KERNEL_COMPILE := true
include ../lwdaarchs.inc


all: kernels
$(LWBIN_ARTIFACTS_DIR)/rfstress%.lwbin : rfstress.h rfstress.postcpp.merc
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l rfstress -s rfstress --merlwry-flow 

rfstress.postcpp.merc: rfstress.merc ../tools/merc_common.h
	$(Q)echo Preproc $@
	$(Q)$(cpp)  < $< > $@

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,sass,mercbin,ptx,report}

.PHONY: clean
