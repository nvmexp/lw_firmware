#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
# This is NOT an official release, so use the module* contrib files.
USE_MODULAR_CONTRIB_DIR = true

COMMON_WS_DIR ?= $(P4ROOT)

VK_LAYERS_VERSION  ?= 1.2.162.1
VK_LAYERS_USE_LWMAKE ?= true
#-------------------------------------------------------------------------------------------------
# For module* branches we pull from the lwogtest location where early development happens
# On CheetAh GVC builds the location of the GLSLANG_PARENT_DIR is in a different git location and
# will be set via build_git.sh
# On release branches point to the most current offical release like
# //sw/mods/vulkan/sdks/1.2.137.0/source
#-------------------------------------------------------------------------------------------------
GLSLANG_PARENT_DIR ?= $(BUILD_TOOLS_DIR)/../apps/gpu/drivers/opengl/lwogtest/thirdparty/shaderc/third_party
GLSLANG_PARENT_DIR_P4 ?= //sw/apps/gpu/drivers/opengl/lwogtest/thirdparty/shaderc/third_party

#-------------------------------------------------------------------------------------------------
# For release branches we would pull from the most current public domain branch available
# Any version prior to 1.1.101 does not support Meshlet shaders in SPIR-V
# Verson 1.1.101 supports Meshlet Shaders but does not support all required extensions for
# Compute shaders in SPIR-V.
# Version 1.2.137.0 supports Raytracing & all required extensions for Compute shaders.
#-------------------------------------------------------------------------------------------------
#GLSLANG_PARENT_DIR ?= $(SHARED_MODS_FILES_DIR)/../mods/vulkan/sdks/$(VK_LAYERS_VERSION)/source
#GLSLANG_PARENT_DIR_P4 ?= //sw/mods/vulkan/sdks/$(VK_LAYERS_VERSION)/source

ifdef CYGWIN_MAKE
    GLSLANG_PARENT_DIR := $(shell $(MODS_DIR)/tools/abs2rel.sh $(GLSLANG_PARENT_DIR))
endif

# When future chips are enabled, string checking and redaction is disabled
ifeq "$(DVS_BUILD_LIKE_RELEASE)" "true"
    INCLUDE_FUTURE_CHIPS := false
else
    INCLUDE_FUTURE_CHIPS := true
endif

# Release (non-dev) archs will be built using a specified compiler build from DVS and
# a specified LWCA toolkit
LWDA_REL_ARCHS := sm_50 sm_52 sm_53 sm_60 sm_61 sm_70 sm_72 sm_75 sm_80 sm_86 sm_87

# Dev archs are built using the LWCA compiler from the compiler branch and
# the LWCA toolkit from //sw/gpgpu/
INCLUDE_LWDA_DEV_ARCHS ?= $(INCLUDE_FUTURE_CHIPS)
LWDA_DEV_ARCHS         ?=

ifeq "$(LWCFG_GLOBAL_ARCH_ADA)" "1"
LWDA_DEV_ARCHS += sm_89
endif

ifeq "$(LWCFG_GLOBAL_ARCH_HOPPER)" "1"
LWDA_DEV_ARCHS += sm_90
endif

# All supported SM architectures
LWDA_ALL_ARCHS = $(LWDA_REL_ARCHS) $(LWDA_DEV_ARCHS)
LWDA_DVS_REL_COMPILER ?= gpu_drv_chips%20Debug%20Linux%20AMD64%20GPGPU%20COMPILER

# ARM architectures require that the runtime be compiled with aarch64
LWDA_ARM_ARCHS ?= sm_53 sm_72 sm_87

ifeq "$(INCLUDE_GPU)" "true"
    # To save DVS resources do not include boards.db in modular branches
    INCLUDE_BOARDS_DB ?= false

    ifeq "$(INCLUDE_GPU_SPECIFIC)" "true"
        # When creating a branch-specific file, only include the boards_g*.db files
        # for the chips that are officially supported
ifeq "$(LWCFG_GLOBAL_ARCH_PASCAL)" "1"
        boards_db_files += $(BOARDS_DB_DIR)/boards_gp100.db
        boards_db_files += $(BOARDS_DB_DIR)/boards_gp102.db
        boards_db_files += $(BOARDS_DB_DIR)/boards_gp104.db
        boards_db_files += $(BOARDS_DB_DIR)/boards_gp106.db
        boards_db_files += $(BOARDS_DB_DIR)/boards_gp107.db
        boards_db_files += $(BOARDS_DB_DIR)/boards_gp108.db
endif

        # GPU-specific JSON fuse files
ifeq "$(LWCFG_GLOBAL_ARCH_PASCAL)" "1"
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gp100_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gp102_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gp104_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gp106_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gp107_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gp108_f.json
endif
ifeq "$(LWCFG_GLOBAL_ARCH_VOLTA)" "1"
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/gv100_f.json
endif
ifeq "$(LWCFG_GLOBAL_GPU_IMPL_TU102)" "1"
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/tu102_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/tu102_fpf.json
endif
ifeq "$(LWCFG_GLOBAL_GPU_IMPL_TU104)" "1"
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/tu104_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/tu104_fpf.json
endif
ifeq "$(LWCFG_GLOBAL_GPU_IMPL_TU106)" "1"
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/tu106_f.json
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/tu106_fpf.json
endif
ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_SVNP01)" "1"
        basic_release_files$(ONLY_PC)    += $(CHIP_XML_DIR)/xml_v1/svnp01_f.json
endif

        # GPU-specific POR JSON files
        CLIENTPATHS += //sw/mods/por_data/...
ifeq "$(LWCFG_GLOBAL_ARCH_VOLTA)" "1"
        basic_release_files$(ONLY_PC)    += $(SHARED_MODS_FILES_DIR)/por_data/gv100_POR.json
endif

        ifndef TEGRA_TOP
            CLIENTPATHS  += //sw/mods/shared_modular_branch_files/...
            PVS_STAT_DIR  = $(SHARED_MODULAR_BRANCH_FILES_DIR)/pvsstatvals
            ifeq "$(IS_BRANCH_MODULAR)" "true"
                include gpu/js/pvsstatvals.inc
            endif
        endif
    endif

    # Branch specific GpuSubdevice file that implements ValidateSoftwareTree
    cpp_files += gpu/gpusbdev_modular.cpp
endif

ifeq "$(VERSION)" "000"
    version_name =
else
    version_name = _$(firstword $(subst ., ,$(VERSION)))
    ifeq "$(wildcard $(MODS_DIR)/docs/engnotes$(version_name).txt)" ""
        $(warning Release notes version name engnotes$(version_name) not found, using base notes)
        version_name =
    endif
endif

CONTRIB_DIR := $(MODS_DIR)/contrib

# Branch specific release notes files
$(gen_cpp_dir)/engnotes.txt: $(MODS_DIR)/docs/engnotes$(version_name).txt
	$(Q)$(CP_TO_RUNSPACE) $< $@

$(gen_cpp_dir)/relnotes.txt: $(MODS_DIR)/docs/relnotes$(version_name).txt
	$(Q)$(CP_TO_RUNSPACE) $< $@
