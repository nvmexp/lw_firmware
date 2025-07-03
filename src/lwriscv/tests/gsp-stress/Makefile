all: release

TARGET_BUILD_TYPE?=dbg

PREBUILT_DIR = $(APP_ROOT)/prebuilt
RELEASE_DIR := $(LW_SOURCE)/drivers/resman/kernel/inc/gsp/bin
# Include common configuration
include config.mk

USE_PREBUILT_LOADER ?= true
FMC_PREFIX:=gsp-stress-fmc
FMC_DIR:=$(APP_ROOT)/prebuilt
SIGNED_FMC_TEXT := $(FMC_DIR)/$(FMC_PREFIX).text_$(TARGET_BUILD_TYPE).encrypt.bin
SIGNED_FMC_DATA := $(FMC_DIR)/$(FMC_PREFIX).data_$(TARGET_BUILD_TYPE).encrypt.bin
SIGNED_MANIFEST := $(FMC_DIR)/$(FMC_PREFIX).manifest_$(TARGET_BUILD_TYPE).encrypt.bin.out.bin

# Custom variables..
LWRISCV_APP_DMEM_LIMIT?=0x2000
LWRISCV_APP_IMEM_LIMIT?=0x2000

LDSCRIPT_IN ?= $(APP_ROOT)/ldscript.ld.in
LDSCRIPT := $(BUILD_DIR)/ldscript.ld

INCLUDES := $(addprefix -I,$(INCLUDES))

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

# Add sources
include $(APP_ROOT)/sources.mk

# Add build dir prefix
OBJS:=$(addprefix $(BUILD_DIR)/,$(OBJS))


TARGET_PREFIX :=g_gsp_ga10x_riscv_stress_test
TARGET :=$(BUILD_DIR)/$(TARGET_PREFIX)

ELF             := $(TARGET)
TARGET_OBJDUMP  := $(ELF).objdump.src
TARGET_READELF  := $(ELF).readelf

TARGET_IDENT:= $(ELF).ident_text
TARGET_TEXT := $(ELF).text
TARGET_DATA := $(ELF).data
TARGET_DATA_FB := $(ELF).data_fb
TARGET_ALL  := $(ELF).all

TARGET_IMAGE := $(BUILD_DIR)/$(TARGET_PREFIX)_$(TARGET_BUILD_TYPE)_image.bin
TARGET_DESC  := $(BUILD_DIR)/$(TARGET_PREFIX)_$(TARGET_BUILD_TYPE)_desc.bin

BUILD_FILES := $(TARGET_OBJDUMP) $(TARGET_READELF) $(TARGET_IMAGE) $(TARGET_DESC) $(ELF)

.PHONY: build gen_image release

build: $(BUILD_FILES)
gen_image: $(TARGET_IMAGE) $(TARGET_DESC)
release: gen_image

$(TARGET_IDENT) : $(ELF)
	$(OBJCOPY) -O binary -j .identity $(ELF) $(TARGET_IDENT)

$(TARGET_TEXT) : $(ELF)
	$(OBJCOPY) -O binary -j .code $(ELF) $(TARGET_TEXT)

$(TARGET_DATA) : $(ELF)
	$(OBJCOPY) -O binary -j .data $(ELF) $(TARGET_DATA)

$(TARGET_DATA_FB) : $(ELF)
	$(OBJCOPY) -O binary -j .data_fb $(ELF) $(TARGET_DATA_FB)

$(TARGET_ALL): $(TARGET_IDENT) $(TARGET_TEXT) $(TARGET_DATA) $(TARGET_DATA_FB)
	cat $^ > $@

RVMKIMG_PARAMS := \
    --monitorCode       $(SIGNED_FMC_TEXT)      \
    --monitorData       $(SIGNED_FMC_DATA)      \
    --manifest          $(SIGNED_MANIFEST)      \
    --loaderFile        $(TARGET_ALL)           \
    --elfFile           $(SIGNED_FMC_DATA)      \
    --readelf           $(TARGET_READELF)       \
    --outFilePrefix     $(TARGET_PREFIX)_$(TARGET_BUILD_TYPE)  \
    --outDir            $(BUILD_DIR)      \
    --useMonitor

$(TARGET_IMAGE) $(TARGET_DESC): $(TARGET_ALL) $(SIGNED_FMC_TEXT) $(SIGNED_FMC_DATA) $(SIGNED_MANIFEST) $(TARGET_READELF)
	$(ECHO) generating $(notdir $(TARGET_IMAGE))
	perl $(BUILD_SCRIPTS)/mkimage/rvmkimg.pl  $(RVMKIMG_PARAMS)

BUILD_SCRIPTS    := $(LW_SOURCE)/uproc/build/scripts
RELEASE_SCRIPT = $(BUILD_SCRIPTS)/release-imgs-if-changed.pl
RELEASE_IMAGES :=$(TARGET_IMAGE) $(TARGET_DESC) $(ELF) $(TARGET_OBJDUMP) $(TARGET_READELF)

COMMA := ,
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

RELEASE_IMG_ARGS += --output-prefix $(TARGET_FINAL_PREFIX)
RELEASE_IMG_ARGS += --image         $(TARGET_IMAGE)
# RELEASE_IMG_ARGS += --p4            $(P4)
# Force installation always
RELEASE_IMG_ARGS += --force
RELEASE_IMG_ARGS += --release-path $(RELEASE_DIR)

RELEASE_IMG_ARGS += --release-files            \
  $(subst $(SPACE),$(COMMA),$(RELEASE_IMAGES))

release: $(RELEASE_IMAGES)
	$(ECHO) $(PERL) $(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS)
	$(PERL) $(RELEASE_SCRIPT) $(RELEASE_IMG_ARGS)

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)

include config_rules.mk
