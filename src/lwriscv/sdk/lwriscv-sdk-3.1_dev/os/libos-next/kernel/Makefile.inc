#############################################################
#				LIBOS TINY Makefile include
#
#   Applications are compiled and linked directly against
#   the kernel in LIBOS_TINY configurations.  
#############################################################


##
##	Core build rules
##
include $(LIBOS_SOURCE)/include/make-rules.inc

ifeq ($(LIBOS_TINY),0)
$(error Makefile.inc is to be used ONLY for TINY kernel builds.)
endif

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
vpath %.c $(LIBOS_SOURCE)/kernel/tiny
vpath %.s $(LIBOS_SOURCE)/kernel/tiny
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

ifeq ($(LIBOS_FEATURE_BAREMODE),1)
else
RISCV_SOURCES += \
		coverity-externs.c \
		tiny_init.c \
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

vpath %.c $(LIBOS_SOURCE)/kernel/tiny
RISCV_SOURCES += trap_ulwectored_tiny.s \
					bootstrap_tiny.s \
					manifest.c

ifeq ($(LIBOS_FEATURE_PAGING),1)
	RISCV_SOURCES += paging.c pagetable.c
else
	RISCV_SOURCES += mpu.c
endif

	RISCV_SOURCES += extintr.c
endif

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
