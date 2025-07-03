# Select a default profile
PEREGRINE_PROFILE ?= fsp-gh100-full-debug

BUILD_CFG ?= debug
PREFIX?=local

LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals

all:

##
##	RISC-V build
##

RISCV_OUT_DIR = $(BUILD_CFG)-kernel-$(PEREGRINE_PROFILE)
BOOTLOADER_OUT_DIR = $(BUILD_CFG)-bootloader-$(PEREGRINE_PROFILE)

-include $(LIBOS_SOURCE)/include/make-rules.inc

vpath %.c $(LIBOS_SOURCE)/root
vpath %.s $(LIBOS_SOURCE)/root
vpath %.c $(LIBOS_SOURCE)/lib
vpath %.c $(LIBOS_SOURCE)/kernel/lwriscv-2.0

BOOTLOADER_OBJECTS = \
	$(BOOTLOADER_OUT_DIR)/start.o \
	$(BOOTLOADER_OUT_DIR)/libmem.o \
	$(BOOTLOADER_OUT_DIR)/libbootfs.o \
	$(BOOTLOADER_OUT_DIR)/init-partition.o \
	$(BOOTLOADER_OUT_DIR)/sbi.o \
	$(BOOTLOADER_OUT_DIR)/root-start.o \
	$(BOOTLOADER_OUT_DIR)/root.o \
	$(BOOTLOADER_OUT_DIR)/libbit.o \
	$(BOOTLOADER_OUT_DIR)/libprintf.o \
	$(BOOTLOADER_OUT_DIR)/libroot.o


# Compute RISC-V objects
CO=$(patsubst %.c,$(RISCV_OUT_DIR)/%.o,$(RISCV_SOURCES))
RISCV_OBJECTS=$(patsubst %.s,$(RISCV_OUT_DIR)/%.o,$(CO))

##
##	Link to generate manifest header (including task size)
##
$(RISCV_OUT_DIR)/manifest_defines.h: $(LIBOS_SOURCE)/tools/libosmc/manifest_defines.c
	@mkdir -p $(RISCV_OUT_DIR)
	@$(RISCV_CC) $(RISCV_CFLAGS) $(RISCV_INCLUDE) -I$(RISCV_OUT_DIR) -S $^ -o - | grep '#define' > $@

					 
##
## Incremental build
##

DEPENDS = $(patsubst %.o,%.d, $(RISCV_OBJECTS)) $(patsubst %.o,%.d, $(BOOTLOADER_OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)


##
##	Copy prebuilt kernel
##

$(RISCV_OUT_DIR)/libos-kernel.elf:
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "libos-kernel.elf"
	@cp $(LIBOS_SOURCE)/bin/kernel-$(PEREGRINE_PROFILE).elf $(RISCV_OUT_DIR)/libos-kernel.elf

##
##	init task elf
##
INIT_OBJECTS = $(RISCV_OUT_DIR)/init_main.o $(RISCV_OUT_DIR)/libprintf.o $(RISCV_OUT_DIR)/libport.o $(RISCV_OUT_DIR)/libinit.o $(RISCV_OUT_DIR)/libmem.o $(RISCV_OUT_DIR)/libipc.o $(RISCV_OUT_DIR)/liblock.o

$(RISCV_OUT_DIR)/init.elf: $(INIT_OBJECTS)
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "init.elf"
	@$(RISCV_CC) -flto -w $(RISCV_CFLAGS) $(INIT_OBJECTS) -o $@ -Wl,--gc-sections

##
##	worker task elf
##
WORKER_OBJECTS = $(RISCV_OUT_DIR)/test.o $(RISCV_OUT_DIR)/libprintf.o $(RISCV_OUT_DIR)/libport.o $(RISCV_OUT_DIR)/libmem.o $(RISCV_OUT_DIR)/libinit.o $(RISCV_OUT_DIR)/liblock.o

$(RISCV_OUT_DIR)/worker.elf: $(WORKER_OBJECTS)
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "worker.elf"
	@$(RISCV_CC) -e _start  -flto -w $(RISCV_CFLAGS) $(WORKER_OBJECTS) -o $@ -Wl,--gc-sections


##
##	bootloader
##
$(BOOTLOADER_OUT_DIR)/start.elf: $(BOOTLOADER_OBJECTS)
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "start.elf"
	@$(RISCV_CC) -e _start -flto -w $(BOOT_CFLAGS) $(RISCV_CFLAGS) $^ -o $@ -Wl,--script=start.ld -Wl,--gc-sections

$(RISCV_OUT_DIR)/start.bin: $(BOOTLOADER_OUT_DIR)/start.elf
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " OBJCOPY" "$@"
	@$(RISCV_OBJCOPY) -O binary -j .text $(BOOTLOADER_OUT_DIR)/start.elf $@

##
##	root filesystem
##
FILESYSTEM_OBJECTS = libos-kernel.elf \
				     init.elf \
					 worker.elf

vpath %.elf $(RISCV_OUT_DIR)

$(RISCV_OUT_DIR)/firmware.image: $(FILESYSTEM_OBJECTS)
	@echo $(FILESYSTEM_OBJECTS) | xargs -n 1 > $(RISCV_OUT_DIR)/image.list  
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " MKBOOT" "$@"
	@cd $(RISCV_OUT_DIR); $(LIBOS_SOURCE)/bin/mkboot @image.list  firmware.image

##
##	final image
##
$(RISCV_OUT_DIR)/firmware: $(RISCV_OUT_DIR)/start.bin $(RISCV_OUT_DIR)/firmware.image 
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " CONCAT" "$@"
	cat $^ > $@

firmware: $(RISCV_OUT_DIR)/firmware
.PHONY: firmware

##
##	Unit test
##
ifeq ($(LIBOS_TINY),0)
all: $(RISCV_OUT_DIR)/diff.log
endif

$(RISCV_OUT_DIR)/test.log: firmware
	@cd $(RISCV_OUT_DIR) && $(LIBOS_SOURCE)/bin/sim-$(PEREGRINE_PROFILE) firmware libos-kernel.elf init.elf > test.log

$(RISCV_OUT_DIR)/diff.log: $(RISCV_OUT_DIR)/test.log reference.log
	@cat reference.log | grep -v Libos: | grep -v SK-Model: | grep -v LWOS-Root: > $(RISCV_OUT_DIR)/reference.user.log
	@cat $(RISCV_OUT_DIR)/test.log  | grep -v Libos: | grep -v SK-Model: | grep -v LWOS-Root: > $(RISCV_OUT_DIR)/test.user.log
	@diff -w $(RISCV_OUT_DIR)/reference.user.log $(RISCV_OUT_DIR)/test.user.log 

clean:
	@rm -rf test.log $(RISCV_OUT_DIR) || true
