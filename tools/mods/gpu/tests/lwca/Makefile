#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

MODS_DIR ?= ../../..

subdirs =
subdirs += c2cbandwidth
subdirs += cbu
subdirs += cilp
subdirs += lwsurf
subdirs += ecc
subdirs += grf
subdirs += linpack
subdirs += mats
subdirs += lwlink
subdirs += lwlinkbandwidth
subdirs += peeratomic
subdirs += radixsort
subdirs += random
subdirs += regfile
subdirs += stress
subdirs += subctx
subdirs += tools
subdirs += ttu
subdirs += cache
subdirs += xbar
subdirs += yield
subdirs += l2noc

kerneldirs = $(subdirs:%=kernel-%)
cleandirs = $(subdirs:%=clean-%)

# If rule isn't specified, make "all"
all: $(subdirs)
kernels: $(kerneldirs)
clean: $(cleandirs)
clobber:
	-$(RM_RF) $(LWDA_ARTIFACTS_DIR)

# *DO NOT* set LWDA_KERNEL_COMPILE here otherwise it will create conflicting
# targets.  The reason this is included here is just for the ARCH definitions
include lwdaarchs.inc

# Template rule for creating targets used only to build specific SM architectures
define sm_arch_rule_template
    $(1)dirs = $$(subdirs:%=$(1)-%)
    $(1)_lwbins: $$($(1)dirs)
    $$($(1)dirs):
	$(MAKE) -C $$(@:$(1)-%=%) $(1)_lwbins
endef

# Create rules for targeting specific architectures (i.e. "sm_60_lwbins" would
# make all lwbins for sm60)
$(foreach sm_arch,$(LWDA_REL_ARCHS),$(eval $(call sm_arch_rule_template,$(sm_arch))))
ifeq "$(INCLUDE_LWDA_DEV_ARCHS)" "true"
    $(foreach sm_arch,$(LWDA_DEV_ARCHS),$(eval $(call sm_arch_rule_template,$(sm_arch))))
endif

$(subdirs):
	$(MAKE) -C $@ all
$(kerneldirs):
	$(MAKE) -C $(@:kernel-%=%) kernels
$(cleandirs):
	$(MAKE) -C $(@:clean-%=%) clean

# Create rules for fetching the various toolkits
# The toolkit that MODS runs is always built out of the current branch
LWDA_TESTS_DIR := $(MODS_DIR)/gpu/tests/lwca
amd64_release_toolkit:
	$(Q)$(PYTHON3) make_kernel_bin.py $(default_make_opts) --fetch-libs-toolkit --toolchain=$(AMD64_TOOLCHAIN_DIR)
	$(Q)$(MKDIR_P) $(LWDA_TESTS_DIR)/lwdart_libs/amd64/release
	$(Q)cp $(TOOLKIT_OUTPUT)/bin/x86_64_Linux_release/liblwdart_static.a $(LWDA_TESTS_DIR)/lwdart_libs/amd64/release
amd64_debug_toolkit:
	$(Q)$(PYTHON3) make_kernel_bin.py $(default_make_opts) --fetch-libs-toolkit --debug-toolkit --toolchain=$(AMD64_TOOLCHAIN_DIR)
	$(Q)$(MKDIR_P) $(LWDA_TESTS_DIR)/lwdart_libs/amd64/debug
	$(Q)cp $(TOOLKIT_OUTPUT)/bin/x86_64_Linux_debug/liblwdart_static.a $(LWDA_TESTS_DIR)/lwdart_libs/amd64/debug
aarch64_release_toolkit:
	$(Q)$(PYTHON3) make_kernel_bin.py $(default_make_opts) --fetch-libs-toolkit --use-arm-compiler --toolchain=$(AARCH64_TOOLCHAIN_DIR)
	$(Q)$(MKDIR_P) $(LWDA_TESTS_DIR)/lwdart_libs/aarch64
	$(Q)cp $(TOOLKIT_OUTPUT)/bin/aarch64_Linux_release/liblwdart_static.a $(LWDA_TESTS_DIR)/lwdart_libs/aarch64

ifneq "$(TOOLKIT_ARTIFACTS)" ""
    TOOLKIT_OUTPUT := $(TOOLKIT_ARTIFACTS)
else
    TOOLKIT_OUTPUT := $(LWDA_ARTIFACTS_DIR)/mods_lwdart
endif
lwdart_headers: amd64_release_toolkit
	$(Q)$(MKDIR_P) $(LWDA_TESTS_DIR)/lwdart_headers
	$(Q)cp -R $(TOOLKIT_OUTPUT)/bin/x86_64_Linux_release/include $(TOOLKIT_OUTPUT)/bin/x86_64_Linux_release/crt $(LWDA_TESTS_DIR)/lwdart_headers
lwdart_libs: amd64_release_toolkit amd64_debug_toolkit aarch64_release_toolkit

.PHONY: amd64_release_toolkit amd64_debug_toolkit aarch64_release_toolkit libs_toolkit libs_headers
.PHONY: all $(subdirs)
.PHONY: kernels $(kerneldirs)
.PHONY: clean $(cleandirs)
