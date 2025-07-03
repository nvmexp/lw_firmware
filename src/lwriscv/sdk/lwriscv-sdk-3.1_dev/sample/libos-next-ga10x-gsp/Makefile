APP_ROOT = $(LWRDIR)
OUT_DIR = $(APP_ROOT)/_out
INSTALL_DIR = $(APP_ROOT)/bin
LWRISCVX_SDK_ROOT = $(APP_ROOT)/../..
ENGINE = gsp
CHIP = ga10x

#USE_PREBUILT_SDK ?= true
ifeq ($(USE_PREBUILT_SDK),true)
    LIBLWRISCV_DIR = $(LWRISCVX_SDK_ROOT)/prebuilt/$(LIBLWRISCV_PROFILE_NAME)
else
    LIBLWRISCV_DIR = $(APP_ROOT)/sdk-$(LIBLWRISCV_PROFILE_NAME).bin
endif

LIBLWRISCV_CONFIG_H = $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK = $(LIBLWRISCV_DIR)/g_lwriscv_config.mk
LIBLWRISCV_INCLUDES = -I$(LIBLWRISCV_DIR)/inc -I$(LWRISCVX_SDK_ROOT)/inc -I$(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
LIBLWRISCV_LIB = -L$(LIBLWRISCV_DIR)/lib

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

TARGET_UCODE_PREFIX = $(OUT_DIR)/g_ucode_$(ENGINE)_$(CHIP)_riscv_image
TARGET_UCODE_MANIFEST = $(TARGET_UCODE_PREFIX).manifest
TARGET_UCODE_TEXT = $(TARGET_UCODE_PREFIX).text
TARGET_UCODE_DATA = $(TARGET_UCODE_PREFIX).data
TARGET_UCODE_MANIFEST_SIGNED = $(TARGET_UCODE_MANIFEST).encrypt.bin.out.bin
TARGET_UCODE_TEXT_SIGNED = $(TARGET_UCODE_TEXT).encrypt.bin
TARGET_UCODE_DATA_SIGNED = $(TARGET_UCODE_DATA).encrypt.bin

SEPKERN_ROOT = /home/scratch.chenx_sw/clone/sw/pvt/chenx/monitor/sepkern
SEPKERN_DIR = $(OUT_DIR)/sepkern
SEPKERN_PREFIX = $(SEPKERN_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv_image
SEPKERN_TEXT = $(SEPKERN_PREFIX).text
SEPKERN_DATA = $(SEPKERN_PREFIX).data

ELWOS_DIR = $(OUT_DIR)/elwos
ELWOS_RUNTIME_IMAGE = $(ELWOS_DIR)/firmware
ELWOS_BRIDGE_TEXT = $(ELWOS_DIR)/bridge.text
ELWOS_BRIDGE_DATA = $(ELWOS_DIR)/bridge.data

INCLUDES += -I$(LIBLWRISCV_DIR)/inc
INCLUDES += -I$(LWRISCVX_SDK_ROOT)/inc
INCLUDES += -I$(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk

DEFINES += -DUPROC_ARCH_RISCV

# Useless defines
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=0
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=0

LIBS += -L$(LIBLWRISCV_DIR)/lib -llwriscv

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

LIBOS_SOURCE=$(APP_ROOT)/../../os/libos-next
export LIBOS_SOURCE

$(ELWOS_DIR):
	$(MKDIR) -p $@

.PHONY: elwos
elwos: $(ELWOS_DIR)
	$(MAKE) -C elwos_basic install INSTALL_DIR=$(ELWOS_DIR) LIBLWRISCV_INC=$(LIBLWRISCV_INCLUDES) LIBLWRISCV_LIB=$(LIBLWRISCV_LIB) LIBLWRISCV_MK=$(LIBLWRISCV_CONFIG_MK) LIBLWRISCV_CONFIG_H=$(LIBLWRISCV_CONFIG_H)

.PHONY: sepkern
sepkern: $(SEPKERN_DIR)
	$(MAKE) -C $(SEPKERN_ROOT) ENGINE=$(ENGINE) CHIP=$(CHIP) \
		POLICY_PATH=$(APP_ROOT) POLICY_FILE_NAME=policies.ads \
		PARTITION_COUNT=5 INSTALL_PATH=$(SEPKERN_DIR) install
$(SEPKERN_DIR):
	$(MKDIR) -p $@

$(TARGET_UCODE_MANIFEST): $(OUT_DIR)/manifest.o
	$(OBJCOPY) -O binary -j .rodata.manifest $^ $@

$(TARGET_UCODE_TEXT): sepkern elwos
	cat $(SEPKERN_TEXT) $(ELWOS_BRIDGE_TEXT) > $@
	truncate -s %256 $@

$(TARGET_UCODE_DATA): sepkern elwos
	cat $(SEPKERN_DATA) $(ELWOS_BRIDGE_DATA) > $@
	truncate -s %256 $@

.PHONY: sign
sign: $(TARGET_UCODE_MANIFEST) $(TARGET_UCODE_TEXT) $(TARGET_UCODE_DATA)
	$(SIGGEN) -manifest $(TARGET_UCODE_MANIFEST) -code $(TARGET_UCODE_TEXT) -data $(TARGET_UCODE_DATA) -chip ga102_rsa3k_0

$(INSTALL_DIR):
	$(MKDIR) -p $@

.PHONY: install
install: $(INSTALL_DIR) sepkern sign elwos
	cp $(TARGET_UCODE_MANIFEST_SIGNED) $(TARGET_UCODE_TEXT_SIGNED) $(TARGET_UCODE_DATA_SIGNED) $(ELWOS_RUNTIME_IMAGE) $(INSTALL_DIR)

$(OUT_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@rm -rf $(OUT_DIR) $(INSTALL_DIR)
