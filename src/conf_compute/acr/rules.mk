# ACR is built as separate library and linked together
ifeq ($(LWRISCV_IS_CHIP_FAMILY),hopper)

ifneq ($(ACRLIB_PROJ_OVERRIDE),)
    ACRLIB_PROJ:=$(ACRLIB_PROJ_OVERRIDE)
else
    ACRLIB_PROJ:= $(PROJ)
endif

ACR_LIB_SRC_DIR := $(LW_SOURCE)/uproc/acr_riscv/src
ACR_LIB_OUT_DIR := $(ACR_LIB_SRC_DIR)/_out/gsp/rv_$(ACRLIB_PROJ)/
ACR_LIB_OUT_LIB := $(ACR_LIB_OUT_DIR)/g_acruc_rv_$(ACRLIB_PROJ).a
# Add library to targets
ACR_LIB_TARGET := $(INSTALL_DIR)/libpart_acr_extra.a


# MK TODO: this is bad hack, rethink
EXTRA_OBJECTS := $(ACR_LIB_TARGET)

# Add dependencies to build acr.a
$(ACR_LIB_TARGET): install.acrlib

# MK TODO: fix CP
install.acrlib: build.acrlib $(INSTALL_DIR)
	/bin/cp $(ACR_LIB_OUT_LIB) $(ACR_LIB_TARGET)

build.acrlib:
	$(MAKE) -C $(ACR_LIB_SRC_DIR) ACRCFG_PROFILE=acr_rv_gsp-$(ACRLIB_PROJ) -f makefile.lwmk build

clean.acrlib:
	$(MAKE) -C $(ACR_LIB_SRC_DIR) ACRCFG_PROFILE=acr_rv_gsp-$(ACRLIB_PROJ) -f makefile.lwmk clobber

clean: clean.acrlib

.PHONY: build.acrlib clean.acrlib install.acrlib

endif
