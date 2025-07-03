APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(APP_ROOT)/../..
ENGINE ?= gsp
CHIP ?= ga10x

default: release
.PHONY: default

# it must be built at this time
BUILD_DIR := $(APP_ROOT)/_out/$(CHIP)_$(ENGINE)
BUILD_DIR_SDK := $(BUILD_DIR)/sdk
BUILD_DIR_SK := $(BUILD_DIR)/sk
BUILD_DIR_PART := $(BUILD_DIR)/partitions
INSTALL_DIR := $(BUILD_DIR)/bin
INSTALL_DIR_SDK  := $(INSTALL_DIR)/sdk
INSTALL_DIR_SK   := $(INSTALL_DIR)
INSTALL_DIR_PART := $(INSTALL_DIR)
RELEASE_DIR      := $(BUILD_DIR)/rel

LIBLWRISCV_INSTALL_DIR  :=$(INSTALL_DIR_SDK)
LIBLWRISCV_BUILD_DIR    :=$(BUILD_DIR_SDK)
LIBLWRISCV_PROFILE_FILE :=$(APP_ROOT)/profiles/$(ENGINE)_$(CHIP).mk
LIBLWRISCV_CONFIG_H     :=$(INSTALL_DIR_SDK)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK    :=$(INSTALL_DIR_SDK)/g_lwriscv_config.mk
LIBLWRISCV_LIB_DIR      :=$(INSTALL_DIR_SDK)/lib
LIBLWRISCV_LIB          :=$(LIBLWRISCV_LIB_DIR)/liblwriscv.a
LIBLWRISCV_ROOT         :=$(LWRISCVX_SDK_ROOT)/liblwriscv
LIBLWRISCV_OUT          :=$(BUILD_DIR_SDK)

# Preconfigures compilers etc for risc-v
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Final files prefixes (base)
INSTALL_NAME_PREFIX := g_sepkern_$(ENGINE)_$(CHIP)_mp_test_riscv_

# Stuff that will be combined/released in final image
PARTITIONS_ROOT    := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)partitions
PARTITIONS_ELF     := $(PARTITIONS_ROOT).elf
PARTITIONS_TEXT    := $(PARTITIONS_ROOT).text
PARTITIONS_DATA    := $(PARTITIONS_ROOT).data
PARTITIONS_OBJDUMP := $(PARTITIONS_ROOT).objdump.src
PARTITIONS_READELF := $(PARTITIONS_ROOT).readelf
FILES_TO_RELEASE += $(PARTITIONS_ELF) $(PARTITIONS_OBJDUMP) $(PARTITIONS_READELF)

MANIFEST_BIN    := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)manifest.bin
MANIFEST_HEX    := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)manifest.hex
FILES_TO_RELEASE += $(MANIFEST_HEX)

# base name for SK
SK_ROOT    := $(INSTALL_DIR_SK)/g_sepkern_$(ENGINE)_$(CHIP)_riscv
SK_ELF     := $(SK_ROOT).elf
SK_OBJDUMP := $(SK_ROOT).objdump
SK_NM      := $(SK_ROOT).nm
SK_READELF := $(SK_ROOT).readelf
# _image because reasons
SK_TEXT    := $(SK_ROOT)_image.text
SK_DATA    := $(SK_ROOT)_image.data

# Final SK images to release
SK_FINAL_ELF     := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)sk.elf
SK_FINAL_OBJDUMP := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)sk.objdump
SK_FINAL_NM      := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)sk.nm
SK_FINAL_READELF := $(INSTALL_DIR_PART)/$(INSTALL_NAME_PREFIX)sk.readelf
FILES_TO_RELEASE += $(SK_FINAL_ELF) $(SK_FINAL_OBJDUMP) $(SK_FINAL_NM) $(SK_FINAL_READELF)

FMC_PLAIN_TEXT     := $(INSTALL_DIR)/$(INSTALL_NAME_PREFIX)debug_image.text
FMC_PLAIN_DATA     := $(INSTALL_DIR)/$(INSTALL_NAME_PREFIX)debug_image.data
FMC_PLAIN_MANIFEST := $(MANIFEST_BIN)

# Final (signed/encrypted) image, must match what RM expects
FMC_FINAL_TEXT     := $(FMC_PLAIN_TEXT).encrypt.bin
FMC_FINAL_DATA     := $(FMC_PLAIN_DATA).encrypt.bin
FMC_FINAL_MANIFEST := $(FMC_PLAIN_MANIFEST)_dbg_enc.bin
FILES_TO_RELEASE   += $(FMC_FINAL_TEXT) $(FMC_FINAL_DATA) $(FMC_FINAL_MANIFEST)

## Exports
export APP_ROOT
export ENGINE
export CHIP
export LIBLWRISCV_CONFIG_H
export LIBLWRISCV_CONFIG_MK
export LIBLWRISCV_INSTALL_DIR
export LIBLWRISCV_BUILD_DIR
export LIBLWRISCV_LIB
export LIBLWRISCV_LIB_DIR
export LIBLWRISCV_OUT
export LIBLWRISCV_PROFILE_FILE
export LIBLWRISCV_ROOT
export LWRISCVX_SDK_ROOT

############################ RULES

build: build.sdk build.sk build.partitions

# we can do hammer rm, because we build in one place :) 
clean: # clean.sdk clean.sk clean.partitions
	[ ! -e $(BUILD_DIR) ] || rm -r $(BUILD_DIR)

install: install.sdk install.sk install.partitions

release: release.image
	@echo ==================================================================
	@echo Remember to rebuild RTOS image.
	@echo ==================================================================
	
.PHONY: build clean install release
## Final image(s) that are to be installed in chips_a

