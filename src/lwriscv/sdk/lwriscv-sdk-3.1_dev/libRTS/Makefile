# Do required basic initializations
LIBRTS_ROOT         ?= $(LWRDIR)
LWRISCVX_SDK_ROOT   ?= $(LIBRTS_ROOT)/..
LIBRTS_OUT          ?= $(LIBRTS_ROOT)/_out
LIBRTS_SRC          := $(LIBRTS_ROOT)/src
LIBRTS_INC          := $(LIBRTS_ROOT)/inc
LIBRTS_BUILD        := $(LIBRTS_ROOT)/build
LIBRTS_LIB          := $(LIBRTS_ROOT)/lib


# Do required pre-config initializations
LIBRTS_INSTALL_DIR  ?= $(LIBRTS_LIB)/$(LIBRTS_CHIP_FAMILY)/riscv/$(LIBRTS_PROFILE)
LIB_INSTALL_DIR     ?= $(LIBRTS_INSTALL_DIR)/lib
INC_INSTALL_DIR     ?= $(LIBRTS_INSTALL_DIR)/inc
TARGET              := $(LIB_INSTALL_DIR)/libRTS.a

# Sanity check on configuration
include $(LIBRTS_BUILD)/checkconfig.mk

# Postconfig of features - configure extra flags etc
include $(LIBRTS_BUILD)/postconfig.mk

#RTS SOURCE FILES
SRC_FILES += libRTS.c

RTS_SRC_FILES += $(addprefix src/,$(SRC_FILES))

# Do required post-config initializations
RTS_HEADER_COPY_DIRS    :=  src/
RTS_HEADER_COPY_DIRS    +=  inc/

RTS_INCLUDES            :=  $(LIBRTS_INC)

INCLUDES                +=  $(RTS_INCLUDES)

RTS_SRC_FILENAMES       :=  $(notdir $(RTS_SRC_FILES))
RTS_SRC_DIRS            :=  $(sort $(dir $(RTS_SRC_FILES)))

OBJECTS                 +=  $(RTS_SRC_FILENAMES:.c=.o)
OBJECTS                 :=  $(addprefix $(LIBRTS_OUT)/,$(OBJECTS))

###########################
#       RISCV build       #
###########################

# Do required pre-check RISCV initializations
LIBLWRISCV_DIR       := $(LIBLWRISCV_INSTALL_DIR)
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk

# Perform basic RISCV checks
ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
    $(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Do required post-check RISCV initializations
LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
AR_OPTIONS          := rsD

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Build environment for RISCV
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

# Check if RTS support is enabled
ifneq ($(LWRISCV_HAS_RTS), y)
    $(error RTS support is not enabled)
endif

# Add includes
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
INCLUDES += $(LIBLWRISCV_DIR)/inc
INCLUDES := $(addprefix -I,$(INCLUDES))

# # Add C flags
CFLAGS += $(INCLUDES)

# PRINT HELLO
$(info ========================================================================)
$(info ==================           LIBRTS            =========================)
$(info Profile:            $(LIBRTS_PROFILE))
$(info Chip configuration: $(LWRISCV_IS_CHIP_FAMILY) $(LWRISCV_IS_CHIP) $(LWRISCV_IS_ENGINE))
$(info RISC-V mode:        $(LWRISCV_CONFIG_CPU_MODE))
$(info Output directory:   $(LIBLWRISCV_BUILD_DIR))
$(info Using manuals:      $(LW_ENGINE_MANUALS))
$(info ========================================================================)

# Define required rules
define add-rule-c =
$(LIBRTS_OUT)/%.o: $(1)/%.c
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach c,$(RTS_SRC_DIRS),$(eval $(call add-rule-c,$(c))))

# Build targets
install: add-dirs $(TARGET)
	$(RSYNC) -avm --chmod=u=rwX --include='*.h' -f 'hide,! */' $(RTS_HEADER_COPY_DIRS) $(INC_INSTALL_DIR)

$(TARGET): $(LIBRTS_OUT) $(OBJECTS)
	$(AR) $(AR_OPTIONS) $(TARGET) $(OBJECTS)

add-dirs:
	$(INSTALL) -d $(LIB_INSTALL_DIR)
	$(INSTALL) -d $(INC_INSTALL_DIR)
	$(INSTALL) -d $(LIBRTS_OUT)

clean:
	$(RM) -f $(LIB_INSTALL_DIR)/*
	$(RM) -f $(INC_INSTALL_DIR)/*
	$(RM) -f $(LIBRTS_OUT)/*

clobber: clean
