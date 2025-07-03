all: release

RELEASE_DIR = $(APP_ROOT)/prebuilt
# Include common configuration
include config.mk

# Custom variables..
LWRISCV_APP_DMEM_LIMIT?=0x0000
LWRISCV_APP_IMEM_LIMIT?=0x1000

LDSCRIPT_IN ?= $(APP_ROOT)/ldscript-fmc.ld.in
LDSCRIPT := $(BUILD_DIR)/ldscript-fmc.ld

INCLUDES := $(addprefix -I,$(INCLUDES))

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

TARGET_PREFIX:=gsp-stress-fmc
TARGET:=$(BUILD_DIR)/$(TARGET_PREFIX)

OBJS := fmc_entry.o fmc.o

# Add build dir prefix
OBJS:=$(addprefix $(BUILD_DIR)/,$(OBJS))

MANIFEST_SRC := $(APP_ROOT)/manifest.c

ELF             := $(TARGET)
FMC_TEXT        := $(ELF).text
FMC_DATA        := $(ELF).data
OBJDUMP_SOURCE  := $(ELF).objdump.src
READELF_TXT     := $(ELF).readelf
MANIFEST        := $(ELF).manifest.o
MANIFEST_BIN    := $(ELF).manifest
MANIFEST_HEX    := $(ELF).manifest.hex

SIGNED_FMC_TEXT_OUT := $(FMC_TEXT).encrypt.bin
SIGNED_FMC_DATA_OUT := $(FMC_DATA).encrypt.bin
SIGNED_MANIFEST_OUT := $(MANIFEST_BIN).encrypt.bin.out.bin

SIGNED_FMC_TEXT_DBG := $(FMC_TEXT)_dbg.encrypt.bin
SIGNED_FMC_DATA_DBG := $(FMC_DATA)_dbg.encrypt.bin
SIGNED_MANIFEST_DBG := $(MANIFEST_BIN)_dbg.encrypt.bin.out.bin

SIGNED_FMC_TEXT_PRD := $(FMC_TEXT)_prd.encrypt.bin
SIGNED_FMC_DATA_PRD := $(FMC_DATA)_prd.encrypt.bin
SIGNED_MANIFEST_PRD := $(MANIFEST_BIN)_prd.encrypt.bin.out.bin

BUILD_FILES := $(SIGNED_FMC_TEXT_DBG) $(SIGNED_FMC_DATA_DBG) $(SIGNED_MANIFEST_DBG)
ifneq ($(SKIP_PROD_SIGN), 1)
BUILD_FILES += $(SIGNED_FMC_TEXT_PRD) $(SIGNED_FMC_DATA_PRD) $(SIGNED_MANIFEST_PRD)
endif

BUILD_FILES += $(OBJDUMP_SOURCE) $(ELF) $(READELF_TXT) $(MANIFEST_HEX)

SIGGEN_ENGINE_OPTIONS =

.PHONY: build sign release

build: $(BUILD_FILES)

$(SIGNED_FMC_TEXT_DBG) $(SIGNED_FMC_DATA_DBG) $(SIGNED_MANIFEST_DBG): sign_debug

$(SIGNED_FMC_TEXT_PRD) $(SIGNED_FMC_DATA_PRD) $(SIGNED_MANIFEST_PRD): sign_prod

sign_prod:  $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA) sign_debug
	echo "Prod signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip $(SIGGEN_CHIP) $(SIGGEN_ENGINE_OPTIONS) -mode 1
	mv $(SIGNED_FMC_TEXT_OUT) $(SIGNED_FMC_TEXT_PRD)
	mv $(SIGNED_FMC_DATA_OUT) $(SIGNED_FMC_DATA_PRD)
	mv $(SIGNED_MANIFEST_OUT) $(SIGNED_MANIFEST_PRD)

sign_debug: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)
	echo "Debug signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip $(SIGGEN_CHIP) $(SIGGEN_ENGINE_OPTIONS)
	mv $(SIGNED_FMC_TEXT_OUT) $(SIGNED_FMC_TEXT_DBG)
	mv $(SIGNED_FMC_DATA_OUT) $(SIGNED_FMC_DATA_DBG)
	mv $(SIGNED_MANIFEST_OUT) $(SIGNED_MANIFEST_DBG)

$(MANIFEST) : $(MANIFEST_SRC)

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST) $(MANIFEST_BIN)

$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC) $(BUILD_DIR)
	$(eval APP_IMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_TEXT) ))
	$(eval APP_DMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_DATA) ))
	$(CC) $(MANIFEST_SRC) $(INCLUDES) $(DEFINES) $(CFLAGS) -include $(LIBLWRISCV_CONFIG_H) -c -o $@

$(FMC_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .fmc $(ELF) $(FMC_TEXT)

# all data embedded in text
$(FMC_DATA):
	touch $(FMC_DATA)

RELEASE_SCRIPT = $(UPROC_BUILD_SCRIPTS)/release-imgs-if-changed.pl
RELEASE_IMAGES :=$(BUILD_FILES)

COMMA := ,
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

RELEASE_IMG_ARGS += --output-prefix $(TARGET_PREFIX)
RELEASE_IMG_ARGS += --image         $(ELF)
# RELEASE_IMG_ARGS += --p4            $(P4)
# Force installation always
RELEASE_IMG_ARGS += --force
RELEASE_IMG_ARGS += --release-path $(RELEASE_DIR)

RELEASE_IMG_ARGS += --release-files            \
  $(subst $(SPACE),$(COMMA),$(RELEASE_IMAGES))

release: $(RELEASE_IMAGES) $(RELEASE_DIR)
	$(ECHO) $(PERL) $(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS)
	$(PERL) $(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS)
.PHONY: release

$(RELEASE_DIR):
	$(MKDIR) -p $(RELEASE_DIR)

clean:
	rm -f $(BUILD_FILES) $(OBJS)

include config_rules.mk
