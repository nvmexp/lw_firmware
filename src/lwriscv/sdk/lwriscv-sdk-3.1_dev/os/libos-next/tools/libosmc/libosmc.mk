# Select a default profile
PEREGRINE_PROFILE ?= fsp-gh100-mpu-debug

all:

include $(LIBOS_SOURCE)/kernel/colors.mk

BUILD_CFG ?= debug
PREFIX?=sim-$(PEREGRINE_PROFILE)
LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals
LW_TOOLS ?= $(P4ROOT)/tools
PEREGRINE_PROFILE_PATH = $(LIBOS_SOURCE)/profiles/$(PEREGRINE_PROFILE)
SIMULATION_OUT_DIR = $(BUILD_CFG)-sim-$(PEREGRINE_PROFILE)

##
##	Simulator toolchain setup
##    (used for test-driver)
ifeq (,$(wildcard /home/utils/gcc-9.3.0))
SIMULATOR_CC=gcc -static 
SIMULATOR_CXX=g++ -static 
else
SIMULATOR_CC=/home/utils/gcc-9.3.0/bin/gcc -static 
SIMULATOR_CXX=/home/utils/gcc-9.3.0/bin/g++ -static
endif

LWRISCVX_SDK_ROOT = $(P4ROOT)/sw/lwriscv/main
LW_SDK_INC = $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals

# Toolchain detection
msys_version := $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)

# Libos-config
-include $(LIBOS_SOURCE)/profiles/$(PEREGRINE_PROFILE)/libos-config.mk

##
##	Simulator search path
##
vpath %.cpp $(LIBOS_SOURCE)/simulator
vpath %.c $(LIBOS_SOURCE)/lib
vpath %.c $(LIBOS_SOURCE)/debug
vpath %.cpp $(LIBOS_SOURCE)/tools/libosmc
vpath %.c $(LIBOS_SOURCE)/tools/libosmc

##
##	Simulator compilation flags
##
SIMULATOR_INCLUDE = -I$(PEREGRINE_PROFILE_PATH) -I$(LIBOS_SOURCE)/include -I$(SIMULATION_OUT_DIR) -I$(LW_ENGINE_MANUALS) -I$(LW_SDK_INC) -I$(LIBOS_SOURCE) -I$(SIMULATION_OUT_DIR) -I$(LIBOS_SOURCE)/kernel

## Simulator C/CPP compilation rules
$(SIMULATION_OUT_DIR)/%.o : %.cpp 
	@mkdir -p $(SIMULATION_OUT_DIR)
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " C++ " $<
	$(SIMULATOR_CXX) -flto -c -MT $@ -MMD -MP -MF $(SIMULATION_OUT_DIR)/$*.Td --std=c++14 -w -Os -DLIBOS_HOST -DTRACE $(SIMULATOR_INCLUDE) -g $< -o $@
	@mv -f $(SIMULATION_OUT_DIR)/$*.Td $(SIMULATION_OUT_DIR)/$*.d > /dev/null

$(SIMULATION_OUT_DIR)/%.o : %.c 
	@mkdir -p $(SIMULATION_OUT_DIR)
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " C " $<
	@$(SIMULATOR_CC) -flto -c -MT $@ -MMD -MP -MF $(SIMULATION_OUT_DIR)/$*.Td --std=c11 -w -Os -DLIBOS_HOST -DTRACE $(SIMULATOR_INCLUDE) -g $< -o $@
	@mv -f $(SIMULATION_OUT_DIR)/$*.Td $(SIMULATION_OUT_DIR)/$*.d > /dev/null

DEPENDS = $(patsubst %.o,%.d, $(SIMULATOR_OBJECTS)) $(patsubst %.o,%.d, $(RISCV_OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)

##
##	Unit test
##
ifeq ($(LIBOS_TINY),1)
all: $(SIMULATION_OUT_DIR)/libosmc
	p4 edit $(LIBOS_SOURCE)/bin/libosmc-$(PEREGRINE_PROFILE) || true
	cp $(SIMULATION_OUT_DIR)/libosmc $(LIBOS_SOURCE)/bin/libosmc-$(PEREGRINE_PROFILE)
	strip $(LIBOS_SOURCE)/bin/libosmc-$(PEREGRINE_PROFILE)
	p4 add $(LIBOS_SOURCE)/bin/libosmc-$(PEREGRINE_PROFILE) || true
endif

clean:
	@rm -rf test.log $(SIMULATION_OUT_DIR) $(LIBOS_SOURCE)/bin/libosmc-$(PEREGRINE_PROFILE) || true

$(SIMULATION_OUT_DIR)/libosmc: $(SIMULATION_OUT_DIR)/libosmc.o $(SIMULATION_OUT_DIR)/libelf.o $(SIMULATION_OUT_DIR)/libdwarf.o
	@$(SIMULATOR_CXX) -Os -m64 -g $^ -o $@
