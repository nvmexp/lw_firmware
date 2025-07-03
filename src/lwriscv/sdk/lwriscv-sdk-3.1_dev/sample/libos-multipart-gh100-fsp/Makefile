APP_ROOT = $(LWRDIR)
OUT_DIR = $(APP_ROOT)/_out
INSTALL_DIR = $(APP_ROOT)/bin
LWRISCVX_SDK_ROOT = $(APP_ROOT)/../..
ENGINE = fsp
CHIP = gh100

LIBLWRISCV_PROFILE_NAME ?= basic-libos-${CHIP}-${ENGINE}
PEREGRINE_PROFILE = fsp-gh100-mpu-sampleapp-debug

USE_PREBUILT_SDK ?= true
ifeq ($(USE_PREBUILT_SDK),true)
    LIBLWRISCV_DIR = $(LWRISCVX_SDK_ROOT)/prebuilt/$(LIBLWRISCV_PROFILE_NAME)
else
    LIBLWRISCV_DIR = $(APP_ROOT)/sdk-$(LIBLWRISCV_PROFILE_NAME).bin
endif

LIBLWRISCV_CONFIG_H = $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK = $(LIBLWRISCV_DIR)/g_lwriscv_config.mk

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
$(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Toolchain detection
msys_version = $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)
ifneq ($(msys_version), 0)
$(info MSYS $(msys_version) detected. Using mingw toolchain.)
RISCV_TOOLCHAIN = riscv64-unknown-elf-
SIGGEN = $(LWRISCVX_SDK_ROOT)/tools/siggen_manifest.exe
export GNAT_PRO_PATH = $(LW_TOOLS)/adacore/gnat_pro_ada/windows/gnat_pro_ent_ada/target_riscv/20200209
export SPARK_PRO_PATH = $(LW_TOOLS)/adacore/gnat_pro_ada/windows/spark_pro/20200212
export GNAT_PATH = $(GNAT_PRO_PATH)
endif

# Preconfigures compilers
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

TARGET_BOOT = $(OUT_DIR)/boot/libboot.a
TARGET_MGMT = $(OUT_DIR)/mgmt/libmgmt.a
TARGET_MGMT_ADA_TASKS = $(OUT_DIR)/mgmt/ada/libtasks.a
TARGET_PARTITIONS = $(TARGET_BOOT) $(TARGET_MGMT) $(TARGET_MGMT_ADA_TASKS)
TARGET_PARTITIONS_PRE_ELF = $(OUT_DIR)/partitions.pre.elf
TARGET_PARTITIONS_ELF = $(OUT_DIR)/partitions.elf
TARGET_PARTITIONS_SEGMENT_TEXT = $(TARGET_PARTITIONS_ELF).text.bin
TARGET_PARTITIONS_SEGMENT_DATA = $(TARGET_PARTITIONS_ELF).data.bin

TARGET_UCODE_PREFIX = $(OUT_DIR)/g_ucode_$(ENGINE)_$(CHIP)_riscv_image
TARGET_UCODE_MANIFEST = $(TARGET_UCODE_PREFIX).manifest
TARGET_UCODE_TEXT = $(TARGET_UCODE_PREFIX).text
TARGET_UCODE_DATA = $(TARGET_UCODE_PREFIX).data
TARGET_UCODE_MANIFEST_SIGNED = $(TARGET_UCODE_MANIFEST).encrypt.bin.out.bin
TARGET_UCODE_TEXT_SIGNED = $(TARGET_UCODE_TEXT).encrypt.bin
TARGET_UCODE_DATA_SIGNED = $(TARGET_UCODE_DATA).encrypt.bin

TARGET_LINKER_SCRIPT = $(OUT_DIR)/ldscript.ld

TARGET_SEGMENT_EXTRACTOR = $(OUT_DIR)/segment-extractor

