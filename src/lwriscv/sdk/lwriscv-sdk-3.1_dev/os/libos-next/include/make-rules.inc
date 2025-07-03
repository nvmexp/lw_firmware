##
##	Environment check
##
include $(LIBOS_SOURCE)/kernel/colors.mk

$(if $(value LIBOS_SOURCE),,$(error LIBOS_SOURCE not set))
$(if $(value P4ROOT),,$(error P4ROOT not set))

LW_TOOLS ?= $(P4ROOT)/tools

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
TOOLCHAIN_PREFIX=riscv64-unknown-elf-
endif

# @todo: Need to decide on host compiler vs so called reference compilers.
#        Walk user through choices and require an environment variable

RISCV_CC=$(TOOLCHAIN_PREFIX)gcc
RISCV_AS=$(TOOLCHAIN_PREFIX)as
RISCV_OBJCOPY?=$(TOOLCHAIN_PREFIX)objcopy

LWRISCVX_SDK_ROOT = $(P4ROOT)/sw/lwriscv/main
LW_SDK_INC = $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals

RISCV_INCLUDE=-I$(PEREGRINE_PROFILE_PATH) -I$(LIBOS_SOURCE) -I$(LW_ENGINE_MANUALS) -I$(LW_SDK_INC) -I$(LIBOS_SOURCE)/include -I$(CHIP_SWREF) -I$(CHIP_HWREF)

-include $(LIBOS_SOURCE)/profiles/$(PEREGRINE_PROFILE)/libos-config.mk


##
##	RISCV compiler rules
##

# @todo: Remove -DLIBOS_PEREGRINE_1_0 to variable
$(RISCV_OUT_DIR)/%.o : %.c 
	@mkdir -p $(RISCV_OUT_DIR)
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " C " $<
	@$(RISCV_CC) $(RISCV_CFLAGS) -I$(RISCV_OUT_DIR) -flto -MT $@ -MMD -MP -MF $(RISCV_OUT_DIR)/$*.Td $(RISCV_INCLUDE) -c $(RISCV_CFLAGS) $< -o $@
	@mv -f $(RISCV_OUT_DIR)/$*.Td $(RISCV_OUT_DIR)/$*.d > /dev/null

# Bootloader files also have BOOT_CFLAGS
$(BOOTLOADER_OUT_DIR)/%.o : %.c
	@mkdir -p $(BOOTLOADER_OUT_DIR)
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " C-PIC " $<
	@$(RISCV_CC) $(BOOT_CFLAGS) $(RISCV_CFLAGS) -I$(RISCV_OUT_DIR) -flto -MT $@ -MMD -MP -MF $(BOOTLOADER_OUT_DIR)/$*.Td $(RISCV_INCLUDE) -c $(RISCV_CFLAGS) $< -o $@
	@mv -f $(BOOTLOADER_OUT_DIR)/$*.Td $(BOOTLOADER_OUT_DIR)/$*.d > /dev/null

# Preprocess and compile assembly
$(RISCV_OUT_DIR)/%.o : %.s $(RISCV_OUT_DIR)/manifest_defines.h
	@mkdir -p $(RISCV_OUT_DIR)
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " PP-AS " $<
	@$(RISCV_CC) -x c $(RISCV_CFLAGS) -I$(RISCV_OUT_DIR) -DASSEMBLY -MMD -MT $@ -E $(RISCV_INCLUDE) -c $(RISCV_CFLAGS) $< -o $@.p
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " AS " $@.p
	@$(RISCV_AS) $(RISCV_ASFLAGS) -I$(RISCV_OUT_DIR) $@.p -o $@

# Preprocess and compile assembly
# (Bootloader rules)
$(BOOTLOADER_OUT_DIR)/%.o : %.s  $(RISCV_OUT_DIR)/manifest_defines.h
	@mkdir -p $(BOOTLOADER_OUT_DIR)
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " PP-AS " $<
	@$(RISCV_CC) -x c $(RISCV_CFLAGS) -I$(RISCV_OUT_DIR) -MMD -MT $@ -E $(RISCV_INCLUDE) -c $(RISCV_CFLAGS) $< -o $@.p
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " AS " $@.p
	@$(RISCV_AS) $(RISCV_ASFLAGS) -I$(RISCV_OUT_DIR) $@.p -o $@


#!
#! @brief Compile code using the RISC-V medium-low model.  This promises that all linker sections are within 2GB.
#!	  This forces the compiler to generate PC relative addressing.
#!        We're leaving linker relaxation on, so these will generally be folded into more efficient absolute addressing.
#!
#! See @ref KernelBootstrap for locations where we must disable linker relaxation.
#!

#! NOTE for f extention in mabi/march settings:
#! Lwrrently enabling f extention in order for kernel objects to be linkable to liblwriscv for 2.x RISCV cores.
#! There are works need to be done to regulate its enablement per profile configs.
ifeq ($(LIBOS_TINY),1)
  RISCV_CFLAGS=-mcmodel=medlow -mabi=lp64f
else
  # Medium any model is required for kernel to allow builtin tasks to be shifted out of the kernel address space
  RISCV_CFLAGS=-mcmodel=medany -mabi=lp64f
endif
RISCV_ASFLAGS=-mabi=lp64f

#!
#!	Enable compressed ISA if present in profile
#!
ifeq ($(LIBOS_CONFIG_RISCV_COMPRESSED),1)
 RISCV_CFLAGS+=-march=rv64imfc
 RISCV_ASFLAGS+=-march=rv64imfc
else
 RISCV_CFLAGS+=-march=rv64imf
 RISCV_ASFLAGS+=-march=rv64imf
endif

#!
#! @brief Linker scripts will fail to place variables in sections if common is used
#!
RISCV_CFLAGS+=-fno-common

#!
#! @brief LIBOS is compiled against the standard headers to simplify future safety work.
#!	  This tells GCC that stdint.h/stddef.h are being used without a CRT.
#!
RISCV_CFLAGS+=-ffreestanding -nostdlib -fno-exceptions

RISCV_CFLAGS+= -I$(LIBOS_SOURCE)/kernel --std=gnu11 -gdwarf-2 -g

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

# Bootloader wants to be position independent
BOOT_CFLAGS=-mplt -no-pie -mcmodel=medany -mno-relax 
