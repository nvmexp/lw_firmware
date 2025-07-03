# Makefile for the PLM/WPR test application.

###############################################################################
## Constants

# Fully-qualified name of the test profile to build.
TEST_PROFILE ?= plm+wpr-$(CHIP)-$(ENGINE)

# Root directory containing all application files.
TEST_ROOT ?= $(LWRDIR)

# Root directory containing all output files.
TEST_OUTPUT_ROOT ?= $(TEST_ROOT)/bin

# Directory to which output files for this profile should be installed.
TEST_OUTPUT_DIR := $(TEST_OUTPUT_ROOT)/$(TEST_PROFILE).out

# Base name for output files.
TEST_OUTPUT_NAME := $(TEST_OUTPUT_DIR)/$(TEST_PROFILE)

# Directory containing source files.
TEST_SOURCE_DIR ?= $(TEST_ROOT)/src

# Directory containing header files.
TEST_INCLUDE_DIR ?= $(TEST_ROOT)/inc

# Location of pre-built liblwriscv files.
LIBLWRISCV_INSTALL_DIR ?= $(TEST_OUTPUT_ROOT)/sdk-$(CHIP)-$(ENGINE).bin

# Location of LWRISCV SDK headers.
LWRISCVX_SDK_ROOT ?= $(TEST_ROOT)/../..

###############################################################################
## Input Files

BUILD_ELW_MK         := $(LWRISCVX_SDK_ROOT)/build/build_elw.mk
BUILD_FLAGS_MK       := $(LWRISCVX_SDK_ROOT)/build/build_flags.mk
LIBLWRISCV           := $(LIBLWRISCV_INSTALL_DIR)/lib/liblwriscv.a
LIBLWRISCV_CONFIG_H  := $(LIBLWRISCV_INSTALL_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_INSTALL_DIR)/g_lwriscv_config.mk
LDSCRIPT_IN          ?= $(TEST_ROOT)/ldscript.ld.in
MAKEFILE             := $(lastword $(MAKEFILE_LIST))
MANIFEST_SRC         ?= $(TEST_SOURCE_DIR)/manifest-$(CHIP).c
SOURCES_MK           ?= $(TEST_ROOT)/sources.mk

LIBS  := $(LIBLWRISCV)
MAKES := $(BUILD_ELW_MK) $(BUILD_FLAGS_MK) $(LIBLWRISCV_CONFIG_MK) $(MAKEFILE)\
         $(wildcard $(SOURCES_MK)) $(EXTRA_MAKES)

###############################################################################
## Sanity Checks

# Variables that should have been defined by the calling script.
ifeq ($(strip $(CHIP)),)
    $(error No chip selected. Are you using the correct build script?)
endif

ifeq ($(strip $(ENGINE)),)
    $(error No engine selected. Are you using the correct build script?)
endif

ifeq ($(strip $(SIGGEN_CHIP)),)
    $(error Missing siggen chip name. Are you using the correct build script?)
endif

ifeq ($(strip $(LWRISCV_APP_DMEM_START)),)
    $(error Missing DMEM origin. Are you using the correct build script?)
endif

ifeq ($(strip $(LWRISCV_APP_IMEM_START)),)
    $(error Missing IMEM origin. Are you using the correct build script?)
endif

ifeq ($(strip $(LWRISCV_APP_DMEM_LIMIT)),)
    $(error Missing DMEM size. Are you using the correct build script?)
endif

ifeq ($(strip $(LWRISCV_APP_IMEM_LIMIT)),)
    $(error Missing IMEM size. Are you using the correct build script?)
endif

ifeq ($(strip $(LWRISCV_APP_STACK_SIZE)),)
    $(error Missing stack size. Are you using the correct build script?)
endif

# Files that should have been built before this makefile was ilwoked (ignore on clean).
ifneq ($(MAKECMDGOALS),clean)
    ifeq ($(wildcard $(LIBLWRISCV)),)
        $(error Missing $(LIBLWRISCV). Have you installed liblwriscv correctly?)
    endif

    ifeq ($(wildcard $(LIBLWRISCV_CONFIG_H)),)
        $(error Missing $(LIBLWRISCV_CONFIG_H). Have you installed liblwriscv correctly?)
    endif

    ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
        $(error Missing $(LIBLWRISCV_CONFIG_MK). Have you installed liblwriscv correctly?)
    endif
