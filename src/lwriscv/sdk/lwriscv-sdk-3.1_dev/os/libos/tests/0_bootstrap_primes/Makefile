# Select a default profile
PEREGRINE_PROFILE ?= test-gh100-mpu

BUILD_CFG ?= debug
PREFIX?=local
E=\e

LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals
all:

##
##	RISC-V build
##

RISCV_OUT_DIR = $(BUILD_CFG)-riscv-$(PEREGRINE_PROFILE)
RISCV_SOURCES = init_main.c \
				libprintf.c \
				libport.c   \
				liblock.c   \
				libtime.c   \
				libinit.c   \
				libdma.c 
		
-include ../../kernel/Makefile.inc

##
## 	SIMULATOR BUILD
##

vpath %.cpp	../common
vpath %.cpp	../exelwtion-validation

SIMULATOR_SOURCES = \
  test-driver.cpp   \
  common.cpp        \
  register-preservation.cpp

-include ../../simulator/Makefile.inc

##
## Incremental build
##

DEPENDS = $(patsubst %.o,%.d, $(SIMULATOR_OBJECTS)) $(patsubst %.o,%.d, $(RISCV_OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)

##
##	Link to generate manifest header (including task size)
##
$(RISCV_OUT_DIR)/manifest_defines.h: $(SIMULATION_OUT_DIR)/manifest_defines.o
	@$(SIMULATOR_CC) -O2 -m64 -g $^ -o $(SIMULATION_OUT_DIR)/manifest_defines
	@mkdir -p $(RISCV_OUT_DIR)
	@$(SIMULATION_OUT_DIR)/manifest_defines > $(RISCV_OUT_DIR)/manifest_defines.h

##
##	Final Link
##
$(RISCV_OUT_DIR)/layout.ld: layout.ld $(RISCV_OUT_DIR)/manifest_defines.h
	@printf  " [ %-28s ] $E[91m$E[1m%-14s$E[39m$E[0m  %s\n" "$(PREFIX)" " LDSCRIPT " "layout.ld"
	@$(RISCV_CC) -E -x c -DMAKEFILE -I$(PEREGRINE_PROFILE_PATH) -I$(LW_ENGINE_MANUALS) -I$(RISCV_OUT_DIR) -I$(SIMULATION_OUT_DIR) -I$(LIBOS_SOURCE)/include -I$(LIBOS_SOURCE)/kernel layout.ld | grep -v '^#' > $(RISCV_OUT_DIR)/layout.ld

$(RISCV_OUT_DIR)/firmware.pre.elf: $(RISCV_OBJECTS)
$(RISCV_OUT_DIR)/firmware.pre.elf: $(RISCV_OUT_DIR)/layout.ld
	@printf  " [ %-28s ] $E[91m$E[1m%-14s$E[39m$E[0m  %s\n" "$(PREFIX)" " LD" "firmware.pre.elf"
	@$(RISCV_CC) -flto -w $(RISCV_CFLAGS) $(RISCV_OBJECTS) -o $@ -Wl,--script=$(RISCV_OUT_DIR)/layout.ld -Wl,--gc-sections

##
##	Manifest compilation
##

$(SIMULATION_OUT_DIR)/libosmc: $(SIMULATION_OUT_DIR)/libosmc.o $(SIMULATION_OUT_DIR)/libelf.o $(SIMULATION_OUT_DIR)/libdwarf.o
	@$(SIMULATOR_CXX) -Od -m64 -g $^ -o $@

$(RISCV_OUT_DIR)/firmware.elf: $(RISCV_OUT_DIR)/firmware.pre.elf $(SIMULATION_OUT_DIR)/libosmc
	@$(SIMULATION_OUT_DIR)/libosmc $(RISCV_OUT_DIR)/firmware.pre.elf -o $(RISCV_OUT_DIR)/firmware.elf

##
##	Helpers
##
simulator: $(SIMULATION_OUT_DIR)/simulator
.PHONY: simulator

firmware: $(RISCV_OUT_DIR)/firmware.elf
.PHONY: firmware

##
##	Unit test
##
all: $(RISCV_OUT_DIR)/diff.log

$(RISCV_OUT_DIR)/test.log: simulator firmware
	@cd $(RISCV_OUT_DIR) && ../$(SIMULATION_OUT_DIR)/simulator > test.log

$(RISCV_OUT_DIR)/diff.log: $(RISCV_OUT_DIR)/test.log reference.log
	@diff -w reference.log $(RISCV_OUT_DIR)/test.log 

clean:
	@rm -rf test.log $(SIMULATION_OUT_DIR) $(RISCV_OUT_DIR) || true
