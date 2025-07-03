# MK TODO: refactor it to build nicely or sth

APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(APP_ROOT)/../..
INSTALL_DIR := $(APP_ROOT)/bin
USE_PREBUILT_SDK ?=true

# it must be built at this time
ifeq ($(USE_PREBUILT_SDK),true)
    PREBUILT_DIR := $(LWRISCVX_SDK_ROOT)/prebuilt/basic-$(CHIP)-$(ENGINE)
    LIBLWRISCV_DIR := $(PREBUILT_DIR)
    LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
    LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk
else
    LIBLWRISCV_DIR := $(APP_ROOT)/sdk-$(CHIP)-$(ENGINE).bin
    LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
    LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk
endif

BUILD_DIR:=$(APP_ROOT)/app-$(CHIP)-$(ENGINE).bin

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
    $(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Preconfigures compilers
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

# Custom variables.. because we can
LWRISCV_APP_DMEM_LIMIT?=0x2000
LWRISCV_APP_IMEM_LIMIT?=0x2000

LDSCRIPT_IN ?= $(APP_ROOT)/ldscript.ld.in
LDSCRIPT := $(BUILD_DIR)/ldscript.ld

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

LIBS:=

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc

#Add libRTS to include path and lib path
ifeq ($(SAMPLE_NEEDS_LIBRTS),true)
INCLUDES += $(LIBLWRISCV_DIR)/inc/libRTS
LIBS     += -L$(LIBLWRISCV_DIR)/lib -lRTS
endif

# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc

# suppal TODO: this should not be needed but CCC "needs help" working properly for now
# Will talk to Juki to fix this...
ifeq ($(SAMPLE_NEEDS_LIBCCC),true)
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

INCLUDES := $(addprefix -I,$(INCLUDES))
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)

# Link with liblwriscv
LIBS += -L$(LIBLWRISCV_DIR)/lib -llwriscv

CFLAGS += -Og
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)

ASFLAGS += $(INCLUDES)
ASFLAGS += $(DEFINES)
ASFLAGS += $(EXTRA_ASFLAGS)

LDFLAGS += $(LIBS) -T$(LDSCRIPT)
LDFLAGS += $(DEFINES) $(EXTRA_LDFLAGS)

LD_INCLUDES += $(INCLUDES)

# so that when doing clean, we just clean all the profiles
ifeq ($(SAMPLE_PROFILE_FILE),)
SAMPLE_PROFILE_FILE := *
endif

TARGET:=$(BUILD_DIR)/sample-$(SAMPLE_PROFILE_FILE)
OBJS += main.o
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

BUILD_FILES := $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(FMC_TEXT) $(FMC_DATA) $(ELF) $(OBJDUMP_SOURCE)

ifeq ($(LWRISCV_PLATFORM_IS_CMOD),y)
    BUILD_FILES += $(TEXT_HEX) $(DATA_HEX)
endif

SIGGEN_ENGINE_OPTIONS =
ifneq ($(findstring fsp,$(ENGINE)),)
    SIGGEN_ENGINE_OPTIONS = -fsp
endif

# TODO: this probably should be done other way
INTERMEDIATE_FILES :=

.PHONY: install build sign

install: build
	install -D -t $(INSTALL_DIR) $(BUILD_FILES)

build: $(BUILD_FILES)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

sign: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)
	echo "Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip $(SIGGEN_CHIP) $(SIGGEN_ENGINE_OPTIONS)
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

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)
