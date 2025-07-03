# Reference build flags to build RISC-V ucodes, include it first (then add own stuff)

# The following variables are clobbered:
# CFLAGS, ASFLAGS, LDFLAGS

# The following variables should be set (if liblwriscv is to be used):
# LIBLWRISCV_CONFIG_H - path to g_lwriscv_config.h

# If liblwriscv is not used, the following variables should be set:
# LWRISCV_HAS_ABI - lwriscv ABI (for example lp64 or lp64f)
# LWRISCV_HAS_ARCH - lwriscv arch (for example rv64imfc)
# LW_ENGINE_MANUALS - path to (chip-specific) manuals (for example .../hwref/ampere/ga102)

# You should include this file *after* sourcing liblwriscv config mk file.

# You may add the following defines:
# EXTRA_CFLAGS, EXTRA_LDFLAGS, EXTRA_ASFLAGS - extra compilation flags

ifeq ($(LWRISCV_HAS_ABI),)
$(error Source g_lwriscv_config.mk before build_flags.mk or set LWRISCV_HAS_ABI)
endif

ifeq ($(LWRISCV_HAS_ARCH),)
$(error Source g_lwriscv_config.mk before build_flags.mk or set LWRISCV_HAS_ARCH)
endif

ifneq ($(and $(LW_MANUALS_ROOT),$(LWRISCV_IS_CHIP_FAMILY),$(LWRISCV_IS_CHIP)),)
LW_ENGINE_MANUALS := $(LW_MANUALS_ROOT)/$(LWRISCV_IS_CHIP_FAMILY)/$(LWRISCV_IS_CHIP)
endif

LWRISCV_OPTIMIZATION_FLAG ?= Os

CFLAGS:=
CFLAGS += -$(LWRISCV_OPTIMIZATION_FLAG)
CFLAGS += -g -std=c11 -pipe
CFLAGS += -mabi=$(LWRISCV_HAS_ABI) -march=$(LWRISCV_HAS_ARCH) -mcmodel=medany
CFLAGS += -nostartfiles -nostdlib -ffreestanding
CFLAGS += -fomit-frame-pointer -fno-strict-aliasing
CFLAGS += -ffunction-sections -fdata-sections -fno-jump-tables
CFLAGS += -Werror  -Wint-in-bool-context
CFLAGS += -Wtype-limits
#CFLAGS += -Wformat-security -Wextra
CFLAGS += -fdiagnostics-color=always
ifneq ($(LIBLWRISCV_CONFIG_H),)
CFLAGS += -include $(LIBLWRISCV_CONFIG_H)
endif
ifneq ($(LW_ENGINE_MANUALS),)
CFLAGS += -I$(LW_ENGINE_MANUALS)
endif
ifeq ($(LWRISCV_FEATURE_SSP),y)
CFLAGS += -fstack-protector-all
CFLAGS += --param=ssp-buffer-size=4
CFLAGS += -Wstack-protector
endif

CFLAGS += $(EXTRA_CFLAGS)

ASFLAGS:=
ASFLAGS += -mabi=$(LWRISCV_HAS_ABI) -march=$(LWRISCV_HAS_ARCH)
ifneq ($(LIBLWRISCV_CONFIG_H),)
ASFLAGS += -include $(LIBLWRISCV_CONFIG_H)
endif
ifneq ($(LW_ENGINE_MANUALS),)
ASFLAGS += -I$(LW_ENGINE_MANUALS)
endif
ASFLAGS += $(EXTRA_ASFLAGS)

LDFLAGS:=
ifneq ($(findstring rv64,$(LWRISCV_HAS_ARCH)),)
    LDFLAGS += -Wl,-melf64lriscv
else ifneq ($(findstring rv32,$(LWRISCV_HAS_ARCH)),)
    LDFLAGS += -Wl,-melf32lriscv
else
$(error unrecognize arch)
endif
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--sort-common=descending
LDFLAGS += -Wl,--sort-section=alignment
LDFLAGS += -nostdlib -nostartfiles
LDFLAGS += $(EXTRA_LDFLAGS)

