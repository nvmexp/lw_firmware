################################################################################
# Build environment
################################################################################

LW_MANUALS_ROOT ?= $(P4ROOT)/sw/resman/manuals

# Clean implicit variables
ASFLAGS :=
CFLAGS  :=
CPPFLAGS:=
LDFLAGS :=

################################################################################
# Build tools
################################################################################
# To be included by build_elw.mk
RISCV_TOOLCHAIN ?= $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-
SIGGEN ?= $(LWRISCVX_SDK_ROOT)/tools/siggen_manifest

ECHO := /bin/echo
ifeq ($(QUIET),1)
ECHOQ:=@$(ECHO)
SILENCE:=@
else
ECHOQ:=@\#$(ECHO)
SILENCE:=
endif

CC := $(SILENCE)$(RISCV_TOOLCHAIN)gcc
AS := $(SILENCE)$(RISCV_TOOLCHAIN)as
AR := $(SILENCE)$(RISCV_TOOLCHAIN)ar
NM := $(SILENCE)$(RISCV_TOOLCHAIN)nm
CPP := $(SILENCE)$(RISCV_TOOLCHAIN)cpp
OBJDUMP := $(SILENCE)$(RISCV_TOOLCHAIN)objdump
OBJCOPY := $(SILENCE)$(RISCV_TOOLCHAIN)objcopy
READELF := $(SILENCE)$(RISCV_TOOLCHAIN)readelf
BIN2HEX := $(LWRISCVX_SDK_ROOT)/tools/freedom-bin2hex.py
LD := $(SILENCE)$(RISCV_TOOLCHAIN)ld

# $(1): input elf, $(2): output hex
define ELF2HEX
$(OBJCOPY) $(1) -O binary $(2).temp
$(PY) $(BIN2HEX) -w 128 $(2).temp > $(2)
$(RM) $(2).temp
endef

DATE        := /bin/date
EXPR        := /usr/bin/expr
HOSTNAME    := /bin/hostname
INSTALL     := /usr/bin/install
MKDIR       := /bin/mkdir
P4          := /usr/bin/p4
PY          := $(LW_TOOLS)/unix/hosts/Linux-x86/python-3.5.1/bin/python3
RM          := /bin/rm
RSYNC       := /usr/bin/rsync
SED         := /bin/sed
WHOAMI      := /usr/bin/whoami
XXD         := /usr/bin/xxd
