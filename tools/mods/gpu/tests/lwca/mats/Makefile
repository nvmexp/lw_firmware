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
sm_50_kernels := $(LWBINS_DIR)/lwmats50.lwbin \
                 $(LWBINS_DIR)/ncmats50.lwbin \
                 $(LWBINS_DIR)/lwrowhammer50.lwbin \
                 $(LWBINS_DIR)/lwmext50.lwbin \
                 $(LWBINS_DIR)/bitlan50.lwbin
sm_52_kernels := $(LWBINS_DIR)/lwmats52.lwbin \
                 $(LWBINS_DIR)/ncmats52.lwbin \
                 $(LWBINS_DIR)/lwrowhammer52.lwbin \
                 $(LWBINS_DIR)/lwmext52.lwbin \
                 $(LWBINS_DIR)/bitlan52.lwbin
sm_53_kernels := $(LWBINS_DIR)/ncmats53.lwbin \
                 $(LWBINS_DIR)/lwrowhammer53.lwbin
sm_60_kernels := $(LWBINS_DIR)/lwmats60.lwbin \
                 $(LWBINS_DIR)/ncmats60.lwbin \
                 $(LWBINS_DIR)/lwrowhammer60.lwbin \
                 $(LWBINS_DIR)/lwmext60.lwbin \
                 $(LWBINS_DIR)/bitlan60.lwbin
sm_61_kernels := $(LWBINS_DIR)/ncmats61.lwbin
sm_62_kernels := $(LWBINS_DIR)/ncmats62.lwbin
sm_70_kernels := $(LWBINS_DIR)/lwmats70.lwbin \
                 $(LWBINS_DIR)/ncmats70.lwbin \
                 $(LWBINS_DIR)/lwrowhammer70.lwbin \
                 $(LWBINS_DIR)/lwmext70.lwbin \
                 $(LWBINS_DIR)/bitlan70.lwbin
sm_72_kernels := $(LWBINS_DIR)/ncmats72.lwbin
sm_75_kernels := $(LWBINS_DIR)/lwmats75.lwbin \
                 $(LWBINS_DIR)/lwmext75.lwbin \
                 $(LWBINS_DIR)/ncmats75.lwbin

sm_80_kernels := $(LWBINS_DIR)/lwmats80.lwbin \
                 $(LWBINS_DIR)/lwmext80.lwbin \
                 $(LWBINS_DIR)/ncmats80.lwbin \
		 $(LWBINS_DIR)/tiledmats80.lwbin

sm_86_kernels := $(LWBINS_DIR)/lwmats86.lwbin \
                 $(LWBINS_DIR)/lwmext86.lwbin \
                 $(LWBINS_DIR)/ncmats86.lwbin

sm_87_kernels := $(LWBINS_DIR)/ncmats87.lwbin

sm_89_kernels := $(LWBINS_DIR)/lwmats89.lwbin \
                 $(LWBINS_DIR)/lwmext89.lwbin \
                 $(LWBINS_DIR)/ncmats89.lwbin

sm_90_kernels := $(LWBINS_DIR)/lwmats90.lwbin \
                 $(LWBINS_DIR)/lwmext90.lwbin \
                 $(LWBINS_DIR)/ncmats90.lwbin \
		 $(LWBINS_DIR)/tiledmats90.lwbin

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

# Override default make options since all kernels in this file use similar options
make_opts := --lwcc-options='--ptxas-options=-dlcm=cg --ptxas-options=-Werror --ptxas-options=--warn-on-spills '

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

all: kernels

lwdamats: lwdamats.lw lwdamats.h
	$(Q)echo C++ $@
	$(Q)lwcc -o $@ -O3 ${SYS} ${TEST} $< lwdamats.lw

lwdamext: lwdamext.lw lwdamext.h
	$(Q)echo C++ $@
	$(Q)lwcc -o $@ -O3 ${SYS} ${TEST} $< lwdamext.lw


$(LWBIN_ARTIFACTS_DIR)/lwmats%.lwbin: lwdamats.lw lwdamats.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l lwdamats -s lwmats
$(LWBIN_ARTIFACTS_DIR)/lwmats70.lwbin: make_opts += --no-lwdisasm-internal
$(LWBIN_ARTIFACTS_DIR)/lwmats75.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/lwmats80.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/lwmats86.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/lwmats89.lwbin: make_opts += --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/lwmats90.lwbin: make_opts += --disasm-options=-ndf

$(LWBIN_ARTIFACTS_DIR)/ncmats%.lwbin: nwlwmats.lw newlwdamats.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l nwlwmats -s ncmats
$(LWBIN_ARTIFACTS_DIR)/ncmats53.lwbin: make_opts := --lwcc-options='--ptxas-options=-dlcm=cg -std=c++11'
$(LWBIN_ARTIFACTS_DIR)/ncmats62.lwbin: make_opts := --lwcc-options='--ptxas-options=-dlcm=cg -std=c++11'
$(LWBIN_ARTIFACTS_DIR)/ncmats72.lwbin: make_opts := --lwcc-options='--ptxas-options=-dlcm=cg -std=c++11'

$(LWBIN_ARTIFACTS_DIR)/lwrowhammer%.lwbin: lwrowhammer.lw lwrowhammer.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l lwrowhammer -s lwrowhammer


$(LWBIN_ARTIFACTS_DIR)/lwmext%.lwbin: lwdamext.lw lwdamext.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin)  -l lwdamext -s lwmext


$(LWBIN_ARTIFACTS_DIR)/bitlan%.lwbin: bitlane.lw bitlane.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l bitlane -s bitlan

$(LWBIN_ARTIFACTS_DIR)/tiledmats%.lwbin: tiledmats.lw tiledmats.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) --disasm-options=-ndf -l tiledmats -s tiledmats

clean:
	$(Q)echo Clean
	$(Q)if [ -d $(LWBIN_ARTIFACTS_DIR) ]; then \
		cd $(LWBIN_ARTIFACTS_DIR) && \
			rm -f lwdamats lwdamext *.lwbin *.ptx *.sass *.report *.linkinfo; \
	fi;

.PHONY: clean
