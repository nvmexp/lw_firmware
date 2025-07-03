# Do required basic initializations
LIBCCC_ROOT       ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(LIBCCC_ROOT)/..
LIBCCC_OUT        ?= $(LIBCCC_ROOT)/_out
LIBCCC_CONFIGS    := $(LIBCCC_ROOT)/configs
LIBCCC_SRC        := $(LIBCCC_ROOT)/src
LIBCCC_INC        := $(LIBCCC_ROOT)/inc
LIBCCC_BUILD      := $(LIBCCC_ROOT)/build
LIBCCC_LIB        := $(LIBCCC_ROOT)/lib

# Perform basic checks
ifeq ($(LIBCCC_CHIP_FAMILY),)
    $(error Please select a LibCCC chip family.)
endif

ifeq ($(LIBCCC_PROFILE),)
    $(error Please select a LibCCC profile to build.)
else
    LIBCCC_PROFILE_FILE := $(LIBCCC_CONFIGS)/$(LIBCCC_CHIP_FAMILY)/$(LIBCCC_PROFILE).mk
    $(info Using profile file $(LIBCCC_PROFILE_FILE).)
endif

# Do required pre-config initializations
UPROC_ARCH           ?= RISCV
UPROC_ARCH_LOWERCASE := $(shell echo $(UPROC_ARCH) | tr A-Z a-z)
LIBCCC_INSTALL_DIR   ?= $(LIBCCC_LIB)/$(LIBCCC_CHIP_FAMILY)/$(UPROC_ARCH_LOWERCASE)/$(LIBCCC_PROFILE)
LIB_INSTALL_DIR      ?= $(LIBCCC_INSTALL_DIR)/lib
INC_INSTALL_DIR      ?= $(LIBCCC_INSTALL_DIR)/inc/libCCC
TARGET               := $(LIB_INSTALL_DIR)/libCCC.a
CCC_SRC_FILES        :=

# Include default configuration
include $(LIBCCC_CONFIGS)/defaults.mk

# Include configuration as per the profile
include $(LIBCCC_PROFILE_FILE)

# Sanity check on configuration
include $(LIBCCC_BUILD)/checkconfig.mk

# Postconfig of features - configure extra flags etc
include $(LIBCCC_BUILD)/postconfig.mk

LIBCCC_LOCAL_MANUAL_DIR ?= inc/hwref
ifeq ($(LIBCCC_USE_LOCAL_MANUALS),)
    LIBCCC_LOCAL_MANUAL_DIR :=
endif

# Do required post-config initializations
AR_OPTIONS           :=

CCC_HEADER_COPY_DIRS := src/                            \
                        inc/common                      \
                        $(LIBCCC_LOCAL_MANUAL_DIR)      \
                        $(CONFIG_INCLUDE)
CCC_INCLUDES         := src/common_crypto               \
                        src/common_crypto/include       \
                        src/common_crypto_private       \
                        src/common_crypto_private/lwpka \
                        inc/common                      \
                        $(LIBCCC_LOCAL_MANUAL_DIR)      \
                        $(CONFIG_INCLUDE)
INCLUDES             += $(CCC_INCLUDES)

CCC_SRC_FILENAMES    := $(notdir $(CCC_SRC_FILES))
CCC_SRC_DIRS         := $(sort $(dir $(CCC_SRC_FILES)))

OBJECTS              += $(CCC_SRC_FILENAMES:.c=.o)
OBJECTS              := $(addprefix $(LIBCCC_OUT)/,$(OBJECTS))

ifeq ($(UPROC_ARCH), RISCV)
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
    LIBLWRISCV_CONFIG_H  := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
    AR_OPTIONS           := rsD

    # Preconfigure chip (mini chip-config)
    include $(LIBLWRISCV_CONFIG_MK)

    # Build environment for RISCV
    include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

    # Do required post-config RISCV initializations
    LW_ENGINE_MANUALS := $(LW_MANUALS_ROOT)/$(LWRISCV_IS_CHIP_FAMILY)/$(LWRISCV_IS_CHIP)

    # Add includes
    INCLUDES += $(LW_ENGINE_MANUALS)
    INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
    INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
    INCLUDES += $(LIBLWRISCV_DIR)/inc
    INCLUDES := $(addprefix -I,$(INCLUDES))

    # Add libs
    LIBS := -L$(LIBLWRISCV_DIR)/lib -llwriscv

    # Add C flags
    CFLAGS += -include $(LIBLWRISCV_CONFIG_H)
    CFLAGS += -Og -g -std=c11
    CFLAGS += -mabi=$(LWRISCV_HAS_ABI) -march=$(LWRISCV_HAS_ARCH)
    CFLAGS += -fomit-frame-pointer -fno-strict-aliasing -mcmodel=medany
    CFLAGS += -nostartfiles -nostdlib -ffreestanding
    CFLAGS += -Werror -Wall -Wstrict-prototypes -ffunction-sections -pipe -Wtype-limits -Wcolwersion -Wsign-colwersion -Wsign-compare -Wint-in-bool-context
    CFLAGS += -fdiagnostics-color=always
    CFLAGS += -DUPROC_ARCH_RISCV
    CFLAGS += $(INCLUDES)
    # Path mangling for deterministic builds
    CFLAGS += -ffile-prefix-map=$(realpath $(LWRISCVX_SDK_ROOT))=.
    CFLAGS += -ffile-prefix-map=$(realpath $(LW_TOOLS))=.
    CFLAGS += $(EXTRA_CFLAGS)

    # Add LD flags
    LDFLAGS += -Wl,-melf64lriscv
    LDFLAGS += -Wl,--gc-sections
    LDFLAGS += -Wl,--sort-common=descending
    LDFLAGS += -Wl,--sort-section=alignment
    LDFLAGS += $(LIBS)
    LDFLAGS += $(EXTRA_LDFLAGS)

