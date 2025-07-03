export ENGINE:=gsp
export CHIP:=ga10x
export SIGGEN_CHIP:=ga102_rsa3k_0
export LWRISCV_SDK_VERSION:=lwriscv-sdk-2.1_dev

APP_ROOT ?= $(LWRDIR)
LWRISCV_SDK_PROFILE ?= gsp-stress-test-ga10x
LWRISCVX_SDK_ROOT ?= $(P4ROOT)/sw/lwriscv/main
LWRISCVX_SDK_PREBUILT :=$(APP_ROOT)/prebuilt/$(LWRISCV_SDK_PROFILE)
LW_MANUALS_ROOT := $(LW_SOURCE)/drivers/common/inc/hwref
UPROC_BUILD_SCRIPTS    := $(LW_SOURCE)/uproc/build/scripts
RELEASE_DIR      ?= $(LW_SOURCE)/drivers/resman/kernel/inc/gsp/bin

LIBLWRISCV_DIR := $(LWRISCVX_SDK_PREBUILT)
LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk

BUILD_DIR:=$(APP_ROOT)/build-$(CHIP)-$(ENGINE).bin

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
    $(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Preconfigures compilers
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

# Check environment
ifeq ($(wildcard $(CC)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

LIBS:=

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc

# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc

# MK TODO: this include is actually part of chips_a, is optional for builds with chips_a
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk

# Add manuals
INCLUDES += $(LW_ENGINE_MANUALS)

# Add RM interface
INCLUDES += $(LW_SOURCE)/drivers/resman/arch/lwalloc/common/inc
INCLUDES += $(LW_SOURCE)/sdk/lwpu/inc
