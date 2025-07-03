# Required environment variables
$(if $(value LW_TOOLS),,$(error LW_TOOLS not set))
$(if $(value LW_SOURCE),,$(error LW_SOURCE not set))
$(if $(value LIBOS_SOURCE),,$(error LIBOS_SOURCE not set))

BUILD_CFG ?= debug
ENGINE ?= gsp
PROJ ?= ga10x
# Whether to bundle the bootloader with the separation kernel as a single FMC
SKBL ?= 0
OUT_DIR = _out/$(ENGINE)_$(PROJ)_$(BUILD_CFG)
IMG_TARGET = $(OUT_DIR)/g_libos_$(ENGINE)_$(PROJ)_riscv

BUILD_SCRIPTS = $(LW_SOURCE)/uproc/build/scripts
MK_SCRIPT = $(BUILD_SCRIPTS)/mkimage/rvmkimg.pl
RELEASE_SCRIPT = $(BUILD_SCRIPTS)/release-imgs-if-changed.pl

TOOLCHAIN = $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin
READELF = $(TOOLCHAIN)/riscv64-elf-readelf
OBJDUMP = $(TOOLCHAIN)/riscv64-elf-objdump

MONITOR_PREFIX = $(LW_SOURCE)/uproc/lwriscv/sepkern/bin/$(ENGINE)/$(PROJ)/g_sepkern_$(ENGINE)_$(PROJ)_riscv
SKBL_PREFIX = $(LIBOS_SOURCE)/bootloader/_out/$(ENGINE)_$(PROJ)/g_skbl_$(ENGINE)_$(PROJ)_riscv
RELEASE_PATH = $(LW_SOURCE)/drivers/resman/kernel/inc/libos/$(ENGINE)/bin

default: install

$(OUT_DIR)/libos-config.h: libos-config.py
	@mkdir -p $(OUT_DIR)
	@python2 libos-config.py $(OUT_DIR)/

$(OUT_DIR)/firmware: $(OUT_DIR)/libos-config.h
	@make -C $(OUT_DIR) BUILD_CFG=$(BUILD_CFG) LIBOS_SOURCE=$(LIBOS_SOURCE)
	@$(OBJDUMP) -S -d $(OUT_DIR)/firmware.elf > $(OUT_DIR)/firmware.elf.objdump

$(OUT_DIR)/firmware_heap.elf: $(OUT_DIR)/firmware
	@# 1MiB minus 16B
	@truncate -s 1048560 $(OUT_DIR)/firmware_heap.elf
	@# Write libOS magic 0x51CB7F1D
	@echo 1d7fcb5100000000 | xxd -r -p >> $(OUT_DIR)/firmware_heap.elf
	@# Write heap size (1MB)
	@echo 0000010000000000 | xxd -r -p >> $(OUT_DIR)/firmware_heap.elf
	@cat "$(OUT_DIR)/firmware.elf.stripped" >> $(OUT_DIR)/firmware_heap.elf

$(OUT_DIR)/libos.readelf: $(OUT_DIR)/firmware
	@$(READELF) -l $(OUT_DIR)/firmware.elf.stripped | sed 's/LOOS..../LOAD    /g' > $(OUT_DIR)/libos.readelf

$(OUT_DIR)/bootloader:
ifeq ($(SKBL), 1)
	@make -C $(LIBOS_SOURCE)/bootloader ENGINE=$(ENGINE) PROJ=$(PROJ) skbl
	@# Pad the bootloader with 4080 zeros so that in the binary generate by
	@# rvmkimg.pl, the ELF is 4K aligned.
	@truncate -s 0 $(OUT_DIR)/bootloader
	@truncate -s 4080 $(OUT_DIR)/bootloader
else
	@make -C $(LIBOS_SOURCE)/bootloader ENGINE=$(ENGINE) PROJ=$(PROJ) PAD_BINARY=1 cleanbuild
	@cp $(LIBOS_SOURCE)/bootloader/_out/$(ENGINE)_$(PROJ)/bootloader $(OUT_DIR)/bootloader
endif

$(OUT_DIR)/bin_archive: $(OUT_DIR)/firmware_heap.elf $(OUT_DIR)/libos.readelf $(OUT_DIR)/bootloader
ifeq ($(SKBL), 1)
	@perl $(MK_SCRIPT) \
		--monitorCode   $(SKBL_PREFIX)_image.text.encrypt.bin \
		--monitorData   $(SKBL_PREFIX)_image.data.encrypt.bin \
		--manifest      $(SKBL_PREFIX)_manifest.bin.encrypt.bin.out.bin \
		--loaderFile    $(OUT_DIR)/bootloader \
		--elfFile       $(OUT_DIR)/firmware_heap.elf \
		--readelf       $(OUT_DIR)/libos.readelf \
		--outFilePrefix $(IMG_TARGET) \
		--useMonitor    1
else
	@perl $(MK_SCRIPT) \
		--monitorCode   $(MONITOR_PREFIX)_debug_image.text.encrypt.bin \
		--monitorData   $(MONITOR_PREFIX)_debug_image.data.encrypt.bin \
		--manifest      $(MONITOR_PREFIX)_manifest.bin_dbg_enc.bin \
		--loaderFile    $(OUT_DIR)/bootloader \
		--elfFile       $(OUT_DIR)/firmware_heap.elf \
		--readelf       $(OUT_DIR)/libos.readelf \
		--outFilePrefix $(IMG_TARGET) \
		--useMonitor    1
endif

install: $(OUT_DIR)/bin_archive
	@perl $(RELEASE_SCRIPT) \
		--release-path $(RELEASE_PATH) \
		--release-files $(IMG_TARGET)_desc.bin,$(IMG_TARGET)_image.bin \
		--image $(IMG_TARGET)_image.bin

clean:
	@rm -rf $(OUT_DIR) || true

cleanbuild: clean default

.PHONY: default
.PHONY: $(OUT_DIR)/firmware
.PHONY: $(OUT_DIR)/bootloader
.PHONY: $(OUT_DIR)/bin_archive
.PHONY: install
.PHONY: clean
.PHONY: cleanbuild
