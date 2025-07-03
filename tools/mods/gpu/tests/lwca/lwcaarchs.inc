#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# Setup default compiler and environment variables
# By default use linuxmfg mods instead of sim mods so that we use the newest version of GCC
BUILD_OS ?= linuxmfg
DRIVERS_DIR ?= $(MODS_DIR)/../../drivers

# Get toolchain dirs
# First save original arch
include $(DRIVERS_DIR)/common/build/mods/makedefs.inc
ORIG_BUILD_ARCH := $(BUILD_ARCH)
ORIG_BUILD_OS   := $(BUILD_OS)
override BUILD_OS := linuxmfg

# ARM toolchain
override BUILD_ARCH := aarch64
include $(DRIVERS_DIR)/common/build/mods/makedefs.inc
AARCH64_TOOLCHAIN_DIR := $(TOOLCHAIN_BIN_DIR)/..

# AMD64 toolchain
override BUILD_ARCH := amd64
include $(DRIVERS_DIR)/common/build/mods/makedefs.inc
AMD64_TOOLCHAIN_DIR := $(TOOLCHAIN_BIN_DIR)/..

# Reset toolchain
override BUILD_ARCH := $(ORIG_BUILD_ARCH)
override BUILD_OS   := $(ORIG_BUILD_OS)
include $(DRIVERS_DIR)/common/build/mods/makedefs.inc

# Force a default value for the artifacts directory, and export it to allow
# make_kernel_bin.py to be able to use it
LWDA_ARTIFACTS_DIR ?= $(MODS_OUTPUT_DIR)/mods_lwda

# Common exports
export COMMON_WS_DIR
export LWDA_ARTIFACTS_DIR
export PERL
export PYTHON
export UNIFDEF := $(BUILD_TOOLS_DIR)/unix/hosts/Linux-x86/unix-build/bin/unifdef

# Generate the lwConfig data: LWCFG_* makefile vars
# This is needed so the sm arch lists are populated correctly later
LWCFG_ROOT ?= $(DRIVERS_DIR)/common/build
LWCFG_OUTPUTDIR ?= $(LWDA_ARTIFACTS_DIR)
include $(LWCFG_ROOT)/lwconfig.lwmk

# Default options for make_kernel_bin.py
default_make_opts += --dvs-rel-compiler=$(LWDA_DVS_REL_COMPILER)
default_make_opts += --driver-root=$(DRIVERS_DIR)
default_make_opts += --lwconfig-path=$(LWCFG_OUTPUTDIR)

# Include the correct branch include file
ifeq "$(IS_BRANCH_MODULAR)" "true"
    branch_inc_file = $(MODS_DIR)/modular_branch.inc
else ifeq "$(IS_BRANCH_MODULAR)" "false"
    branch_inc_file = $(MODS_DIR)/non_modular_branch.inc
endif
ifeq "$(wildcard $(branch_inc_file))" ""
    ifneq "$(branch_inc_file)" ".inc"
        $(warning Release branch configuration file $(branch_inc_file) not found, using modular file)
    endif
    branch_inc_file = $(MODS_DIR)/modular_branch.inc
endif
$(info Using $(branch_inc_file))
include $(branch_inc_file)

