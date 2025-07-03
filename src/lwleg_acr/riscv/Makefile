#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

APP_ROOT ?= $(LWRDIR)
LWRISCVX_SDK_ROOT ?= $(P4ROOT)/sw/lwriscv/main/
INSTALL_DIR := $(APP_ROOT)/output
USE_PREBUILT_SDK ?=true

# it must be built at this time
ifeq ($(USE_PREBUILT_SDK),true)
    PREBUILT_DIR := $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-$(ENGINE)
    LIBLWRISCV_DIR := $(PREBUILT_DIR)
else
    LIBLWRISCV_DIR := $(APP_ROOT)/liblwriscv-$(ENGINE).bin
endif
LIBLWRISCV_CONFIG_H := $(LIBLWRISCV_DIR)/inc/liblwriscv/g_lwriscv_config.h
LIBLWRISCV_CONFIG_MK := $(LIBLWRISCV_DIR)/g_lwriscv_config.mk
LIBLWRISCV_BUILD_FLAGS_MK :=$(LWRISCVX_SDK_ROOT)/build/build_flags.mk
###############################################################################
# Target inspecific paths: these are paths used by the build but not dependent
# upon the build.
###############################################################################
LW_SOURCE      := $(P4ROOT)/sw/dev/gpu_drv/chips_a
ACR_SW         := $(LW_SOURCE)/uproc/tegra_acr
LWUPROC        := $(LW_SOURCE)/uproc
RESMAN_ROOT    := $(LW_SOURCE)/drivers/resman
UPROC_LWOS_SRC := $(LWUPROC)/libs/lwos/$(LWOS_VERSION)

ifeq ($(wildcard $(LIBLWRISCV_CONFIG_MK)),)
$(error It seems liblwriscv is not installed. Missing $(LIBLWRISCV_CONFIG_MK))
endif

# Basic build elw
include $(LWRISCVX_SDK_ROOT)/build/build_elw.mk

# Preconfigure chip (mini chip-config)
include $(LIBLWRISCV_CONFIG_MK)

# Include build_flags.mk configs
include $(LIBLWRISCV_BUILD_FLAGS_MK)

LW_ENGINE_MANUALS := $(LW_MANUALS_ROOT)/$(LWRISCV_IS_CHIP_FAMILY)/$(LWRISCV_IS_CHIP)

# Custom variables.. because we can
# Used to set imemm/dmem limit in manifest, can change to desired value when required
# For now actual dmem/imem size is set as the limit
LWRISCV_APP_DMEM_LIMIT:= ${LWRISCV_DMEM_SIZE}
LWRISCV_APP_IMEM_LIMIT:= ${LWRISCV_IMEM_SIZE}

SIGGEN_CSG_PROD_SIGNING = false
LDSCRIPT_IN ?= $(APP_ROOT)/ldscript.ld.in
LDSCRIPT := ldscript.ld

# Check environment
ifeq ($(wildcard $(CC)),)
#$(error Set LW_TOOLS to directory where //sw/tools is checked out. Most likely missing toolchain (Missing $(CC)))
endif

ifeq ($(wildcard $(LW_ENGINE_MANUALS)),)
$(error Set LW_ENGINE_MANUALS to where manuals are checked out.)
endif

# Add liblwriscv to include path
INCLUDES += $(LIBLWRISCV_DIR)/inc
# Add LWRISCV SDK to include path
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc
# Add chip headers to include path
INCLUDES += $(LW_ENGINE_MANUALS)
INCLUDES += $(LW_MANUALS_SWREF_ROOT)/$(LWRISCV_IS_CHIP_FAMILY)/$(LWRISCV_IS_CHIP)
# Add Common Crypto Code items
INCLUDES += $(P4ROOT)/sw/lwriscv/main/libCCC/src/common_crypto
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC/hwref
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC/common
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC/common_crypto
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC/common_crypto/include
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC/common_crypto_private
INCLUDES += $(LWRISCVX_SDK_ROOT)/prebuilt/cheetah-acr-$(CHIP)-gsp/inc/libCCC/common_crypto_private/lwpka
# Add RM common headers used by tegra_acr
INCLUDES += $(RESMAN_ROOT)/arch/lwalloc/common/inc
#Add tegra_acr specific header
INCLUDES += $(ACR_SW)/inc
INCLUDES += $(ACR_SW)/inc/external
INCLUDES += $(ACR_SW)/riscv/wpr/t23x/
INCLUDES += $(ACR_SW)/riscv/dma/t23x/
INCLUDES += $(ACR_SW)/riscv/boot_falcon/t23x/
INCLUDES += $(ACR_SW)/riscv/falcon/t23x/
INCLUDES += $(ACR_SW)/riscv/acrlib/t23x/
INCLUDES += $(ACR_SW)/riscv/security/t23x/
INCLUDES += $(ACR_SW)/riscv/sig_verif/t23x/

