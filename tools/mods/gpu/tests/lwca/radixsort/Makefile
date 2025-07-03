#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2014,2016-2018,2021 by LWPU Corporation.  All rights reserved.  All
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
sm_50_kernels := $(LWBINS_DIR)/radixsort50.lwbin
sm_52_kernels := $(LWBINS_DIR)/radixsort52.lwbin
sm_60_kernels := $(LWBINS_DIR)/radixsort60.lwbin
sm_70_kernels := $(LWBINS_DIR)/radixsort70.lwbin
sm_75_kernels := $(LWBINS_DIR)/radixsort75.lwbin
sm_80_kernels := $(LWBINS_DIR)/radixsort80.lwbin
sm_86_kernels := $(LWBINS_DIR)/radixsort86.lwbin
sm_89_kernels := $(LWBINS_DIR)/radixsort89.lwbin
sm_90_kernels := $(LWBINS_DIR)/radixsort90.lwbin

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

# Override default make options since all kernels in this file use the same
# options
make_opts := --lwcc-options='-I./,../../../../../../drivers/gpgpu/lwca/inc'

include ../lwdaarchs.inc

all: kernels

$(LWBIN_ARTIFACTS_DIR)/radixsort%.lwbin: b40c_radixsort.lw 
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l b40c_radixsort -s radixsort

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,linkinfo,ptx,report,sass,pdb}

.PHONY: clean
