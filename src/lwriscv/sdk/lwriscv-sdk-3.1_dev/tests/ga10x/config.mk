LW_TOOLS ?= $(ROOT)/../../../tools/
ENGINE ?= pmu
CHIP ?= ga102
LW_TARGET_ARCH ?= x86_64
INSTALL_DIR := $(ROOT)/bin
INC := $(ROOT)/inc
RISCV_TOOLCHAIN ?= $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-
SIGGEN ?= $(ROOT)/tools/siggen_manifest

export ENGINE
export CHIP
export RISCV_TOOLCHAIN
export INSTALL_DIR
export INC
export SIGGEN
export LW_TARGET_ARCH