# MK TODO: this include is actually part of chips_a, is optional for builds with chips_a
INCLUDES += $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk/
INCLUDES := $(addprefix -I,$(INCLUDES))
DEFINES += -DLWRISCV_APP_DMEM_LIMIT=$(LWRISCV_APP_DMEM_LIMIT)
DEFINES += -DLWRISCV_APP_IMEM_LIMIT=$(LWRISCV_APP_IMEM_LIMIT)
DEFINES += -DLW_RISCV_ACR
DEFINES += -DACR_PROFILE_$(SAMPLE_PROFILE)=1
DEFINES += -DLW_MISRA_COMPLIANCE_REQUIRED

# Link with liblwriscv
LIBS := -L$(LIBLWRISCV_DIR)/lib -llwriscv
LIBS += -L$(LIBLWRISCV_DIR)/lib -lCCC

CFLAGS += -fno-jump-tables -fstack-protector-strong -Wno-extra
CFLAGS += $(INCLUDES)
CFLAGS += $(DEFINES)

ASFLAGS += $(DEFINES)

LDFLAGS += -Wl,--start-group $(LIBS) -Wl,--end-group -nostdlib -nostartfiles -T$(LDSCRIPT)
LDFLAGS += $(DEFINES)
LD_INCLUDES += $(INCLUDES)

# so that when doing clean, we just clean all the profiles
ifeq ($(ACR_PROFILE),)
ACR_PROFILE := *
endif

#Source build flags
ifeq ($(ACR_PROFILE), "pmu")
CFLAGS += -DACR_RISCV_PMU
else ifeq($(ACR_PROFILE), "gsp")
CFLAGS += -DACR_RISCV_GSPLITE
endif

# Due to memory constraints on t239, we disbale certain unrequired functions
ifeq ($(CHIP), t239)
CFLAGS += -DACR_CHIP_PROFILE_T239
endif
# TODO: Signature verification does not work on fmodel due to bug.
CFLAGS += -DACR_SIG_VERIF

ifeq ($(LWRISCV_FEATURE_SSP), y)
    CFLAGS += -DIS_SSP_ENABLED
endif

TARGET:=acr-$(ACR_PROFILE)
OBJS += $(APP_ROOT)/trap.o
OBJS += $(APP_ROOT)/stack_fail_check.o
OBJS += $(APP_ROOT)/main.o
OBJS += $(APP_ROOT)/wpr/t23x/acr_rv_wprt234.o
OBJS += $(APP_ROOT)/dma/t23x/acr_rv_dmat234.o
OBJS += $(APP_ROOT)/boot_falcon/t23x/acr_rv_boott234.o
OBJS += $(APP_ROOT)/falcon/t23x/acr_rv_falct234.o
OBJS += $(APP_ROOT)/acrlib/t23x/acruc_rv_libt234.o
OBJS += $(APP_ROOT)/security/t23x/acr_rv_sect234.o
OBJS += $(APP_ROOT)/sig_verif/t23x/acr_rv_verift234.o

MANIFEST_SRC := $(APP_ROOT)/manifest.c

ELF             := $(TARGET)
FMC_TEXT        := $(ELF).text
FMC_DATA        := $(ELF).data
OBJDUMP_SOURCE  := $(ELF).objdump.src
MANIFEST        := $(ELF).manifest.o
MANIFEST_BIN    := $(ELF).manifest

