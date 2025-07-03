TEST_CASE ?= example
TEST_CASE_ROOT ?= $(LWRDIR)/$(TEST_CASE)
LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_INSTALL_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_INSTALL_DIR)/g_lwriscv_config.mk

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
$(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Preconfigures compilers
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

include $(TEST_CASE_ROOT)/Makefile.inc
LDSCRIPT_IN ?= $(LWRDIR)/$(LD_FILE).in
LDSCRIPT := $(BUILD_DIR)/$(LD_FILE)

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_INSTALL_DIR)/inc
# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc

# In case we need lwmisc etc.
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
# Add manuals
INCLUDES += $(LW_ENGINE_MANUALS)
# Add testlib
INCLUDES += $(LIBTEST_DIR)

INCLUDES := $(addprefix -I,$(INCLUDES))
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(TEST_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(TEST_IMEM_LIMIT)

# Link with liblwriscv
LIBS := -L$(LIBLWRISCV_INSTALL_DIR)/lib -llwriscv

# Link with libtest
LIBS += -L$(LIBTEST_BUILD_DIR) -ltest

CFLAGS += -Og
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)

ASFLAGS += $(INCLUDES)
ASFLAGS += $(DEFINES)
ASFLAGS += $(EXTRA_ASFLAGS)

LDFLAGS += -Wl,--print-gc-sections
LDFLAGS += -Wl,--print-memory-usage
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/link.log
LDFLAGS += $(LIBS) -T$(LDSCRIPT)
LDFLAGS += $(DEFINES) $(EXTRA_LDFLAGS)

LD_INCLUDES += $(INCLUDES)

OBJS:=$(addprefix $(BUILD_DIR)/,$(OBJS))

ELF             := $(BUILD_DIR)/$(TEST_CASE).elf
PARTITIONS_TEXT := $(ELF).text
PARTITIONS_DATA := $(ELF).data
OBJDUMP_SOURCE  := $(ELF).objdump.src

FMC_TEXT        := $(BUILD_DIR)/$(CHIP)-$(ENGINE)-$(TEST_CASE).text
FMC_DATA        := $(BUILD_DIR)/$(CHIP)-$(ENGINE)-$(TEST_CASE).data
MANIFEST_BIN    := $(BUILD_DIR)/$(CHIP)-$(ENGINE)-$(TEST_CASE).manifest
MANIFEST_SRC    := $(MANIFEST_DIR)/$(CHIP)-$(ENGINE).manifest

CAT                  := /bin/cat

SEPKERN_IMAGE_TEXT   := $(SEPKERN_INSTALL_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv_image.text
SEPKERN_IMAGE_DATA   := $(SEPKERN_INSTALL_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv_image.data

FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin

#TODO: Maybe add SK objdump
BUILD_FILES := $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(ELF) $(OBJDUMP_SOURCE)

# if it's a cmodel target, generate the hex files needed for cmodel loading and testing
ifeq ($(LWRISCV_PLATFORM_IS_CMOD),y)
    include $(LWRDIR)/gen_cmodel_hex.mk
endif

ifeq ($(ENGINE), fsp)
IS_FSP := -fsp
endif
.PHONY: test
test:
	echo "ROOT:$(TEST_CASE_ROOT)"

.PHONY: install build sign

install: build
	install -D -t $(INSTALL_DIR) $(BUILD_FILES)

build: $(BUILD_FILES)

sign: $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) &: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)
	echo "Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip $(SIGGEN_CHIP) $(IS_FSP)

$(MANIFEST_BIN): $(MANIFEST_SRC)
	$(RSYNC) $(MANIFEST_SRC) $@

$(LDSCRIPT): $(LDSCRIPT_IN) $(BUILD_DIR)
	$(CPP) -x c -P -o $@ $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $<

$(PARTITIONS_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .common_code -j .partition_0_code -j .partition_1_code $(ELF) $(PARTITIONS_TEXT)

$(FMC_TEXT): $(SEPKERN_IMAGE_TEXT) $(PARTITIONS_TEXT)
	$(CAT) $(SEPKERN_IMAGE_TEXT) $(PARTITIONS_TEXT) > $(FMC_TEXT)

$(PARTITIONS_DATA): $(ELF)
	$(OBJCOPY) -O binary -j .common_data -j .partition_0_data -j .partition_1_data $(ELF) $(PARTITIONS_DATA)

$(FMC_DATA): $(SEPKERN_IMAGE_DATA) $(PARTITIONS_DATA)
	$(CAT) $(SEPKERN_IMAGE_DATA) $(PARTITIONS_DATA) > $(FMC_DATA)

$(OBJDUMP_SOURCE): $(ELF)
	$(OBJDUMP) -S $(ELF) > $(OBJDUMP_SOURCE)

$(ELF): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_INSTALL_DIR)/lib/liblwriscv.a  $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(TEST_CASE_ROOT)/%.c $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_CASE_ROOT)/%.S $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)&:
	$(MKDIR) $(BUILD_DIR)
