# MK TODO: refactor it to build nicely or sth

APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(APP_ROOT)/../../../../../../../lwriscv/release/lwriscv-sdk-3.0_dev/
RELEASE_DIR := $(APP_ROOT)/../../release/bin
SW_ROOT ?= ../../../..
USE_PREBUILT_SDK ?=true
CSG_PROD_SIGNING ?=false
AUTOMATE_CSG_SUBMIT ?= false
INSTALL_DIR ?= $(APP_ROOT)/../_out/$(SIGN_PROFILE)

VPATH := ../utils

# it must be built at this time
ifeq ($(USE_PREBUILT_SDK),true)
    PREBUILT_DIR := $(LWRISCVX_SDK_ROOT)/prebuilt/basic-$(CHIP)-$(ENGINE)
    LIBLWRISCV_DIR := $(PREBUILT_DIR)
    LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
    LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk
else
    LIBLWRISCV_DIR := $(INSTALL_DIR)/sdk-$(CHIP)-$(ENGINE).bin
    LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
    LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk
endif

BUILD_DIR:=$(INSTALL_DIR)/skate-$(CHIP)-$(ENGINE).bin
CSG_REQUEST_PACKAGE     ?= $(BUILD_DIR)/$(SKATE_PROFILE_FILE)_CSG_REQ.tgz
CSG_RESULT_PACKAGE      ?= $(BUILD_DIR)/$(SKATE_PROFILE_FILE)_CSG_RES.zip
CSG_POST_SIGN_SCRIPT    ?= $(BUILD_DIR)/csg_post_sign.sh

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
    $(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Preconfigures compilers
include $(APP_ROOT)/../../build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(APP_ROOT)/../../build/build_flags.mk

# Custom variables.. because we can
LWRISCV_APP_DMEM_LIMIT?=0x2000
LWRISCV_APP_IMEM_LIMIT?=0x2000

LDSCRIPT_IN ?= $(APP_ROOT)/../../build/ldscript.ld.in
LDSCRIPT := $(BUILD_DIR)/ldscript.ld
TAR      ?= PATH=/bin/:$$PATH /bin/tar
CSG_AUTOMATION_SCRIPT ?= $(P4ROOT)/sw/pvt/dknobloch/automation/fwsSubmitPeregrine.py
APP_P4_CL = $(word 2, $(shell $(PERL) -e 'chdir(qw(..)); delete $$ELW{PWD}; \
                  print `$(P4) changes -m1 "$(SW_ROOT)/..."`;'))

# MMINTS-TODO: this python will only work on LSF! Change this once automation supports python3.3, which is the latest version in //sw/tools/linux/python/
PYTHON3         ?= PATH=/usr/bin:$$PATH /home/utils/Python/3.9/3.9.5-20210517/bin/python3

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

LIBS:=

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc

# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc

INCLUDES += $(APP_ROOT)
INCLUDES += $(APP_ROOT)/../utils

# suppal TODO: this should not be needed but CCC "needs help" working properly for now
# Will talk to Juki to fix this...
ifeq ($(SKATE_NEEDS_LIBCCC),true)
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC/hwref
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC/common
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC/common_crypto
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC/common_crypto/include
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC/common_crypto_private
    INCLUDES += $(LIBLWRISCV_DIR)/inc/libCCC/common_crypto_private/lwpka

    ifeq ($(strip $(LIBCCC_SE_CONFIG)),FSP_SE_ONLY)
        DEFINES  += -DLIBCCC_FSP_SE_ENABLED
    else ifeq ($(strip $(LIBCCC_SE_CONFIG)),SECHUB_SE_ONLY)
        DEFINES  += -DLIBCCC_SECHUB_SE_ENABLED
    else ifeq ($(strip $(LIBCCC_SE_CONFIG)),SELITE_SE_ONLY)
        DEFINES  += -DLIBCCC_SELITE_SE_ENABLED
    else ifeq ($(strip $(LIBCCC_SE_CONFIG)),SECHUB_AND_SELITE_SE)
        DEFINES  += -DLIBCCC_SECHUB_SE_ENABLED -DLIBCCC_SELITE_SE_ENABLED
    else
        $(error LibCCC profile support pending!)
    endif
    DEFINES  += -DUPROC_ARCH_RISCV

    LIBS     += -L$(LIBLWRISCV_DIR)/lib -lCCC
endif

# MK TODO: this include is actually part of chips_a, is optional for builds with chips_a
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk

# Add manuals
INCLUDES += $(LW_ENGINE_MANUALS)

# Include header path
INCLUDES +=  $(APP_ROOT)/../../inc

INCLUDES := $(addprefix -I,$(INCLUDES))
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)

