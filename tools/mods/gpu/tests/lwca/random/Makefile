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
REPORT_COVERAGE ?= true
MODS_DIR ?= ../../../..
LWBINS_DIR ?= ../../traces
sm_50_kernels := $(LWBINS_DIR)/crnd50.lwbin
sm_52_kernels := $(LWBINS_DIR)/crnd52.lwbin
sm_53_kernels := $(LWBINS_DIR)/crnd53.lwbin
sm_60_kernels := $(LWBINS_DIR)/crnd60.lwbin
sm_61_kernels := $(LWBINS_DIR)/crnd61.lwbin
sm_62_kernels := $(LWBINS_DIR)/crnd62.lwbin
sm_70_kernels := $(LWBINS_DIR)/crnd70.lwbin
sm_72_kernels := $(LWBINS_DIR)/crnd72.lwbin
sm_75_kernels := $(LWBINS_DIR)/crnd75.lwbin
sm_80_kernels := $(LWBINS_DIR)/crnd80.lwbin
sm_86_kernels := $(LWBINS_DIR)/crnd86.lwbin
sm_87_kernels := $(LWBINS_DIR)/crnd87.lwbin
sm_89_kernels := $(LWBINS_DIR)/crnd89.lwbin
sm_90_kernels := $(LWBINS_DIR)/crnd90.lwbin

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

ifeq (${INJECT_ERRORS},1)
    TEST = -DINJECT_ERRORS=1
else
    TEST = -DINJECT_ERRORS=0
endif

ifeq ($(shell uname),Linux)
    SYS = -D__LINUX__ -Xcompiler -Wall -Xlinker -s
else
    SYS =
endif

#Build the crnd??.exe

all: kernels

$(LWBIN_ARTIFACTS_DIR)/crnd%.lwbin : lwrkernel.lw lwrkernel.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l lwrkernel -s crnd -t LwdaRandom
	$(Q)if [ "$(REPORT_COVERAGE)" == "true" ]; then \
		cp $(LWBIN_ARTIFACTS_DIR)/lwrkernel.sm_*.sass $(LWRDIR)/ && \
		cp $(LWBIN_ARTIFACTS_DIR)/lwrkernel.sm_*.report $(LWRDIR)/ ; \
	fi;

$(LWBIN_ARTIFACTS_DIR)/crnd50.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt'

# For super verbose compile output use : --lwcc-options=--verbose --lwcc-options=--source-in-ptx
# For kernels that load other kernels you need the -dc command line argument.
$(LWBIN_ARTIFACTS_DIR)/crnd52.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd53.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd60.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd61.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd62.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd70.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd72.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd75.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd80.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd86.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd87.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd89.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'
$(LWBIN_ARTIFACTS_DIR)/crnd90.lwbin: make_opts := --lwcc-options='-dlink -llwdadevrt -dc'

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{exe,lwbin,linkinfo,ptx,pdb,sass,report}

.PHONY: clean