FINAL_FMC_TEXT := $(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA := $(FMC_DATA).encrypt.bin
FINAL_MANIFEST := $(MANIFEST_BIN).encrypt.bin.out.bin

ifeq ($(ACR_PROFILE),ldpc)
BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(ELF) $(OBJDUMP_SOURCE)
else
BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(ELF) $(OBJDUMP_SOURCE)
endif

ifeq ($(SIGGEN_CSG_PROD_SIGNING),true)
OUTPUTDIR = objdir_riscv
CSG_TAR_DIR = $(OUTPUTDIR)
CSG_ARCHIVE ?= $(ACR_PROFILE)_CSG_SIGNING_ARCHIVE
PROFILE_LABEL = orin-iacr
endif

# TODO: this probably should be done other way
INTERMEDIATE_FILES :=

.PHONY: install build sign

install: build
	$(ECHO) Installing build output files...
	/usr/bin/install -D -t $(INSTALL_DIR) $(BUILD_FILES)

build: $(BUILD_FILES)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

sign: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)


ifeq ($(SIGGEN_CSG_PROD_SIGNING),true)
	$(ECHO) Creating the archive for CSG prod signing...
	$(ECHO) $(OUTPUTDIR)
	$(MKDIR) -p $(CSG_TAR_DIR)/$(CSG_ARCHIVE)
	$(MKDIR) -p $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/$(OUTPUTDIR)
	$(ECHO)  -manifest $(OUTPUTDIR)/$(MANIFEST_BIN) -code $(OUTPUTDIR)/$(FMC_TEXT) -data $(OUTPUTDIR)/$(FMC_DATA) -logLevel 4 -chip ga10b_rsa3k_0 -mode 0 -forceCsgFlow > $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/siggen_params.txt
	$(ECHO) $(PROFILE_LABEL) > $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/profile.txt
	cp $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA) $(CSG_TAR_DIR)/$(CSG_ARCHIVE)/$(OUTPUTDIR)
	/bin/tar -cf $(CSG_TAR_DIR)/$(CSG_ARCHIVE).tar -C $(CSG_TAR_DIR) $(CSG_ARCHIVE)
	/bin/gzip $(CSG_TAR_DIR)/$(CSG_ARCHIVE).tar
else
	echo "Debug Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip ga10b_rsa3k_0
INTERMEDIATE_FILES+= $(MANIFEST_BIN).encrypt.bin $(MANIFEST_BIN).encrypt.bin.sig.bin
endif

ifeq ($(SIGGEN_CSG_PROD_SIGNING),true)
BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_BIN) $(ELF) $(OBJDUMP_SOURCE)
else
BUILD_FILES := $(FMC_TEXT) $(FMC_DATA) $(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST) $(ELF) $(OBJDUMP_SOURCE)
endif

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata.manifest $(MANIFEST) $(MANIFEST_BIN)
INTERMEDIATE_FILES += $(MANIFEST_BIN)

$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC)
	$(eval APP_IMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_TEXT) ))
	$(eval APP_DMEM_SIZE := $(shell /usr/bin/stat -c %s $(FMC_DATA) ))
	$(CC) $(MANIFEST_SRC) $(INCLUDES) $(DEFINES) $(CFLAGS) -include $(LIBLWRISCV_CONFIG_H) -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)

$(LDSCRIPT): $(LDSCRIPT_IN)
	$(CPP) -x c -P -o $@ $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $^
INTERMEDIATE_FILES += $(LDSCRIPT)

$(FMC_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .code $(ELF) $(FMC_TEXT)
INTERMEDIATE_FILES += $(FMC_TEXT)

$(FMC_DATA): $(ELF)
	$(OBJCOPY) -O binary -j .data $(ELF) $(FMC_DATA)
INTERMEDIATE_FILES += $(FMC_DATA)

$(OBJDUMP_SOURCE): $(ELF)
	$(OBJDUMP) -S $(ELF) > $(OBJDUMP_SOURCE)
INTERMEDIATE_FILES += $(OBJDUMP_SOURCE)

$(ELF): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_DIR)/lib/liblwriscv.a $(LIBLWRISCV_DIR)/lib/libCCC.a
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
INTERMEDIATE_FILES += $(ELF)

%.o: %.c $(LIBLWRISCV_CONFIG_H)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(LIBLWRISCV_CONFIG_H)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	/bin/rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)
