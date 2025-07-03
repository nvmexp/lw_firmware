APP_ROOT = $(LWRDIR)/..
OUT_DIR = $(APP_ROOT)/_out/mgmt
LWRISCVX_SDK_ROOT = $(APP_ROOT)/../..

# Select a default profile
LIBLWRISCV_PROFILE_NAME ?= basic-libos-gh100-fsp
PEREGRINE_PROFILE ?= fsp-gh100-mpu-sampleapp-debug

USE_PREBUILT_SDK ?= true
ifeq ($(USE_PREBUILT_SDK),true)
    LIBLWRISCV_DIR = $(LWRISCVX_SDK_ROOT)/prebuilt/$(LIBLWRISCV_PROFILE_NAME)
else
    LIBLWRISCV_DIR = $(APP_ROOT)/sdk-$(LIBLWRISCV_PROFILE_NAME).bin
endif

BUILD_CFG ?= debug
PREFIX ?= local

LW_ENGINE_MANUALS = $(P4ROOT)/sw/resman/manuals

##
##	RISC-V build
##

RISCV_OUT_DIR = $(OUT_DIR)
SIMULATION_OUT_DIR = $(OUT_DIR)/sim
RISCV_SOURCES = init_main.c \
				libprintf.c \
				libport.c   \
				libtime.c   \
				libinit.c   \
				libdma.c

RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc
RISCV_INCLUDE += -I$(LWRISCVX_SDK_ROOT)/inc
RISCV_INCLUDE += -I$(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC/hwref
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC/common
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto/include
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto_private
RISCV_INCLUDE += -I$(LIBLWRISCV_DIR)/inc/libCCC/common_crypto_private/lwpka

TARGET = $(OUT_DIR)/libmgmt.a
MANIFEST_HEADER = $(RISCV_OUT_DIR)/manifest_defines.h
MANIFEST_COMPILER = $(SIMULATION_OUT_DIR)/libosmc

.PHONY: default
default: $(TARGET) $(MANIFEST_HEADER) $(MANIFEST_COMPILER)

-include $(LIBOS_SOURCE)/kernel/Makefile.inc
-include $(LIBOS_SOURCE)/simulator/Makefile.inc

$(TARGET): $(RISCV_OBJECTS)
	$(RISCV_CC_AR) -crs $@ $^

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

$(MANIFEST_HEADER): $(SIMULATION_OUT_DIR)/manifest_defines.o
	@$(SIMULATOR_CC) -O2 -m64 -g $^ -o $(SIMULATION_OUT_DIR)/manifest_defines
	@$(SIMULATION_OUT_DIR)/manifest_defines > $(RISCV_OUT_DIR)/manifest_defines.h

##
##	Manifest compilation
##

$(MANIFEST_COMPILER): $(SIMULATION_OUT_DIR)/libosmc.o $(SIMULATION_OUT_DIR)/libelf.o $(SIMULATION_OUT_DIR)/libdwarf.o
	@$(SIMULATOR_CXX) -Od -m64 -g $^ -o $@

clean:
	@rm -rf $(OUT_DIR)
