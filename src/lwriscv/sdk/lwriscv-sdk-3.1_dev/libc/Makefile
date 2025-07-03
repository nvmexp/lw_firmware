LW_TOOLS ?= /home/mkulikowski/p4-tools/sw/tools
QUIET ?= 1

LIBC_ROOT ?= .
LWRISCVX_SDK_ROOT ?= $(LIBC_ROOT)/..

LIBC_SRC := $(LIBC_ROOT)/src
LIBC_INC := $(LIBC_ROOT)/inc
LIBC_BUILD_DIR?=_out
LIBC_INSTALL_DIR?= $(LIBC_BUILD_DIR)/staging

LWRISCV_HAS_ABI  ?= lp64f
LWRISCV_HAS_ARCH ?= rv64imfc

include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk
include $(LWRISCVX_SDK_ROOT)/build/build_flags.mk

TARGET := $(LIBC_BUILD_DIR)/libc.a

# Inlude sources
SRC:=
# Add libc sources that live outside of liblwriscv tree
src-y:=
subdirs-y:=
include $(LIBC_SRC)/sources.mk
SRC := $(addprefix $(LIBC_SRC)/,$(src-y))
SRC_DIRS:= $(sort $(dir $(SRC)))

# Make sure no two files have the same name
ifneq ($(words $(basename $(notdir $(SRC)))), $(words $(sort $(basename $(notdir $(SRC))))))
$(error Libc source file names are not unique. Fix that.)
endif

OBJS:= 
DEPS:=
# generate rule for files, assume unique files only
define add-file =
$(2): $(1) | $(LIBC_BUILD_DIR)
	$$(ECHOQ) $(if $(filter %.c,$(1)),CC,AS) $(1)
	$$(CC) $(if $(filter %.c,$(1)),$$(CFLAGS) $$(DEPFLAGS),$$(ASFLAGS)) -c $(1) -o $$@

OBJS += $(2)
DEPS += $(addsuffix .d,$(2))
endef

$(foreach c,$(SRC),$(eval $(call add-file,$(c),$(LIBC_BUILD_DIR)/$(basename $(notdir $(c))).o)))

vpath %.c $(SRC_DIRS)
vpath %.S $(SRC_DIRS)

# include dirs
INCLUDES += $(LIBC_BUILD_DIR) $(LIBC_INC)

CFLAGS += $(addprefix -I,$(INCLUDES))
# Path mangling for deterministic builds
CFLAGS += -ffile-prefix-map=$(realpath $(LWRISCVX_SDK_ROOT))=.
CFLAGS += -ffile-prefix-map=$(realpath $(LW_TOOLS))=.
ASFLAGS += $(addprefix -I,$(INCLUDES))

DEPFLAGS = -MT $@ -MMD -MP -MF $@.d

.DEFAULT_GOAL:= $(TARGET)

$(info ========================================================================)
$(info ==================         LWRISCV LIBC        =========================)
$(info Output directory:   $(LIBC_BUILD_DIR))
$(info ========================================================================)

$(TARGET): $(OBJS)
	$(ECHOQ) AR $(TARGET)
	$(AR) rsD $(TARGET) $(OBJS)

$(LIBC_BUILD_DIR):
	$(MKDIR) -p $@

clean:
	$(RM) -f $(OBJS) $(TARGET) $(DEPS)

install_headers:
	$(INSTALL) -d $(LIBC_INSTALL_DIR)/inc/libc
	$(INSTALL) -m 644 -t $(LIBC_INSTALL_DIR)/inc/libc $(wildcard $(LIBC_INC)/libc/*.h)

install: $(TARGET) install_headers
	$(INSTALL) -d $(LIBC_INSTALL_DIR)
	$(INSTALL) -d $(LIBC_INSTALL_DIR)/lib
	$(INSTALL) -m 644 -t $(LIBC_INSTALL_DIR)/lib $(TARGET)
	
.PHONY: install install_headers clean

$(DEPS):

-include $(DEPS)
