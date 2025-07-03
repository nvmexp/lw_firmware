#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
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
sm_60_kernels := $(LWBINS_DIR)/eccsm60.lwbin \
                 $(LWBINS_DIR)/eccshm60.lwbin \
                 $(LWBINS_DIR)/ecctex60.lwbin \
                 $(LWBINS_DIR)/eccl260.lwbin
sm_62_kernels := $(LWBINS_DIR)/eccsm62.lwbin \
                 $(LWBINS_DIR)/eccshm62.lwbin \
                 $(LWBINS_DIR)/ecctex62.lwbin
sm_70_kernels := $(LWBINS_DIR)/eccl270.lwbin \
                 $(LWBINS_DIR)/eccsm70.lwbin 
                 #$(LWBINS_DIR)/ecctex70.lwbin
                 # TODO - reenable the above when compilation is fixed
sm_75_kernels := $(LWBINS_DIR)/eccl275.lwbin
sm_80_kernels := $(LWBINS_DIR)/eccl280.lwbin


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

vpath %.lwasm	$(LWRDIR)
vpath %.ptx 	$(LWRDIR)
vpath %.lw		$(LWRDIR)
vpath %.h		$(LWRDIR)

.INTERMEDIATE: eccsm70.sm70.lwasm

all: kernels

%.sm60.ptx: %.ptx eccdefs.h
	$(Q)echo Preprocessing $< ...
	$(Q)$(cpp) -DSM_VER=60 < $< > $@

%.sm62.ptx: %.ptx eccdefs.h
	$(Q)echo Preprocessing $< ...
	$(Q)$(cpp) -DSM_VER=62 < $< > $@

%.sm70.ptx: %.ptx eccdefs.h
	$(Q)echo Preprocessing $< ...
	$(Q)$(cpp) -DSM_VER=70 < $< > $@

%.sm60.lwasm: %.lwasm eccdefs.h lwdaeccl2.h
	$(Q)echo Preprocessing $< ...
	$(Q)cat $< | $(cpp) -DSM_VER=60 | sed -n '/headerflags/,$$p' > $@

%.sm62.lwasm: %.lwasm eccdefs.h lwdaeccl2.h
	$(Q)echo Preprocessing $< ...
	$(Q)cat $< | $(cpp) -DSM_VER=62 | sed -n '/headerflags/,$$p' > $@

%.sm70.lwasm: %.lwasm eccdefs.h lwdaeccl2.h
	$(Q)echo Preprocessing $< ...
	$(Q)cat $< | $(cpp) -DSM_VER=70 | sed -n '/headerflags/,$$p' > $@

%.sm75.lwasm: %.lwasm eccdefs.h lwdaeccl2.h
	$(Q)echo Preprocessing $< ...
	$(Q)cat $< | $(cpp) -DSM_VER=75 | sed -n '/headerflags/,$$p' > $@

%.sm80.lwasm: %.lwasm eccdefs.h lwdaeccl2.h
	$(Q)echo Preprocessing $< ...
	$(Q)cat $< | $(cpp) -DSM_VER=80 | sed -n '/headerflags/,$$p' > $@

$(LWBIN_ARTIFACTS_DIR)/eccl2%.lwbin: lwdaeccl2.sm%.lwasm lwdaeccl2.lw lwdaeccl2.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l lwdaeccl2 -s eccl2 --multi-format

$(LWBIN_ARTIFACTS_DIR)/eccsm%.lwbin: eccsm20.sm%.lwasm
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l $(<:.lwasm=) -s eccsm

$(LWBIN_ARTIFACTS_DIR)/eccsm70.lwbin: $(LWBIN_ARTIFACTS_DIR)/eccsm%.lwbin: eccsm70.sm%.lwasm
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s eccsm

$(LWBIN_ARTIFACTS_DIR)/eccshm%.lwbin: eccshm60.sm%.ptx
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l $(<:.ptx=) -s eccshm --lwcc-options='-O0'

$(LWBIN_ARTIFACTS_DIR)/ecctex%.lwbin: ecctex60.sm%.lwasm
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l $(<:.lwasm=) -s ecctex

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{ptx,lwasm,lwbin,sass,report} 

.PHONY: clean
