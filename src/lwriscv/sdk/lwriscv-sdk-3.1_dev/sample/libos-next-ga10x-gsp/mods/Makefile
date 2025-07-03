# Required environment variables
$(if $(value LW_TOOLS),,$(error LW_TOOLS not set))
$(if $(value LW_SOURCE),,$(error LW_SOURCE not set))

BUILD_CFG ?= debug
ENGINE ?= gsp
PROJ ?= ga10x
LWR_DIR=$(LWRDIR)
OUT_DIR = $(LWRDIR)/_out
IMG_TARGET = g_libos_$(ENGINE)_$(PROJ)_riscv

BUILD_SCRIPTS = $(LW_SOURCE)/uproc/build/scripts
MK_SCRIPT = $(BUILD_SCRIPTS)/mkimage/rvmkimg.pl
RELEASE_SCRIPT = $(BUILD_SCRIPTS)/release-imgs-if-changed.pl

TOOLCHAIN = $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin
READELF = $(TOOLCHAIN)/riscv64-elf-readelf
OBJDUMP = $(TOOLCHAIN)/riscv64-elf-objdump

MONITOR_PREFIX = $(LWR_DIR)/../bin/g_ucode_$(ENGINE)_$(PROJ)_riscv
RELEASE_PATH = $(LWR_DIR)/bin
APP_FILE_SRC = $(LWR_DIR)/../bin/firmware
APP_FILE = $(OUT_DIR)/firmware_with_heap

$(OUT_DIR):
	@mkdir $(OUT_DIR)

$(RELEASE_PATH):
	@mkdir $(RELEASE_PATH)

default: install

$(APP_FILE): $(APP_FILE_SRC) $(OUT_DIR)
	@cp $(APP_FILE_SRC) $(APP_FILE)
	@# 4M for actual heap, 2M for max numa alignment adjustment
	@truncate -s 6M $(APP_FILE)

$(OUT_DIR)/bin_archive: $(OUT_DIR) $(APP_FILE)
	@perl $(MK_SCRIPT) \
		--monitorCode   $(MONITOR_PREFIX)_image.text.encrypt.bin \
		--monitorData   $(MONITOR_PREFIX)_image.data.encrypt.bin \
		--manifest      $(MONITOR_PREFIX)_image.manifest.encrypt.bin.out.bin \
		--app           $(APP_FILE) \
		--outFilePrefix $(IMG_TARGET) \
		--outDir        $(OUT_DIR) \
		--useMonitor    1

install: $(OUT_DIR)/bin_archive $(RELEASE_PATH)
	@perl $(RELEASE_SCRIPT) \
		--release-path $(RELEASE_PATH) \
		--release-files $(OUT_DIR)/$(IMG_TARGET)_desc.bin,$(OUT_DIR)/$(IMG_TARGET)_image.bin \
		--image $(OUT_DIR)/$(IMG_TARGET)_image.bin

clean:
	@rm -rf $(OUT_DIR) || true

cleanbuild: clean default

.PHONY: default
.PHONY: install
.PHONY: clean
.PHONY: cleanbuild