ifeq "$(LWDA_KERNEL_COMPILE)" "true"
    define sm_arch_rule_template
        $(1)_lwbins: $$($(1)_kernels)
    endef

    add_kernels = $(foreach sm_arch,$(1), $($(sm_arch)_kernels))
    kernel_files := $(call add_kernels,$(LWDA_REL_ARCHS))
    ifeq "$(INCLUDE_LWDA_DEV_ARCHS)" "true"
        kernel_files += $(call add_kernels,$(LWDA_DEV_ARCHS))
    endif

    # Variables that can be lwstomized through the environment:
    #     DEBUG_MAKE  : Perform the make steps without actually doing anything
    #                   but echoing the commands that would be run
    #                  (default = false)
    #     SHOW_OUTPUT : Show the make commands
    #                   (default = false)
    DEBUG_MAKE ?= false
    SHOW_OUTPUT ?= false

    # Variables that can be lwstomized by makefiles using lwdaarchs.inc by
    # setting explicit values before including lwdaarchs.inc:
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
    LWBINS_DIR ?= $(LWRDIR)/../../traces
    make_dir  ?= $(LWRDIR)/..
    mach ?= 64
    make_opts ?=

    ifeq "$(DEBUG_MAKE)" "true"
        no_exec := echo
    endif

    Q := @
    ifeq "$(SHOW_OUTPUT)" "true"
        Q :=
    endif

    # Create rules that all lwbins use
    kernels : $(kernel_files)

    # Create rules for targeting specific architectures (i.e. "sm_60_lwbins" 
    # would make all lwbins for sm60)
    $(foreach sm_arch,$(LWDA_REL_ARCHS),$(eval $(call sm_arch_rule_template,$(sm_arch))))
    ifeq "$(INCLUDE_LWDA_DEV_ARCHS)" "true"
        $(foreach sm_arch,$(LWDA_DEV_ARCHS),$(eval $(call sm_arch_rule_template,$(sm_arch))))
    endif

    # Create implicit rule for dependencies on library headers
    # This simplifies the Makefiles elsewhere, at the expense of depending
    # on headers that might not be used
    %.merc : ../merc_common.h
	$(Q)touch $@
    %.lw : ../tools/tools.h ../tools/lwda_vectors.h ../tools/lwda_atomic.h ../tools/random.lw ../tools/newrandom.h
	$(Q)touch $@
    # random.lw is an exception since it is used like a header
    ../tools/random.lw : ;

    # Note, it is important that the installation rule be a static pattern rule
    # rather than an implicit rule.  Using an implicit rule will conflict with
    # the implicit rules used to actually build the lwbins

    LWRRENT_DIR_BASENAME := $(shell basename $(LWRDIR))
    export LWBIN_ARTIFACTS_DIR := $(LWDA_ARTIFACTS_DIR)/$(LWRRENT_DIR_BASENAME)
    $(kernel_files) : $(LWBINS_DIR)/%.lwbin: $(LWBIN_ARTIFACTS_DIR)/%.lwbin
	$(Q)echo Copying $(LWBIN_ARTIFACTS_DIR)/$(<F) to $@
	$(Q)mkdir -p $(LWBIN_ARTIFACTS_DIR)
	$(Q)$(copy) $< $@

    COMPUTE_CAP = $(*F)
    use_dev_comp = $(if $(findstring sm_$(1),$(LWDA_DEV_ARCHS)),"--force-dev-compiler",)
    use_arm_comp = $(if $(findstring sm_$(1),$(LWDA_ARM_ARCHS)),"--use-arm-compiler",)
    select_toolchain = $(if $(findstring sm_$(1),$(LWDA_ARM_ARCHS)),"--toolchain-path=$(AARCH64_TOOLCHAIN_DIR)","--toolchain-path=$(AMD64_TOOLCHAIN_DIR)")
    arch_make_opts = -a $(COMPUTE_CAP) --machine=$(mach) $(call use_dev_comp,$(COMPUTE_CAP)) $(call use_arm_comp,$(COMPUTE_CAP)) $(call select_toolchain,$(COMPUTE_CAP))

    # Commands uses by all lwbin makefiles
    #
    # Note : using make_kernel_bin requires the corresponding rule be an
    # implicit rule where the pattern match portion is the SM architecture
    # For instance:
    #
    #  foo%.lwbin: foo.lw
    #
    make_kernel_bin=$(no_exec) $(PYTHON3) $(make_dir)/make_kernel_bin.py $(default_make_opts) $(arch_make_opts) $(make_opts)
    copy=$(no_exec) cp
    move=$(no_exec) mv

    # Use clang C preprocessor to avoid preprocessing issues
    cpp ?= $(no_exec) $(TOOLCHAIN_BIN_DIR)/cpp
endif
