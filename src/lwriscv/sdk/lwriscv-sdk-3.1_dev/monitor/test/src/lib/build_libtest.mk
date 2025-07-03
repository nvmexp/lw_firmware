LIB_ROOT ?= $(LWRDIR)
LIB_OUT ?= test_lib
TARGET := $(BUILD_DIR)/libtest.a

LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_INSTALL_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_INSTALL_DIR)/g_lwriscv_config.mk

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
    $(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Preconfigures compilers
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Setup compilation flags
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

# Check environment
ifeq ($(wildcard $(CC)),)
    $(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_INSTALL_DIR)/inc
# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
# In case we need lwmisc etc.
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
# Add manuals
INCLUDES += $(LW_ENGINE_MANUALS)
INCLUDES := $(addprefix -I,$(INCLUDES))

# Link with liblwriscv
LIBS := -L$(LIBLWRISCV_INSTALL_DIR)/lib -llwriscv

CFLAGS += -Og
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)

LIB_OBJS += test.o
LIB_OBJS += set_timer.o
LIB_OBJS += shutdown.o
LIB_OBJS += switch_to.o
LIB_OBJS += ilwalid_extension.o
LIB_OBJS += release_priv_lockdown.o
LIB_OBJS += update_tracectl.o
LIB_OBJS += fbif_transcfg.o
LIB_OBJS += fbif_regioncfg.o
LIB_OBJS += tfbif_transcfg.o
LIB_OBJS += tfbif_regioncfg.o
LIB_OBJS := $(addprefix $(BUILD_DIR)/,$(LIB_OBJS))

$(BUILD_DIR)/%.o: $(LIB_ROOT)/%.c $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
		$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(LIB_ROOT)/%.S $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
		$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(LIB_OBJS)
	$(AR) rsD $(TARGET) $(LIB_OBJS)

.PHONY: build_libtest
build_libtest: $(TARGET)
