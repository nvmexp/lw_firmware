# Hard code the tool-chain
CHAIN=$(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin
CC=$(CHAIN)/riscv64-elf-gcc
OBJCOPY = $(CHAIN)/riscv64-elf-objcopy
AR = $(CHAIN)/riscv64-elf-ar

# Input sources
SOURCES= \
	bootstrap.c \
	coverity-externs.c \
	dma.c \
	extintr.c \
	init.c \
	interrupts.c \
	paging.c \
	profiler.c \
	task.c \
	port.c \
	timer.c \
	trap.c \
	list.c \
	bitops.c \
	ilwerted_pagetable.c \
	kernel_copy.c \
	mmu.c \
	memory.c \
	tcm_resident_page.c \
	suspend.c

COMMON_SOURCES = \
	bootstrap-chipmux.c

GH100_SOURCES = $(SOURCES) gdma.c partition.c

BUILD_CFG ?= debug

E=\e
OUT_DIR ?= $(shell pwd)/_out/$(BUILD_CFG)

# Create output dirs before rules are run (every time this file is parsed)
$(shell mkdir -p $(OUT_DIR)/tu10x $(OUT_DIR)/ga100 $(OUT_DIR)/ga10x $(OUT_DIR)/gh100)

default: $(OUT_DIR)/kernel.a

#!
#! @brief This should eventually be controled by libos.py
#!
CFLAGS_BASE=-DLIBOS_FEATURE_ISOLATION -DLIBOS_KERNEL

#!
#! @brief Compile code using the RISC-V medium-low model.  This promises that all linker sections are within 2GB.
#!	  This forces the compiler to generate PC relative addressing.
#!        We're leaving linker relaxation on, so these will generally be folded into more efficient absolute addressing.
#!
#! See @ref libos_start and @ref kernel_bootstrap for locations where we must disable linker relaxation.
#!
CFLAGS_BASE+=-mcmodel=medlow -march=rv64im  -mabi=lp64

#!
#! @brief LIBOS is compiled against the standard headers to simplify future safety work.
#!	  This tells GCC that stdint.h/stddef.h are being used without a CRT.
#!
CFLAGS_BASE+=-ffreestanding -nostdlib -fno-exceptions

#!
#! @brief LIBOS is compiled once for each platform.  The global symbol kernel_main is then renamed, and all other symbols
#!         are marked as local (static) by objcopy.  Unfortunatelly, objcopy will not localize "common" symbols.
#!	  This guarantees that no such symbols are emitted.
#!
#!  Impact: This prevents some data symbols from the GA100 and Tu10x kernels from being merged.  This would
#!	   immediately present as a fatal fault in the test harness
#!
CFLAGS_BASE+=-fno-common

CFLAGS_BASE+=-fomit-frame-pointer

CFLAGS_BASE+= -I. --std=gnu11 -g -gdwarf-2 -I$(LIBOS_SOURCE) -I$(LW_SOURCE) -I$(LW_SOURCE)/sdk/lwpu/inc -I$(LIBOS_SOURCE)/include -g

#!
#! @brief LIBOS is compiled with global optimizations enabled.  We link the object files together to form a
#!	  kernel library.  That library will be linked against the file data-tables.
#!
#!  Impact: This allows the intermediate ELF to contain references to external symbols.
#!	    Any unresolved references will be seen at the final link @ref Makefile.template
#!
CFLAGS_BASE+=-Wl,-r

CFLAGS_BASE+=-Wall -Werror -Wundef

CFLAGS=$(CFLAGS_BASE) -Os

$(OUT_DIR)/tu10x/%.o : %.c
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C TU10X" $(subst $(LW_SOURCE)/,,$<)
	@$(CC) -flto -MT $@ -MMD -MP -MF $(OUT_DIR)/tu10x/$*.Td -DLIBOS_EXOKERNEL_TU10X -I$(LW_SOURCE)/drivers/common/inc/swref/turing/tu102 -I$(LW_SOURCE)/drivers/common/inc/hwref/turing/tu102 -c $(CFLAGS) $< -o $@
	@mv -f $(OUT_DIR)/tu10x/$*.Td $(OUT_DIR)/tu10x/$*.d > /dev/null

$(OUT_DIR)/ga100/%.o : %.c
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C GA100" $(subst $(LW_SOURCE)/,,$<)
	@$(CC) -flto -MT $@ -MMD -MP -MF $(OUT_DIR)/ga100/$*.Td -DLIBOS_EXOKERNEL_GA100 -I$(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100 -I$(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100  -c $(CFLAGS) $< -o $@
	@mv -f $(OUT_DIR)/ga100/$*.Td $(OUT_DIR)/ga100/$*.d > /dev/null

$(OUT_DIR)/ga10x/%.o : %.c
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C GA10X" $(subst $(LW_SOURCE)/,,$<)
	@$(CC) -flto -MT $@ -MMD -MP -MF $(OUT_DIR)/ga10x/$*.Td -DLIBOS_EXOKERNEL_GA10X -I$(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102 -I$(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102  -c $(CFLAGS) $< -o $@
	@mv -f $(OUT_DIR)/ga10x/$*.Td $(OUT_DIR)/ga10x/$*.d > /dev/null

$(OUT_DIR)/gh100/%.o : %.c
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C GH100" $(subst $(LW_SOURCE)/,,$<)
	@$(CC) -flto -MT $@ -MMD -MP -MF $(OUT_DIR)/gh100/$*.Td -DLIBOS_EXOKERNEL_GH100 -I$(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100 -I$(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100  -c $(CFLAGS) $< -o $@
	@mv -f $(OUT_DIR)/gh100/$*.Td $(OUT_DIR)/gh100/$*.d > /dev/null

$(OUT_DIR)/%.o : %.c
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C" $(subst $(LW_SOURCE)/,,$<)
	@$(CC) -MT $@ -MMD -MP -MF $(OUT_DIR)/$*.Td -I$(LW_SOURCE)/drivers/common/inc/swref/ampere/ga100 -I$(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga100 -c $(CFLAGS) $< -o $@
	@mv -f $(OUT_DIR)/$*.Td $(OUT_DIR)/$*.d > /dev/null

# Build a kernel specialized (and optimized for) TU10X
TU10X_OBJECTS=$(patsubst %.c,$(OUT_DIR)/tu10x/%.o,$(SOURCES))
$(OUT_DIR)/kernel-tu10x.o: $(TU10X_OBJECTS)
	@$(CC) $(CFLAGS)  -flto $^ -o $@.raw  -Wl,--script=kernel-section.ldscript
	@$(OBJCOPY) -G tu10x_kernel_bootstrap  $@.raw $@ --redefine-sym kernel_bootstrap=tu10x_kernel_bootstrap

# Build a GA100 optimized kernel
GA100_OBJECTS=$(patsubst %.c,$(OUT_DIR)/ga100/%.o,$(SOURCES))
$(OUT_DIR)/kernel-ga100.o: $(GA100_OBJECTS)
	@$(CC) $(CFLAGS)  -flto $^ -o $@.raw  -Wl,--script=kernel-section.ldscript -Wl,-r
	@$(OBJCOPY) -G ga100_kernel_bootstrap $@.raw $@ --redefine-sym kernel_bootstrap=ga100_kernel_bootstrap

# Build a GA10X optimized kernel
GA10X_OBJECTS=$(patsubst %.c,$(OUT_DIR)/ga10x/%.o,$(SOURCES))
$(OUT_DIR)/kernel-ga10x.o: $(GA10X_OBJECTS)
	@$(CC) $(CFLAGS)  -flto $^ -o $@.raw  -Wl,--script=kernel-section.ldscript -Wl,-r
	@$(OBJCOPY) -G ga10x_kernel_bootstrap $@.raw $@ --redefine-sym kernel_bootstrap=ga10x_kernel_bootstrap

# Build a GH100 optimized kernel
GH100_OBJECTS=$(patsubst %.c,$(OUT_DIR)/gh100/%.o,$(GH100_SOURCES))
$(OUT_DIR)/kernel-gh100.o: $(GH100_OBJECTS)
	@$(CC) $(CFLAGS)  -flto $^ -o $@.raw  -Wl,--script=kernel-section.ldscript -Wl,-r
	@$(OBJCOPY) -G gh100_kernel_bootstrap $@.raw $@ --redefine-sym kernel_bootstrap=gh100_kernel_bootstrap

# Objects compiled once
COMMON_OBJECTS:=$(patsubst %.c,$(OUT_DIR)/%.o,$(COMMON_SOURCES))

DEPENDS = $(patsubst %.o,%.d, $(COMMON_OBJECTS) $(TU10X_OBJECTS) $(GA100_OBJECTS) $(GA10X_OBJECTS) $(GH100_OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)

# See: https://www.sifive.com/blog/all-aboard-part-4-risc-v-code-models
#

$(OUT_DIR)/kernel.a: $(OUT_DIR)/bootstrap-chipmux.o $(OUT_DIR)/kernel-tu10x.o $(OUT_DIR)/kernel-ga100.o $(OUT_DIR)/kernel-ga10x.o $(OUT_DIR)/kernel-gh100.o
	@printf  " [ %-28s ] $E[93m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " AR" $(shell basename $@)
	@$(AR) rcs $@ $^

clean:
	@rm -rf $(OUT_DIR)
