SIMULATION_OUT_DIR ?= $(BUILD_CFG)-sim-$(PEREGRINE_PROFILE)

##
##	Simulator toolchain setup
##    (used for test-driver)
ifeq (,$(wildcard /home/utils/gcc-9.3.0))
SIMULATOR_CC  ?= gcc -static
SIMULATOR_CXX ?= g++ -static
else
SIMULATOR_CC  ?= /home/utils/gcc-9.3.0/bin/gcc -static
SIMULATOR_CXX ?= /home/utils/gcc-9.3.0/bin/g++ -static
endif

LWRISCVX_SDK_ROOT = $(P4ROOT)/sw/lwriscv/main
LW_SDK_INC = $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals

# Toolchain detection
msys_version := $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)

ifeq ($(msys_version), 0)
SIMULATOR_LIBS=
else
SIMULATOR_LIBS=-lwsock32 -lws2_32
endif

##
##	Simulator search path
##
vpath %.cpp $(LIBOS_SOURCE)/simulator
vpath %.c $(LIBOS_SOURCE)/lib
vpath %.c $(LIBOS_SOURCE)/debug
vpath %.cpp $(LIBOS_SOURCE)/manifest-compiler
vpath %.c $(LIBOS_SOURCE)/manifest-compiler

##
##	Simulator source files
##

SIMULATOR_SOURCES += \
	libelf.c \
	riscv-core.cpp \
	riscv-trace.cpp \
	riscv-gdb.cpp  \
	gsp.cpp \
	libdwarf.c \
	logdecode.c

ifeq ($(LIBOS_LWRISCV), 100)
	SIMULATOR_SOURCES += gsp_tu11x.cpp
endif

ifeq ($(LIBOS_LWRISCV), 101)
	SIMULATOR_SOURCES += gsp_ga100.cpp
endif

ifeq ($(LIBOS_LWRISCV), 200)
	SIMULATOR_SOURCES += gsp_ga102.cpp
endif

SIMULATOR_OBJECTS=$(patsubst %.cpp,$(SIMULATION_OUT_DIR)/%.o,$(patsubst %.c,$(SIMULATION_OUT_DIR)/%.o,$(SIMULATOR_SOURCES)))

$(SIMULATION_OUT_DIR)/libos-config.h: $(PEREGRINE_PROFILE_PATH)/config
	@printf  " [ %-28s ] %-14s$  %s\n" $(PREFIX) " CONFIG " $<
	@mkdir -p $(SIMULATION_OUT_DIR)
	@sed -e 's:#.*::;s/\([^=]*\)=\(.*\)/#define \1 \2/' $(PEREGRINE_PROFILE_PATH)/config > $(SIMULATION_OUT_DIR)/libos-config.h

##
##	Simulator compilation flags
##
SIMULATOR_INCLUDE = -I$(PEREGRINE_PROFILE_PATH) -I$(LIBOS_SOURCE)/include -I$(SIMULATION_OUT_DIR) -I$(LW_ENGINE_MANUALS) -I$(LW_SDK_INC) -I$(LIBOS_SOURCE) -I$(SIMULATION_OUT_DIR)

## Simulator C/CPP compilation rules
$(SIMULATION_OUT_DIR)/%.o : %.cpp $(SIMULATION_OUT_DIR)/libos-config.h
	@mkdir -p $(SIMULATION_OUT_DIR)
	@printf  " [ %-28s ] %-14s$  %s\n" $(PREFIX) " C++ " $<
	@$(SIMULATOR_CXX) -c -MT $@ -MMD -MP -MF $(SIMULATION_OUT_DIR)/$*.Td --std=c++14 -w -Og -DSIM -DTRACE $(SIMULATOR_INCLUDE) -g $< -o $@
	@mv -f $(SIMULATION_OUT_DIR)/$*.Td $(SIMULATION_OUT_DIR)/$*.d > /dev/null

$(SIMULATION_OUT_DIR)/%.o : %.c $(SIMULATION_OUT_DIR)/libos-config.h
	@mkdir -p $(SIMULATION_OUT_DIR)
	@printf  " [ %-28s ] %-14s$  %s\n" $(PREFIX) " C " $<
	@$(SIMULATOR_CC) -c -MT $@ -MMD -MP -MF $(SIMULATION_OUT_DIR)/$*.Td --std=c11 -w -Os -DSIM -DTRACE $(SIMULATOR_INCLUDE) -g $< -o $@
	@mv -f $(SIMULATION_OUT_DIR)/$*.Td $(SIMULATION_OUT_DIR)/$*.d > /dev/null

## Simulator final binary
$(SIMULATION_OUT_DIR)/simulator: $(SIMULATOR_OBJECTS)
	@printf  " [ %-28s ] %-14s$  %s\n" $(PREFIX) " LD" $<
	@rm -f $(SIMULATION_OUT_DIR)/passed.*
	@$(SIMULATOR_CXX) --std=c++14 -w -Os $(SIMULATOR_TRACE) $(SIMULATOR_INCLUDE) -g $^ -o $(SIMULATION_OUT_DIR)/simulator $(SIMULATOR_LIBS)