# Link with liblwriscv
LIBS += -L$(LIBLWRISCV_DIR)/lib -llwriscv

CFLAGS += -Og
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)
CFLAGS += $(APP_ROOT/../utils/)

ASFLAGS += $(INCLUDES)
ASFLAGS += $(DEFINES)
ASFLAGS += $(EXTRA_ASFLAGS)

LDFLAGS += $(LIBS) -T$(LDSCRIPT)
LDFLAGS += $(DEFINES) $(EXTRA_LDFLAGS)

LD_INCLUDES += $(INCLUDES)

# so that when doing clean, we just clean all the profiles
ifeq ($(SKATE_PROFILE_FILE),)
SKATE_PROFILE_FILE := *
endif

TARGET:=$(BUILD_DIR)/skate_$(CHIP)_$(ENGINE)
OBJS += main.o riscv_scp_crypt.o
# Add extra source files (if needed)
-include $(APP_ROOT)/sources.mk

# Add build dir prefix
OBJS:=$(addprefix $(BUILD_DIR)/,$(OBJS))

MANIFEST_SRC := $(APP_ROOT)/manifest.c

ELF             := $(TARGET)
FMC_TEXT        := $(ELF).text
FMC_DATA        := $(ELF).data
OBJDUMP_SOURCE  := $(ELF).objdump.src
MANIFEST        := $(ELF).manifest.o
MANIFEST_BIN    := $(ELF).manifest
ifeq ($(LWRISCV_PLATFORM_IS_CMOD),y)
    ELF_HEX         := $(ELF).hex
    TEXT_HEX        := $(ELF_HEX).text
    DATA_HEX        := $(ELF_HEX).data
endif

FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin
FINAL_MANIFEST_PROD := $(MANIFEST_BIN)_prd.encrypt.bin.out.bin
FINAL_FMC_TEXT_PROD := $(FMC_TEXT)_prd.encrypt.bin
FINAL_FMC_DATA_PROD := $(FMC_DATA)_prd.encrypt.bin

CSG_RELEASE_FILES := $(FINAL_FMC_TEXT_PROD) $(FINAL_FMC_DATA_PROD) $(FINAL_MANIFEST_PROD)
PDI_PATH = $(APP_ROOT)/pdi.txt

BUILD_FILES := $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) 

ifeq ($(LWRISCV_PLATFORM_IS_CMOD),y)
    BUILD_FILES += $(TEXT_HEX) $(DATA_HEX)
endif

# TODO: this probably should be done other way
INTERMEDIATE_FILES :=

.PHONY: install build sign

install: build sign
ifeq ("$(CSG_PROD_SIGNING)", "false")
	install -D -t $(RELEASE_DIR) $(BUILD_FILES)
endif

build: $(BUILD_FILES)

CSG_FILES := $(BUILD_DIR)/profile.txt
CSG_FILES += $(BUILD_DIR)/siggen_params.txt
CSG_FILES += $(BUILD_DIR)/pdi.txt
CSG_FILES += $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)

CSG_AUTOMATION_ARGS := --elw prod --description PMU --changelist $(APP_P4_CL)
CSG_AUTOMATION_ARGS += --package $(CSG_REQUEST_PACKAGE) --waitForRes $(CSG_RESULT_PACKAGE)

#ifeq ("$(CSG_PROD_SIGNING)", "false")
$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

