APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(abspath ../..)
INSTALL_DIR := $(APP_ROOT)/bin
ENGINE ?=
CHIP ?=

# it must be built at this time
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
# Dir with SK installation, SK must be built already
SK_INSTALL_DIR:=$(APP_ROOT)/sdk-$(CHIP)-$(ENGINE).bin
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

LDSCRIPT_IN ?= $(APP_ROOT)/ldscript.ld.in
LDSCRIPT := $(BUILD_DIR)/ldscript.ld

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc
# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc

# In case we need lwmisc etc.
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
# Add manuals
INCLUDES += $(LW_ENGINE_MANUALS)

INCLUDES := $(addprefix -I,$(INCLUDES))
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)

# Link with liblwriscv
LIBS := -L$(LIBLWRISCV_DIR)/lib -llwriscv

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

BUILD_DIRS:=$(BUILD_DIR)

TARGET:=$(BUILD_DIR)/sample-$(CHIP)-$(ENGINE)
OBJS += partition_0/partition_0_startup.o
OBJS += partition_0/partition_0_main.o

BUILD_DIRS +=$(BUILD_DIR)/partition_0

OBJS += partition_1/partition_1_startup.o
OBJS += partition_1/partition_1_main.o

BUILD_DIRS +=$(BUILD_DIR)/partition_1

OBJS:=$(addprefix $(BUILD_DIR)/,$(OBJS))

MANIFEST_SRC := $(APP_ROOT)/manifest.c

ELF             := $(TARGET)
PARTITIONS_TEXT := $(ELF).text
PARTITIONS_DATA := $(ELF).data
OBJDUMP_SOURCE  := $(ELF).objdump.src

FMC_TEXT        := $(BUILD_DIR)/sepkern-$(CHIP)-$(ENGINE)-sample-basic.text
FMC_DATA        := $(BUILD_DIR)/sepkern-$(CHIP)-$(ENGINE)-sample-basic.data
MANIFEST        := $(BUILD_DIR)/sepkern-$(CHIP)-$(ENGINE)-sample-basic.manifest.o
MANIFEST_BIN    := $(BUILD_DIR)/sepkern-$(CHIP)-$(ENGINE)-sample-basic.manifest

CAT                  := /bin/cat

SEPKERN_IMAGE_TEXT   := $(SEPKERN_INSTALL_DIR)/g_sepkern_$(ENGINE)_$(SK_CHIP)_riscv_image.text
SEPKERN_IMAGE_DATA   := $(SEPKERN_INSTALL_DIR)/g_sepkern_$(ENGINE)_$(SK_CHIP)_riscv_image.data

FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin

#TODO: Maybe add SK objdump
BUILD_FILES := $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(ELF) $(OBJDUMP_SOURCE)

INTERMEDIATE_FILES :=

ifeq ($(ENGINE), fsp)
IS_FSP := -fsp
endif

.PHONY: install build sign

install: build
	install -D -t $(INSTALL_DIR) $(BUILD_FILES)

build: $(BUILD_FILES)

sign: $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) &: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)
	echo "Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip $(SIGGEN_CHIP) $(IS_FSP)
INTERMEDIATE_FILES+= $(MANIFEST_BIN).encrypt.bin $(MANIFEST_BIN).encrypt.bin.sig.bin

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST) $(MANIFEST_BIN)
INTERMEDIATE_FILES += $(MANIFEST_BIN)

#TODO: Don't hardcode SEPKERN IMEM/DMEM limit
$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC) $(BUILD_DIRS)
	$(eval APP_IMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_TEXT) ))
	$(eval APP_DMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_DATA) ))
	$(CC) $(CFLAGS) $(MANIFEST_SRC) $(INCLUDES) $(DEFINES) -DSEPKERN_IMEM_LIMIT=0x1000 -DSEPKERN_DMEM_LIMIT=0x1000 -include $(LIBLWRISCV_CONFIG_H) -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)

$(LDSCRIPT): $(LDSCRIPT_IN) $(BUILD_DIRS)
	$(CPP) -x c -P -o $@ $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $<
INTERMEDIATE_FILES += $(LDSCRIPT)

$(PARTITIONS_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .common_code -j .partition_0_code -j .partition_1_code $(ELF) $(PARTITIONS_TEXT)
INTERMEDIATE_FILES += $(PARTITIONS_TEXT)

$(FMC_TEXT): $(SEPKERN_IMAGE_TEXT) $(PARTITIONS_TEXT)
	$(CAT) $(SEPKERN_IMAGE_TEXT) $(PARTITIONS_TEXT) > $(FMC_TEXT)
INTERMEDIATE_FILES += $(FMC_TEXT)

$(PARTITIONS_DATA): $(ELF)
	$(OBJCOPY) -O binary -j .common_data -j .partition_0_data -j .partition_1_data $(ELF) $(PARTITIONS_DATA)
INTERMEDIATE_FILES += $(PARTITIONS_DATA)

$(FMC_DATA): $(SEPKERN_IMAGE_DATA) $(PARTITIONS_DATA)
	$(CAT) $(SEPKERN_IMAGE_DATA) $(PARTITIONS_DATA) > $(FMC_DATA)
INTERMEDIATE_FILES += $(FMC_DATA)

$(OBJDUMP_SOURCE): $(ELF)
	$(OBJDUMP) -S $(ELF) > $(OBJDUMP_SOURCE)
INTERMEDIATE_FILES += $(OBJDUMP_SOURCE)

$(ELF): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_DIR)/lib/liblwriscv.a  $(BUILD_DIRS)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
INTERMEDIATE_FILES += $(ELF)

$(BUILD_DIR)/%.o: $(APP_ROOT)/%.c $(LIBLWRISCV_CONFIG_H) $(BUILD_DIRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(APP_ROOT)/%.S $(LIBLWRISCV_CONFIG_H) $(BUILD_DIRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIRS)&:
	$(MKDIR) $(BUILD_DIRS)

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)
