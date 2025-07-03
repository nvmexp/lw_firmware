#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

ifeq "$(base_os)" "win32"
    CHMOD_SFD =
else
    CHMOD_SFD = $(Q)chmod 755 $1
endif

$(o_dir)/specfiledownloader.py: $(MODS_DIR)/tools/specfiledownloader/specfiledownloader.py
	$(Q)$(ECHO) Creating $@
	$(Q)$(SED) 's/VERSION=\"branch\"/VERSION=\"$(VERSION)\"/' < $(MODS_DIR)/tools/specfiledownloader/specfiledownloader.py  > $@
	$(call CHMOD_SFD, $@)


ifeq "$(INCLUDE_GPU)" "true"
    fatool_files =
    fatool_files += __main__.py
    fatool_files += fatool_libs/Datalogger.py
    fatool_files += fatool_libs/Exceptions.py
    fatool_files += fatool_libs/FailureStrategy.py
    fatool_files += fatool_libs/GpuResetHelper.py
    fatool_files += fatool_libs/__init__.py
    fatool_files += fatool_libs/Utilities.py

    floorsweep_files =
    floorsweep_files += __main__.py
    floorsweep_files += floorsweep_libs/__init__.py
    floorsweep_files += floorsweep_libs/Datalogger.py
    floorsweep_files += floorsweep_libs/Exceptions.py
    floorsweep_files += floorsweep_libs/FsLib.py
    floorsweep_files += floorsweep_libs/GpuList.py
    floorsweep_files += floorsweep_libs/SkuConfig.py
    floorsweep_files += floorsweep_libs/TestInfo.py
    floorsweep_files += floorsweep_libs/Utilities.py
    floorsweep_files += floorsweep_libs/FsInfo/__init__.py
    floorsweep_files += floorsweep_libs/FsInfo/FsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/GM20xFsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/GP10xFsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/VoltaFsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/TuringFsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/GA100FsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/GA10xFsInfo.py
    floorsweep_files += floorsweep_libs/FsInfo/GH100FsInfo.py
    floorsweep_files += floorsweep_libs/GpuInfo/__init__.py
    floorsweep_files += floorsweep_libs/GpuInfo/GpuInfo.py
    floorsweep_files += floorsweep_libs/GpuInfo/GP10xGpuInfo.py
    floorsweep_files += floorsweep_libs/GpuInfo/GA100GpuInfo.py
    floorsweep_files += floorsweep_libs/GpuInfo/GA10xGpuInfo.py
    floorsweep_files += floorsweep_libs/GpuInfo/GH100GpuInfo.py
    floorsweep_files += floorsweep_libs/UnitTest/__init__.py
    floorsweep_files += floorsweep_libs/UnitTest/Utilities.py
    floorsweep_files += floorsweep_libs/UnitTest/testGA10x.py
    floorsweep_files += floorsweep_libs/UnitTest/testGH100.py
    internal_release_files += $(o_dir)/specfiledownloader.py
    exelwtable_files += $(o_dir)/specfiledownloader.py
    all_gen_files += $(o_dir)/specfiledownloader.py

    ifneq "$(base_os)" "macosx"
        basic_release_files$(ONLY_PC)    += $(o_dir)/fatool.egg
        internal_release_files$(ONLY_PC) += $(o_dir)/floorsweep.egg
        gen_contrib_files$(ONLY_PC) += $(o_dir)/fatool.egg
        gen_contrib_files$(ONLY_PC) += $(o_dir)/floorsweep.egg
    endif

    floorsweep_zip_files =
    floorsweep_zip_files += $(addprefix tools/floorsweep/, $(floorsweep_files))

    ifneq (,$(filter $(BUILD_OS),linuxmfg))
        ifeq "$(INCLUDE_FSLIB)" "true"
            floorsweep_zip_files += $(GEN_LIB_DIR)/libfsegg/libfsegg$(dll_suffix)
        endif
    endif

    # Package SMC + SR-IOV script with internal release files
    ifeq "$(base_os)" "linux"
        internal_release_files += tools/smc_sriov
        exelwtable_files       += tools/smc_sriov
    endif

    # MLA JSON2 Schema files used for MLA schema validation
    ifeq "$(BUILD_OS)" "linuxmfg"
        ## MLA Test files
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8232.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8233.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8243.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8245.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8330.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8347.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8364.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8365.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8368.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8369.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8370.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8371.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8601.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8602.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8642.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8643.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8703.json
        pvs_release_files += tools/mla_schemas/mla_schema_ausvrl8763.json

        ## Memory repair PVS test files
        pvs_release_files += tools/mla_schemas/memrepair/pvs_memrepair_validator.py

        # ausvrl8216
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmdump.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmvhardrowrepair.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmvmemrepair.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmvprintrepairedlanesgpu.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmvprintrepairedlaneshbm.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmvprintrepairedmem.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8216_hbmvsoftlanerepair.json

        # ausvrl8224
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmdump.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmvhardrowrepair.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmvmemrepair.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmvprintrepairedlanesgpu.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmvprintrepairedlaneshbm.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmvprintrepairedmem.json
        pvs_release_files += tools/mla_schemas/memrepair/mla_schema_ausvrl8224_hbmvsoftlanerepair.json
    endif
endif

ifeq "$(base_os)" "win32"
    # On Windows, o_dir is already relative
    TOOLS_OUTPUT_DIR = $(o_dir)
else
    TOOLS_OUTPUT_DIR = $(shell tools/abs2rel.sh $(o_dir))
endif


# libfs is built as a static library by default
$(GEN_LIB_DIR)/libfsegg/libfsegg$(dll_suffix): submake.fsegg.build

clean: submake.fsegg.clean
submake.fsegg.%:
	$(Q)$(MAKE) -C $(FS_LIB_DIR)/contrib_sw/mods fsegg=true dll_build=true $*

$(o_dir)/fatool.egg: $(addprefix tools/fatool/, $(fatool_files)) $(MODS_DIR_UNIX)/tools/createzip.py
ifeq "$(INCLUDE_GPU)" "true"
	$(Q)$(PYTHON3) tools/createzip.py --strip tools/fatool $(TOOLS_OUTPUT_DIR)/fatool.egg $(addprefix tools/fatool/,$(fatool_files)) > $(NULLDEV)
endif

$(o_dir)/floorsweep.egg: $(floorsweep_zip_files) $(MODS_DIR_UNIX)/tools/createzip.py
ifeq "$(INCLUDE_GPU)" "true"
	@$(ECHO) "Creating floorsweep.egg"
	$(Q)$(PYTHON3) tools/createzip.py --strip tools/floorsweep --strip $(GEN_LIB_DIR) $(TOOLS_OUTPUT_DIR)/floorsweep.egg $(floorsweep_zip_files) > $(NULLDEV)
endif