sign: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA) $(RELEASE_DIR) $(OBJDUMP_SOURCE)
ifeq ("$(CSG_PROD_SIGNING)", "true")
	echo "Preparing tgz for Prod Signing..."
	$(CP) $(PDI_PATH) $(BUILD_DIR)/pdi.txt
	$(ECHO) "-manifest $(notdir $(MANIFEST_BIN)) -code $(notdir $(FMC_TEXT)) -data $(notdir $(FMC_DATA)) -chip $(SIGGEN_CHIP) -pdi $(notdir $(PDI_PATH)) -mode 1" > $(BUILD_DIR)/siggen_params.txt
	$(ECHO) $(FWS_SIGN_PROFILE) > $(BUILD_DIR)/profile.txt
	$(TAR) -C $(BUILD_DIR) -czf $(CSG_REQUEST_PACKAGE) $(notdir $(CSG_FILES))
	$(ECHO) '#!/bin/bash' > $(CSG_POST_SIGN_SCRIPT)
	$(ECHO) 'cd $(BUILD_DIR)' > $(CSG_POST_SIGN_SCRIPT)
	$(ECHO) "$(MOVE) $(addsuffix .encrypt.bin.out.bin,$(notdir $(MANIFEST_BIN))) $(FINAL_MANIFEST_PROD);" >> $(CSG_POST_SIGN_SCRIPT)
	$(ECHO) "$(MOVE) $(addsuffix .encrypt.bin,$(notdir $(FMC_TEXT))) $(FINAL_FMC_TEXT_PROD);" >> $(CSG_POST_SIGN_SCRIPT)
	$(ECHO) "$(MOVE) $(addsuffix .encrypt.bin,$(notdir $(FMC_DATA))) $(FINAL_FMC_DATA_PROD);" >> $(CSG_POST_SIGN_SCRIPT)
	$(ECHO) "$(INSTALL) -D -t $(RELEASE_DIR) $(CSG_RELEASE_FILES);" >> $(CSG_POST_SIGN_SCRIPT)   
	$(ECHO) "Saved CSG package as $(CSG_REQUEST_PACKAGE);"
	$(RM) -f $(BUILD_DIR)/pdi.txt
ifeq ("$(AUTOMATE_CSG_SUBMIT)", "true")
	$(ECHO) "$(CSG_AUTOMATION_ARGS)"
	$(PYTHON3) $(CSG_AUTOMATION_SCRIPT) $(CSG_AUTOMATION_ARGS)
	$(UNZIP) -d $(BUILD_DIR) -o $(CSG_RESULT_PACKAGE)
	$(BASH) $(CSG_POST_SIGN_SCRIPT)
endif
else
	echo "Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -pdi $(PDI_PATH) -chip $(SIGGEN_CHIP)
endif

INTERMEDIATE_FILES+= $(MANIFEST_BIN).encrypt.bin $(MANIFEST_BIN).encrypt.bin.sig.bin

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST) $(MANIFEST_BIN)
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

ifeq ($(LWRISCV_PLATFORM_IS_CMOD),y)
# Build a hex file to be used by CMOD.
#
$(ELF_HEX): $(ELF)
	$(call ELF2HEX,$<,$@)
INTERMEDIATE_FILES += $(ELF_HEX)

#
# Split $(ELF_HEX) at offset 0x80000. Everything before is code
# and everything after is data. Truncate both to 2048 lines (32KB)
# since this is the maximum that the CModel supports right now.
#
$(TEXT_HEX) $(DATA_HEX): $(ELF_HEX)
	(head -32768 > $(TEXT_HEX); cat > $(DATA_HEX)) < $(ELF_HEX)
	sed -i '2049,$$ d' $(TEXT_HEX)
	sed -i '2049,$$ d' $(DATA_HEX)
INTERMEDIATE_FILES += $(TEXT_HEX) $(DATA_HEX)
endif

$(ELF): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_DIR)/lib/liblwriscv.a $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
INTERMEDIATE_FILES += $(ELF)

$(BUILD_DIR)/%.o: %.c $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

$(RELEASE_DIR):
	$(MKDIR) -p $(RELEASE_DIR)

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)

