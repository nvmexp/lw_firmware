APP_ROOT = $(LWRDIR)/..
OUT_DIR = $(APP_ROOT)/_out/boot
LWRISCVX_SDK_ROOT = $(APP_ROOT)/../..

LIBLWRISCV_PROFILE_NAME ?= basic-libos-gh100-fsp

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
endif

# Preconfigures compilers
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

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

CFLAGS += -Os
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)

OBJS += $(OUT_DIR)/boot_main.o
OBJS += $(OUT_DIR)/boot_startup.o

TARGET = $(OUT_DIR)/libboot.a

.PHONY: default
default: $(OUT_DIR) $(TARGET)

$(OUT_DIR):
	$(MKDIR) -p $@

$(TARGET): $(OBJS)
	$(AR) -crs $@ $^

$(OUT_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@rm -rf $(OUT_DIR)
