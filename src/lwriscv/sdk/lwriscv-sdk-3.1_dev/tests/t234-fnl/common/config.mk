# Defaults that may be overwritten
ENGINE ?= sec
CHIP ?= t234
INSTALL_DIR ?= $(ROOT)/bin

# Most updated part :)
COMMON_OBJS := io.o
COMMON_OBJS += print.o
COMMON_OBJS += vprintfmt.o

INC := $(ROOT)/inc
RISCV_TOOLCHAIN ?= $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-
SIGGEN ?= $(ROOT)/tools/siggen_manifest
CC:=$(RISCV_TOOLCHAIN)gcc
LD:=$(RISCV_TOOLCHAIN)ld
STRIP:=$(RISCV_TOOLCHAIN)strip
OBJDUMP:=$(RISCV_TOOLCHAIN)objdump
OBJCOPY:=$(RISCV_TOOLCHAIN)objcopy

ARCH:=rv64im
ISA:=RV64IM

COMMON_INC := -I$(INC)
COMMON_INC += -I$(COMMON_DIR)
COMMON_INC += -I$(HWREF) -I$(HWREF)/t234 -I$(HWREF)/ga10b
COMMON_INC += -I$(INC)/lwpu-sdk

COMMON_DEFINES :=
COMMON_DEFINES += LWRISCV64_MANUAL_CSR=\"dev_$(ENGINE)_riscv_csr_64.h\"
COMMON_DEFINES += LWRISCV64_MANUAL_ADDRESS_MAP=\"lw_$(ENGINE)_riscv_address_map.h\"
COMMON_DEFINES += LWRISCV64_MANUAL_LOCAL_IO=\"dev_$(ENGINE)_prgnlcl.h\"
COMMON_DEFINES += LWRISCV_CONFIG_CPU_MODE=1

COMMON_CFLAGS :=
COMMON_CFLAGS +=-std=c11 -Wall -Wextra -ffunction-sections
COMMON_CFLAGS +=-ffreestanding -ffast-math -fno-common -fno-jump-tables
COMMON_CFLAGS +=-Og -g -fdiagnostics-color=always
COMMON_CFLAGS += -DENGINE_$(ENGINE)
COMMON_CFLAGS +=-mabi=lp64 -march=$(ARCH)
COMMON_CFLAGS += $(addprefix -D,$(DEFINES))
COMMON_CFLAGS += -MMD -MF $(@F).d

COMMON_LDFLAGS := -nostdlib -nostartfiles -fdiagnostics-color=always

COMMON_OBJS := $(addprefix $(COMMON_DIR)/,$(COMMON_OBJS))

export ENGINE
export CHIP
export RISCV_TOOLCHAIN
export INSTALL_DIR
export INC
export SIGGEN