endif

###############################################################################
## Includes

# Paths to various tools required by this makefile.
include $(BUILD_ELW_MK)

ifeq ($(wildcard $(CC)),)
    $(error Missing toolchain ($(CC)). Ensure that LW_TOOLS has been configured\
        to point to //sw/tools in your local workspace.)
endif

ifeq ($(wildcard $(SIGGEN)),)
    $(error Missing siggen ($(SIGGEN)). Where is your LWRISCV SDK?)
endif

# Includes need only when building (ignore on clean).
ifneq ($(MAKECMDGOALS),clean)
    # Target-specific liblwriscv configuration settings.
    include $(LIBLWRISCV_CONFIG_MK)

    # Common/default lwriscv build parameters.
    include $(BUILD_FLAGS_MK)

    ifeq ($(wildcard $(LW_ENGINE_MANUALS)),)
        $(error Cannot find engine manuals in $(LW_ENGINE_MANUALS). Ensure that \
            LW_MANUALS_ROOT has been configured to point to //sw/resman/manuals,\
            or the equivalent for your local workspace.)
    endif
endif

###############################################################################
## Object Files

# Main should always be present.
OBJS := main.o

# Additional files are specified in sources.mk if needed.
-include $(SOURCES_MK)

# Prepend output directory to each object file.
OBJS := $(addprefix $(TEST_OUTPUT_DIR)/,$(OBJS))

###############################################################################
## Intermediate Files

ELF_HEX        := $(TEST_OUTPUT_NAME).hex
LDSCRIPT       := $(TEST_OUTPUT_NAME).ld
MANIFEST_BIN   := $(TEST_OUTPUT_NAME).manifest
MANIFEST_DEP   := $(MANIFEST_BIN).d
MANIFEST_OBJ   := $(MANIFEST_BIN).o
MANIFEST_OUT   := $(MANIFEST_BIN).encrypt.bin
MANIFEST_SIG   := $(MANIFEST_BIN).encrypt.bin.sig.bin

BUILD_INTERMEDIATES := $(ELF_HEX) $(LDSCRIPT) $(MANIFEST_BIN) $(MANIFEST_DEP) \
                       $(MANIFEST_OBJ) $(MANIFEST_OUT) $(MANIFEST_SIG)

###############################################################################
## Dependency Files

# Every object file should have an associated dependency file of the same name.
DEPS := $(OBJS:.o=.d)

# Update rules to include header dependencies.
-include $(DEPS)
-include $(MANIFEST_DEP)

###############################################################################
## Output Files

ELF            := $(TEST_OUTPUT_NAME).elf
FMC_TEXT       := $(TEST_OUTPUT_NAME).text
FMC_DATA       := $(TEST_OUTPUT_NAME).data
FMC_TEXT_HEX   := $(ELF_HEX).text
FMC_DATA_HEX   := $(ELF_HEX).data
FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin
OBJDUMP_SOURCE := $(TEST_OUTPUT_NAME).objdump.src

BUILD_OUTPUTS := $(ELF) $(FMC_TEXT) $(FMC_DATA) $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA)\
                 $(FINAL_MANIFEST) $(OBJDUMP_SOURCE)

ifeq ($(LWRISCV_PLATFORM_IS_CMOD),y)
    BUILD_OUTPUTS += $(FMC_TEXT_HEX) $(FMC_DATA_HEX)
endif

###############################################################################
## Options and Flags

DEFINES += -DLWRISCV_APP_DMEM_START=$(LWRISCV_APP_DMEM_START)
DEFINES += -DLWRISCV_APP_IMEM_START=$(LWRISCV_APP_IMEM_START)
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)
DEFINES += -DLWRISCV_APP_STACK_SIZE=$(LWRISCV_APP_STACK_SIZE)

