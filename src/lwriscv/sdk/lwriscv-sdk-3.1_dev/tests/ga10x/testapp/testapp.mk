# TODO: Don't dump .o files with source files

# Mateusz's make-fu is bad
TESTNAME = $(notdir $(patsubst %/, %, $(LWRDIR)))
ROOT = $(LWRDIR)/../..
TESTAPP_ROOT = $(LWRDIR)/../
BUILD_DIR = $(LWRDIR)/bin

include $(ROOT)/config.mk

OBJS += $(TESTAPP_ROOT)/common/debug.o

# Mangle objects to be per-engine
OBJS := $(patsubst %.o,%-$(ENGINE).o,$(OBJS))

MANIFEST_SRC ?= $(TESTAPP_ROOT)/common/manifest.c

ELF := $(BUILD_DIR)/$(TESTNAME)-$(ENGINE)
FMC_TEXT := $(ELF).text
FMC_DATA := $(ELF).data
MANIFEST := $(ELF).manifest.o
MANIFEST_BIN := $(ELF).manifest

FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin

BUILD_FILES := $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST)

# TODO: this probably should be done other way
INTERMEDIATE_FILES :=

LDSCRIPT_IN ?= $(TESTAPP_ROOT)/common/ldscript.in.ld
LDSCRIPT := $(BUILD_DIR)/ldscript-$(ENGINE).ld

CC := $(RISCV_TOOLCHAIN)gcc
LD := $(RISCV_TOOLCHAIN)ld
STRIP := $(RISCV_TOOLCHAIN)strip
OBJDUMP := $(RISCV_TOOLCHAIN)objdump
OBJCOPY := $(RISCV_TOOLCHAIN)objcopy

ARCH := rv64imfc
ISA := RV64IM

INCFLAGS := -I$(INC) -I$(INC)/$(CHIP) -I$(TESTAPP_ROOT)/common

DEPFLAGS = -MMD -MF $(BUILD_DIR)/$(@F).d
DEPS := $(addprefix $(BUILD_DIR)/, $(addsuffix .d, $(notdir $(OBJS))))
CFLAGS += -std=c11 -Wall -Wextra
CFLAGS += -ffreestanding -ffast-math -fno-common -fno-jump-tables
CFLAGS += -Og -g -fdiagnostics-color=always
CFLAGS += $(INCFLAGS)
CFLAGS += -DENGINE_$(ENGINE)
CFLAGS += -mabi=lp64f -march=$(ARCH)
CFLAGS += $(DEPFLAGS)

ASFLAGS := $(CFLAGS)
LDFLAGS += $(LIBS) -nostdlib -nostartfiles -T$(LDSCRIPT) -fdiagnostics-color=always

LD_DEFINES += -D__LINKER__
LD_INCLUDES += $(INCFLAGS)

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
	$(OBJCOPY) -O binary -j .rodata $(MANIFEST) $(MANIFEST_BIN)
INTERMEDIATE_FILES += $(MANIFEST_BIN)

$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC)
	$(eval FMC_IMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_TEXT) ))
	$(eval FMC_DMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_DATA) ))
	$(CC) $(MANIFEST_SRC) -DFMC_IMEM_SIZE=$(FMC_IMEM_SIZE) -DFMC_DMEM_SIZE=$(FMC_DMEM_SIZE) -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)

$(LDSCRIPT): $(LDSCRIPT_IN) | $(BUILD_DIR)
	$(CPP) -x c -P -o $@ $(LD_DEFINES) $(LD_INCLUDES) $^
INTERMEDIATE_FILES += $(LDSCRIPT)

$(FMC_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .monitor_code $(ELF) $(FMC_TEXT)
INTERMEDIATE_FILES += $(FMC_TEXT)

$(FMC_DATA): $(ELF)
	$(OBJCOPY) -O binary -j .monitor_data $(ELF) $(FMC_DATA)
INTERMEDIATE_FILES += $(FMC_DATA)

$(ELF): $(OBJS) $(LDSCRIPT)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
INTERMEDIATE_FILES += $(ELF)

%-$(ENGINE).o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

%-$(ENGINE).o: %.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)
	rmdir $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)