else ifeq ($(UPROC_ARCH), FALCON)
    ###########################
    #       FALCON build      #
    ###########################

    # Do required pre-check Falcon initializations
    # <Add here if needed>

    # Perform basic Falcon checks
    ifeq ($(wildcard $(LW_ENGINE_MANUALS)),)
        $(error Set LW_ENGINE_MANUALS to where your engine manuals are.)
    endif

    ifeq ($(LIBCCC_ENGINE),)
        $(error Set LIBCCC_ENGINE to the engine you are building libCCC for.)
    endif

    # Do required post-check Falcon initializations
    LIBCCC_ENGINE_LOWERCASE := $(shell echo $(LIBCCC_ENGINE) | tr A-Z a-z)

    # Build environment for Falcon
    FALCON_MAJOR_VERSION ?= 6
    FALCON_MINOR_VERSION ?= 5.4
    FALCON_TOOLCHAIN     := $(LW_TOOLS)/falcon-gcc/falcon$(FALCON_MAJOR_VERSION)/$(FALCON_MAJOR_VERSION).$(FALCON_MINOR_VERSION)/Linux/falcon-elf-gcc/bin/falcon-elf-
    INSTALL              := /usr/bin/install
    RSYNC                := /usr/bin/rsync
    CC                   := $(FALCON_TOOLCHAIN)gcc
    AR                   := $(FALCON_TOOLCHAIN)ar
    AR_OPTIONS           := -r -c

    # Add includes
    INCLUDES += $(LW_ENGINE_MANUALS)
    INCLUDES := $(addprefix -I,$(INCLUDES))

    # Add C flags
    CFLAGS += -DCCC_ENGINE_$(LIBCCC_ENGINE)
    CFLAGS += -Os -g
    CFLAGS += -fomit-frame-pointer -fno-strict-aliasing
    CFLAGS += -nostartfiles -nostdlib -ffreestanding
    CFLAGS += -Werror -Wall -Wstrict-prototypes -ffunction-sections
    CFLAGS += -DUPROC_ARCH_FALCON
    CFLAGS += $(INCLUDES)
    # Path mangling for deterministic builds
    CFLAGS += -fdebug-prefix-map=$(realpath $(LWRISCVX_SDK_ROOT))=.
    CFLAGS += -fdebug-prefix-map=$(realpath $(LW_TOOLS))=.
    CFLAGS += $(EXTRA_CFLAGS)

    # Add LD flags
    LDFLAGS += -Wl,--gc-sections
    LDFLAGS += -Wl,--sort-common=descending
    LDFLAGS += -Wl,--sort-section=alignment
    LDFLAGS += $(LIBS)
    LDFLAGS += $(EXTRA_LDFLAGS)
else
    ###########################
    #       INVALID build     #
    ###########################

    $(error Invalid architecture selected. Only 'RISCV' and 'FALCON' supported.)
endif

# Define required rules
define add-rule-c =
$(LIBCCC_OUT)/%.o: $(1)/%.c
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach c,$(CCC_SRC_DIRS),$(eval $(call add-rule-c,$(c))))

# Build targets
install: add-dirs $(TARGET)
	$(RSYNC) -avm --chmod=u=rwX --include='*.h' -f 'hide,! */' $(CCC_HEADER_COPY_DIRS) $(INC_INSTALL_DIR)

$(TARGET): $(LIBCCC_OUT) $(OBJECTS)
	$(AR) $(AR_OPTIONS) $(TARGET) $(OBJECTS)

add-dirs:
	$(INSTALL) -d $(LIB_INSTALL_DIR)
	$(INSTALL) -d $(INC_INSTALL_DIR)
	$(INSTALL) -d $(LIBCCC_OUT)

clean:
	$(RM) -f $(LIB_INSTALL_DIR)/*
	$(RM) -f $(INC_INSTALL_DIR)/*
	$(RM) -f $(LIBCCC_OUT)/*

clobber: clean
