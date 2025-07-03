# Select a default profile
PEREGRINE_PROFILE ?= fsp-gh100-mpu-debug

BUILD_CFG ?= debug
PREFIX?=local

LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals
all:

##
##	RISC-V build
##

LIBOS_FEATURE_BAREMODE=1
RISCV_SOURCES = init_main.c \
				sbi.c \
				bootstrap_tiny.s \
				context.s \
				list.c \
				kprintf.c \
				mpu.c \
				libtime.c \
				panic.c \
				libbit.c \
				libprintf.c

RISCV_OUT_DIR = $(BUILD_CFG)-riscv-$(PEREGRINE_PROFILE)
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
##	Final Link
##
$(RISCV_OUT_DIR)/layout.ld: layout.ld $(RISCV_OUT_DIR)/manifest_defines.h
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LDSCRIPT " "layout.ld"
	@$(RISCV_CC) -E -x c -DMAKEFILE -I$(PEREGRINE_PROFILE_PATH) -I$(LW_ENGINE_MANUALS) -I$(RISCV_OUT_DIR) -I$(SIMULATION_OUT_DIR) -I$(LIBOS_SOURCE)/include -I$(LIBOS_SOURCE)/kernel layout.ld | grep -v '^#' > $(RISCV_OUT_DIR)/layout.ld

$(RISCV_OUT_DIR)/firmware.elf: $(RISCV_OBJECTS)
$(RISCV_OUT_DIR)/firmware.elf: $(RISCV_OUT_DIR)/layout.ld
	@printf  " [ %-55s ] $(COLOR_RED_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" "$(PREFIX)" " LD" "firmware.elf"
	$(RISCV_CC) -flto -w $(RISCV_CFLAGS) $(RISCV_OBJECTS) -o $@ -Wl,--script=$(RISCV_OUT_DIR)/layout.ld -Wl,--gc-sections


firmware: $(RISCV_OUT_DIR)/firmware.elf
.PHONY: firmware

##
##	Unit test
##
ifeq ($(LIBOS_TINY),1)
all: $(RISCV_OUT_DIR)/diff.log
endif
$(RISCV_OUT_DIR)/test.log: firmware
	@cd $(RISCV_OUT_DIR) && $(LIBOS_SOURCE)/bin/sim-$(PEREGRINE_PROFILE) firmware.elf > test.log

$(RISCV_OUT_DIR)/diff.log: $(RISCV_OUT_DIR)/test.log reference.log
	@cat reference.log | grep -v Libos: | grep -v SK-Model: > $(RISCV_OUT_DIR)/reference.user.log
	@cat $(RISCV_OUT_DIR)/test.log  | grep -v Libos: | grep -v SK-Model: > $(RISCV_OUT_DIR)/test.user.log
	@diff -w $(RISCV_OUT_DIR)/reference.user.log $(RISCV_OUT_DIR)/test.user.log 

clean:
	@rm -rf test.log $(RISCV_OUT_DIR) || true