build.image: build.sdk build.sk build.partitions $(FMC_PLAIN_TEXT) $(FMC_PLAIN_DATA) $(FMC_PLAIN_MANIFEST)

$(FMC_PLAIN_TEXT): build.sk build.partitions
	cat $(SK_TEXT) $(PARTITIONS_TEXT) > $@
	
$(FMC_PLAIN_DATA): build.sk build.partitions
	cat $(SK_DATA) $(PARTITIONS_DATA) > $@
	
$(FMC_PLAIN_MANIFEST) : build.partitions

# Nothing to install, build generates files in install dir direcftly
install.image: install.sdk install.sk install.partitions build.image

$(FMC_FINAL_TEXT) $(FMC_FINAL_DATA) $(FMC_FINAL_MANIFEST) &: sign.image
sign.image: install.image
	$(SIGGEN) -manifest $(FMC_PLAIN_MANIFEST) -code $(FMC_PLAIN_TEXT) -data $(FMC_PLAIN_DATA) -chip ga102_rsa3k_0 -mode 0
	# MK TODO: hack because we truncated manifest name (in release)
	mv $(FMC_PLAIN_MANIFEST).encrypt.bin.out.bin $(FMC_FINAL_MANIFEST)

# Release script from chips_a
RELEASE_SCRIPT = $(LW_SOURCE)/uproc/build/scripts/release-imgs-if-changed.pl
RELEASE_PATH = $(LW_SOURCE)/uproc/lwriscv/sepkern/bin/$(ENGINE)/$(CHIP)_mp_test
COMMA := ,
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

# Always install.. for now
RELEASE_IMG_ARGS += --force
# RELEASE_IMG_ARGS += --output-prefix $(BUILD_PROJECT_NAME)
RELEASE_IMG_ARGS += --image $(FMC_FINAL_TEXT)
# RELEASE_IMG_ARGS += --p4            $(P4)
RELEASE_IMG_ARGS += --release-path $(RELEASE_PATH)
RELEASE_IMG_ARGS += --release-files $(subst $(SPACE),$(COMMA),$(FILES_TO_RELEASE))

release.image: sign.image $(RELEASE_DIR) $(FILES_TO_RELEASE)
	install -D -t $(RELEASE_DIR) $(FILES_TO_RELEASE)
	$(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS)
.PHONY: build.image install.image sign.image release.image

## Partitions, need separate makefile because it sources liblwriscv installation
install.partitions: build.partitions
	make -f Makefile.partitions install BUILD_DIR=$(BUILD_DIR_PART) INSTALL_DIR=$(INSTALL_DIR_PART) INSTALL_NAME_PREFIX=$(INSTALL_NAME_PREFIX)

build.partitions: install.sdk $(BUILD_DIR_PART) $(INSTALL_DIR_PART)
	make -f Makefile.partitions build BUILD_DIR=$(BUILD_DIR_PART) INSTALL_DIR=$(INSTALL_DIR_PART) INSTALL_NAME_PREFIX=$(INSTALL_NAME_PREFIX)
.PHONY: install.partitions build.partitions

## SK - Spark SK + policies
SEPKERN_ROOT:=$(LWRISCVX_SDK_ROOT)/monitor/sepkern
SEPKERN_INSTALL_DIR:=$(INSTALL_DIR_SK)
SEPKERN_POLICY_FILE_PATH:=$(APP_ROOT)/policies/$(ENGINE)
SEPKERN_POLICY_FILE_NAME:=policies.ads
SEPKERN_PARTITION_COUNT:=2

SK_BUILD_ARGS := ENGINE=$(ENGINE) CHIP=$(CHIP) INSTALL_PATH=$(INSTALL_DIR_SK) 
SK_BUILD_ARGS += BUILD_PATH=$(BUILD_DIR_SK) OUTPUT_PATH=$(BUILD_DIR_SK) 
SK_BUILD_ARGS += POLICY_PATH=$(SEPKERN_POLICY_FILE_PATH) POLICY_FILE_NAME=$(SEPKERN_POLICY_FILE_NAME)
SK_BUILD_ARGS += PARTITION_COUNT=$(SEPKERN_PARTITION_COUNT)

$(SK_FINAL_ELF) $(SK_FINAL_OBJDUMP) $(SK_FINAL_NM) $(SK_FINAL_READELF) &: install.sk

# We need to copy stuff to have unified naming
install.sk: build.sk
	cp $(SK_ELF) $(SK_FINAL_ELF)
	cp $(SK_NM) $(SK_FINAL_NM)
	cp $(SK_OBJDUMP) $(SK_FINAL_OBJDUMP)
	cp $(SK_READELF) $(SK_FINAL_READELF)

build.sk: $(BUILD_DIR_SK) $(INSTALL_DIR_SK)
	make -C $(SEPKERN_ROOT) $(SK_BUILD_ARGS) install
.PHONY: install.sk build.sk

## SDK - liblwriscv + stuff around

install.sdk: build.sdk

LIBLWRISCV_CONFIG_H LIBLWRISCV_CONFIG_MK LIBLWRISCV_LIB: build.sdk
build.sdk: $(INSTALL_DIR_SDK)
	make -C $(LIBLWRISCV_ROOT) install
.PHONY: install.sdk build.sdk

# Rules to create directories
DIRS_TO_CREATE:= $(filter INSTALL_DIR_%,$(.VARIABLES)) $(filter BUILD_DIR_%,$(.VARIABLES)) RELEASE_DIR
define add-dir =
$(info adding dir rule: $(1))
$(1):
	mkdir -p $(1)
endef
$(foreach c,$(sort $(DIRS_TO_CREATE)),$(eval $(call add-dir,$($(c)))))
