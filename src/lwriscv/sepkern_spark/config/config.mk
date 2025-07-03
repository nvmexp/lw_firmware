LWUPROC         := $(LW_SOURCE)/uproc
LWRISCV         := $(LWUPROC)/lwriscv
BUILD_SCRIPTS   := $(LWUPROC)/build/scripts

SEPKERN_SW      := $(LWRISCV)/sepkern_spark
SEPKERN_SW_SRC  := $(SEPKERN_SW)/src
SEPKERN_CFG     := $(SEPKERN_SW)/config


MANIFEST_LOC    := $(SEPKERN_SW)/manifests

######################################################################################
# GPRBUILD ###########################################################################
PROJ            ?= ga10x
ENGINE          ?= pmu

IMEM_START_ADDRESS := 0x0000000000100000
DMEM_START_ADDRESS := 0x0000000000180000

IMEM_LIMIT      := 0x800
DMEM_LIMIT      := 0x400

GPR_NAME        := separation_kernel.gpr
SPARK_OBJ_DIR   := obj/$(ENGINE)_$(PROJ)
OUTPUTDIR       := _out/$(ENGINE)_$(PROJ)
TARGET_NAME     ?= g_sepkern_$(ENGINE)_$(PROJ)_riscv

RELEASE_PATH    := $(LW_SOURCE)/drivers/resman/kernel/inc/fmc/monitor/bin/$(ENGINE)/$(PROJ)

###############################################################################
# Define the names and locations for the various images that are generated
# during the build-process (IMG_*). Also create the list of files that need
# updated with those images when the install-mode is enabled.
###############################################################################
IMG_TARGET          := $(OUTPUTDIR)/$(TARGET_NAME)
IMG_TARGET_DEBUG    := $(OUTPUTDIR)/$(TARGET_NAME)_debug
ELF_TARGET          := $(IMG_TARGET)

ELF_FILE            := $(ELF_TARGET).elf
ELF_NO_SYM          := $(ELF_TARGET)_nosym.elf
ELF_OBJDUMP         := $(ELF_TARGET).objdump
ELF_NM              := $(ELF_TARGET).nm
ELF_READELF         := $(ELF_TARGET).readelf

IMG_CODE            := $(IMG_TARGET)_image.text
IMG_DATA            := $(IMG_TARGET)_image.data
IMG_CODE_ENC        := $(IMG_TARGET)_image.text.encrypt.bin
IMG_CODE_ENC_DEBUG  := $(IMG_TARGET_DEBUG)_image.text.encrypt.bin
IMG_DATA_ENC        := $(IMG_TARGET)_image.data.encrypt.bin
IMG_DATA_ENC_DEBUG  := $(IMG_TARGET_DEBUG)_image.data.encrypt.bin
IMG_DESC_BIN        := $(IMG_TARGET)_desc.bin
IMG_SIGN            := $(IMG_TARGET)_sign.bin
IMG_SIG_H           := $(IMG_TARGET)_sig.h

######################################################################################
# MANIFEST ###########################################################################
MANIFEST_H      := $(MANIFEST_LOC)/manifest_$(PROJ).h
# Template for manifest source: $(MANIFEST_LOC)/manifest_$(ENGINE)_$(PROJ)$(SUFFIXES)
MANIFEST_SUFFIXES   := .c
ifeq ($(ENGINE), pmu)
# inst_in_sys manifest
MANIFEST_SUFFIXES     += _inst.c
endif

# MANIFEST_OUTS and MANIFEST_HEX is generated in makefile
# Warning: do *not* use := here, it has to be lazy expanded.

# Add SK IMG_CODE/IMG_DATA to RELEASE_FILES so SK || BL can be bundled together and signed as FMC for GSP-RM

RELEASE_FILES = $(IMG_CODE) $(IMG_DATA)

# Those files are needed to actually build image
ifneq ($(SKIP_PROD_SIGN), 1)
RELEASE_FILES +=  $(IMG_CODE_ENC) $(IMG_DATA_ENC) $(MANIFEST_OUTS_PRD)
endif
RELEASE_FILES += $(IMG_CODE_ENC_DEBUG) $(IMG_DATA_ENC_DEBUG) $(MANIFEST_OUTS_DBG)
# And those files are for debugging
RELEASE_FILES += $(ELF_FILE) $(ELF_NM) $(ELF_OBJDUMP)
RELEASE_FILES += $(ELF_READELF) $(MANIFEST_HEXS)

INC := -I$(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
INC += -I$(LW_SOURCE)/sdk/lwpu/inc
INC += -I$(LW_SOURCE)/drivers/common/inc
INC += -I$(LWRISCV)/inc

ifeq ($(ENGINE), gsp)
  CFLAGS += -DGSP_RTOS=1
endif

ifeq ($(ENGINE), pmu)
  CFLAGS += -DPMU_RTOS=1
endif

CFLAGS += $(INC)

######################################################################################
# TOOLS ##############################################################################
GNAT_PRO_PATH     := $(LW_TOOLS)/adacore/gnat_pro_ada/linux/gnat_pro_ent_ada/target_riscv/20200209
SPARK_PRO_PATH    := $(LW_TOOLS)/adacore/gnat_pro_ada/linux/spark_pro/20200212
RISCV_TOOLCHAIN   := $(LW_TOOLS)/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-
MANIFEST_SIGGEN   ?= $(MANIFEST_LOC)/siggen_manifest
######################################################################################

export IMEM_LIMIT
export DMEM_LIMIT
export IMEM_START_ADDRESS
export DMEM_START_ADDRESS
