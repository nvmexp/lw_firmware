##
##	Environment check
##

$(if $(value LIBOS_SOURCE),,$(error LIBOS_SOURCE not set))
$(if $(value P4ROOT),,$(error P4ROOT not set))

LW_TOOLS ?= $(P4ROOT)/sw/tools

## Profile setup
PEREGRINE_PROFILE_PATH = $(LIBOS_SOURCE)/profiles/$(PEREGRINE_PROFILE)

##
##	RISCV toolchain setup
##

# Toolchain detection
msys_version := $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)

ifeq ($(msys_version), 0)
$(info Linux detected using tools/riscv/toolchain/linux/release/20190625.)
TOOLCHAIN_PREFIX?=$(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-
else
$(info MSYS $(msys_version) detected. Using mingw toolchain.)
TOOLCHAIN_PREFIX?=riscv64-unknown-elf-
endif



# @todo: Need to decide on host compiler vs so called reference compilers.
#        Walk user through choices and require an environment variable

RISCV_CC=$(TOOLCHAIN_PREFIX)gcc
RISCV_OBJCOPY?=$(TOOLCHAIN_PREFIX)objcopy
RISCV_CC_AR=$(TOOLCHAIN_PREFIX)gcc-ar

LWRISCVX_SDK_ROOT = $(P4ROOT)/sw/lwriscv/main
LW_SDK_INC = $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals

RISCV_INCLUDE += -I$(PEREGRINE_PROFILE_PATH) -I$(LIBOS_SOURCE) -I$(LW_ENGINE_MANUALS) -I$(LW_SDK_INC) -I$(LIBOS_SOURCE)/include -I$(CHIP_SWREF) -I$(CHIP_HWREF)

##
##	RISCV compiler rules
##

# @todo: Remove -DLIBOS_PEREGRINE_1_0 to variable
$(RISCV_OUT_DIR)/%.o : %.c $(RISCV_OUT_DIR)/libos-config.h
	@mkdir -p $(RISCV_OUT_DIR)
	@printf  " [ %-28s ] %-14s  %s\n" $(PREFIX) " C " $<
	@$(RISCV_CC) -I$(RISCV_OUT_DIR) -MT $@ -MMD -MP -MF $(RISCV_OUT_DIR)/$*.Td $(RISCV_INCLUDE) -c $(RISCV_CFLAGS) $< -o $@
	@mv -f $(RISCV_OUT_DIR)/$*.Td $(RISCV_OUT_DIR)/$*.d > /dev/null

#!
#! @brief Compile code using the RISC-V medium-low model.  This promises that all linker sections are within 2GB.
#!	  This forces the compiler to generate PC relative addressing.
#!        We're leaving linker relaxation on, so these will generally be folded into more efficient absolute addressing.
#!
#! See @ref KernelBootstrap for locations where we must disable linker relaxation.
#!
RISCV_CFLAGS=-mcmodel=medlow -mabi=lp64f

#!
#! @brief Linker scripts will fail to place variables in sections if common is used
#!
RISCV_CFLAGS+=-fno-common

#!
#! @brief LIBOS is compiled against the standard headers to simplify future safety work.
#!	  This tells GCC that stdint.h/stddef.h are being used without a CRT.
#!
RISCV_CFLAGS+=-ffreestanding -nostdlib -fno-exceptions

RISCV_CFLAGS+= -I$(LIBOS_SOURCE)/kernel --std=gnu11 -g -gdwarf-2 -g

RISCV_CFLAGS+=-Wall -Werror -Wundef -Wno-pointer-to-int-cast

# Enable full inliner for full builds (import for buddy allocator)
ifeq ($(LIBOS_DEBUG),1)
    RISCV_CFLAGS+=-Og
else
	ifeq ($(LIBOS_TINY),1)
	RISCV_CFLAGS+=-Os -fomit-frame-pointer
	else
	    RISCV_CFLAGS+=-Os -fomit-frame-pointer
	endif
endif

RISCV_CFLAGS+=-DLIBOS

##
##	Kernel source files
##

vpath %.c $(LIBOS_SOURCE)/kernel

$(RISCV_OUT_DIR)/libos-config.h: $(PEREGRINE_PROFILE_PATH)/config
	@printf  " [ %-28s ] %-14s  %s\n" $(PREFIX) " CONFIG " $<
	@mkdir -p $(RISCV_OUT_DIR)
	@sed -e 's:#.*::;s/\([^=]*\)=\(.*\)/#define \1 \2/' $(PEREGRINE_PROFILE_PATH)/config > $(RISCV_OUT_DIR)/libos-config.h

-include $(PEREGRINE_PROFILE_PATH)/config

#!
#!	Enable compressed ISA if present in profile
#!
ifeq ($(LIBOS_CONFIG_RISCV_COMPRESSED),1)
	RISCV_CFLAGS += -march=rv64imfc
else
	RISCV_CFLAGS += -march=rv64imf
endif

ifeq ($(LIBOS_CONFIG_LTO),1)
	RISCV_CFLAGS += -flto
endif

ifeq ($(LIBOS_FEATURE_PAGING),1)
   vpath %.c $(LIBOS_SOURCE)/kernel/mmu-hashed
   RISCV_CFLAGS+=-I$(LIBOS_SOURCE)/kernel/mmu-hashed
else
   vpath %.c $(LIBOS_SOURCE)/kernel/mpu
   RISCV_CFLAGS+=-I$(LIBOS_SOURCE)/kernel/mpu
endif

ifeq ($(shell echo '$(LIBOS_LWRISCV) >= 200' | bc),1)
	vpath %.c $(LIBOS_SOURCE)/kernel/lwriscv-2.0
endif

ifeq ($(LIBOS_FEATURE_BAREMODE),1)
else
RISCV_SOURCES += \
		bootstrap.c \
		coverity-externs.c \
		init.c \
		configuration-validation.c \
		panic.c \
		task.c \
		scheduler.c \
		port.c \
		lock.c \
		syscall_port.c \
		syscall_timer.c \
		timer.c \
		trap.c \
		list.c \
		cache.c \
		bitops.c \
		kprintf.c 

ifeq ($(shell echo '$(LIBOS_LWRISCV) >= 200' | bc),1)
	RISCV_SOURCES += sbi.c gdma.c
endif

ifeq ($(LIBOS_FEATURE_PAGING),1)
	RISCV_SOURCES += paging.c pagetable.c
else
	RISCV_SOURCES += mpu.c
endif

ifeq ($(LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND),1)
	RISCV_SOURCES += profiler.c
endif

ifeq ($(LIBOS_CONFIG_EXTERNAL_INTERRUPT),1)
	RISCV_SOURCES += extintr.c
endif
endif

RISCV_OBJECTS=$(patsubst %.c,$(RISCV_OUT_DIR)/%.o,$(RISCV_SOURCES))
