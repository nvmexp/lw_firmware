ENGINE ?= gsp
PROJ ?= ga10x
PAD_BINARY ?= 0
DEBUG_MANIFEST ?= 0
OUT_DIR = _out/$(ENGINE)_$(PROJ)
ifeq ($(DEBUG_MANIFEST), 0)
	SKBL_TARGET = $(OUT_DIR)/g_skbl_$(ENGINE)_$(PROJ)_riscv
else
	SKBL_TARGET = $(OUT_DIR)/g_skbl_$(ENGINE)_$(PROJ)_riscv_dbg
endif

CHAIN = $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin
CC = $(CHAIN)/riscv64-elf-gcc
OBJCOPY = $(CHAIN)/riscv64-elf-objcopy
OBJDUMP = $(CHAIN)/riscv64-elf-objdump

MANIFEST_PATH = $(LW_SOURCE)/uproc/lwriscv/sepkern_spark/manifests
MANIFEST_SIGGEN = $(MANIFEST_PATH)/siggen_manifest
ifeq ($(DEBUG_MANIFEST), 0)
	MANIFEST_C = $(MANIFEST_PATH)/manifest_$(ENGINE)_$(PROJ)_gsprm_unsigned.c
else
	MANIFEST_C = $(MANIFEST_PATH)/manifest_$(ENGINE)_$(PROJ)_gsprm_unsigned_dbg.c
endif
MONITOR_PREFIX = $(LW_SOURCE)/drivers/resman/kernel/inc/fmc/monitor/bin/$(ENGINE)/$(PROJ)/g_sepkern_$(ENGINE)_$(PROJ)_riscv_image
RELEASE_SCRIPT = $(LW_SOURCE)/uproc/build/scripts/release-imgs-if-changed.pl
SKBL_RELEASE_PATH = $(LW_SOURCE)/drivers/resman/kernel/inc/fmc/bin/$(ENGINE)/$(PROJ)

SOURCES = main.c start.S ../lib/loader.c dma.c

INCLUDE_DIRS += -I..
INCLUDE_DIRS += -I../include
INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc
INCLUDE_DIRS += -I$(LW_SOURCE)/sdk/lwpu/inc
INCLUDE_DIRS += -I$(LW_SOURCE)/uproc/lwriscv/inc

ifeq ($(PROJ), tu10x)
	CFLAGS += -DLIBOS_EXOKERNEL_TU10X -march=rv64im
	INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102 -I$(LW_SOURCE)/drivers/common/inc/swref/turing/tu102
else ifeq ($(PROJ), ga100)
	CFLAGS += -DLIBOS_EXOKERNEL_GA100 -march=rv64im
	INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100 -I$(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100
else ifeq ($(PROJ), ga10x)
	CFLAGS += -DLIBOS_EXOKERNEL_GA10X -fstack-protector-all -march=rv64imc
	INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
	INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
	SOURCES += rand.c ssp.c
	MANIFEST_IMEM_LIMIT = 4096
	MANIFEST_DMEM_LIMIT = 1024
	MANIFEST_SK_IMEM_LIMIT = $(shell stat -c %s $(MONITOR_PREFIX).text)
    SIGGEN_CHIPS = ga102_rsa3k_0
else ifeq ($(PROJ), gh100)
	CFLAGS += -DLIBOS_EXOKERNEL_GH100 -fstack-protector-all -march=rv64imc
	INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
	INCLUDE_DIRS += -I$(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
	SOURCES += rand.c ssp.c
	MANIFEST_IMEM_LIMIT = 4096
	MANIFEST_DMEM_LIMIT = 1024
	MANIFEST_SK_IMEM_LIMIT = $(shell stat -c %s $(MONITOR_PREFIX).text)
	SIGGEN_CHIPS = ga102_rsa3k_0 # TODO: update to a gh100 profile for bug 200668832
else
    $(error Invalid PROJ specified; must be PROJ=[tu10x|ga100|ga10x|gh100])
endif

CFLAGS += -Wundef -ffreestanding -Wl,--no-relax -Wl,-T,layout_$(PROJ).ld -mcmodel=medany -mabi=lp64 -nostdlib -Os $(INCLUDE_DIRS) -fPIC -g -fno-jump-tables

default: $(OUT_DIR)/bootloader

skbl: cleanbuild
	cp $(MONITOR_PREFIX).data $(SKBL_TARGET)_image.data
	cat $(MONITOR_PREFIX).text $(OUT_DIR)/bootloader > $(SKBL_TARGET)_image.text
	@# The FMC DTCM and ITCM should not exceed 4KB
	test $$(stat -c %s $(SKBL_TARGET)_image.data) -le 4096
	test $$(stat -c %s $(SKBL_TARGET)_image.text) -le 4096
	truncate -s %256 $(SKBL_TARGET)_image.text
	$(CC) $(MANIFEST_C) $(CFLAGS) -c -o $(SKBL_TARGET)_manifest.o \
		-DGSP_RTOS -DIMEM_LIMIT=$(MANIFEST_IMEM_LIMIT) -DDMEM_LIMIT=$(MANIFEST_DMEM_LIMIT) \
		-DSK_IMEM_LIMIT=$(MANIFEST_SK_IMEM_LIMIT)
	$(OBJCOPY) -O binary -j .rodata $(SKBL_TARGET)_manifest.o $(SKBL_TARGET)_manifest.bin
	$(MANIFEST_SIGGEN) -manifest $(SKBL_TARGET)_manifest.bin \
		-code $(SKBL_TARGET)_image.text \
		-data $(SKBL_TARGET)_image.data \
		-chip $(SIGGEN_CHIPS) -mode 0
	perl $(RELEASE_SCRIPT) \
		--release-path $(SKBL_RELEASE_PATH) \
		--release-files $(SKBL_TARGET)_image.data.encrypt.bin,$(SKBL_TARGET)_image.text.encrypt.bin,$(SKBL_TARGET)_manifest.bin.encrypt.bin.out.bin \
		--image $(SKBL_TARGET)_manifest.bin.encrypt.bin.out.bin

$(OUT_DIR)/bootloader: $(SOURCES)
	mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $^ -o $@.elf
	$(OBJCOPY) --dump-section .text=$@ $@.elf
ifeq ($(PAD_BINARY), 1)
	@# Pad to multiples of 4096 minus 16 so that in the binary generate by
	@# rvmkimg.pl, the ELF is 4K aligned.
	truncate -s +16 $@
	truncate -s %4096 $@
	truncate -s -16 $@
endif
	$(OBJDUMP) -d -S $@.elf > $@.elf.objdump

clean:
	@rm -rf $(OUT_DIR)

cleanbuild: clean default

.PHONY: default
.PHONY: skbl
.PHONY: clean
.PHONY: cleanbuild