SEPKERN_ROOT = $(LWRISCVX_SDK_ROOT)/monitor/sepkern
SEPKERN_DIR = $(OUT_DIR)/sepkern
SEPKERN_PREFIX = $(SEPKERN_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv_image
SEPKERN_TEXT = $(SEPKERN_PREFIX).text
SEPKERN_DATA = $(SEPKERN_PREFIX).data

INCLUDES += -I$(LIBLWRISCV_DIR)/inc
INCLUDES += -I$(LWRISCVX_SDK_ROOT)/inc
INCLUDES += -I$(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC/hwref
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC/common
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto/include
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto_private
INCLUDES += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto_private/lwpka

DEFINES += -DCCC_USE_LIBLWRISCV
DEFINES += -DLIBCCC_FSP_SE_ENABLED
DEFINES += -DUPROC_ARCH_RISCV

# Useless defines
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=0
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=0

LIBS += -L$(LIBLWRISCV_DIR)/lib -llwriscv -lCCC

CFLAGS += -Os
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)

LDFLAGS += -T$(TARGET_LINKER_SCRIPT)
LDFLAGS += -Wl,-whole-archive $(TARGET_PARTITIONS) -Wl,-no-whole-archive
LDFLAGS += $(LIBS)

.PHONY: default
default: $(OUT_DIR) install

$(OUT_DIR):
	$(MKDIR) -p $@

.PHONY: $(TARGET_BOOT)
$(TARGET_BOOT):
	$(MAKE) -C boot USE_PREBUILT_SDK=$(USE_PREBUILT_SDK)

.PHONY: $(TARGET_MGMT)
$(TARGET_MGMT):
	$(MAKE) -C mgmt USE_PREBUILT_SDK=$(USE_PREBUILT_SDK)

.PHONY: $(TARGET_MGMT_ADA_TASKS)
$(TARGET_MGMT_ADA_TASKS):
	$(MAKE) -C mgmt/ada

.PHONY: sepkern
sepkern:
	$(MAKE) -C $(SEPKERN_ROOT) ENGINE=$(ENGINE) CHIP=$(CHIP) \
		POLICY_PATH=$(APP_ROOT) POLICY_FILE_NAME=policies.ads \
		PARTITION_COUNT=2 INSTALL_PATH=$(SEPKERN_DIR) install

$(TARGET_UCODE_MANIFEST): $(OUT_DIR)/manifest.o
	$(OBJCOPY) -O binary -j .rodata.manifest $^ $@

$(TARGET_LINKER_SCRIPT): ldscript.ld.in
	$(CC) -E -x c -DMAKEFILE -I$(OUT_DIR)/mgmt -I$(LIBOS_SOURCE)/kernel \
		-I$(LIBOS_SOURCE)/profiles/$(PEREGRINE_PROFILE) -I$(LW_MANUALS_ROOT) \
		$^ | grep -v '^#' > $@

$(TARGET_PARTITIONS_PRE_ELF): $(TARGET_PARTITIONS) $(TARGET_LINKER_SCRIPT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@

$(TARGET_PARTITIONS_ELF): $(TARGET_PARTITIONS_PRE_ELF)
	$(OUT_DIR)/mgmt/sim/libosmc $^ -o $@

.PHONY: TARGET_SEGMENT_EXTRACTOR
$(TARGET_SEGMENT_EXTRACTOR):
	$(MAKE) -C $(LIBOS_SOURCE)/segment-extractor
	cp $(LIBOS_SOURCE)/segment-extractor/segment-extractor $@

.PHONY: partitions_segments
partitions_segments: $(TARGET_SEGMENT_EXTRACTOR) $(TARGET_PARTITIONS_ELF)
	$(TARGET_SEGMENT_EXTRACTOR) $(TARGET_PARTITIONS_ELF) $(TARGET_PARTITIONS_ELF)

$(TARGET_UCODE_TEXT): sepkern partitions_segments
	cat $(SEPKERN_TEXT) $(TARGET_PARTITIONS_SEGMENT_TEXT) > $@
	truncate -s %256 $@

$(TARGET_UCODE_DATA): sepkern partitions_segments
	cat $(SEPKERN_DATA) $(TARGET_PARTITIONS_SEGMENT_DATA) > $@
	truncate -s %256 $@

.PHONY: sign
sign: $(TARGET_UCODE_MANIFEST) $(TARGET_UCODE_TEXT) $(TARGET_UCODE_DATA)
	$(SIGGEN) -manifest $(TARGET_UCODE_MANIFEST) -code $(TARGET_UCODE_TEXT) -data $(TARGET_UCODE_DATA) -chip gh100_rsa3k_0 -fsp

$(INSTALL_DIR):
	$(MKDIR) -p $@

.PHONY: install
install: $(INSTALL_DIR) sign
	cp $(TARGET_UCODE_MANIFEST_SIGNED) $(TARGET_UCODE_TEXT_SIGNED) $(TARGET_UCODE_DATA_SIGNED) $(INSTALL_DIR)

$(OUT_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@rm -rf $(OUT_DIR) $(INSTALL_DIR)
