#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT=${APP_ROOT}/../..

USE_PREBUILT_SDK ?=true

BUILD_DIR := $(APP_ROOT)/$(APP_NAME)-$(CHIP)-$(ENGINE).build
INSTALL_DIR := $(APP_ROOT)/$(APP_NAME)-$(CHIP)-$(ENGINE).bin

LIBLWRISCV_DIR := $(LIBLWRISCV_INSTALL_DIR)
LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk

###############################################################################
# Target inspecific paths: these are paths used by the build but not dependent
# upon the build.
###############################################################################
GSP_STRESS_TEST_SW   := $(APP_ROOT)

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
$(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Basic build elw
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

LW_ENGINE_MANUALS := $(LW_MANUALS_ROOT)/$(LWRISCV_IS_CHIP_FAMILY)/$(LWRISCV_IS_CHIP)

# Custom variables.. because we can
# TODO: The hard coded values must be changed to appropriate variables.
LWRISCV_APP_DMEM_LIMIT:= 0x10000
LWRISCV_APP_IMEM_LIMIT:= 0x10000

SIGGEN_CSG_PROD_SIGNING:=false
LDSCRIPT_IN ?= $(APP_ROOT)/ldscript.ld.in
LDSCRIPT := $(BUILD_DIR)/ldscript.ld

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

ifeq ($(wildcard $(LW_ENGINE_MANUALS)),)
$(error Set LW_ENGINE_MANUALS to where manuals are checked out.)
endif

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc
# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
# Add chip headers to include path
INCLUDES += $(LW_ENGINE_MANUALS)
#Add App specific header
INCLUDES += $(GSP_STRESS_TEST_SW)/inc

# MK TODO: this include is actually part of chips_a, is optional for builds with chips_a
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk/

INCLUDES := $(addprefix -I,$(INCLUDES))

DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)

# Link with liblwriscv
LIBS := -L$(LIBLWRISCV_DIR)/lib -llwriscv

CFLAGS += -Og -g -Werror -std=c11
CFLAGS += -mabi=$(LWRISCV_HAS_ABI) -march=$(LWRISCV_HAS_ARCH)
CFLAGS += -fomit-frame-pointer -fno-strict-aliasing -mcmodel=medany
CFLAGS += -nostartfiles -nostdlib -ffreestanding
CFLAGS += -Werror -Wall -Wstrict-prototypes -ffunction-sections -pipe -Wtype-limits  -Wsign-compare -Wint-in-bool-context
CFLAGS += -fdiagnostics-color=always
CFLAGS += $(INCLUDES)
# WARNING : this is imporant, it includes liblwriscv configuration file in every compiled file (that way we don't need to explicitly include it)
CFLAGS += -include $(LIBLWRISCV_CONFIG_H)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)

ASFLAGS += $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H)
ASFLAGS += -mabi=$(LWRISCV_HAS_ABI) -march=$(LWRISCV_HAS_ARCH)
ASFLAGS += $(DEFINES)

LDFLAGS += -Wl,-melf64lriscv
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--sort-common=descending
LDFLAGS += -Wl,--sort-section=alignment
LDFLAGS += $(LIBS) -nostdlib -nostartfiles -T$(LDSCRIPT)
LDFLAGS += $(DEFINES)

LD_INCLUDES += $(INCLUDES)

TARGET:=$(BUILD_DIR)/gsp-stress
OBJS += main.o
OBJS += src/stress.o
OBJS += src/lz4.o

# Add build dir prefix
#OBJS:=$(addprefix $(BUILD_DIR)/,$(OBJS))

MANIFEST_SRC := $(APP_ROOT)/manifest.c

ELF             := $(TARGET)
FMC_TEXT        := $(ELF).text
FMC_DATA        := $(ELF).data
OBJDUMP_SOURCE  := $(ELF).objdump.src
MANIFEST        := $(ELF).manifest.o
MANIFEST_BIN    := $(ELF).manifest

FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin

BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(ELF) $(OBJDUMP_SOURCE)

ifeq ($(SIGGEN_CSG_PROD_SIGNING),true)
OUTPUTDIR = objdir_riscv
CSG_TAR_DIR = $(OUTPUTDIR)
CSG_ARCHIVE ?= GSP_STRESS_TEST_T234_CSG_SIGNING_ARCHIVE
# TODO Reusing same profile as ACR.
PROFILE_LABEL = orin-iacr
endif

# TODO: this probably should be done other way
INTERMEDIATE_FILES :=

.PHONY: install build sign

install: build
	/usr/bin/install -D -t $(INSTALL_DIR) $(BUILD_FILES)

build: $(BUILD_FILES)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

sign: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)

ifeq ($(SIGGEN_CSG_PROD_SIGNING),true)
	$(ECHO) Creating the archive for CSG prod signing...
	$(ECHO) $(OUTPUTDIR)
	$(MKDIR) -p $(CSG_TAR_DIR)/$(CSG_ARCHIVE)
	$(MKDIR) -p $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/$(OUTPUTDIR)
	$(ECHO)  -manifest $(OUTPUTDIR)/$(MANIFEST_BIN) -code $(OUTPUTDIR)/$(FMC_TEXT) -data $(OUTPUTDIR)/$(FMC_DATA) -logLevel 4 -chip ga10b_rsa3k_0 -mode 0 -forceCsgFlow > $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/siggen_params.txt
	$(ECHO) $(PROFILE_LABEL) > $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/profile.txt
	cp $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA) $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/$(OUTPUTDIR)
	/bin/tar -cf $(CSG_TAR_DIR)/$(CSG_ARCHIVE).tar -C $(CSG_TAR_DIR) $(CSG_ARCHIVE)
	/bin/gzip $(CSG_TAR_DIR)/$(CSG_ARCHIVE).tar
else
	echo "Debug Signing files..."
	$(SIGGEN) -openssl $(P4ROOT)/sw/dev/gpu_drv/chips_a/uproc/utilities/openssl/openssl -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip ga10b_rsa3k_0
INTERMEDIATE_FILES+= $(MANIFEST_BIN).encrypt.bin $(MANIFEST_BIN).encrypt.bin.sig.bin
endif

ifeq ($(SIGGEN_CSG_PROD_SIGNING),true)
BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_BIN) $(ELF) $(OBJDUMP_SOURCE)
else
BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(ELF) $(OBJDUMP_SOURCE)
endif

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata $(MANIFEST) $(MANIFEST_BIN)
INTERMEDIATE_FILES += $(MANIFEST_BIN)

$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC) $(BUILD_DIR)
	$(eval APP_IMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_TEXT) ))
	$(eval APP_DMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_DATA) ))
	$(CC) $(MANIFEST_SRC) $(INCLUDES) $(DEFINES) $(CFLAGS) -include $(LIBLWRISCV_CONFIG_H) -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)

$(LDSCRIPT): $(LDSCRIPT_IN) $(BUILD_DIR)
	$(CPP) -x c -P -o $@ $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $<
INTERMEDIATE_FILES += $(LDSCRIPT)

$(FMC_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .code $(ELF) $(FMC_TEXT)
INTERMEDIATE_FILES += $(FMC_TEXT)

$(FMC_DATA): $(ELF)
	$(OBJCOPY) -O binary -j .data $(ELF) $(FMC_DATA)
INTERMEDIATE_FILES += $(FMC_DATA)

$(OBJDUMP_SOURCE): $(ELF)
	$(OBJDUMP) -S $(ELF) > $(OBJDUMP_SOURCE)
INTERMEDIATE_FILES += $(OBJDUMP_SOURCE)

$(ELF): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_DIR)/lib/liblwriscv.a $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
INTERMEDIATE_FILES += $(ELF)

$(BUILD_DIR)/%.o: %.c $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

clean:
	/bin/rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)