INCLUDES += $(TEST_INCLUDE_DIR)
INCLUDES += $(LIBLWRISCV_INSTALL_DIR)/inc
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
INCLUDES += $(LW_ENGINE_MANUALS)

INCLUDES := $(addprefix -I,$(INCLUDES))

CFLAGS += -Og
CFLAGS += -Wno-error=unused-function
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS += -L$(dir $(LIBLWRISCV)) -llwriscv
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += $(EXTRA_LDFLAGS)

ifneq ($(findstring fsp,$(ENGINE)),)
    SIGFLAGS := -fsp
else
    SIGFLAGS := 
endif

###############################################################################
## Object Rules

$(TEST_OUTPUT_DIR)/%.o: $(TEST_SOURCE_DIR)/%.c $(TEST_OUTPUT_DIR) $(LIBLWRISCV_CONFIG_H) $(MAKES)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(TEST_OUTPUT_DIR)/%.o: $(TEST_SOURCE_DIR)/%.S $(TEST_OUTPUT_DIR) $(LIBLWRISCV_CONFIG_H) $(MAKES)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

###############################################################################
## Intermediate Rules

$(ELF_HEX): $(TEST_OUTPUT_DIR) $(ELF) $(MAKES)
	$(call ELF2HEX,$(ELF),$(ELF_HEX))

$(LDSCRIPT): $(TEST_OUTPUT_DIR) $(LDSCRIPT_IN) $(LIBLWRISCV_CONFIG_H) $(MAKES)
	$(CPP) -x c -P $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $(LDSCRIPT_IN) -o $(LDSCRIPT)

$(MANIFEST_BIN): $(TEST_OUTPUT_DIR) $(MANIFEST_OBJ) $(MAKES)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST_OBJ) $(MANIFEST_BIN)

$(MANIFEST_OBJ): $(TEST_OUTPUT_DIR) $(MANIFEST_SRC) $(LIBLWRISCV_CONFIG_H) $(MAKES)
	$(CC) $(CFLAGS) -MMD -include $(LIBLWRISCV_CONFIG_H) -c $(MANIFEST_SRC) -o $(MANIFEST_OBJ)

###############################################################################
## Output Rules

$(ELF): $(TEST_OUTPUT_DIR) $(OBJS) $(LIBS) $(LDSCRIPT) $(MAKES)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)

$(FMC_TEXT): $(TEST_OUTPUT_DIR) $(ELF) $(MAKES)
	$(OBJCOPY) -O binary -j .code $(ELF) $(FMC_TEXT)

$(FMC_DATA): $(TEST_OUTPUT_DIR) $(ELF) $(MAKES)
	$(OBJCOPY) -O binary -j .data $(ELF) $(FMC_DATA)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

$(OBJDUMP_SOURCE): $(TEST_OUTPUT_DIR) $(ELF) $(MAKES)
	$(OBJDUMP) -S $(ELF) > $(OBJDUMP_SOURCE)

$(FMC_TEXT_HEX) $(FMC_DATA_HEX): $(TEST_OUTPUT_DIR) $(ELF_HEX) $(MAKES)
	(head -32768 > $(FMC_TEXT_HEX); cat > $(FMC_DATA_HEX)) < $(ELF_HEX)
	sed -i '2049,$$ d' $(FMC_TEXT_HEX)
	sed -i '2049,$$ d' $(FMC_DATA_HEX)

$(TEST_OUTPUT_DIR): $(MAKES)
	$(MKDIR) -p $(TEST_OUTPUT_DIR)

###############################################################################
## Top-Level Rules

.PHONY: build clean install sign
.DEFAULT_GOAL := install

build: $(BUILD_OUTPUTS)

clean:
	$(RM) -f $(BUILD_OUTPUTS) $(BUILD_INTERMEDIATES) $(OBJS) $(DEPS)
	-$(RM) -d $(TEST_OUTPUT_DIR)

install: build

sign: $(TEST_OUTPUT_DIR) $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_BIN)
	echo "Signing files..."
	$(SIGGEN) -code $(FMC_TEXT) -data $(FMC_DATA) -manifest $(MANIFEST_BIN) -chip $(SIGGEN_CHIP) $(SIGFLAGS)
