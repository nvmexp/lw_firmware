# Select a default profile
PEREGRINE_PROFILE ?= gsp-ga10x-debug
LIBOS_SOURCE ?= $(LWRDIR)/../../../os/libos-next

BUILD_CFG ?= debug
PREFIX?=local

LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals
all:

##
##	RISC-V build
##

RISCV_OUT_DIR = $(BUILD_CFG)-kernel-$(PEREGRINE_PROFILE)
BOOTLOADER_OUT_DIR = $(BUILD_CFG)-bootloader-$(PEREGRINE_PROFILE)
		
-include $(LIBOS_SOURCE)/kernel/Makefile.inc
-include $(LIBOS_SOURCE)/simulator/Makefile.inc

# Include liblwriscv headers
RISCV_INCLUDE += $(LIBLWRISCV_INC)
RISCV_CFLAGS += -include $(LIBLWRISCV_CONFIG_H)

include $(LIBLWRISCV_MK)

BRIDGE_OBJECTS = \
	$(BOOTLOADER_OUT_DIR)/sbi.o \
	$(BOOTLOADER_OUT_DIR)/init-partition-bridge.o \
	$(BOOTLOADER_OUT_DIR)/init-partition-bridge-entry.o \


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
					 
##
## Incremental build
##

DEPENDS = $(patsubst %.o,%.d, $(SIMULATOR_OBJECTS)) $(patsubst %.o,%.d, $(RISCV_OBJECTS)) $(patsubst %.o,%.d, $(BOOTLOADER_OBJECTS)) $(patsubst %.o,%.d, $(BRIDGE_OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)


##
##	init task elf
##
INIT_OBJECTS = $(RISCV_OUT_DIR)/init_main.o $(RISCV_OUT_DIR)/libprintf.o $(RISCV_OUT_DIR)/libport.o $(RISCV_OUT_DIR)/libinit.o $(RISCV_OUT_DIR)/libmem.o $(RISCV_OUT_DIR)/libipc.o

$(RISCV_OUT_DIR)/init.elf: $(INIT_OBJECTS)
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "init.elf"
	$(RISCV_CC) -flto -w $(RISCV_CFLAGS) $(INIT_OBJECTS) $(LIBLWRISCV_LIB) -llwriscv -o $@ -Wl,--gc-sections -Wl,--no-relax

##
##	worker task elf
##
WORKER_OBJECTS = $(RISCV_OUT_DIR)/test.o $(RISCV_OUT_DIR)/libprintf.o $(RISCV_OUT_DIR)/libport.o $(RISCV_OUT_DIR)/libmem.o $(RISCV_OUT_DIR)/libinit.o

$(RISCV_OUT_DIR)/worker.elf: $(WORKER_OBJECTS)
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "worker.elf"
	@$(RISCV_CC) -e _start  -flto -w $(RISCV_CFLAGS) $(WORKER_OBJECTS) -o $@ -Wl,--gc-sections


##
##	bootloader
##
$(BOOTLOADER_OUT_DIR)/start.elf: $(BOOTLOADER_OBJECTS)
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "start.elf"
	$(RISCV_CC) -e _start -flto -w $(BOOT_CFLAGS) $(RISCV_CFLAGS) $^ -o $@ -Wl,--script=start.ld -Wl,--gc-sections

$(RISCV_OUT_DIR)/start.bin: $(BOOTLOADER_OUT_DIR)/start.elf
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " OBJCOPY" "$@"
	@$(RISCV_OBJCOPY) -O binary -j .text -j .data $(BOOTLOADER_OUT_DIR)/start.elf $@

##
##	init partition bridge
##
$(BOOTLOADER_OUT_DIR)/bridge.elf: $(BRIDGE_OBJECTS)
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "bridge.elf"
	$(RISCV_CC) -e bridge_entry -flto -w $(BOOT_CFLAGS) $(RISCV_CFLAGS) $^ -o $@ -Wl,--script=bridge.ld -Wl,--gc-sections

$(RISCV_OUT_DIR)/bridge.text: $(BOOTLOADER_OUT_DIR)/bridge.elf
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " OBJCOPY" "$@"
	@$(RISCV_OBJCOPY) -O binary -j .text $(BOOTLOADER_OUT_DIR)/bridge.elf $@

$(RISCV_OUT_DIR)/bridge.data: $(BOOTLOADER_OUT_DIR)/bridge.elf
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " OBJCOPY" "$@"
	@$(RISCV_OBJCOPY) -O binary -j .data $(BOOTLOADER_OUT_DIR)/bridge.elf $@

##
##	root filesystem
##
FILESYSTEM_OBJECTS = kernel.elf \
                     init.elf \
		     worker.elf

vpath %.elf $(RISCV_OUT_DIR)

$(RISCV_OUT_DIR)/firmware.image: $(SIMULATION_OUT_DIR)/mkboot $(FILESYSTEM_OBJECTS)
	@echo $(FILESYSTEM_OBJECTS) | xargs -n 1 > $(RISCV_OUT_DIR)/image.list  
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " MKBOOT" "$@"
	@cd $(RISCV_OUT_DIR); ../$(SIMULATION_OUT_DIR)/mkboot @image.list  firmware.image

##
##	final image
##
$(RISCV_OUT_DIR)/firmware: $(RISCV_OUT_DIR)/start.bin $(RISCV_OUT_DIR)/firmware.image 
	@printf  " [ %-28s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " CONCAT" "$@"
	cat $^ > $@

firmware: $(RISCV_OUT_DIR)/firmware
.PHONY: firmware

bridge: $(RISCV_OUT_DIR)/bridge.text $(RISCV_OUT_DIR)/bridge.data
.PHONY: bridge

all: firmware bridge

.PHONY: install
install: firmware bridge
	cp $(BOOTLOADER_OUT_DIR)/start.elf $(BOOTLOADER_OUT_DIR)/bridge.elf $(RISCV_OUT_DIR)/worker.elf $(RISCV_OUT_DIR)/init.elf $(RISCV_OUT_DIR)/kernel.elf $(RISCV_OUT_DIR)/start.bin $(RISCV_OUT_DIR)/firmware $(RISCV_OUT_DIR)/bridge.text $(RISCV_OUT_DIR)/bridge.data $(INSTALL_DIR)
