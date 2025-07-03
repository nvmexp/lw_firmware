
BUILD_CFG ?= debug
PREFIX?=local

LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals
-include $(LIBOS_SOURCE)/profiles/$(PEREGRINE_PROFILE)/libos-config.mk

all:

RISCV_OUT_DIR = $(BUILD_CFG)-kernel-$(PEREGRINE_PROFILE)
BOOTLOADER_OUT_DIR = $(BUILD_CFG)-bootloader-$(PEREGRINE_PROFILE)

##
##	Core build rules
##
include $(LIBOS_SOURCE)/include/make-rules.inc

##
## The kernel contains server tasks that run in s-mode
## The libport.c code needs to know not to use 'ecall'
##
RISCV_CFLAGS+=-DLIBOS_SYSCALL_DIRECT_DISPATCH

##
##	Root partition source files
##
vpath %.c $(LIBOS_SOURCE)/root
vpath %.s $(LIBOS_SOURCE)/root
vpath %.c $(LIBOS_SOURCE)/lib
vpath %.c $(LIBOS_SOURCE)/debug



##
##	Kernel source files
##

vpath %.c $(LIBOS_SOURCE)/kernel
vpath %.s $(LIBOS_SOURCE)/kernel
vpath %.c $(LIBOS_SOURCE)/kernel/mm
vpath %.s $(LIBOS_SOURCE)/kernel/mm
vpath %.c $(LIBOS_SOURCE)/kernel/sched
vpath %.s $(LIBOS_SOURCE)/kernel/sched

ifeq ($(LIBOS_FEATURE_PAGING),1)
else
   vpath %.c $(LIBOS_SOURCE)/kernel/mpu
   RISCV_CFLAGS+=-I$(LIBOS_SOURCE)/kernel/mpu
endif

ifeq ($(shell echo '$(LIBOS_LWRISCV) >= 200' | bc),1)
	vpath %.c $(LIBOS_SOURCE)/kernel/lwriscv-2.0
endif

RISCV_SOURCES += \
		coverity-externs.c \
		init.c \
		panic.c \
		task.c \
		scheduler.c \
		port.c \
		lock.c \
		scheduler_syscall.c \
		syscall_timer.c \
		timer.c \
		list.c \
		cache.c \
		libbit.c \
		kprintf.c \
        context.s \
		libmem.c \
		syscall_dispatch.s  

ifeq ($(shell echo '$(LIBOS_LWRISCV) >= 200' | bc),1)
	RISCV_SOURCES += sbi.c
endif

ifeq ($(LIBOS_CONFIG_GDMA_SUPPORT),1)
	RISCV_SOURCES += gdma.c
endif


# Enable the full kernel
vpath %.c $(LIBOS_SOURCE)/kernel/server
RISCV_SOURCES += memorypool.c       \
					memoryset.c        \
					spinlock.c         \
					identity.c         \
					pagestate.c        \
					pin.c              \
					server.c 			\
					buddy.c 			\
					trap_ulwectored.s 	\
					bootstrap_softmmu.s\
					softmmu.s 			\
					softmmu_core.c     \
					pagetable.c        \
					objectpool.c 		\
					address_space.c 	\
					librbtree.c 		\
					rootfs.c 			\
					libbootfs.c 		\
					loader.c			\
					libtime.c			\
					libprintf.c		    \
					libroot.c			\
					global_port.c				\
					libport.c	 		\
					libdma.c            \
					mpu.c				\
					profiler.c			\
					extintr.c		


# Compute RISC-V objects
CO=$(patsubst %.c,$(RISCV_OUT_DIR)/%.o,$(RISCV_SOURCES))
RISCV_OBJECTS=$(patsubst %.s,$(RISCV_OUT_DIR)/%.o,$(CO))

# Compute bootloader objects
CO2=$(patsubst %.c,$(BOOTLOADER_OUT_DIR)/%.o,$(BOOTLOADER_SOURCES))
BOOTLOADER_OBJECTS=$(patsubst %.s,$(BOOTLOADER_OUT_DIR)/%.o,$(CO2))

##
##	Link to generate manifest header (including task size)
##
$(RISCV_OUT_DIR)/manifest_defines.h: $(LIBOS_SOURCE)/tools/libosmc/manifest_defines.c
	@mkdir -p $(RISCV_OUT_DIR)
	@$(RISCV_CC) $(RISCV_CFLAGS) $(RISCV_INCLUDE) -I$(RISCV_OUT_DIR) -S $^ -o - | grep '#define' > $@

##
## Incremental build
##

DEPENDS = $(patsubst %.o,%.d, $(RISCV_OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)

##
## kernel.elf link rules
##

vpath %.ld $(LIBOS_SOURCE)/kernel
$(RISCV_OUT_DIR)/kernel.ld: kernel.ld $(RISCV_OUT_DIR)/manifest_defines.h
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LDSCRIPT " "kernel.ld"
	@$(RISCV_CC) -E -x c -DMAKEFILE -I$(PEREGRINE_PROFILE_PATH) -I$(LW_ENGINE_MANUALS) -I$(RISCV_OUT_DIR) -I$(SIMULATION_OUT_DIR) -I$(LIBOS_SOURCE)/include -I$(LIBOS_SOURCE)/kernel $(LIBOS_SOURCE)/kernel/kernel.ld | grep -v '^#' > $(RISCV_OUT_DIR)/kernel.ld

$(RISCV_OUT_DIR)/kernel.elf: $(RISCV_OBJECTS)
$(RISCV_OUT_DIR)/kernel.elf: $(RISCV_OUT_DIR)/kernel.ld
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "kernel.elf"
	@$(RISCV_CC) -flto -w $(RISCV_CFLAGS) $(RISCV_OBJECTS) -o $@ -Wl,--script=$(RISCV_OUT_DIR)/kernel.ld -Wl,--gc-sections

##
## Pseudo-build rules
##

ifeq ($(LIBOS_TINY),0)
firmware: $(RISCV_OUT_DIR)/kernel.elf
	@p4 add $(LIBOS_SOURCE)/bin/kernel-$(PEREGRINE_PROFILE).elf || true
	@p4 add $(LIBOS_SOURCE)/bin/kernel-symbols-$(PEREGRINE_PROFILE).elf || true
	@p4 edit $(LIBOS_SOURCE)/bin/kernel-$(PEREGRINE_PROFILE).elf || true
	@p4 edit $(LIBOS_SOURCE)/bin/kernel-symbols-$(PEREGRINE_PROFILE).elf || true
	cp debug-kernel-$(PEREGRINE_PROFILE)/kernel.elf $(LIBOS_SOURCE)/bin/kernel-$(PEREGRINE_PROFILE).elf 
	$(TOOLCHAIN_PREFIX)strip $(LIBOS_SOURCE)/bin/kernel-$(PEREGRINE_PROFILE).elf
	cp debug-kernel-$(PEREGRINE_PROFILE)/kernel.elf $(LIBOS_SOURCE)/bin/kernel-symbols-$(PEREGRINE_PROFILE).elf

else
firmware:
endif

.PHONY: firmware

##
##	Unit test
##

clean:
	@rm -rf $(RISCV_OUT_DIR) || true
