#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

INCLUDE_MDIAGUTL ?= true

ifneq "$(INCLUDE_GPU)" "true"
    $(error mdiag support with INCLUDE_GPU!=true has been retired)
endif

ifeq "$(INCLUDE_MDIAGUTL)" "true"
    defines += -DINCLUDE_MDIAGUTL=1
endif

ifeq (,$(filter $(BUILD_ARCH),arm armv7))
    # disable these files for arm-mods build
    cpp_files += gpu/tests/vga/legacyvga.cpp
    cpp_files += gpu/tests/vga/mdiagrpt.cpp

    # These VGA tests aren't mdiag tests per-se, but they do require some
    # mdiag code (as things are lwrrently partitioned) to run.
    cpp_files += gpu/tests/vga/corec0.cpp
    cpp_files += gpu/tests/vga/corec1.cpp
    cpp_files += gpu/tests/vga/corec2.cpp
    cpp_files += gpu/tests/vga/corec3.cpp
    cpp_files += gpu/tests/vga/corec4.cpp
    cpp_files += gpu/tests/vga/corec5.cpp
    cpp_files += gpu/tests/vga/corec6.cpp
    cpp_files += gpu/tests/vga/coredata.cpp
    cpp_files += gpu/tests/vga/crccheck.cpp
    cpp_files += gpu/tests/vga/part2.cpp
    cpp_files += gpu/tests/vga/part3.cpp
    cpp_files += gpu/tests/vga/part4.cpp
    cpp_files += gpu/tests/vga/part5.cpp
    cpp_files += gpu/tests/vga/part6.cpp
    cpp_files += gpu/tests/vga/part7.cpp
    cpp_files += gpu/tests/vga/part8.cpp
    cpp_files += gpu/tests/vga/part9.cpp
    cpp_files += gpu/tests/vga/part10.cpp
    cpp_files += gpu/tests/vga/part11.cpp
    cpp_files += gpu/tests/vga/part12.cpp
    cpp_files += gpu/tests/vga/vectors.cpp
    cpp_files += gpu/tests/vga/vgasim.cpp
endif

cpp_files += mdiag/mdiag.cpp
cpp_files += mdiag/mdiagthread.cpp
cpp_files += mdiag/mrmtestinfra.cpp
cpp_files += mdiag/regmap.cpp
cpp_files += mdiag/sysspec.cpp
cpp_files += mdiag/XMLWriter.cpp
cpp_files += mdiag/lwgpu.cpp
cpp_files += mdiag/vaspace.cpp
cpp_files += mdiag/subctx.cpp
cpp_files += mdiag/lwgpu_channel.cpp
cpp_files += mdiag/pm_vaspace.cpp
cpp_files += mdiag/pm_smcengine.cpp
cpp_files += mdiag/advschd/js_pcymn.cpp
cpp_files += mdiag/vmiopmods/js_vmiopmdiagelwmgr.cpp
cpp_files += mdiag/zbctable.cpp
cpp_files += mdiag/channelallocinfo.cpp
cpp_files += mdiag/vmiopmods/vmiopmdiagelw.cpp
cpp_files += mdiag/vmiopmods/vmiopmdiagelwmgr.cpp
cpp_files += mdiag/smc/smcresourcecontroller.cpp
cpp_files += mdiag/smc/smcengine.cpp
cpp_files += mdiag/smc/gpupartition.cpp
cpp_files += mdiag/smc/smcparser.cpp
cpp_files += mdiag/vgpu_migration/vm_mem.cpp
cpp_files += mdiag/vgpu_migration/vm_stream.cpp
cpp_files += mdiag/vgpu_migration/vm_hexfile.cpp
cpp_files += mdiag/vgpu_migration/vm_gpu.cpp
cpp_files += mdiag/vgpu_migration/vgpu_migration.cpp

# for advschd
include_dirs += $(DRIVERS_DIR)/common/shared/inc
include_dirs += $(DRIVERS_DIR)/resman/inc/libraries
c_files      += $(DRIVERS_DIR)/resman/src/libraries/mmu/gmmu_fmt.c
c_files      += $(DRIVERS_DIR)/resman/src/libraries/mmu/mmu_fmt.c

basic_release_files += gpu/js/mdiag.js
basic_release_files += gpu/js/gpu_pre_init.js
internal_release_files += gpu/js/mrmtestinfra.js
internal_release_files += gpu/js/replayer.js
basic_release_files += mdiag/advschd/policyfiles/default.pcy
internal_release_files += mdiag/advschd/policyfiles/loopback.pcy
internal_release_files += mdiag/advschd/policyfiles/ns_int.pcy
internal_release_files += mdiag/advschd/policyfiles/ome.pcy
internal_release_files += mdiag/advschd/policyfiles/opme.pcy
internal_release_files += mdiag/advschd/policyfiles/osr.pcy
internal_release_files += mdiag/advschd/policyfiles/rc_err.pcy
internal_release_files += mdiag/advschd/policyfiles/sample.pcy
internal_release_files += mdiag/advschd/policyfiles/simple_force_fault.pcy
internal_release_files += mdiag/advschd/policyfiles/simple_surface_fault.pcy
internal_release_files += mdiag/advschd/policyfiles/simple_trigger_count.pcy
internal_release_files += mdiag/advschd/policyfiles/wfi.pcy

libraries += mdiag
target_lib_dirs += $(MODS_OUTPUT_DIR)/$(rel_o_dir)/libmdiag

ifeq "$(base_os)" "linux"
    ifeq "$(BUILD_ARCH)" "amd64"
        # Lwrrently only supports libelf on Linux and amd64
        libraries                     += elf
        basic_release_files           += $(wildcard $(MODS_DIR)/$(base_mods_lib_dir)/libelf.so*)
        exelwtable_files              += $(wildcard $(MODS_DIR)/$(base_mods_lib_dir)/libelf.so*)
    endif
endif
