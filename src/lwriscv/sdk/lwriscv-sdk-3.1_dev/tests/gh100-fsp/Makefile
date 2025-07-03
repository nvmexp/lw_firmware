# MK TODO: refactor it to build nicely or sth

APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(APP_ROOT)/../..
INSTALL_DIR := $(APP_ROOT)/bin

# it must be built at this time
LIBLWRISCV_DIR := $(APP_ROOT)/liblwriscv.bin
LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk

LIBCCC_DIR ?= $(APP_ROOT)/libCCC.bin

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
LWRISCV_APP_DMEM_LIMIT:=0x8000
LWRISCV_APP_IMEM_LIMIT:=0x8000

LDSCRIPT_IN ?= $(APP_ROOT)/ldscript.ld.in
LDSCRIPT := ldscript.ld

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

# Add sample app header files to include path
INCLUDES += $(APP_ROOT)
# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc
# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
# Add CCC stuff
INCLUDES += $(LIBCCC_DIR)/inc/common_crypto_private/lwpka
INCLUDES += $(LIBCCC_DIR)/inc/common_crypto_private
INCLUDES += $(LIBCCC_DIR)/inc/common_crypto/include
INCLUDES += $(LIBCCC_DIR)/inc/common_crypto
INCLUDES += $(LIBCCC_DIR)/inc/

# MK TODO: this include is actually part of chips_a, is optional for builds with chips_a
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
INCLUDES := $(addprefix -I,$(INCLUDES))
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)
DEFINES += -DSAMPLE_PROFILE_$(SAMPLE_PROFILE)=1

# Link with libCCC
LIBS := -L$(LIBCCC_DIR)/lib/$(LIBCCC_ENGINE) -lCCC
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
ifeq ($(SAMPLE_PROFILE),)
SAMPLE_PROFILE := *
endif

TARGET:=sample-$(SAMPLE_PROFILE)
OBJS += $(APP_ROOT)/utils.o
OBJS += $(APP_ROOT)/sha256.o
OBJS += $(APP_ROOT)/rsa3kpss.o
OBJS += $(APP_ROOT)/ecdsa.o
OBJS += $(APP_ROOT)/main.o
OBJS += $(APP_ROOT)/gdma_test.o

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

BUILD_FILES := $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(FMC_TEXT) $(FMC_DATA) $(ELF) $(OBJDUMP_SOURCE)

# TODO: this probably should be done other way
INTERMEDIATE_FILES :=

.PHONY: install build sign

install: build
	install -D -t $(INSTALL_DIR) $(BUILD_FILES)

build: $(BUILD_FILES)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

sign: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)
	echo "Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip ga102_rsa3k_0
INTERMEDIATE_FILES+= $(MANIFEST_BIN).encrypt.bin $(MANIFEST_BIN).encrypt.bin.sig.bin

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST) $(MANIFEST_BIN)
INTERMEDIATE_FILES += $(MANIFEST_BIN)

$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC)
	$(eval APP_IMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_TEXT) ))
	$(eval APP_DMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_DATA) ))
	$(CC) $(MANIFEST_SRC) $(INCLUDES) $(DEFINES) $(CFLAGS)  -include $(LIBLWRISCV_CONFIG_H) -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)

$(LDSCRIPT): $(LDSCRIPT_IN)
	$(CPP) -x c -P -o $@ $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $^
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

$(ELF): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_DIR)/lib/liblwriscv.a $(LIBCCC_DIR)/lib/$(LIBCCC_ENGINE)/libCCC.a
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
INTERMEDIATE_FILES += $(ELF)

%.o: %.c $(LIBLWRISCV_CONFIG_H)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(LIBLWRISCV_CONFIG_H)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)
