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
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(TEST_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(TEST_IMEM_LIMIT)

# Link with liblwriscv
LIBS := -L$(LIBLWRISCV_INSTALL_DIR)/lib -llwriscv

CFLAGS += -Og
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)
CFLAGS += $(EXTRA_CFLAGS)

MANIFEST_SRC    := $(MANIFEST_DIR)/$(MANIFEST_FILE)
MANIFEST        := $(BUILD_DIR)/$(CHIP)-$(ENGINE).manifest.o
MANIFEST_BIN    := $(BUILD_DIR)/$(CHIP)-$(ENGINE).manifest

.PHONY: build_manifest

build_manifest : $(MANIFEST_BIN)

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST) $(MANIFEST_BIN)

$(MANIFEST): $(MANIFEST_SRC)
	$(CC) $(CFLAGS) $(MANIFEST_SRC) $(INCLUDES) $(DEFINES) -DSEPKERN_IMEM_LIMIT=0x1000 -DSEPKERN_DMEM_LIMIT=0x1000 -include $(LIBLWRISCV_CONFIG_H) -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)
