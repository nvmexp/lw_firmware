#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#


#-------------------------------------------------------------------------------
# MODS make file.
# See //sw/<tree>/drivers/common/build/mods/makedefs.inc for more information about the MODS
# makefiles.
#-------------------------------------------------------------------------------


#-------------------------------------------------------------------------------
# Environment variables:
#
#    BOUND_JS          Produce a bound exelwtable that uses only the internal
#                      .js file and accepts no command-line arguments.
#                      This should point at the JS you wish to bind
#                      (e.g. "gpu/js/gputest.js")
#                      Defaults to "false" (which is off)
#
#    BUILD_OS_SUBTYPE  The subtype of the OS.  Primarily used for release
#                      versions of linux (RH7.3, RH9, FC3, WH), as well as
#                      future win32 elwironments (msvc90).
#
#    CHIP_XML_DIR      This is the location where chip XMLs could be found.
#                      Defaults to //sw/mods/chipfuse
#
#    LINUX_DRV_DIR     MODS Linux kernel driver source directory.
#                      Defaults to //sw/mods/linuxdrv
#
#    CHANGELIST        The synced changelist used for the build.
#                      Defaults to "0".
#
#    COMPRESS_REL      Compress release build
#                      Defaults to "true"
#
#    DRIVERS_DIR       Drivers directory.
#                      Defaults to "../../drivers"
#
#    ENABLE_CCOVER     Link with code coverage runtime library.
#                      Defaults to "false"
#
#    ENCRYPT_JS        If true, zip target will build encrypt utility and
#                      include only encrypted JS files.
#                      Defaults to true.
#
#    EXTRA_FILES       Extra files to include in MODS packages.
#                      Defaults to none
#
#    INCLUDE_GPU       Include GPU specific code.
#                      Defaults to "true"
#
#    INCLUDE_GPUTEST   Include GPU manufacturing tests.
#                      Defaults to "true"
#
#    INCLUDE_COMMON_TEGRA Include common cheetah files common to the CheetAh build.
#
#    INCLUDE_MINIRM    Include miniRM instead of fullRM.
#                      Defaults to "false"
#
#    INCLUDE_LWOGTEST  Include lwogtest code.
#                      Defaults to "false"
#
#    INCLUDE_UNITTEST  Include MODS unit tests.
#                      Defaults to "false"
#
#    STUB_LWOGTEST     Include lwogtest.cpp, but not the library.
#                      Used by DVS which builds lwogtest separately and does
#                      its own packaging.
#                      Defaults to "false"
#
#    INCLUDE_OGL       Include OpenGL code.
#                      Defaults to "true"
#
#    INCLUDE_LWDA      Include the MODS LWCA driver and LWCA tests.
#                      Defaults to "true"
#
#    INCLUDE_LWDART    Include the LWCA runtime environment.
#                      Defaults to "true" on some linux configurations
#
#    INCLUDE_OGL_APIC  Include OpenGL API capture.
#                      Defaults to "false"
#
#    INCLUDE_VULKAN    Includes vulkan graphics libraries.
#                      Also includes vulkan raytracing libraries - RTCore, CBL, LWVM(aka Spirv).
#                      When set to "true" forces INCLUDE_LWDA=true
#                      Defaults to "true" in most configurations
#
#    INCLUDE_VKLAYERS  Include the vulkan validation layers for Vulkan tests.
#                      Defaults to "false"
#
#    INCLUDE_LWLWVID   Includes the video library.
#                      Defaults to "false"
#
#    INCLUDE_PMU       Include the PMU ucode builds.
#                      Defaults to "false"
#
#    INCLUDE_DPU       Include the DPU ucode builds.
#                      Defaults to "false"
#
#    INCLUDE_DP        Include the Display Port library.
#                      Defaults to "true"
#
#    INCLUDE_DGPU      Includes support for discrete GPUs.
#                      Defaults to "false" on Android and "true" everywhere else.
#
#    INCLUDE_GPU_SPECIFIC Includes GPU specific files (like *.rom).
#                      Defaults to "true"
#
#    INCLUDE_MODSALLOC Use MODS allocator for all memory allocation for memory debug.
#
#    INCLUDE_TEGRA_OGL Include the GL tests for cheetah
#                      These are dependent on drivers in the git tree which need to built
#                      separately
#                      Defaults to "false"
#
#    INCLUDE_LEAK_DETECTOR Build MODS with a debug feature for printing all leaks.
#                      Works only in debug builds. Includes leak detector script
#                      Which leverages the leak printing feature to catch memory
#                      leaks.
#
#    INCLUDE_MODS_GTEST Build MODS' google-unit-tests. Unit Tests are built along with
#                      full MODS source code except modsmain.cpp.
#
#    INCLUDE_SEQ_FAIL  Enable sequential fail testing.
#
#    INCLUDE_BYTECODE  Use JS engine version which supports loading bytecode.
#
#    LOGGING_DIR       Location of MODS Logging tools
#                      Defaults to "$(COMMON_WS_DIR)/sw/mods/logging".
#
#    MATS_DIR          Stand-alone MATS.EXE directory.
#                      Defaults to "../memory/mats2".
#
#    MLA_DIR           Location of MLA sources
#                      Defaults to "$(LOGGING_DIR)/mla".
#
#    MODS_TOOLS_DIR    Location of non-branched tools packaged with MODS
#                      Defaults to "$(COMMON_WS_DIR)/sw/mods/tools".
#
#    PRESERVE_RUNSPACE Do not overwrite files in the runspace if they
#                      are newer than the source.  Default is "false".
#
#    RECORD_REG_WR     Record register writes.
#                      Defaults to "false".
#
#    REMOTE_DEBUG_PORT Enable remote (serial) debugging with GDB.
#                      Defaults to 0. Set evironment variable to the serial
#                      COM port connected to the debug target.
#
#    SDK_DIR           SDK directory.
#                      Defaults to "../../sdk"
#
#    SERVER_MODE       Makes mods a loadable library providing GPU simulation
#                      service to an outside application (e.g. DX driver)
#
#    SYMBOLIZE         Force debugging symbols to be added to the exelwtable
#                      regardless of BUILD_CFG.
#
#    USE_SHARED_LIB_STUBS    Use stubs for shared libraries which allows the MODS
#                            exelwtable to be built in parallel with any shared
#                            libraries with a C interface
#
#    UPROC_DIR       Drivers directory.
#                      Defaults to "../../uproc"
#
#    VERSION           Software version.
#                      Defaults to "000"
#
#    CLEAN_ME          files to delete on 'make clean'
#
#    BUILD_TEGRA_EMBEDDED Sets environment for embedded builds.
#                         Includes/excludes files for embedded builds.
#                         Defaults to "false".
#
#    ENABLE_GUARDWORD Enforces guardword checks.  Guardword checks are always
#                     performed if string redaction is enabled, but this option
#                     tells whether guardword checks should fail or not.
#
#    ENABLE__FUNCTION__ Enables verbose function/file printfs in release builds.
#                       This applies to MODS_FUNCTION/MODS_FILE macros and when
#                       this option is set to true, they will emit __FUNCTION__
#                       and __FILE__, respectively.
#                       This is the default behavior in non-release builds.
#
# Targets:
#
#    target          Create target exelwtable for remote debugging.
#
#    zip             Zip the exelwtable and script files.
#                    Generate $(VERSION).zip.
#
#    zip_pvs         Zip the exelwtable and script files necessary
#                    for PVS regression run.
#                    Generate $(VERSION).zip.
#
#    zip_pvs_nodep   Same thing, without dependencies (just the zip)
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#
# Environment variables: (set default values if not defined by user)
#
#-------------------------------------------------------------------------------
BOUND_JS          ?= false
CHANGELIST        ?= 0
DRIVERS_DIR       ?= ../../drivers
LWDA_DIR          ?= $(DRIVERS_DIR)/gpgpu/lwca
ENABLE_CCOVER     ?= false
ENCRYPT_JS        ?= true
INCLUDE_GPUTEST   ?= true
INCLUDE_LWDA      ?= true
INCLUDE_FLCNDBG   ?= false
INCLUDE_MDIAG     ?= false
INCLUDE_MDIAG_REFS?= true
INCLUDE_MDIAGUTL  ?= $(INCLUDE_MDIAG)
INCLUDE_RMTEST    ?= false
INCLUDE_REFS      ?= false
INCLUDE_LWOGTEST  ?= false
INCLUDE_OGL       ?= true
INCLUDE_OGL_APIC  ?= false
INCLUDE_GPU       ?= true
INCLUDE_TEGRA_OGL ?= false
INCLUDE_MINIRM    ?= false
INCLUDE_LWWATCH   ?= false
INCLUDE_UNITTEST  ?= false
INCLUDE_PMU       ?= false
INCLUDE_DPU       ?= false
INCLUDE_DP        ?= true
INCLUDE_VBIOSSEC  ?= true
INCLUDE_MODSALLOC ?= true
INCLUDE_THREADSTATS ?= false
INCLUDE_RMAPI_TEGRA ?= false
INCLUDE_ASTC_FILES ?= false
INCLUDE_VGPU_PLUGIN ?= false
INCLUDE_MODS_GTEST ?= false
INCLUDE_FSLIB     ?= true
INCLUDE_BYTECODE  ?= false
UPROC_DIR         ?= ../../uproc

# Lwrrently only Linux platform supports IST/MATHS Link tests
ifeq "$(BUILD_OS)" "linuxmfg"
    INCLUDE_MATHS_IST   ?= true
endif
ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        INCLUDE_YAML    ?= true
    endif
endif
# If running google tests, we need to include the IST tests as well
ifeq "$(INCLUDE_MODS_GTEST)" "true"
    INCLUDE_MATHS_IST   ?= true
endif
INCLUDE_MATHS_IST       ?= false
ifeq "$(INCLUDE_MATHS_IST)" "true"
    INCLUDE_YAML        ?= true
endif
INCLUDE_YAML            ?= false

# Lwrrently only Linux platform supports libsub
ifeq "$(BUILD_OS)" "linuxmfg"
    INCLUDE_XUSB ?= true
endif
INCLUDE_XUSB	  ?= false

ifneq (,$(filter $(BUILD_OS), winsim winmfg win32))
    INCLUDE_MDIAGUTL = false
endif

ifeq "$(BUILD_ARCH)" "ppc64le"
    INCLUDE_MDIAGUTL = false
endif

# Lwrrently, libspdm only required for RMTest on certain platforms.
ifneq (,$(filter $(BUILD_OS), linuxmfg sim))
    ifeq "$(BUILD_ARCH)" "amd64"
        INCLUDE_LIBSPDM ?= $(INCLUDE_RMTEST)
    endif
endif
INCLUDE_LIBSPDM ?= false

ifeq "$(BUILD_OS)" "cheetah"
    INCLUDE_DGPU  ?= false
else
    INCLUDE_DGPU  ?= true
endif
INCLUDE_GPU_SPECIFIC ?= true
INCLUDE_POKEGPU   ?= false
INCLUDE_MATS_DEFAULT := false
INCLUDE_MLA       ?= false
ifeq "$(BUILD_OS)" "cheetah"
    INCLUDE_MME64 ?= false
else
    INCLUDE_MME64 ?= true
endif
ifeq "$(filter $(BUILD_OS), cheetah linda winda)" ""
    ifneq "$(SERVER_MODE)" "true"
        INCLUDE_MUCC  ?= $(INCLUDE_GPU)
    endif
endif
INCLUDE_MUCC      ?= false
INCLUDE_FATBINLD  ?= false
USE_SYSTEM_LIBS   ?= false
USE_LWDA_SYSTEM_LIB ?= false
MME_SIM_DIR       ?= ../lwmme
MATS_DIR          ?= ../memory/mats2
LOGGING_DIR       ?= $(COMMON_WS_DIR)/sw/mods/logging
MLA_DIR           ?= $(LOGGING_DIR)/mla
PROTOUTIL_DIR     ?= ../protoutil
MODS_DIR          ?= .
MODS_TOOLS_DIR    ?= $(COMMON_WS_DIR)/sw/mods/tools
MODS_TOOLS_DIR_P4 ?= //sw/mods/tools
PRESERVE_RUNSPACE ?= false
RECORD_REG_WR     ?= false
REMOTE_DEBUG_PORT ?= 0
SDK_DIR           ?= ../../sdk
SERVER_MODE       ?= false
VERSION           ?= 000
DEBUG_TRACE_LEVEL ?= 0
DEBUG_GEN_PROFILE ?= false
SHARED_MODS_FILES_DIR ?= $(COMMON_WS_DIR)/sw/mods
LINUX_DRV_DIR     ?= $(SHARED_MODS_FILES_DIR)/linuxdrv
LINUX_PPC_DRIVER_DIR ?= $(COMMON_WS_DIR)/sw/main/drivers/lwppc_linux_driver
CHIP_XML_DIR      ?= $(SHARED_MODS_FILES_DIR)/chipfuse
GLM_DIR           ?= $(BUILD_TOOLS_DIR)/mods/glm/0.9.9.7/glm
LZ4_DIR           ?= $(BUILD_TOOLS_DIR)/../mods/tools/lz4-1.8.1.2/lz4
LZ4_DIR_P4        ?= //sw/mods/tools/lz4-1.8.1.2/lz4
SHARED_FILES_DIR  ?= $(SHARED_MODS_FILES_DIR)/shared_files
UPHY_FILES_DIR    ?= $(SHARED_FILES_DIR)/uphy
export HULK_DIR   ?= $(SHARED_MODS_FILES_DIR)/hulk
MODS_RMTEST_DIR   ?= $(SHARED_MODS_FILES_DIR)/rmtest
SHARED_MODULAR_BRANCH_FILES_DIR ?= $(SHARED_MODS_FILES_DIR)/shared_modular_branch_files
SYMBOLIZE         ?= false
FLCNDWARF_PATH    ?= ../../apps/lwflcndbg/flcndwarf
LW_DWARF_PATH     ?= $(FLCNDWARF_PATH)/libdwarf
LW_ELF_PATH       ?= $(FLCNDWARF_PATH)/libelf
export MODS_RUNSPACE ?= $(abspath .)/runspace
TRIGGER_RMCFG     ?= false
BUILD_TEGRA_EMBEDDED ?= false
MODS_KERNEL_RM_VER_CHK  ?= false
PKG_OUT_DIR       ?= .
TGZ_TEMP_DIR      ?= "$(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/tgz_temp"
USE_PCH           ?= true
PROTOBUF_PATH     ?= ../../apps/fabricmanager/libs/protobuf-2.6.0
FS_LIB_DIR        ?= $(MODS_DIR)/gpu/floorsweep/fs_lib
FM_PATH           ?= ../../apps/fabricmanager
TRANSPORT_PATH    ?= ../../diag/multinode
LWML_PATH         ?= ../../apps/lwml
LWWATCH_PATH      ?= ../../apps/lwwatch
LWFLCNDBG_PATH    ?= ../../apps/lwflcndbg
BRANCH_BASE       ?= ../..

ifdef TEGRA_TOP
    LWMAKE_DRV_OUT_DIR ?= $(GEN_LIB_DIR_UNIX)/lwmake/drv
endif
LWMAKE_DRV_OUT_DIR  ?= $(GEN_LIB_DIR_UNIX)/lwmake
LWMAKE_DIAG_OUT_DIR ?= $(GEN_LIB_DIR_UNIX)/lwmake

ifeq "$(BUILD_OS)" "linda"
    INCLUDE_SEQ_FAIL ?= true
endif
INCLUDE_SEQ_FAIL ?= false

# Include ASTC files for CheetAh
ifeq "$(BUILD_OS)" "cheetah"
    INCLUDE_ASTC_FILES := true
endif

# This is for temorarily support of both old LWDEC interface and the new LWDEC
# interface (LWDEC 3.0) in the old LWDEC classes: A0B0 (GM107), B0B0 (GM200 and
# GM204), B6B0 (GM206) and C0B0 (T210). If this variable is true, then LWDEC
# test will use the new interface in these classes. When all engines are
# transferred to the new interface and the changes synced with other teams, this
# variable should be removed along with the code that corresponds to its 'false'
# value.
BACKPORT_LWDEC3_INTERFACE ?= true
ifeq "$(BUILD_OS)" "cheetah"
    BACKPORT_LWDEC3_INTERFACE := true
endif
ifeq "$(BACKPORT_LWDEC3_INTERFACE)" "true"
    # T210 LWDEC engine supports the backport
    defines += -DCLASS_C0B0_USES_LWDEC3
endif

pvs_exclude_files :=
exelwtable_files  :=

dll_build = true
base_name = mods

-include $(DRIVERS_DIR)/common/build/mods/makedefs.inc

ifdef CYGWIN_MAKE
    # The windows build of the encrypt tool doesn't support the full cygwin paths:
    BOARDS_DB_DIR ?= $(shell $(MODS_DIR)/tools/abs2rel.sh $(SHARED_MODS_FILES_DIR)/boardsdb)
else
    BOARDS_DB_DIR ?= $(SHARED_MODS_FILES_DIR)/boardsdb
endif

ENABLE__FUNCTION__ ?= $(if $(filter-out release,$(BUILD_CFG)),true,false)
ifeq "$(ENABLE__FUNCTION__)" "true"
    defines += -DENABLE__FUNCTION__
endif

# We need to set GEN_LIB_DIR after including makedefs.inc & before including core/makesrc.inc
ifdef CYGWIN_MAKE
    GEN_LIB_DIR  := $(shell $(MODS_DIR_UNIX)/tools/abs2rel.sh $(MODS_OUTPUT_DIR)/$(rel_o_dir))
else
    GEN_LIB_DIR  := $(MODS_OUTPUT_DIR)/$(rel_o_dir)
endif
GEN_LIB_DIR_UNIX = $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)

# Precompiled headers settings
use_precompiled_headers = false
ifeq "$(USE_PCH)" "true"
    # GCC has a bug in ub sanitizer when precompiled headers are enabled
    ifneq "$(SANITIZER)" "undefined"
        ifeq "$(base_os)" "linux"
	    use_precompiled_headers = true

	    pch_h_name = $(base_os)/pch.hpp
	    pch_name = pch.hpp.gch

	    # .hpp extension is necessary for HOS. It is compiled with clang,
	    # which complains that pch.h is a C header used in C++ mode.
	    PCH_PREPARE_OPTIONS = $(addprefix -I , $(base_os))
	    PCH_USE_OPTIONS = $(addprefix -include , $(o_dir)/pch.hpp)
	    PCH_USE_OPTIONS += $(addprefix -I , $(base_os))
	    depend_files += $(o_dir)/$(notdir $(pch_h_name)).d
        endif
    endif
endif

ifeq "$(INCLUDE_MODS_GTEST)" "true"
    GTEST_DIR   ?= $(BUILD_TOOLS_DIR)/mods/googletest
    CLIENTPATHS += //sw/tools/mods/googletest/...
endif

exelwtable_files += $(o_dir)/$(exe_name)

default_target: all

encrypt_exe = encrypt$(exe_suffix)

ifeq "$(sim_build)" "true"
    ifeq "$(toolchain)" "gnu"
        # Set visibility=hidden on gcc 4.0 on up
        ifeq "$(filter 0 1.% 2.% 3.%,$(toolchain_rev))" ""
            CFLAGS += -fvisibility=hidden
        endif
    endif
endif

LWCFG_ROOT ?= $(DRIVERS_DIR)/common/build
LWCFG_OUTPUTDIR ?= $(gen_cpp_dir)
-include $(LWCFG_ROOT)/lwconfig.lwmk
include_dirs += $(LWCFG_OUTPUTDIR)

ifneq "$(LWCFG_GLOBAL_ARCH_LWSWITCH)" "1"
    override INCLUDE_LWSWITCH := false
endif

# Flags used when including source and release files
# ONLY_X86   - build for x86 (32-bit) architecture
# ONLY_AMD64 - build for x86_64/amd64 (64-bit) architecture
# ONLY_PC    - build for either x86 or x86_64/amd64 architecture (32-bit or 64-bit)
# ONLY_ARM   - build for any ARM architecture (32-bit or 64-bit)
# ONLY_DGPU  - build which supports dGPU (e.g. linuxmfg on CheetAh, but not cheetah)
# ONLY_IGPU  - build which supports iGPU - Lwrrently this maps to
#                  - BUILD_OS=(sim|winsim) since ARCH tests are run off dut with non-ARM build ARCH
#                  - BUILD_ARCH=aarch64 and BUILD_OS!=linuxmfg since ARM linuxmfg builds
#                    only support dGPUs
#

# iGPUs are always supported on sim builds and default to off everywhere else
ifeq (,$(filter $(BUILD_OS), winsim sim))
    ONLY_IGPU = _skip
else
    ONLY_IGPU =
endif

ifeq "$(BUILD_ARCH)" "x86"
    ONLY_X86 =
    ONLY_AMD64 = _skip
    ONLY_PC =
    ONLY_ARM = _skip
    ONLY_DGPU =
endif

ifeq "$(BUILD_ARCH)" "amd64"
    ONLY_X86 = _skip
    ONLY_AMD64 =
    ONLY_PC =
    ONLY_ARM = _skip
    ONLY_DGPU =
endif

ifeq "$(BUILD_ARCH)" "ppc64le"
    ONLY_X86 = _skip
    ONLY_AMD64 = _skip
    ONLY_PC =
    ONLY_ARM = _skip
    ONLY_DGPU =
endif

ifeq "$(BUILD_ARCH)" "aarch64"
    ONLY_X86 = _skip
    ONLY_AMD64 = _skip
    ONLY_PC = _skip
    ONLY_ARM =

    # iGPUs are allowed on all non linuxmfg ARM builds
    ifneq "$(BUILD_OS)" "linuxmfg"
        ONLY_IGPU =
    endif

    ifeq "$(INCLUDE_DGPU)" "true"
        ONLY_DGPU =
    else
        ONLY_DGPU = _skip
    endif
endif

# Flags from lwconfig that make it easier to include individual files

ifeq "$(LWCFG_GLOBAL_ARCH_KEPLER)" "1"
    ONLY_KEPLER =
else
    ONLY_KEPLER = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_MAXWELL)" "1"
    ONLY_MAXWELL =
else
    ONLY_MAXWELL = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_PASCAL)" "1"
    ONLY_PASCAL =
else
    ONLY_PASCAL = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_VOLTA)" "1"
    ONLY_VOLTA =
else
    ONLY_VOLTA = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_TURING)" "1"
    ONLY_TURING =
else
    ONLY_TURING = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_AMPERE)" "1"
    ONLY_AMPERE =
else
    ONLY_AMPERE = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_ADA)" "1"
    ONLY_ADA =
else
    ONLY_ADA = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_HOPPER)" "1"
    ONLY_HOPPER =
else
    ONLY_HOPPER = _skip
endif

ifeq "$(LWCFG_GLOBAL_ARCH_BLACKWELL)" "1"
    ONLY_BLACKWELL =
else
    ONLY_BLACKWELL = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_TU116)" "1"
    ONLY_TU116 =
else
    ONLY_TU116 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_TU117)" "1"
    ONLY_TU117 =
else
    ONLY_TU117 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GA100)" "1"
    ONLY_GA100 =
else
    ONLY_GA100 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GA102)" "1"
    ONLY_GA102 =
else
    ONLY_GA102 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GA103)" "1"
    ONLY_GA103 =
else
    ONLY_GA103 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GA104)" "1"
    ONLY_GA104 =
else
    ONLY_GA104 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GA106)" "1"
    ONLY_GA106 =
else
    ONLY_GA106 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GA107)" "1"
    ONLY_GA107 =
else
    ONLY_GA107 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_AD102)" "1"
    ONLY_AD102 =
else
    ONLY_AD102 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GH100)" "1"
    ONLY_GH100 =
else
    ONLY_GH100 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GH202)" "1"
    ONLY_GH202 =
else
    ONLY_GH202 = _skip
endif

ifeq "$(LWCFG_GLOBAL_GPU_IMPL_GB100)" "1"
    ONLY_GB100 =
else
    ONLY_GB100 = _skip
endif

ifeq "$(LWCFG_GLOBAL_CHIP_T194)" "1"
    ONLY_T194 =
else
    ONLY_T194 = _skip
endif

ifeq "$(LWCFG_GLOBAL_CHIP_T234)" "1"
    ONLY_T234 =
else
    ONLY_T234 = _skip
endif

ifeq "$(LWCFG_GLOBAL_CHIP_T256)" "1"
    ONLY_T256 =
else
    ONLY_T256 = _skip
endif

MODS_KERNEL_DRIVER_TGZ = $(o_dir)/driver.tgz
MODS_PPC_DRIVER_TGZ = $(o_dir)/ppcdriver.tgz

# Useful for generating multi-line shell commands with foreach
define NEWLINE


endef

# Fix some paths to use only forward slashes
EXTRA_FILES := $(subst \,/,$(EXTRA_FILES))

resman_name = rm

# All platforms need JS library
ifeq "$(INCLUDE_BYTECODE)" "true"
    libraries    += ejs17
    lib_dirs     += $(GEN_LIB_DIR)/libejs17
    include_dirs += $(o_dir)/../libejs17
    include_dirs += ../jsprotect/include
    defines      += -DINCLUDE_BYTECODE
else
    libraries += js17
    lib_dirs  += $(LWMAKE_DIAG_OUT_DIR)/diag/js1.7
endif

# Iperf libraries for TegraMODS
ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        libraries += iperf
        lib_dirs  += $(LWMAKE_DIAG_OUT_DIR)/diag/mods/cheetah/utility/iperf
    endif
endif


libraries += jsdb17
lib_dirs  += $(LWMAKE_DIAG_OUT_DIR)/diag/js1.7/jsdb

lib_dirs  += $(GEN_LIB_DIR)/lwmake/diag/utils

# include the JS debugger files for internal releases
internal_release_files += ../js1.7/jsdb/debugger.js
internal_release_files += ../js1.7/jsdb/f.js

# Release builds with JS1.7 and MSVC require additional linking flags
# to avoid compiling out required code see:
# http://www.mozilla.org/js/spidermonkey/release-notes/NOICF.html
ifeq ($(base_os),win32)
    ifneq "$(BUILD_CFG)" "debug"
	LDFLAGS += /OPT:NOICF /OPT:REF
    endif
endif

ifeq "$(toolchain)" "gnu"
    libraries += m
endif

ifeq "$(base_os)" "linux"
    ifeq "$(use_pthreads)" "true"
        ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
            # Needed by clock_gettime in Xp::QueryPerformanceCounter
            libraries += rt
        endif
    endif
endif

ifeq "$(BUILD_OS_SUBTYPE)" "qnx"
    ifeq "$(filter 5.%,$(toolchain_rev))" "" # TODO remove this condition after we upgrade toolchain
    libraries += regex
    endif
endif

# Always include the OS's modsinc.h
force_include_files = $(base_os)/modsinc.h
force_include_files += core/include/jsprotect.h

# Include std namespace in the global namespace in types.h
defines += -DINCLUDE_STD_NAMESPACE

# It is wasteful to build OGL if GPU is not being built.
ifeq "$(INCLUDE_GPU)" "false"
    INCLUDE_OGL = false
    INCLUDE_DP = false
    INCLUDE_VBIOSSEC = false
    INCLUDE_LWDA = false
    INCLUDE_FSLIB = false
endif

# We use system libraries on CheetAh
ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        INCLUDE_RMAPI_TEGRA = true
    endif
    INCLUDE_MODSALLOC = false
endif

# if we are building CheetAh MODS we need only the CheetAh files.
# The MCP files are not required
INCLUDE_COMMON_TEGRA = false
ifeq "$(BUILD_OS)" "cheetah"
    INCLUDE_COMMON_TEGRA = true
    INCLUDE_VBIOSSEC = false
endif

ifeq "$(INCLUDE_DGPU)" "true"
    defines += -DINCLUDE_DGPU=1
endif

ifeq "$(SERVER_MODE)" "true"
    INCLUDE_OGL    = false
    INCLUDE_LWDA   = false
endif

# LWDART is part of LWCA
# Only AMD64 and aarch64 builds on (non-CheetAh non-sim) linux support LWDART for now
ifeq "$(INCLUDE_LWDA)" "true"
    ifeq "$(base_os)" "linux"
        ifneq (,$(filter $(BUILD_ARCH), aarch64 amd64))
            INCLUDE_LWDART ?= true
        endif
    endif
endif
INCLUDE_LWDART ?= false

# Vulkan driver is part of OpenGL driver
ifeq "$(INCLUDE_OGL)" "true"
    # Only selected configurations support Vulkan now
    ifeq "$(BUILD_OS)" "linuxmfg"
        INCLUDE_VULKAN ?= true

        # If Vulkan was force disabled in the environment, do not include VKLAYERS
        ifeq "$(INCLUDE_VULKAN)" "true"
            ifneq (,$(filter $(BUILD_ARCH), amd64 aarch64))
                ifneq "$(BUILD_CFG)" "release"
                    ifeq "$(SANITIZER)" ""
                        INCLUDE_VKLAYERS = true
                    endif
                endif
            endif
        endif
    endif

    ifeq "$(BUILD_OS)" "cheetah"
        INCLUDE_VULKAN ?= true
        ifneq "$(BUILD_CFG)" "release"
            INCLUDE_VKLAYERS = true
        endif
    endif

    ifeq "$(BUILD_OS)" "sim"
        INCLUDE_VULKAN ?= true
    endif

    ifeq "$(BUILD_OS)" "winmfg"
        INCLUDE_VULKAN ?= true
    endif
    ifeq "$(BUILD_OS)" "winsim"
        INCLUDE_VULKAN ?= true
    endif
    ifeq "$(BUILD_OS)" "winda"
        INCLUDE_VULKAN ?= true
    endif
    ifeq "$(BUILD_OS)" "linda"
        INCLUDE_VULKAN ?= true
    endif
endif

# If we want to add support for non-MODS OGL library we should define a separate flag
# such as USE_OGL_SYSTEM_LIB here. This way we can choose which sys libraries to use.
ifeq "$(USE_SYSTEM_LIBS)" "true"
    USE_LWDA_SYSTEM_LIB := true
endif

INCLUDE_VULKAN    ?= false
INCLUDE_VKLAYERS  ?= false

ifeq "$(INCLUDE_VULKAN)" "true"
    # Raytracing requires the LWCA library and the fatbin loader.
    INCLUDE_LWDA = true
    INCLUDE_FATBINLD = true
endif

ifeq "$(INCLUDE_VKLAYERS)" "true"
    defines += -DINCLUDE_VKLAYERS=1
endif

# LWVID tests use LWCA
ifeq "$(INCLUDE_LWDA)" "true"
    ifneq (,$(filter $(BUILD_OS), linuxmfg winmfg winda))
        ifeq "$(INCLUDE_GPU)" "true"
            INCLUDE_LWLWVID ?= true
        endif
    endif
endif
INCLUDE_LWLWVID ?= false

ifneq (,$(filter $(BUILD_OS), sim linuxmfg))
    ifeq "$(INCLUDE_GPU)" "true"
        INCLUDE_VGPU_PLUGIN = true
    endif
endif

ifeq "$(INCLUDE_MATHS_IST)" "true"
    libraries += maths-access
    CLIENTPATHS += //sw/tools/cryptopp-8.2/...
    MAL_DIR = ../maths-access/src
    include_dirs += $(MAL_DIR)
    include_dirs += $(MAL_DIR)/mods
    include_dirs += $(MAL_DIR)/third_party
    defines += -DUSE_MONGO=0
    defines += -DUSE_ENCRYPT=0
    defines += -DUSE_SRAM=1
    defines += -DUSE_MODS=1
    defines += -DUSE_CRYPTOPP=1
    defines += -DMODS_THREADS=1
    defines += -DUSE_DEPRECATED_MEMDEVICE_MGR=1
endif

ifeq "$(INCLUDE_YAML)" "true"
    libraries += yaml
    CLIENTPATHS += //sw/tools/mods/yaml-cpp-0.6.2/...
    include_dirs += $(BUILD_TOOLS_DIR)/mods/yaml-cpp-0.6.2/include
endif

libraries += lz4

# Various RMTests use libspdm library. Consult SWIPAT before using elsewhere.
# If used elsewhere, be sure to update diag/mods/docs/licenses.txt as well.
# Add to libraries here, as it depends upon CryptoPP library linked later.
ifeq "$(INCLUDE_LIBSPDM)" "true"
    lib_dirs  += $(LWMAKE_DIAG_OUT_DIR)/diag/spdm
    libraries += spdm
    CLIENTPATHS += //sw/tools/cryptopp-8.2/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/resman/src/libraries/libspdm/...
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/library/spdm_selwred_message_lib
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/include
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/include/library
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/include/hal
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/include/hal/x64
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/library/spdm_common_lib
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/library/spdm_responder_lib
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/library/spdm_requester_lib
    include_dirs += $(DRIVERS_DIR)/resman/src/libraries/libspdm/library/spdm_selwred_message_lib
    include_dirs += ../spdm
endif

# Disable MODS allocator in sim MODS because Fmodel (etc.) would
# also use it and run 2x+ slower
ifneq (,$(filter $(BUILD_OS), sim linda))
    INCLUDE_MODSALLOC = false
endif

# Allow UPX only in builds where it is supported
ifneq (,$(filter sanity_linuxmfg_amd64 release_linuxmfg_amd64,$(BUILD_CFG)_$(BUILD_OS)_$(BUILD_ARCH)))
    COMPRESS_REL ?= true
else
    COMPRESS_REL ?= false
endif

# Get around the lack of an OR operator
SYM_OR_DEBUG = false
ifeq "$(SYMBOLIZE)" "true"
    SYM_OR_DEBUG = true
    COMPRESS_REL = false
endif
ifeq "$(BUILD_CFG)" "debug"
    SYM_OR_DEBUG = true
endif

# Include standalone MATS in LinuxMfg builds
ifeq "$(INCLUDE_GPU)" "true"
    ifeq "$(BUILD_OS)" "linuxmfg"
        ifeq "$(BUILD_ARCH)" "amd64"
            ifeq "$(SANITIZER)" ""
                INCLUDE_MATS_DEFAULT := true
            endif
        endif
    endif
endif
INCLUDE_MATS ?= $(INCLUDE_MATS_DEFAULT)

# The protoutil always causes trouble when building with sanitizers
# if libboost has been recompiled with sanitizer support.  Disable
# protoutil with sanitizers.
ifneq "$(SANITIZER)" ""
    INCLUDE_PROTOUTIL := false
endif

ifeq "$(DEBUG_GEN_PROFILE)" "true"
    CFLAGS += $(CFLAGS_PROF_OPTS)
    LDFLAGS += $(LDFLAGS_PROF_OPTS)
endif

# Sim builds don't support the new Tasker on purpose,
# the old Tasker provides exact thread switching guarantee,
# required by simulators.
ifeq "$(BUILD_OS)" "sim"
    # Except the old Tasker does not work on PPC
    ifneq "$(BUILD_ARCH)" "ppc64le"
        USE_NEW_TASKER ?= false
    endif
endif
ifeq "$(BUILD_OS)" "winsim"
    USE_NEW_TASKER ?= false
endif

USE_NEW_TASKER ?= true

ifeq "$(USE_NEW_TASKER)" "true"
    defines += -DUSE_NEW_TASKER=1
endif

# Choose the command to copy files to MODS_RUNSPACE
ifeq "$(PRESERVE_RUNSPACE)" "true"
    CP_TO_RUNSPACE = $(CP_IF_CHANGED)
else
    CP_TO_RUNSPACE = $(CP)
endif

ifeq "$(IS_BRANCH_MODULAR)" "false"
    ifneq "$(TEGRA_TOP)" ""
        branch_inc_file = tegra_rel.inc
    else
        branch_inc_file = non_modular_branch.inc
    endif
    ifeq "$(wildcard $(branch_inc_file))" ""
        $(warning Release branch configuration file $(branch_inc_file) not found, using modular file)
        branch_inc_file = modular_branch.inc
    endif
else
    ifneq ("$(wildcard non_modular_branch.inc)","")
        $(error Existence of file non_modular_branch.inc is not allowed in modular branches to not override it during rebases)
    endif
    branch_inc_file = modular_branch.inc
endif

include $(branch_inc_file)

# The modular_branch.inc file sets the GLSLANG_PARENT_DIR variable or the build_git.sh will
# override it for CheetAh and I want to use simple variable expansion. So this must go after
# including branch_inc_file
VK_LAYERS_DIR      ?= $(SHARED_MODS_FILES_DIR)/vulkan/validationlayers/$(VK_LAYERS_VERSION)
GLSLANG_DIR        := $(GLSLANG_PARENT_DIR)/glslang
SPIRV_TOOLS_DIR    := $(GLSLANG_PARENT_DIR)/spirv-tools/include
SPIRV_TOOLS_DIR_P4 := $(GLSLANG_PARENT_DIR_P4)/spirv-tools/include
CLIENTPATHS        += $(GLSLANG_PARENT_DIR_P4)/...

# Force disable support for future chips in all bound JS builds
ifneq "$(BOUND_JS)" "false"
    override INCLUDE_FUTURE_CHIPS := false
endif

# Enable redacting of future chip names in release branches
ifeq "$(INCLUDE_FUTURE_CHIPS)" "true"
    REDACT_STRINGS ?= false
else
    REDACT_STRINGS ?= true
endif
# Disable redacting for certain builds, like sim MODS
ifneq (,$(filter $(BUILD_OS), macosxmfg sim winsim))
    REDACT_STRINGS := false
endif
# Disable redacting in debug builds
ifeq "$(BUILD_CFG)" "debug"
    REDACT_STRINGS := false
endif
# Disable redacting in symbolized builds
ifeq "$(SYMBOLIZE)" "true"
    REDACT_STRINGS := false
endif
# Disable redacting in gtest build
ifeq "$(INCLUDE_MODS_GTEST)" "true"
    REDACT_STRINGS := false
endif
# Disable redacting with MINGW make, redaction uses a linux shell script which
# is unavailable
ifdef MINGW_MAKE
    REDACT_STRINGS := false
endif

REDACT_RANDOMS = $(o_dir)/redact_randoms
GUARDWORD_FILE = $(o_dir)/guardwords.txt
GUARDWORD_IGNORE = $(o_dir)/gw_ignore.txt
ifeq "$(REDACT_STRINGS)" "true"
    install_files += $(REDACT_RANDOMS)
    REDACT_SCRIPT = $(MODS_DIR)/tools/redact_future_chips.sh $(REDACT_RANDOMS)
else
    # The UNIX program "true" available in the shell ignores all arguments
    REDACT_SCRIPT = true
endif

ifeq "$(REDACT_STRINGS)" "true"
    FUTURE_DEV := _SKIP
else
    defines += -DENABLE_FUTURE_CHIPS=1
    FUTURE_DEV :=
endif

# Enable guardword in release branches
# This is controlled via the branch inc file.
ifeq "$(INCLUDE_FUTURE_CHIPS)" "true"
    ENABLE_GUARDWORD ?= false
else
    # Enable guardword only in release builds
    ifeq "$(BUILD_CFG)" "release"
        ENABLE_GUARDWORD ?= true
    else
        ENABLE_GUARDWORD ?= false
    endif

    # Skip guardword on CheetAh, because lwconfig on CheetAh disables Turing chips
    # and causes guardword to mistakenly flag Turing.
    ifeq "$(BUILD_OS)" "cheetah"
        ENABLE_GUARDWORD := false
    endif

    # Skip guardword with RM tests.
    # RM tests include a bunch of stuff for DVS which triggers guardword.
    ifeq "$(INCLUDE_RMTEST)" "true"
        ENABLE_GUARDWORD := false
    endif
endif

CLIENTPATHS += //sw/$(BRANCH)/diag/...
CLIENTPATHS += //sw/$(BRANCH)/drivers/common/...
CLIENTPATHS += //sw/$(BRANCH)/drivers/resman/...
CLIENTPATHS += //sw/$(BRANCH)/drivers/cheetah/platform/drivers/memory/...
CLIENTPATHS += //sw/$(BRANCH)/drivers/modeset/...
CLIENTPATHS += //sw/$(BRANCH)/drivers/displayport/...
CLIENTPATHS += //sw/$(BRANCH)/drivers/mm/...
CLIENTPATHS += //sw/$(BRANCH)/sdk/...
CLIENTPATHS += //sw/tools/lpng1251/...
CLIENTPATHS += //sw/tools/mods/boost/...
CLIENTPATHS += //sw/tools/mods/boost2/...
CLIENTPATHS += //sw/tools/mods/rapidjson/...
CLIENTPATHS += //sw/tools/mods/openssl-1.1.0f/include/...
CLIENTPATHS += //sw/tools/openssl-1.0.0c/include/...

ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        CLIENTPATHS += //sw/tools/mods/iperf/...
    endif
endif

#------------------------------------------------------------------------------#
# TRACE LEVEL 0 - same as release, simple allocation counting / threshold      #
# TRACE LEVEL 1 - log allocations, save file and line number if possible       #
# TRACE LEVEL 2 - record fast stack trace (decode symbols after exelwtion)     #
# TRACE LEVEL 3 - record full trace (covers shared libraries, slower)          #
#------------------------------------------------------------------------------#
defines += -DDEBUG_TRACE_LEVEL=$(DEBUG_TRACE_LEVEL)
defines += -DINCLUDE_UNITTEST=$(INCLUDE_UNITTEST)

INCLUDE_DRVEXPORT = false
# The drvexport library calls all modsdrv.h funtions so that gcc will allow
# other .so files (such as external tests) to call them, even if MODS does not
# link against them.
ifeq "$(base_os)" "linux"

    CLIENTPATHS += //sw/$(BRANCH)/drivers/unix/common/utils/...

    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        # The dynamic loader
        libraries += dl
    endif

    ifeq "$(dll_build)" "true"
        INCLUDE_DRVEXPORT = true
        libraries += drvexport
        lib_dirs += $(GEN_LIB_DIR)/libdrvexport
    endif
endif

ifeq "$(dll_build)" "true"
    ifneq (,$(filter $(BUILD_OS), linda linux linuxmfg sim cheetah))
        USE_SHARED_LIB_STUBS ?= true
    endif
endif
USE_SHARED_LIB_STUBS ?= false

ifeq "$(USE_SHARED_LIB_STUBS)" "true"
    ifneq "$(dll_build)" "true"
        $(error USE_SHARED_LIB_STUBS is not supported for non shared builds)
    endif
endif

#--------------------------------------------------------------------------------
# decision variable for cheetah/makesrc.inc core/makesrc.inc and device/makesrc.inc
#--------------------------------------------------------------------------------

include_azalia_support=false
include_smb_support=false

ifeq "$(INCLUDE_COMMON_TEGRA)" "true"
    include_smb_support=false
    include_azalia_support=true
else
    ifeq "$(base_os)" "linux"
        include_azalia_support=true
        include_smb_support=true
    endif
    ifeq "$(BUILD_OS)" "macosxmfg"
        include_azalia_support=true
    endif
    ifeq "$(BUILD_OS)" "winmfg"
        include_smb_support=true
    endif
endif

#-------------------------------------------------------------------------------
# Core Files
#-------------------------------------------------------------------------------

include core/makesrc.inc

ifneq "$(INCLUDE_MODSALLOC)" "false"
    defines += -DINCLUDE_MODSALLOC=1
endif

ifeq "$(INCLUDE_THREADSTATS)" "true"
    ifeq "$(USE_NEW_TASKER)" "true"
        defines += -DTHREAD_STATISTICS=1
    endif
endif

ifneq "$(BOUND_JS)" "false"
    defines += -DBOUND_JS=1
endif

defines += -DJS_C_STRINGS_ARE_UTF8

ifeq "$(INCLUDE_GPU)" "true"
    defines += -DINCLUDE_GPU=1
endif

ifeq "$(INCLUDE_MDIAG)" "true"
    defines += -DINCLUDE_MDIAG=1
endif

ifeq "$(INCLUDE_MDIAGUTL)" "true"
    defines += -DINCLUDE_MDIAGUTL=1
endif

ifeq "$(INCLUDE_RMTEST)" "true"
    defines += -DINCLUDE_RMTEST=1
endif

ifeq "$(RECORD_REG_WR)" "true"
    defines += -DRECORD_REG_WR=1
endif

ifeq ($(base_os),win32)
    # Must turn on C++ exception handling when using the C++ standard library.
    # Would really prefer to not have to turn this on... unwinding logic is not
    # free, and we don't really want C++ exceptions.
    CFLAGS += /EHsc

    # MODS is pushing the limit on the number of sections in an MSVC object
    # file. This flag increase the maximum number of sections from 2^16 to 2^32.
    CFLAGS += /bigobj

    # Explicitly enable RTTI in winsim build, since for some reason VC++ 7.1
    # defaults to /GR-
    ifeq "$(sim_build)" "true"
        CFLAGS += /GR
    endif

    # Treat warnings as errors
    ifeq "$(BUILD_OS)" "winsim"
        ifeq "$(BUILD_ARCH)" "amd64"
            warnings += /WX
        endif
    endif
else
    # Warn about various picky things
    warnings += -Wsign-compare
    ifneq "$(INCLUDE_MODS_GTEST)" "true"
        warnings += -Wundef
    endif
    cc_warnings += -Wnested-externs

    # Treat warnings as errors
    ifeq "$(BUILD_OS)" "cheetah"
        warnings += -Werror
    endif
    ifeq "$(BUILD_OS)" "linuxmfg"
        ifneq "$(BUILD_TEGRA_EMBEDDED)" "true"
            warnings += -Werror
        endif
    endif
    ifeq "$(BUILD_OS)" "linux"
        warnings += -Werror
    endif
    ifeq "$(BUILD_OS)" "sim"
        warnings += -Werror
    endif
    ifeq "$(BUILD_OS)" "macosxmfg"
        warnings += -Werror
    endif

    ifneq "$(BUILD_CFG)" "debug"
        optimize = $(SYMFLAGS) -O2

        ifeq "$(base_os)" "macosx"
            #OSX doesn't support -s, we will strip it later
            strip =
        else
            ifneq "$(SYMBOLIZE)" "true"
                strip = -s
            else
                strip =
            endif
        endif
    else
        strip    =
    endif

    LDFLAGS += $(strip)
endif

ifeq "$(INCLUDE_LWWATCH)" "true"
    defines += -DINCLUDE_LWWATCH
    CLIENTPATHS += //sw/$(BRANCH)/apps/lwwatch/...
endif

ifeq "$(INCLUDE_LWLINK)" "true"
    defines += -DINCLUDE_LWLINK
    CLIENTPATHS += //sw/$(BRANCH)/drivers/lwlink/...
endif

ifeq "$(INCLUDE_LWSWITCH)" "true"
    defines += -DINCLUDE_LWSWITCH
    CLIENTPATHS += //sw/$(BRANCH)/drivers/lwswitch/...
    CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/unix-build/...
    defines += -DINCLUDE_LWSWITCH
    gdm_gen_h_files += $(gen_cpp_dir)/gdm_message_reader.h
    gdm_gen_h_files += $(gen_cpp_dir)/gdm_message_writer.h
    gdm_gen_h_files += $(gen_cpp_dir)/gdm_message_structs.h
    gdm_gen_h_files += $(gen_cpp_dir)/gdm_message_gen.h
    gdm_gen_h_files += $(gen_cpp_dir)/gdm_message_handler.h
    gdm_gen_cpp_files += $(gen_cpp_dir)/gdm_message_handler.cpp

    gen_h_files += $(gdm_gen_h_files)
    gen_cpp_files += $(gdm_gen_cpp_files)

    $(o_files) : $(gdm_gen_h_files)

    $(gdm_gen_h_files) $(gdm_gen_cpp_files): $(TRANSPORT_PATH)/common/messages.proto $(MODS_DIR)/../utils/protobuf/protobuf.py
	    $(Q)@$(ECHO) Generating tranport layer msg structures
	    $(Q)@$(PYTHON3) $(MODS_DIR)/../utils/protobuf/protobuf.py                \
			--reader_filename=$(gen_cpp_dir)/gdm_message_reader.h   \
			--reader_namespace=GdmMessageReader                           \
			--writer_filename=$(gen_cpp_dir)/gdm_message_writer.h   \
			--writer_namespace=GdmMessageWriter                           \
			--struct_filename=$(gen_cpp_dir)/gdm_message_structs.h  \
			--struct_namespace=GdmMessages                                \
			--header_filename=$(gen_cpp_dir)/gdm_message_gen.h      \
			--handler_basename=$(gen_cpp_dir)/gdm_message_handler   \
			--handler_namespace=GdmMessageHandler                         \
			$(TRANSPORT_PATH)/common/messages.proto
endif

ifeq "$(INCLUDE_FM)" "true"
    defines += -DINCLUDE_FM
    CLIENTPATHS += //sw/$(BRANCH)/apps/fabricmanager/...
    CLIENTPATHS += //sw/$(BRANCH)/apps/lwml/build/...
    CLIENTPATHS += //sw/$(BRANCH)/apps/lwml/common/...
endif

ifeq "$(INCLUDE_TRANSPORT)" "true"
    defines += -DINCLUDE_TRANSPORT
    CLIENTPATHS += //sw/$(BRANCH)/diag/multinode/...
endif

ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    CLIENTPATHS += //sw/$(BRANCH)/drivers/vgpu/...
endif

ifeq "$(INCLUDE_FLCNDBG)" "true"
    CLIENTPATHS += //sw/$(BRANCH)/apps/lwflcndbg/...
endif

# paths common to all falcon builds (lwrrently limited to PMU and DPU)
ifeq "$(include_falcon)" "true"
    CLIENTPATHS += //sw/tools/falcon-gcc/...
    CLIENTPATHS += //sw/$(BRANCH)/uproc/...
    CLIENTPATHS += //sw/$(BRANCH)/tools/restricted/pmu/...
else
    CLIENTPATHS += //sw/$(BRANCH)/uproc/os/...
endif

# additional paths required for the PMU build
ifeq "$(INCLUDE_PMU)" "true"
    CLIENTPATHS += //sw/$(BRANCH)/pmu_sw/...
endif

ifeq "$(COMPRESS_REL)" "true"
    CLIENTPATHS += //sw/tools/linux/upx-3.94-amd64_linux/...
endif

ifeq "$(INCLUDE_GSP)" "true"
	ifneq "$(shell uname)" "Linux"
        $(error GSP-RM firmware must be built on Linux)
    endif
    # additional paths required for the GSP build
    CLIENTPATHS += //sw/tools/riscv/toolchain/linux/release/20190625/...
    CLIENTPATHS += //sw/misc/linux/unix-build
endif

#-------------------------------------------------------------------------------
# CPU Files
#-------------------------------------------------------------------------------

include device/cpu/makesrc.inc

#-------------------------------------------------------------------------------
#
# Common (all target platform) includes, libraries, and source files.
#-------------------------------------------------------------------------------

ifeq "$(base_os)" "macosx"
    include_dirs += $(base_os) # For IOKit
endif
ifeq "$(INCLUDE_LWDA)" "true"
    ifeq "$(USE_LWDA_SYSTEM_LIB)" "true"
        include_dirs += $(abspath $(DRIVERS_DIR)/gpgpu/compiler/gpgpu/export/bin/x86_64_Linux_develop/stub)
    else
        include_dirs += $(LWDA_DIR)/mods/inc
        include_dirs += $(LWDA_DIR)/inc
    endif
endif
include_dirs += $(DRIVERS_DIR)
include_dirs += ../xml
include_dirs += ../utils
include_dirs += ../encryption
include_dirs += $(BOOST_DIR)
include_dirs += $(BUILD_TOOLS_DIR)/mods/rapidjson
ifeq "$(INCLUDE_VULKAN)" "true"
    include_dirs += $(GLSLANG_DIR)
    include_dirs += $(SPIRV_TOOLS_DIR)
    include_dirs += $(GLM_DIR)
    CLIENTPATHS += //sw/apps/gpu/drivers/opengl/lwogtest/SrcList.inc
    CLIENTPATHS += //sw/apps/gpu/drivers/opengl/lwogtest/thirdparty/shaderc/...
    CLIENTPATHS += //sw/tools/mods/glm/0.9.9.7/glm/...
endif

ifeq "$(BUILD_OS)" "win32"
    include_dirs += $(BUILD_TOOLS_DIR)/sdk/DirectX_Mar2009/Include
    include_dirs += $(DRIVERS_DIR)/lwapi/sdk/_out/Windows/lwsdk/$(BUILD_ARCH)
endif # BUILD_OS == win32

ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        include_dirs += $(MODS_DIR)/cheetah/utility/iperf
    endif
endif

include_dirs += ../js1.7
include_dirs += ../thermalres
include_dirs += ../thermalres/include
include_dirs += .
include_dirs += $(DRIVERS_DIR)/common
include_dirs += $(DRIVERS_DIR)/common/inc
include_dirs += $(DRIVERS_DIR)/common/shared/inc
include_dirs += $(DRIVERS_DIR)/common/inc/displayport
include_dirs += $(DRIVERS_DIR)/OpenGL/interface
include_dirs += $(DRIVERS_DIR)/modeset/hdmipacket
include_dirs += $(DRIVERS_DIR)/modeset/timing
include_dirs += gpu/display/modeset.nxt/INCLUDE
include_dirs += $(DRIVERS_DIR)/OpenGL/interface
include_dirs += $(DRIVERS_DIR)/resman/arch/lwalloc/mods/inc
include_dirs += $(DRIVERS_DIR)/resman/arch/lwalloc/common/inc
include_dirs += $(DRIVERS_DIR)/resman/inc/libraries
include_dirs += $(DRIVERS_DIR)/resman/inc/physical
include_dirs += $(DRIVERS_DIR)/resman/interface
include_dirs += $(DRIVERS_DIR)/common/inc/hwref
include_dirs += $(DRIVERS_DIR)/common/inc/swref
include_dirs += $(DRIVERS_DIR)/resman/kernel/inc
include_dirs += $(DRIVERS_DIR)/common/amaplib/include
include_dirs += $(DRIVERS_DIR)/common/dram_hal/include

RMAPI_TEGRA_MIRROR=$(MODS_OUTPUT_DIR)/$(rel_o_dir)/rmapi_tegra_mirror
ifeq "$(BUILD_OS)" "cheetah"
    include_dirs += $(DRIVERS_DIR)/unix/rmapi_tegra/include

    include_dirs += $(DRIVERS_DIR)/unix/rmapi_tegra/lwrminclude
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/gen
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/tegra_out/obj/include
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/tegra_top/core/include
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/tegra_top/display/lwdc/include
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/tegra_top/multimedia/lwvideo/include/
    include_dirs += $(RMAPI_TEGRA_MIRROR)/include/outmirror/tegra_top/multimedia/tvmr/include
    CLIENTPATHS += //sw/$(BRANCH)/drivers/unix/rmapi_tegra/...
endif
ifeq "$(BUILD_OS)$(BUILD_ARCH)" "linuxmfgaarch64"
    INCLUDE_RMAPI_TEGRA_MIRROR = true
    CLIENTPATHS += //sw/$(BRANCH)/drivers/unix/rmapi_tegra/...
else
    INCLUDE_RMAPI_TEGRA_MIRROR = false
endif
include_dirs += $(SDK_DIR)/lwpu/inc
include_dirs += $(DRIVERS_DIR)/displayport/inc

ifeq "$(INCLUDE_LWLINK)" "true"
    include_dirs += $(DRIVERS_DIR)/lwlink/interface

    ifeq "$(use_mods_lwlink)" "false"
        ifneq "$(BUILD_OS)" "cheetah"
            include $(DRIVERS_DIR)/lwlink/user/lwlink/mods/makesrc.inc
        endif
    endif
endif

ifeq "$(INCLUDE_LWSWITCH)" "true"
    include_dirs += $(DRIVERS_DIR)/lwswitch/interface
    include_dirs += $(DRIVERS_DIR)/lwswitch/kernel/inc
    ifeq "$(use_mods_lwswitch)" "false"
        include $(DRIVERS_DIR)/lwswitch/user/MODS/makesrc.inc
    endif
endif

#-------------------------------------------------------------------------------
# GPU files
#-------------------------------------------------------------------------------
ifeq "$(USE_SHARED_LIB_STUBS)" "true"
    LIB_STUBS_DIR = $(GEN_LIB_DIR)/libstubs
    lib_dirs  += $(LIB_STUBS_DIR)
endif

cpp_files += core/main/threadsync.cpp
ifeq "$(use_mods_rm)" "true"
    libraries += rm
    # When using shared library stubs, MODS will link against librm in the
    # libstubs directory instead
    ifneq "$(USE_SHARED_LIB_STUBS)" "true"
        lib_dirs  += $(GEN_LIB_DIR)/librm
    endif
endif

ifeq "$(INCLUDE_MDIAG)" "true"
    ifeq "$(INCLUDE_MDIAG_REFS)" "true"
        INCLUDE_REFS := true
    endif
endif

ifdef CYGWIN_MAKE
    UPHY_FILES_DIR      := $(shell cygpath -m '$(UPHY_FILES_DIR)')
    UPHY_FILES_DIR_UNIX := $(shell cygpath -u '$(UPHY_FILES_DIR)')
else
    UPHY_FILES_DIR_UNIX := $(UPHY_FILES_DIR)
endif
ifeq "$(INCLUDE_GPU)" "true"
    include gpu/makesrc.inc
    include gpu/reghal/makesrc.inc

    ifeq "$(INCLUDE_GPUTEST)" "true"
        defines += -DINCLUDE_GPUTEST=1
        include gpu/tests/makesrc.inc
    endif
    ifeq "$(INCLUDE_RMTEST)" "true"
        include gpu/tests/rm/makesrc.inc
        ifeq "$(base_os)" "linux"
            include gpu/tests/rm/ucodefuzz/ucodefuzz.mk
        endif
    endif
endif

#-------------------------------------------------------------------------------
# Amap files
#-------------------------------------------------------------------------------
ifeq "$(INCLUDE_GPU)" "true"
    libraries += amap dramhal
    lib_dirs += $(GEN_LIB_DIR)/libamap
    lib_dirs += $(GEN_LIB_DIR)/libdramhal
endif

ifeq "$(INCLUDE_LWLWVID)" "true"
    libraries += lwlwvid
    lib_dirs += $(GEN_LIB_DIR)/liblwlwvid

    CLIENTPATHS += //sw/$(BRANCH)/drivers/multimedia/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/unix/common/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/unix/lwpu-push/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/video/encode/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/video/opticalflow/...
    CLIENTPATHS += //sw/tools/sdk/LWCA/10.0/x86_64/include/...

    ifeq "$(BUILD_OS)" "winda"
        CLIENTPATHS += //sw/$(BRANCH)/drivers/ddraw/common/inc/...
    endif
endif

#-------------------------------------------------------------------------------
# CheetAh files
#-------------------------------------------------------------------------------

include cheetah/makesrc.inc

#-------------------------------------------------------------------------------
# MCP files
#-------------------------------------------------------------------------------

include device/makesrc.inc

ifeq "$(INCLUDE_MDIAG)" "true"
    include mdiag/makesrc.inc
    ifeq "$(INCLUDE_MDIAGUTL)" "true"
        include mdiag/utl/makesrc.inc
    endif
endif

#-------------------------------------------------------------------------------
# Tools files
#-------------------------------------------------------------------------------

include tools/makesrc.inc

#-------------------------------------------------------------------------------
# Non-branched Tools files
#-------------------------------------------------------------------------------

CLIENTPATHS += $(MODS_TOOLS_DIR_P4)/build/makesrc.inc
-include $(MODS_TOOLS_DIR)/build/makesrc.inc

#-------------------------------------------------------------------------------
# Spec files
#-------------------------------------------------------------------------------

include specs/makesrc.inc

#-------------------------------------------------------------------------------
# Contrib Files
# The contrib folder does not have any submake processes and should not be included
# when we use any submake target. This is because the DVS build scripts don't sync
# the CONTRIB_DIR (//sw/rel/<branch>/... folders for those submake only processes.
# Removing this filter would require DVS to do unnecessary syncs and that would
# slow down the DVS build process.
#-------------------------------------------------------------------------------
ifneq (,$(filter-out submake.%, $(MAKECMDGOALS) ))
    ifdef CYGWIN_MAKE
        CONTRIB_DIR                := $(shell cygpath -m '$(CONTRIB_DIR)')
        CONTRIB_DIR_UNIX           := $(shell cygpath -u '$(CONTRIB_DIR)')
    else
        CONTRIB_DIR_UNIX    := $(CONTRIB_DIR)
        UPHY_FILES_DIR_UNIX := $(UPHY_FILES_DIR)
    endif

    -include $(CONTRIB_DIR_UNIX)/makesrc.inc

    print_contrib_dir:
	    @$(ECHO) "Contrib:"$(CONTRIB_DIR_UNIX)
endif

include protobuf/makesrc.inc

libraries += xml

ifeq "$(use_static_lwwatch)" "true"
    libraries += lwwatch
    lib_dirs  += $(GEN_LIB_DIR)/liblwwatch
endif

ifeq "$(INCLUDE_VULKAN)" "true"
    CLIENTPATHS += //sw/$(BRANCH)/drivers/compiler/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/gpgpu/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/khronos/interface/...
    CLIENTPATHS += //sw/tools/linux/mods/python-3.6.3-glibc-2.5/...
    CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/perl-5.10.0/...
    CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/llvm/3.8.0/bin/...
    CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/unix-build/...
    CLIENTPATHS += //sw/tools/unix/targets/common/bison/m4sugar/...
endif

ifeq "$(INCLUDE_LWDA)" "true"
    ifeq "$(USE_LWDA_SYSTEM_LIB)" "true"
        defines   += -DUSE_LWDA_SYSTEM_LIB
        libraries += lwca
        lib_dirs  += $(abspath $(DRIVERS_DIR)/gpgpu/compiler/gpgpu/export/bin/x86_64_Linux_develop)
        lib_dirs  += $(abspath $(DRIVERS_DIR)/gpgpu/compiler/gpgpu/export/bin/x86_64_Linux_develop/stub)
    else
        libraries += modslwda
        lib_dirs  += $(GEN_LIB_DIR)/libmodslwda
    endif
    defines += -DINCLUDE_LWDA=1
    CLIENTPATHS += //sw/$(BRANCH)/drivers/gpgpu/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/common/amodel/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/compiler/gpgpucomp/...
    CLIENTPATHS += //sw/$(BRANCH)/drivers/compiler/utilities/...
    ifeq "$(base_os)" "linux"
        CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/targets/Linux-x86_64/configs/...
        CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/targets/Linux-x86_64/gcc-4.1.1as2/...
    endif
    ifeq "$(BUILD_OS)" "cheetah"
        CLIENTPATHS += //sw/$(BRANCH)/drivers/unix/common/lwos/interface/...
    endif
    LINUXDOXYGEN=0
    ifeq "$(base_os)" "linux"
        LINUXDOXYGEN=1
    endif
    ifeq "$(HOST_OS)" "linux"
        LINUXDOXYGEN=1
    endif
    ifeq "$(LINUXDOXYGEN)" "1"
        CLIENTPATHS += //sw/tools/linux/doxygen/1.5.8/bin/...
    else
        ifeq "$(base_os)" "macosx"
            CLIENTPATHS += //sw/tools/macos/doxygen/1.5.8/bin/...
        else
            CLIENTPATHS += //sw/tools/win32/doxygen/1.5.8/bin/...
        endif
    endif
endif

ifeq "$(INCLUDE_FM)" "true"
    libraries += fabricmanager
    lib_dirs  += $(LWMAKE_DRV_OUT_DIR)/apps/fabricmanager/mods
    include_dirs += $(FM_PATH)
    include_dirs += $(FM_PATH)/globalfm
    include_dirs += $(FM_PATH)/localfm
    include_dirs += $(FM_PATH)/common
    include_dirs += $(FM_PATH)/config
    include_dirs += $(FM_PATH)/infra/transport
    include_dirs += $(FM_PATH)/sdk/public
    include_dirs += $(FM_PATH)/libs/libevent-2.0.22-stable/include
    include_dirs += $(FM_PATH)/libs/libevent-2.0.22-stable/include/event2/
    include_dirs += $(LWML_PATH)/common
    include_dirs += $(LWML_PATH)/common/mods
    include_dirs += $(DRIVERS_DIR)/lwlink/user/lwlink
    include_dirs += $(DRIVERS_DIR)/lwswitch/user
    include_dirs += $(MODS_DIR)/core/include
    include_dirs += $(LWMAKE_DRV_OUT_DIR)/apps/fabricmanager/mods
    include_dirs += $(PROTOBUF_PATH)/include
endif

ifeq "$(INCLUDE_TRANSPORT)" "true"
    ifeq "$(INCLUDE_FM)" "true"
        LWMAKE_ARGS += INCLUDE_LIBEVENT=false
    endif
    libraries += multinode_transport
    lib_dirs  += $(LWMAKE_DRV_OUT_DIR)/diag/multinode/transport
    include_dirs += $(TRANSPORT_PATH)
    include_dirs += $(TRANSPORT_PATH)/transport
    include_dirs += $(TRANSPORT_PATH)/common
endif

ifeq "$(USE_LWDA_SYSTEM_LIB)" "true"
    # Needed for linking fabricmanager when using LWCA system lib
    libraries += lwos
endif

ifeq "$(use_mods_lwswitch)" "true"
    libraries += lwswitch
    lib_dirs  += $(GEN_LIB_DIR)/liblwswitch
endif
ifeq "$(use_mods_lwlink)" "true"
    libraries += lwlink
    lib_dirs  += $(GEN_LIB_DIR)/liblwlink
endif

ifeq "$(INCLUDE_LWDART)" "true"

    # LWDART
    defines      += -DINCLUDE_LWDART=1
    libraries    += lwdart_static

    # Lwrrently only AMD64 has a debug build of LWDART
    ifeq "$(BUILD_ARCH)" "amd64"
        ifeq "$(BUILD_CFG)" "debug"
            LWDART_LIB_CFG ?= debug
        else
            LWDART_LIB_CFG ?= release
        endif
    endif
    LWDART_LIB_CFG ?= .

    # Use the MODS-specific LWCA runtime binary and headers
    # Only the MODS version can be linked against a static LWCA driver
    LWDA_TESTS_DIR := $(abspath $(MODS_DIR)/gpu/tests/lwca)
    include_dirs += $(LWDA_TESTS_DIR)/lwdart_headers
    include_dirs += $(LWDA_TESTS_DIR)/lwdart_headers/include
    lib_dirs += $(LWDA_TESTS_DIR)/lwdart_libs/$(BUILD_ARCH)/$(LWDART_LIB_CFG)

    # fatbinary-loader
    INCLUDE_FATBINLD = true

    # The Cask library contains GEMM kernels used by LwdaLinpackCask
    CASK_DIR := $(abspath $(MODS_DIR)/gpu/tests/lwca/cask/$(BUILD_ARCH))
    CASK_SPECIFIER :=

    # Contains the CASK LWCA kernel init code
    libraries += init_kernels_lib_static
    # Contains the core of the CASK host C++ code
    libraries += cask.core$(CASK_SPECIFIER)

    # CASK plugin for SASS-LIB
    libraries += sass.cask$(CASK_SPECIFIER)
    # CASK plugin for LWTLASS (force link)
    libraries += lwtlass.cask$(CASK_SPECIFIER)
    # CASK plugin for XMMA (force link)
    libraries += xmma.cask.gemm$(CASK_SPECIFIER)
    libraries += xmma.cask.common$(CASK_SPECIFIER)

    # Re-link due to cirlwlar dependencies
    libraries += cask.core$(CASK_SPECIFIER)

    # Configure cask libraries and include directories
    lib_dirs     += $(CASK_DIR)/lib
    lib_dirs     += $(CASK_DIR)/lib64
    include_dirs += $(CASK_DIR)/include

    CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/perl-5.10.0/...
    CLIENTPATHS += //sw/tools/unix/hosts/Linux-x86/unix-build/bin/...
    CLIENTPATHS += //sw/tools/unix/targets/common/...
endif

ifeq "$(INCLUDE_FATBINLD)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        # fatbinary-loader
        libraries += fatbinloader
        lib_dirs  += $(LWMAKE_DRV_OUT_DIR)/drivers/gpgpu/lwca/mods/fatbinaryLoader
        LWDA_MAKE_FLAGS += MODS_FATBINARY=true
    endif
endif

ifeq "$(INCLUDE_DP)" "true"
    libraries += displayport
    lib_dirs  += $(GEN_LIB_DIR)/libdisplayport
endif

ifeq "$(INCLUDE_OGL)" "true"
    ifeq "$(INCLUDE_OGL_APIC)" "true"
        libraries += oglapic
    endif
    ifneq "$(USE_SYSTEM_LIBS)" "true"
        libraries += modsgl
        ifneq "$(USE_SHARED_LIB_STUBS)" "true"
            # When using shared library stubs, MODS will link against libmodsgl
            # in the libstubs directory instead
            lib_dirs  += $(GEN_LIB_DIR)/libmodsgl
        endif
    else
        libraries += GL
    endif
endif

ifeq "$(INCLUDE_VULKAN)" "true"
# ENABLE_OPT=1 only applies to HLSL (Direct3D shaders), however we need to define this to zero to
# prevent compilation errors because we have all warnings generate errors.
# glslang needs to go after modsgl
    defines += -DENABLE_OPT=0
    libraries += spirv-tools
    libraries += glslang
    lib_dirs  += $(GEN_LIB_DIR)/libglslang
endif

ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    libraries += rmapi_tegra
    lib_dirs  += $(GEN_LIB_DIR)/librmapi_tegra

    # Link against libraries which come with the OS.
    # For HOS, link against actual object files,
    # while on other platforms, link against stubs.

    rmapi_libraries += lwdc
    rmapi_libraries += lwmedia
    rmapi_libraries += lwos
    rmapi_libraries += lwparser
    rmapi_libraries += lwrm_chip
    rmapi_libraries += lwrm_gpu
    rmapi_libraries += lwrm_host1x
    rmapi_libraries += lwrm_mem
    rmapi_libraries += lwrm_stream
    rmapi_libraries += lwrm_surface
    rmapi_libraries += lwrm_sync
    rmapi_libraries += lwtvmr
    rmapi_libraries += lwvideo

    libraries += $(rmapi_libraries)
    lib_dirs  += $(addprefix $(GEN_LIB_DIR)/lib, $(rmapi_libraries))
endif

basic_release_files += $(EXTRA_FILES)
exelwtable_files    += $(EXTRA_FILES)

INSTALL_MODS_DRIVER = false
ifeq "$(BRANCH)" "dev/gpu_drv/chips_a"
    ifneq "$(BUILD_CFG)" "release"
        # In non-release builds in chips_a, package the driver from dev branch
        # for testing purposes.  The header chips_a is compiled against still
        # comes from rel branch to avoid regressions.
        LINUX_DRIVER_SRC_PATH ?= $(LINUX_DRV_DIR)/dev
    endif
endif
LINUX_DRIVER_SRC_PATH ?= $(LINUX_DRV_DIR)/rel
LINUX_DRIVER_INC_PATH ?= $(LINUX_DRV_DIR)/rel

ifeq "$(BUILD_OS)" "sim"
    ifneq "$(wildcard $(LINUX_DRV_DIR) )" ""
        INSTALL_MODS_DRIVER = true
    else
        $(warning Add //sw/mods/linuxdrv/... to the P4 client to install the MODS kernel driver. )
    endif
endif
ifeq "$(BUILD_OS)" "linuxmfg"
    INSTALL_MODS_DRIVER = true
endif

ifeq "$(INSTALL_MODS_DRIVER)" "true"
    include_dirs                  += $(LINUX_DRIVER_INC_PATH)
    basic_release_files           += $(LINUX_DRV_DIR)/install_module.sh
    exelwtable_files              += $(LINUX_DRV_DIR)/install_module.sh
    basic_release_files           += $(MODS_KERNEL_DRIVER_TGZ)
    basic_release_files$(ONLY_PC) += tools/check_config.sh
    exelwtable_files$(ONLY_PC)    += tools/check_config.sh
    CLIENTPATHS                   += //sw/mods/linuxdrv/...
endif

ifeq "$(INCLUDE_XUSB)" "true"
    defines                       += -DINCLUDE_XUSB
    include_dirs                  += $(BUILD_TOOLS_DIR)/mods/libusb/libusb-1.0.21/libusb
    basic_release_files           += $(MODS_DIR)/$(base_mods_lib_dir)/libusb-1.0.so
    internal_release_files        += $(LINUX_PPC_DRIVER_DIR)/install_ppc_module.sh
    exelwtable_files              += $(LINUX_PPC_DRIVER_DIR)/install_ppc_module.sh
    internal_release_files        += $(MODS_PPC_DRIVER_TGZ)
    CLIENTPATHS                   += //sw/tools/mods/libusb/...
    CLIENTPATHS                   += //sw/main/drivers/lwppc_linux_driver/...
endif

ifeq "$(BUILD_OS)" "cheetah"
    include_dirs += $(LINUX_DRIVER_INC_PATH)
    CLIENTPATHS  += //sw/mods/linuxdrv/...
endif

external_release_files += $(basic_release_files)
external_release_files += $(dll_release_files)
external_release_files += $(trace_release_files)
external_release_files += $(tegra_release_files)

ifeq "$(INCLUDE_MATS)" "true"
    external_release_files += $(o_dir)/mats$(exe_suffix)
    internal_release_files += $(o_dir)/mats$(exe_suffix)
    exelwtable_files       += $(o_dir)/mats$(exe_suffix)
endif

ifeq "$(INCLUDE_MLA)" "true"
    internal_release_files += $(o_dir)/mla$(exe_suffix)
    exelwtable_files       += $(o_dir)/mla$(exe_suffix)
    CLIENTPATHS += //sw/mods/logging/...
endif

ifeq "$(INCLUDE_PROTOUTIL)" "true"
    internal_release_files += $(shell echo $(o_dir) | sed 's=_tsan/=/=')/protoutil$(exe_suffix)
    exelwtable_files       += $(shell echo $(o_dir) | sed 's=_tsan/=/=')/protoutil$(exe_suffix)
endif

ifeq "$(INCLUDE_SEQ_FAIL)" "true"
    defines += -DSEQ_FAIL
    internal_release_files += tools/run_seq_fail.sh
    exelwtable_files       += tools/run_seq_fail.sh
endif

pvs_release_files += $(basic_release_files)
pvs_release_files += $(trace_release_files)
pvs_release_files += $(internal_release_files)
pvs_release_files += $(dll_release_files)
pvs_release_files += $(unencrypted_rel_files)
pvs_release_files += $(unencrypted_internal_files)
pvs_release_files += $(pvs_only_files)
pvs_release_files += $(bypass_files)
pvs_release_files += $(tegra_release_files)
pvs_release_files += $(tegra_internal_files)

ifeq "$(BUILD_ARCH)" "amd64"
    pvs_release_files += $(names_package_files)
endif

ifeq "$(LW_BULLSEYE)" "1"
    pvs_release_files += $(MODS_RUNSPACE)/rmtest.cov
endif

INCLUDE_PVS_TEGRA_SCRIPTS = 0
ifeq "$(BUILD_OS)" "cheetah"
    INCLUDE_PVS_TEGRA_SCRIPTS = 1
endif
ifeq "$(BUILD_OS)" "linuxmfg"
    ifeq "$(BUILD_ARCH)" "aarch64"
        INCLUDE_PVS_TEGRA_SCRIPTS = 1
    endif
endif

ifeq "$(INCLUDE_PVS_TEGRA_SCRIPTS)" "1"
    MODS_ANDROID_SCRIPT ?= tools/mods_android # Overriden in git
    pvs_release_files += $(MODS_ANDROID_SCRIPT)
    exelwtable_files  += $(MODS_ANDROID_SCRIPT)
    pvs_release_files += tools/android_boards
    ifeq "$(BUILD_OS)" "linuxmfg"
        pvs_release_files += tools/mods_t186_l4t_image
        pvs_release_files += tools/mods_t186_linux_image
        pvs_release_files += tools/mods_t186_embedded_linux_image
        pvs_release_files += tools/embedded_linux
        exelwtable_files  += tools/embedded_linux
        pvs_release_files += tools/runl4t
        exelwtable_files  += tools/runl4t
    else
        pvs_release_files += $(DRIVERS_DIR)/unix/rmapi_tegra/mirror/mods_t186_linux_image
    endif
endif

install_files += $(basic_release_files)
install_files += $(trace_release_files)
install_files += $(internal_release_files)
install_files += $(rmtest_release_files)
install_files += $(unencrypted_rel_files)
install_files += $(unencrypted_internal_files)
install_files += $(pvs_only_files)
install_files += $(bypass_files)
install_files += $(tegra_release_files)
install_files += $(tegra_internal_files)
install_files += $(coverage_gcno_files)

ifeq "$(INCLUDE_MME64)" "true"
    libraries += mme64sim
    lib_dirs  += $(GEN_LIB_DIR)/libmme64sim
    include_dirs += $(MME_SIM_DIR)/..
    include_dirs += $(MME_SIM_DIR)
endif

ifeq "$(INCLUDE_MUCC)" "true"
    libraries += mucc
    lib_dirs  += $(GEN_LIB_DIR)/libmucc
    internal_release_files += $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/libmucc/mucc$(exe_suffix)
    exelwtable_files       += $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/libmucc/mucc$(exe_suffix)
endif

ifeq "$(INCLUDE_FSLIB)" "true"
    defines   += -DINCLUDE_FSLIB=1
    libraries += fs
    lib_dirs  += $(GEN_LIB_DIR)/libfs
    include_dirs += $(FS_LIB_DIR)/include
endif

libraries += thermalres
lib_dirs  += $(LWMAKE_DIAG_OUT_DIR)/diag/thermalres

#-------------------------------------------------------------------------------
#
# Assemble lists from shared and target platform specific macros.
#
#-------------------------------------------------------------------------------
gen_cpp_files += $(VERSION_CPP)

ifeq "$(base_os)" "win32"
    PREBUILT_LIBS_DIR = $(base_mods_lib_dir)
else
    PREBUILT_LIBS_DIR := $(shell cd -P $(base_mods_lib_dir) && pwd)
endif

# Some builds have some libraries that are different in debug/release builds.
# For sanity builds, use the release version of the library
ifeq "$(BUILD_CFG)" "debug"
    PREBUILT_LIB_CFG := debug
else
    PREBUILT_LIB_CFG := release
endif
lib_dirs += $(PREBUILT_LIBS_DIR) $(PREBUILT_LIBS_DIR)/$(PREBUILT_LIB_CFG) $(target_lib_dirs)

ifeq "$(BUILD_OS)" "winsim"
    lib_dirs += $(mods_lib_dir)
endif

# All platforms need certain libraries
libraries += png xml encryption cryptopp lwdiag_utils boost z

#-------------------------------------------------------------------------------
# Targets...
#-------------------------------------------------------------------------------
.PHONY : all build_all change_build_cfg print_start_build_message \
         clean clean_all clean_official_build \
         clobber clobber_all FORCE remote zip zip_internal zip_names zip_external zip_pvs \
         implib compile install install_all uninstall uninstall_all \
         zip_pvs_nodep shared_build_all non_shared_build_all \
         parallel ccache noccache client check_client jenkins \
         unpack_crypto_keys pch

#-------------------------------------------------------------------------------
# Exports...
#-------------------------------------------------------------------------------
# Warning: Before exporting a make variable, consider setting it in
# makedefs.inc instead.  When DVS builds libraries such as RM, it often
# calls the RM makefile directly instead of going through the mods Makefile,
# so it doesn't see the exported varibles.
export INCLUDE_PMU INCLUDE_DPU INCLUDE_VBIOSSEC

#-------------------------------------------------------------------------------
#
# (PHONY) DEFAULT TARGET
#
#-------------------------------------------------------------------------------
all : check_client print_start_build_message
	$(Q)$(MAKE) --no-print-directory gen_h_files
	$(Q)$(MAKE) --no-print-directory $(o_dir)/$(exe_name)

#-------------------------------------------------------------------------------

-include $(DRIVERS_DIR)/common/build/mods/makerule.inc

ifeq "$(use_precompiled_headers)" "true"
    $(o_dir)/$(notdir $(pch_h_name)): $(pch_h_name)
	    $(CP) $(pch_h_name) $(o_dir)/$(notdir $(pch_h_name))

    $(o_dir)/$(pch_name): $(o_dir)/$(notdir $(pch_h_name))
	    $(call COMPILE_PCH,$<)

    $(o_files): $(o_dir)/$(pch_name)
endif

submake.mme64sim.%:
	$(Q)$(MAKE) -C $(MME_SIM_DIR) -f Makefile.mods $*

submake.mucc.%:
	$(Q)$(MAKE) -C ../mucc $*

submake.vgpu_plugin.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/vgpu -f Makefile.mods $*

submake.lwlink.%:
	@$(ECHO) BUILD_OS_SUBTYPE=$(BUILD_OS_SUBTYPE)
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/lwlink -f Makefile.mods $*

submake.lwswitch.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/lwswitch -f Makefile.mods $*

submake.lwwatch.%:
ifdef CYGWIN_MAKE
	$(Q)$(MAKE) -C $(LWWATCH_PATH) -f Makefile.mods MODS_DIR=$(shell $(MODS_DIR)/tools/abs2rel.sh $(shell pwd) $(LWWATCH_PATH)) _lwwatch_config_all $*
else
	$(Q)$(MAKE) -C $(LWWATCH_PATH) -f Makefile.mods MODS_DIR=$(abspath .) _lwwatch_config_all $*
endif


submake.flcndbg.%:
ifdef CYGWIN_MAKE
	$(Q)$(MAKE) -C $(LWFLCNDBG_PATH) -f Makefile.mods MODS_DIR=$(shell $(MODS_DIR)/tools/abs2rel.sh $(shell pwd) $(LWFLCNDBG_PATH)) _lwflcndbg_config_all $*
else
	$(Q)$(MAKE) -C $(LWFLCNDBG_PATH) -f Makefile.mods MODS_DIR=$(abspath .) _lwflcndbg_config_all $*
endif

ENCRYPT_INSTALL_DIR = $(o_dir)

AES_FILE_KEY_CPP := $(gen_cpp_dir)/aes_file_key.cpp
AES_LOG_KEY_CPP  := $(gen_cpp_dir)/aes_log_key.cpp

cpp_files     += $(AES_FILE_KEY_CPP)
cpp_files     += $(AES_LOG_KEY_CPP)
gen_cpp_files += $(AES_FILE_KEY_CPP)
gen_cpp_files += $(AES_LOG_KEY_CPP)

# Figures out the location of the generated key file, generated by encrypt target
GEN_KEY_FILE = $(addsuffix .gen, $(addprefix $(MODS_OUTPUT_DIR)/, $(notdir $1)))

# Creates rule that copies key file to o_dir
# Usage: COPY_KEY <key_target_file>
define COPY_KEY
$1: submake.encrypt.all
	$(Q)$(CP_IF_CHANGED) $$(call GEN_KEY_FILE, $$@) $$@
endef

$(eval $(call COPY_KEY,$(AES_FILE_KEY_CPP)))
$(eval $(call COPY_KEY,$(AES_LOG_KEY_CPP)))

DECRYPT_TOOL = $(o_dir)/decrypt$(exe_suffix)

$(DECRYPT_TOOL): submake.decrypt.all
	$(Q)$(CP_IF_CHANGED) $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/decrypt/decrypt$(exe_suffix) $@

$(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir_host)/decrypt/bypass.decrypt.INTERNAL.bin: submake.decrypt.all

exelwtable_files += $(DECRYPT_TOOL)

INCLUDE_ENCRYPT_INTERNAL ?= false

ifeq "$(ENCRYPT_JS)" "true"
    ifeq "$(SANITIZER)" ""
        BUILD_ENCRYPT_INTERNAL ?= true
    endif
endif

BUILD_ENCRYPT_INTERNAL ?= false

ifeq "$(BUILD_ENCRYPT_INTERNAL)" "true"
    ENCRYPT_INTERNAL_TOOL = $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir_host)/encrypt.INTERNAL/encrypt.INTERNAL$(exe_suffix)

    install_files    += $(ENCRYPT_INTERNAL_TOOL)
    exelwtable_files += $(ENCRYPT_INTERNAL_TOOL)
    install: submake.encrypt_internal.all
    $(ENCRYPT_INTERNAL_TOOL): submake.encrypt_internal.all
    zip: $(ENCRYPT_INTERNAL_TOOL)

    ifeq "$(INCLUDE_ENCRYPT_INTERNAL)" "true"
    zip: encrypt.INTERNAL$(exe_suffix)
    encrypt.INTERNAL$(exe_suffix): submake.encrypt_internal.all
	$(Q)$(CP_IF_CHANGED) $(ENCRYPT_INTERNAL_TOOL) $@
    endif
endif

ifeq "$(BUILD_OS)" "$(HOST_OS)"
    ENCRYPT_BUILD_OS_SUBTYPE = "$(BUILD_OS_SUBTYPE)"
else
    ENCRYPT_BUILD_OS_SUBTYPE =
endif

submake.encrypt.%:
	$(Q)$(MAKE) -C ../encrypt BUILD_OS=$(HOST_OS) BUILD_OS_SUBTYPE=$(ENCRYPT_BUILD_OS_SUBTYPE) BUILD_ARCH=$(HOST_ARCH) INSTALL_OUT_DIR=$(ENCRYPT_INSTALL_DIR) INCLUDE_OGL=$(INCLUDE_OGL) VERSION=$(VERSION) BUILD_DECRYPT=false INCLUDE_VULKAN=$(INCLUDE_VULKAN) INCLUDE_LWDA=$(INCLUDE_LWDA) $*

submake.encrypt_internal.%:
	$(Q)$(MAKE) -C ../encrypt BUILD_OS=$(HOST_OS) BUILD_OS_SUBTYPE=$(ENCRYPT_BUILD_OS_SUBTYPE) BUILD_ARCH=$(HOST_ARCH) INSTALL_OUT_DIR=$(MODS_DIR) INCLUDE_OGL=$(INCLUDE_OGL) VERSION=$(VERSION) BUILD_DECRYPT=false ENFORCE_LW_NETWORK=true INCLUDE_VULKAN=$(INCLUDE_VULKAN) INCLUDE_LWDA=$(INCLUDE_LWDA) $*

submake.decrypt.%:
	$(Q)$(MAKE) -C ../encrypt BUILD_OS=$(HOST_OS) BUILD_OS_SUBTYPE=$(ENCRYPT_BUILD_OS_SUBTYPE) BUILD_ARCH=$(HOST_ARCH) INSTALL_OUT_DIR=$(ENCRYPT_INSTALL_DIR) INCLUDE_OGL=$(INCLUDE_OGL) VERSION=$(VERSION) BUILD_DECRYPT=true $*

submake.decryptall.%:
	$(Q)$(MAKE) -C ../encrypt BUILD_OS=$(HOST_OS) BUILD_OS_SUBTYPE=$(ENCRYPT_BUILD_OS_SUBTYPE) BUILD_ARCH=$(HOST_ARCH) INSTALL_OUT_DIR=$(ENCRYPT_INSTALL_DIR) INCLUDE_OGL=$(INCLUDE_OGL) VERSION=$(VERSION) BUILD_DECRYPT=true DECRYPT_LOG_ONLY=false $*

submake.decrypt.all: submake.encrypt.all

submake.decryptall.all: submake.encrypt.all

submake.encrypt_internal.all : submake.encrypt.all

submake.encrypt.all: submake.lwdiag_utils.build

ifeq "$(PKG_PREFIX)" ""
    PKG_NAME="$(VERSION)"
else
    PKG_NAME="$(PKG_PREFIX)$(VERSION)"
endif

jenkins_files := $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/decryptall/decryptall$(exe_suffix)
jenkins_files += $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/decryptall/bypass.decrypt.INTERNAL.bin
jenkins_files += $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/encrypt/$(encrypt_exe)

jenkins_pkg = $(PKG_OUT_DIR)/$(PKG_NAME).jenkins$(zip_suffix)
crypto_keys_pkg = $(PKG_OUT_DIR)/$(PKG_NAME).crypto_keys.zip

jenkins: $(jenkins_pkg)

ifneq "$(zip_suffix)" ".zip"
jenkins: $(crypto_keys_pkg)
endif

unpack_crypto_keys:
	$(Q)$(MKDIR_P) -p $(MODS_OUTPUT_DIR)
	$(Q)$(UNZIP) $(UNZIPFLAGS) $(crypto_keys_pkg) -d $(MODS_OUTPUT_DIR)
	$(Q)$(TOUCH) $(call GEN_KEY_FILE, $(AES_FILE_KEY_CPP) $(AES_LOG_KEY_CPP))

$(jenkins_pkg): submake.decryptall.all
	@$(ECHO) "Packaging $@"
ifeq "$(zip_suffix)" ".zip"
	$(Q)$(ZIP) $(ZIPFLAGS) $@ $(jenkins_files)
else
	$(Q)-$(RM_RF) $(TGZ_TEMP_DIR)_jenkins
	$(Q)mkdir $(TGZ_TEMP_DIR)_jenkins
	$(Q)cp -p $(jenkins_files) $(TGZ_TEMP_DIR)_jenkins/
	$(Q)$(ZIP) $(ZIPFLAGS) $@ -C $(TGZ_TEMP_DIR)_jenkins .
	$(Q)$(RM_RF) $(TGZ_TEMP_DIR)_jenkins
endif

$(crypto_keys_pkg): submake.decryptall.all
	$(Q)$(ECHO) "Packaging $(crypto_keys_pkg)"
	$(Q)zip -j -v $(crypto_keys_pkg) $(call GEN_KEY_FILE, $(AES_FILE_KEY_CPP) $(AES_LOG_KEY_CPP))

# The crypto_tools package contains encrypt and decrypt tools built for a different OS
crypto_tools_pkg = $(PKG_OUT_DIR)/$(PKG_NAME).crypto_tools$(zip_suffix)
crypto_tools: $(crypto_tools_pkg)

crypto_tools_files =  $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/decrypt/decrypt$(exe_suffix)
crypto_tools_files += $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/decrypt/bypass.decrypt.INTERNAL.bin
crypto_tools_files += $(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/encrypt/$(encrypt_exe)

$(crypto_tools_pkg): submake.decrypt.all
	@$(ECHO) "Packaging $@"
ifeq "$(zip_suffix)" ".zip"
	$(Q)$(ZIP) $(ZIPFLAGS) $@ $(crypto_tools_files)
else
	$(Q)-rm -rf $(TGZ_TEMP_DIR)_crypto_tools
	$(Q)mkdir $(TGZ_TEMP_DIR)_crypto_tools
	$(Q)cp -p $(crypto_tools_files) $(TGZ_TEMP_DIR)_crypto_tools/
	$(Q)$(ZIP) $(ZIPFLAGS) $@ -C $(TGZ_TEMP_DIR)_crypto_tools .
	$(Q)rm -rf $(TGZ_TEMP_DIR)_crypto_tools
endif

gen_h_files += $(gen_cpp_dir)/mle_protobuf_writer.h $(gen_cpp_dir)/mle.h

$(gen_cpp_dir)/mle_protobuf_writer.h $(gen_cpp_dir)/mle.h : $(MODS_DIR)/../mle/mle.proto $(MODS_DIR)/../utils/protobuf/protobuf.py
	@$(ECHO) Generating MLE protobuf interface
	@$(PYTHON3) $(MODS_DIR)/../utils/protobuf/protobuf.py                \
            --writer_filename=$(gen_cpp_dir)/mle_protobuf_writer.h           \
            --writer_namespace=Mle                                           \
            --header_filename=$(gen_cpp_dir)/mle.h                           \
            $(MODS_DIR)/../mle/mle.proto
$(o_files): $(gen_cpp_dir)/mle_protobuf_writer.h $(gen_cpp_dir)/mle.h

submake.mats.%:
	$(Q)$(MAKE) -C $(MATS_DIR) LINUX_DRIVER_SRC_PATH=$(LINUX_DRIVER_INC_PATH) $*

submake.mats.build: submake.lwdiag_utils.build $(reghal_tables_h)

submake.mats.build_install: submake.lwdiag_utils.build $(reghal_tables_h)

submake.mla.%:
	$(Q)$(MAKE) -C $(MLA_DIR) MLA_DIR=$(MLA_DIR) MLA_RUNSPACE=$(MODS_OUTPUT_DIR)/$(rel_o_dir)/mods BRANCH_BASE=$(abspath $(BRANCH_BASE)) $*

submake.protoutilhost.%:
	$(Q)$(MAKE) -C $(PROTOUTIL_DIR) BUILD_OS=$(HOST_OS) BUILD_OS_SUBTYPE= BUILD_ARCH=$(HOST_ARCH) $* BUILD_HOST=true

submake.protoutil.%:
	$(Q)$(MAKE) -C $(PROTOUTIL_DIR) $*

submake.amap.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/common/amaplib/contrib_sw/mods STANDALONE_MATS="false" $*

submake.dramhal.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/common/dram_hal/contrib_sw/mods STANDALONE_MATS="false" $*

submake.drvexport.%:
	$(Q)$(MAKE) -C core/drvexport use_mods_lwlink=$(use_mods_lwlink) use_mods_lwswitch=$(use_mods_lwswitch) $*

submake.mdiag.%:
	$(Q)$(MAKE) -C mdiag $*

submake.mdiag.build: $(reghal_tables_h)

submake.mdiag.build_install: $(reghal_tables_h)

submake.mdiagutl.%:
	$(Q)$(MAKE) -C mdiag/utl $*

submake.resman.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/resman/arch/lwalloc/mods INCLUDE_LWSWITCH=$(INCLUDE_LWSWITCH) $*

submake.lwelf.%:
	$(Q)$(MAKE) -C $(LW_ELF_PATH) $*

submake.lwdwarf.%:
	$(Q)$(MAKE) -C $(LW_DWARF_PATH) $*

submake.flcndwarf.%:
	$(Q)$(MAKE) -C $(FLCNDWARF_PATH) $*

submake.lwlwvid.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/multimedia/codecs/lwca/lwlwvid/mods $*

submake.lwca.%:
	$(Q)$(MAKE) -C $(LWDA_DIR)/mods $(LWDA_MAKE_FLAGS) $*

submake.ogl.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/OpenGL/mods $*

submake.fs.%:
	$(Q)$(MAKE) -C $(FS_LIB_DIR)/contrib_sw/mods $*

submake.glslang.%:
	$(Q)$(MAKE) -C ../glslang GLSLANG_PARENT_DIR=$(GLSLANG_PARENT_DIR) $*

# Macro which creates rules for components built with lwmake
#
# Usage:
#     $(eval $(call LWMAKE_COMPONENT,name,rootdir,compdir))
#
# Parameters:
#   - name     - component name used for constructing submake rules, e.g. submake.name.build_install
#   - rootdir  - location of LW_SOURCE, but it's different for some MODS components in git
#   - compdir  - component directory relative to LW_SOURCE
#
# Notes:
#   - submake.name.clean will remove directory, lwmake's clean works differently
define LWMAKE_COMPONENT
submake.$1.build:
	$$(Q)$$(MAKE) -C $2/$3 -f makefile.lwmk build $$(LWMAKE_ARGS)
submake.$1.clean:
	$$(Q)-$$(RM_RF) $$(GEN_LIB_DIR)/lwmake/$3 $$(GEN_LIB_DIR)/lwmake/drv/$3
submake.$1.install:
	$$(Q)$$(MAKE) -C $2/$3 -f makefile.lwmk install_shared $$(LWMAKE_ARGS)
submake.$1.build_install: submake.$1.build
	$$(Q)$$(MAKE) -C $2/$3 -f makefile.lwmk install_shared $$(LWMAKE_ARGS)
endef

# Macro which connects dependencies for components built with lwmake
#
# Usage:
#     $(eval $(call LWMAKE_COMPONENT_DEPS,name,builddep))
#
# Parameters:
#   - name     - component name used for constructing submake rules, e.g. submake.name.build_install
#   - builddep - target which depends on the submake.name.build_install rule
define LWMAKE_COMPONENT_DEPS
    submake.$1.clobber: submake.$1.clean
    $2: submake.$1.build_install
    clean_all: submake.$1.clean
    clobber_all: submake.$1.clobber
    install_all: submake.$1.install
endef

$(eval $(call LWMAKE_COMPONENT,iperf,$(MODS_DIR),cheetah/utility/iperf))
$(eval $(call LWMAKE_COMPONENT,js17,$(MODS_DIR)/../..,diag/js1.7))
$(eval $(call LWMAKE_COMPONENT,jsdb17,$(MODS_DIR)/../..,diag/js1.7/jsdb))
$(eval $(call LWMAKE_COMPONENT,mle,$(MODS_DIR)/../..,diag/mle))
$(eval $(call LWMAKE_COMPONENT,thermalres,$(MODS_DIR)/../..,diag/thermalres))
$(eval $(call LWMAKE_COMPONENT,lwdiag_utils,$(MODS_DIR)/../..,diag/utils))
$(eval $(call LWMAKE_COMPONENT,fabricmanager,$(DRIVERS_DIR)/..,apps/fabricmanager/mods))
$(eval $(call LWMAKE_COMPONENT,fatbinloader,$(DRIVERS_DIR)/..,drivers/gpgpu/lwca/mods/fatbinaryLoader))
$(eval $(call LWMAKE_COMPONENT,rtcore,$(DRIVERS_DIR)/..,drivers/rtcore))
$(eval $(call LWMAKE_COMPONENT,multinode_transport,$(MODS_DIR)/../..,diag/multinode/transport))
$(eval $(call LWMAKE_COMPONENT,spirv,$(DRIVERS_DIR)/..,drivers/compiler/spirv/build/mods))
$(eval $(call LWMAKE_COMPONENT_DEPS,thermalres,non_shared_build_all))

# Since libspdm only needed for some RMTests, only allow it in those builds.
ifeq "$(INCLUDE_LIBSPDM)" "true"
    $(eval $(call LWMAKE_COMPONENT,spdm,$(MODS_DIR)/../..,diag/spdm))
endif

# Macro which connects dependencies for core libraries, mostly for use in DVS
#
# Usage:
#     $(eval $(call CORE_LIB_DEP,name,path))
#
# Parameters:
#   - name     - component name used for constructing submake rules, e.g. submake.name.build_install
#   - path     - path relative to rel_o_dir where the component is compiled
define CORE_LIB_DEP
    submake.core_libs.build: submake.$1.build
    submake.core_libs.clean: submake.$1.clean
    core_libs += $2$(lib_suffix)
endef
ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        $(eval $(call CORE_LIB_DEP,iperf,lwmake/diag/iperf/libiperf))
    endif
endif

$(eval $(call CORE_LIB_DEP,js17,lwmake/diag/js1.7/libjs17))
$(eval $(call CORE_LIB_DEP,jsdb17,lwmake/diag/js1.7/jsdb/libjsdb17))
$(eval $(call CORE_LIB_DEP,lwdiag_utils,lwmake/diag/utils/liblwdiag_utils))
ifeq "$(INCLUDE_LIBSPDM)" "true"
    $(eval $(call CORE_LIB_DEP,spdm,lwmake/diag/spdm/libspdm))
endif
$(eval $(call CORE_LIB_DEP,thermalres,lwmake/diag/thermalres/libthermalres))

submake.core_libs.build_install: submake.core_libs.build

submake.core_libs.zip:
	$(Q)cd $(MODS_OUTPUT_DIR)/$(rel_o_dir) && $(ZIP) $(ZIPFLAGS) core_libs$(zip_suffix) $(core_libs)

vklayers.build_install: submake.vkkhronosvalidation.build_install
vklayers.clean: submake.vkkhronosvalidation.clean

VK_LAYERS_USE_LWMAKE ?= false

ifeq "$(VK_LAYERS_USE_LWMAKE)" "true"
submake.vkkhronosvalidation.build:
	$(Q)$(MAKE) -C $(VK_LAYERS_DIR)/khronos_validation -f makefile.lwmk build $(LWMAKE_ARGS) this_component=khronos_validation
submake.vkkhronosvalidation.clean:
	$(Q)-$(RM_RF) $(GEN_LIB_DIR)/lwmake/khronos_validation $(GEN_LIB_DIR)/lwmake/drv/khronos_validation
submake.vkkhronosvalidation.install:
	$(Q)$(MAKE) -C $(VK_LAYERS_DIR)/khronos_validation -f makefile.lwmk install_shared $(LWMAKE_ARGS) this_component=khronos_validation
submake.vkkhronosvalidation.build_install: submake.vkkhronosvalidation.build
	$(Q)$(MAKE) -C $(VK_LAYERS_DIR)/khronos_validation -f makefile.lwmk install_shared $(LWMAKE_ARGS) this_component=khronos_validation
else
##############################################################################
# Support for old version using legacy Makefile
vklayers.clobber: submake.vkkhronosvalidation.clobber
vklayers.uninstall: submake.vkkhronosvalidation.uninstall

ifdef CYGWIN_MAKE
    MODS_DIR_FOR_VK_LAYERS = $(shell $(MODS_DIR)/tools/abs2rel.sh $(shell pwd) $(VK_LAYERS_DIR)/core_validation)
else
    MODS_DIR_FOR_VK_LAYERS = $(abspath .)
endif

submake.vkkhronosvalidation.%:
	$(Q)$(MAKE) -C $(VK_LAYERS_DIR)/khronos_validation MODS_DIR=$(MODS_DIR_FOR_VK_LAYERS) MODS_RUNSPACE=$(MODS_RUNSPACE) BUILD_RELEASE_DLLS=true $*
##############################################################################
endif

submake.lwogtest.%:
ifdef CYGWIN_MAKE
	$(Q)$(MAKE) -C $(COMMON_WS_DIR)/sw/apps/gpu/drivers/opengl/lwogtest/mods MODS_DIR=$(shell $(MODS_DIR)/tools/abs2rel.sh $(shell pwd) $(COMMON_WS_DIR)/sw/apps/gpu/drivers/opengl/lwogtest/mods) $*
else
	$(Q)$(MAKE) -C $(COMMON_WS_DIR)/sw/apps/gpu/drivers/opengl/lwogtest/mods MODS_DIR=$(abspath .) $*
endif

submake.displayport.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/displayport/ $*

submake.lwapi.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/lwapi/sdk/ $*  PROFILE=lwsdk LW_TARGET_ARCH=$(BUILD_ARCH) LW_TOOLS=$(BUILD_TOOLS_DIR)

submake.mackext.%:
	$(Q)$(MAKE) -C $(MODS_DIR)/macosx/kext DSTROOT=$(MODS_RUNSPACE) OBJROOT=$(o_dir) $*

RMAPI_TEGRA_OS =

submake.rmapi_tegra.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/unix/rmapi_tegra/mods LW_TARGET_OS=$(RMAPI_TEGRA_OS) $*

submake.rmapi_tegra_mirror.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/unix/rmapi_tegra/mods/mirror LW_TARGET_OS=$(RMAPI_TEGRA_OS) $*

ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        INCLUDE_POKEGPU = true
    endif
endif

ifeq "$(BUILD_OS)" "cheetah"
    submake.lwca.build_install: submake.rmapi_tegra.build_install
    submake.ogl.build_install: submake.rmapi_tegra.build_install
endif

ifeq "$(INCLUDE_POKEGPU)" "true"
    submake.pokegpu.install submake.pokegpu.build: submake.rmapi_tegra.build_install

    submake.pokegpu.%:
	$(Q)$(MAKE) -C $(MODS_DIR)/android/pokegpu $*

    exelwtable_files       += $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/pokegpu/pokegpu
    internal_release_files += $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/pokegpu/pokegpu
endif

$(MODS_RUNSPACE)/dummy.txt:
	@$(ECHO) MODS_RUNSPACE=$(MODS_RUNSPACE)
	$(Q)$(MKDIR_P) $(MODS_RUNSPACE) && $(TOUCH) $(MODS_RUNSPACE)/dummy.txt

# Full build, etc. of all modules

# Note this must be first in the dependancy list so that the MODS
# runspace is created before exelwting any submakes
build_all:$(MODS_RUNSPACE)/dummy.txt
build_all:check_client

ifeq "$(INCLUDE_BYTECODE)" "true"
    $(o_dir)/$(exe_name) : $(GEN_LIB_DIR)/libejs17/libejs17$(lib_suffix)
else
    $(eval $(call LWMAKE_COMPONENT_DEPS,js17,non_shared_build_all))
endif
ifeq "$(BUILD_OS)" "cheetah"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        $(eval $(call LWMAKE_COMPONENT_DEPS,iperf,non_shared_build_all))
    endif
endif
$(eval $(call LWMAKE_COMPONENT_DEPS,jsdb17,non_shared_build_all))
$(eval $(call LWMAKE_COMPONENT_DEPS,lwdiag_utils,non_shared_build_all))

ifeq "$(INCLUDE_LIBSPDM)" "true"
    $(eval $(call LWMAKE_COMPONENT_DEPS,spdm,non_shared_build_all))
endif

ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    non_shared_build_all:submake.vgpu_plugin.build_install
endif
ifeq "$(use_mods_lwlink)" "true"
    non_shared_build_all:submake.lwlink.build_install
endif
ifeq "$(use_mods_lwswitch)" "true"
    non_shared_build_all:submake.lwswitch.build_install
endif
ifeq "$(INCLUDE_FM)" "true"
    $(eval $(call LWMAKE_COMPONENT_DEPS,fabricmanager,non_shared_build_all))
endif
ifeq "$(INCLUDE_TRANSPORT)" "true"
    $(eval $(call LWMAKE_COMPONENT_DEPS,multinode_transport,non_shared_build_all))
endif
ifeq "$(INCLUDE_LWWATCH)" "true"
    non_shared_build_all:submake.lwwatch.build_install
endif
ifeq "$(INCLUDE_FLCNDBG)" "true"
    non_shared_build_all: submake.lwelf.build_install
    non_shared_build_all: submake.lwdwarf.build_install
    non_shared_build_all: submake.flcndwarf.build_install
    non_shared_build_all: submake.flcndbg.build_install
endif
ifeq "$(INCLUDE_GPU)" "true"
   non_shared_build_all:submake.amap.build_install
   non_shared_build_all:submake.dramhal.build_install
endif
ifeq "$(INCLUDE_FSLIB)" "true"
   non_shared_build_all:submake.fs.build_install
endif
ifeq "$(INCLUDE_LWLWVID)" "true"
   non_shared_build_all:submake.lwlwvid.build_install
endif
ifeq "$(INCLUDE_DRVEXPORT)" "true"
    non_shared_build_all:submake.drvexport.build_install
endif
ifeq "$(INCLUDE_MDIAG)" "true"
    non_shared_build_all: submake.mdiag.build_install
endif
ifeq "$(INCLUDE_MDIAGUTL)" "true"
    non_shared_build_all: submake.mdiagutl.build_install
endif
ifeq "$(use_mods_rm)" "true"
    ifeq "$(USE_SHARED_LIB_STUBS)" "true"
       $(o_dir)/$(exe_name): $(LIB_STUBS_DIR)/librm$(dll_suffix)
       shared_build_all: submake.resman.build_install
    else
       non_shared_build_all: submake.resman.build_install
    endif
endif
ifeq "$(INCLUDE_DP)" "true"
    non_shared_build_all: submake.displayport.build_install
endif
ifeq "$(INCLUDE_LWDA)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        non_shared_build_all: submake.lwca.build_install
    endif
endif
ifeq "$(INCLUDE_FATBINLD)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        $(eval $(call LWMAKE_COMPONENT_DEPS,fatbinloader,non_shared_build_all))
    endif
endif
ifeq "$(INCLUDE_OGL)" "true"
    ifneq "$(USE_SYSTEM_LIBS)" "true"
        ifeq "$(USE_SHARED_LIB_STUBS)" "true"
            $(o_dir)/$(exe_name): $(LIB_STUBS_DIR)/libmodsgl$(dll_suffix)
            shared_build_all: submake.ogl.build_install
        else
            non_shared_build_all: submake.ogl.build_install
        endif
    endif
endif
ifeq "$(INCLUDE_VULKAN)" "true"
    non_shared_build_all: submake.glslang.build
    $(eval $(call LWMAKE_COMPONENT_DEPS,spirv,shared_build_all))
    $(eval $(call LWMAKE_COMPONENT_DEPS,rtcore,shared_build_all))
endif
ifeq "$(INCLUDE_VKLAYERS)" "true"
    ifeq "$(VK_LAYERS_USE_LWMAKE)" "true"
        $(eval $(call LWMAKE_COMPONENT_DEPS,vkkhronosvalidation,shared_build_all))
    else
        shared_build_all: submake.vkkhronosvalidation.build_install
    endif
endif
ifeq "$(INCLUDE_LWOGTEST)" "true"
    non_shared_build_all: submake.lwogtest.build_install
    exelwtable_files += output/liblwogtest$(lib_suffix)
endif
ifeq "$(BUILD_OS)" "win32"
    non_shared_build_all: submake.lwapi.build_all
endif
ifeq "$(INCLUDE_MATS)" "true"
    non_shared_build_all: submake.mats.build_install
endif
ifeq "$(INCLUDE_MLA)" "true"
    non_shared_build_all: submake.mla.build_install
endif
ifeq "$(INCLUDE_PROTOUTIL)" "true"
    non_shared_build_all: submake.protoutil.build_install
endif
ifeq "$(BUILD_OS)" "macosxmfg"
    ifeq "$(INCLUDE_GPU)" "true"
        non_shared_build_all: submake.mackext.build_install
    endif
endif
ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    non_shared_build_all: submake.rmapi_tegra.build_install
endif
ifeq "$(INCLUDE_RMAPI_TEGRA_MIRROR)" "true"
    non_shared_build_all: submake.rmapi_tegra_mirror.build
endif
ifeq "$(INCLUDE_POKEGPU)" "true"
    non_shared_build_all: submake.pokegpu.install
endif
ifeq "$(INCLUDE_MME64)" "true"
    non_shared_build_all: submake.mme64sim.build_install
endif
ifeq "$(INCLUDE_MUCC)" "true"
    non_shared_build_all: submake.mucc.build_install
endif

submake.mucc.build_install: submake.lwdiag_utils.build

# Only shared libraries with a C interface can be generated by the stub generator
$(LIB_STUBS_DIR)/dummy.txt:
	$(Q)$(MKDIR_P) $(LIB_STUBS_DIR); $(TOUCH) $(LIB_STUBS_DIR)/dummy.txt
$(LIB_STUBS_DIR)/libmodsgl.cpp: $(DRIVERS_DIR)/OpenGL/mods/libmodsgl.def $(LIB_STUBS_DIR)/dummy.txt
	$(PYTHON3) tools/mods_exports.py $< $@
$(LIB_STUBS_DIR)/librm.cpp: $(DRIVERS_DIR)/resman/arch/lwalloc/mods/librm.def $(LIB_STUBS_DIR)/dummy.txt
	$(PYTHON3) tools/mods_exports.py $< $@ Lw04MapMemoryInternal Lw04UnmapMemoryInternal Lw04ControlInternal

$(LIB_STUBS_DIR)/%$(dll_suffix): $(LIB_STUBS_DIR)/%.cpp
	$(Q)$(ECHO) "Building library stub : $@"
	$(Q)$(CXX) $(CPPFLAGS) $< -o $@ -shared

# Temporary until build systems can be adjusted
shared_lib_stubs:

# create a MODS target that only depends on the non shared libraries (or shared
# libraries that are incapable of being stubbed) and and the stubs of shared
# libraries for which creating a stub is possible
shared_mods_build: non_shared_build_all
	$(Q)$(MAKE)

# shared_build_all will have had any shared libraries that can be linked against
# using a stub added as dependencies earlier.  Add shared_mods_build as a
# dependency here.  This will allow all the stubbable shared libraries to be
# built in parallel with mods (which will link against the stubs instead of the
# actual shared libraries)
shared_build_all: shared_mods_build

build_all: shared_build_all
	$(Q)$(MAKE) install

ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    clean_all:submake.vgpu_plugin.clean
endif
ifeq "$(INCLUDE_LWWATCH)" "true"
    clean_all:submake.lwwatch.clean
endif
ifeq "$(use_mods_lwlink)" "true"
    clean_all:submake.lwlink.clean
endif
ifeq "$(use_mods_lwswitch)" "true"
    clean_all:submake.lwswitch.clean
endif
ifeq "$(INCLUDE_GPU)" "true"
    clean_all:submake.amap.clean
    clean_all:submake.dramhal.clean
endif
ifeq "$(INCLUDE_FSLIB)" "true"
    clean_all:submake.fs.clean
endif
ifeq "$(INCLUDE_LWLWVID)" "true"
    clean_all:submake.lwlwvid.clean
endif
ifeq "$(INCLUDE_DRVEXPORT)" "true"
    clean_all:submake.drvexport.clean
endif
ifeq "$(INCLUDE_MDIAG)" "true"
    clean_all: submake.mdiag.clean
endif
ifeq "$(INCLUDE_MDIAGUTL)" "true"
    clean_all: submake.mdiagutl.clean
endif
ifeq "$(use_mods_rm)" "true"
    clean_all: submake.resman.clean
endif
ifeq "$(INCLUDE_FLCNDBG)" "true"
    clean_all: submake.lwelf.clean
    clean_all: submake.lwdwarf.clean
    clean_all: submake.flcndwarf.clean
    clean_all: submake.flcndbg.clean
endif
ifeq "$(INCLUDE_DP)" "true"
    clean_all: submake.displayport.clean
endif
ifeq "$(INCLUDE_LWDA)" "true"
    clean_all: submake.lwca.clean
endif
ifeq "$(INCLUDE_OGL)" "true"
    clean_all: submake.ogl.clean
endif
ifeq "$(INCLUDE_VULKAN)" "true"
    clean_all: submake.glslang.clean
endif
ifeq "$(INCLUDE_VKLAYERS)" "true"
    ifneq "$(VK_LAYERS_USE_LWMAKE)" "true"
        clean_all: submake.vkkhronosvalidation.clean
    endif
endif
ifeq "$(INCLUDE_LWOGTEST)" "true"
    clean_all: submake.lwogtest.clean
endif
ifeq "$(BUILD_OS)" "win32"
    clean_all: submake.lwapi.clean
endif
ifeq "$(INCLUDE_MATS)" "true"
    clean_all: submake.mats.clean
endif
ifeq "$(INCLUDE_MLA)" "true"
    clean_all: submake.mla.clean
endif
ifeq "$(INCLUDE_PROTOUTIL)" "true"
    clean_all: submake.protoutil.clean_all
    clean_all: submake.protoutilhost.clean_all
endif
ifeq "$(ENCRYPT_JS)" "true"
    clean_all: submake.encrypt.clean
    clean_all: submake.encrypt_internal.clean
    clean_all: submake.decrypt.clean
    clean_all: submake.decryptall.clean
else
    ifneq "$(BOUND_JS)" "false"
        clean_all: submake.encrypt.clean
        clean_all: submake.encrypt_internal.clean
        clean_all: submake.decrypt.clean
        clean_all: submake.decryptall.clean
    endif
endif
ifeq "$(BUILD_OS)" "macosxmfg"
    ifeq "$(INCLUDE_GPU)" "true"
        clean_all: submake.mackext.clean
    endif
endif
ifeq "$(INCLUDE_MME64)" "true"
    clean_all: submake.mme64sim.clean
endif
ifeq "$(INCLUDE_MUCC)" "true"
    clean_all: submake.mucc.clean
endif
clean_all: clean
ifeq "$(TRIGGER_RMCFG)" "true"
    clean_all: clean_rmconfig
    clobber_all: clean_rmconfig
endif
ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    clean_all: submake.rmapi_tegra.clean
endif
ifeq "$(INCLUDE_RMAPI_TEGRA_MIRROR)" "true"
    clean_all: submake.rmapi_tegra_mirror.clean
endif
ifeq "$(INCLUDE_POKEGPU)" "true"
    clean_all: submake.pokegpu.clean
endif

ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    clobber_all:submake.vgpu_plugin.clobber
endif
ifeq "$(INCLUDE_LWWATCH)" "true"
    clobber_all:submake.lwwatch.clobber
endif
ifeq "$(use_mods_lwlink)" "true"
    clobber_all:submake.lwlink.clobber
endif
ifeq "$(use_mods_lwswitch)" "true"
    clobber_all:submake.lwswitch.clobber
endif
ifeq "$(INCLUDE_GPU)" "true"
    clobber_all:submake.amap.clobber
    clobber_all:submake.dramhal.clobber
endif
ifeq "$(INCLUDE_FSLIB)" "true"
    clobber_all:submake.fs.clobber
endif
ifeq "$(INCLUDE_LWLWVID)" "true"
    clobber_all:submake.lwlwvid.clobber
endif
ifeq "$(INCLUDE_DRVEXPORT)" "true"
    clobber_all:submake.drvexport.clobber
endif
ifeq "$(INCLUDE_MDIAG)" "true"
    clobber_all: submake.mdiag.clobber
endif
ifeq "$(INCLUDE_MDIAGUTL)" "true"
    clobber_all: submake.mdiagutl.clobber
endif
ifeq "$(use_mods_rm)" "true"
    clobber_all: submake.resman.clobber
endif
ifeq "$(INCLUDE_FLCNDBG)" "true"
    clobber_all: submake.lwelf.clobber
    clobber_all: submake.lwdwarf.clobber
    clobber_all: submake.flcndwarf.clobber
    clobber_all: submake.flcndbg.clobber
endif
ifeq "$(INCLUDE_DP)" "true"
    clobber_all: submake.displayport.clobber
endif
ifeq "$(INCLUDE_LWDA)" "true"
    clobber_all: submake.lwca.clobber
endif
ifeq "$(INCLUDE_OGL)" "true"
    clobber_all: submake.ogl.clobber
endif
ifeq "$(INCLUDE_VULKAN)" "true"
    clobber_all: submake.glslang.clean
endif
ifeq "$(INCLUDE_VKLAYERS)" "true"
    ifneq "$(VK_LAYERS_USE_LWMAKE)" "true"
        clobber_all: submake.vkkhronosvalidation.clobber
    endif
endif
ifeq "$(INCLUDE_LWOGTEST)" "true"
    clobber_all: submake.lwogtest.clobber
endif
ifeq "$(BUILD_OS)" "win32"
    clobber_all: submake.lwapi.clobber
endif
ifeq "$(ENCRYPT_JS)" "true"
    clobber_all: submake.encrypt.clobber
    clobber_all: submake.encrypt_internal.clobber
    clobber_all: submake.decrypt.clobber
    clobber_all: submake.decryptall.clobber
else
    ifneq "$(BOUND_JS)" "false"
        clobber_all: submake.encrypt.clobber
        clobber_all: submake.encrypt_internal.clobber
        clobber_all: submake.decrypt.clobber
        clobber_all: submake.decryptall.clobber
    endif
endif
ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    clobber_all: submake.rmapi_tegra.clobber
endif
ifeq "$(INCLUDE_RMAPI_TEGRA_MIRROR)" "true"
    clobber_all: submake.rmapi_tegra_mirror.clobber
endif
ifeq "$(INCLUDE_MME64)" "true"
    clobber_all: submake.mme64sim.clobber
endif
ifeq "$(INCLUDE_MUCC)" "true"
    clobber_all: submake.mucc.clobber
endif
clobber_all: clobber

# Note this must be first in the dependancy list so that the MODS
# runspace is created before exelwting any submakes
install_all:$(MODS_RUNSPACE)/dummy.txt
ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    install_all:submake.vgpu_plugin.install
endif
ifeq "$(INCLUDE_LWWATCH)" "true"
    install_all:submake.lwwatch.install
endif
ifeq "$(use_mods_lwlink)" "true"
    install_all:submake.lwlink.install
endif
ifeq "$(use_mods_lwswitch)" "true"
    install_all:submake.lwswitch.install
endif
ifeq "$(INCLUDE_GPU)" "true"
    install_all:submake.amap.install
    install_all:submake.dramhal.install
endif
ifeq "$(INCLUDE_FSLIB)" "true"
    install_all:submake.fs.install
endif
ifeq "$(INCLUDE_LWLWVID)" "true"
    install_all:submake.lwlwvid.install
endif
ifeq "$(INCLUDE_DRVEXPORT)" "true"
    install_all:submake.drvexport.install
endif
ifeq "$(INCLUDE_MDIAG)" "true"
    install_all: submake.mdiag.install
endif
ifeq "$(INCLUDE_MDIAGUTL)" "true"
    install_all: submake.mdiagutl.install
endif
ifeq "$(use_mods_rm)" "true"
    install_all: submake.resman.install
endif
ifeq "$(INCLUDE_FLCNDBG)" "true"
    install_all: submake.lwelf.install
    install_all: submake.lwdwarf.install
    install_all: submake.flcndwarf.install
    install_all: submake.flcndbg.install
endif
ifeq "$(INCLUDE_DP)" "true"
    install_all: submake.displayport.install
endif
ifeq "$(INCLUDE_LWDA)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        install_all: submake.lwca.install
    endif
endif
ifeq "$(INCLUDE_OGL)" "true"
    install_all: submake.ogl.install
endif
ifeq "$(INCLUDE_VULKAN)" "true"
    install_all: submake.glslang.build
endif
ifeq "$(INCLUDE_VKLAYERS)" "true"
    ifneq "$(VK_LAYERS_USE_LWMAKE)" "true"
        install_all: submake.vkkhronosvalidation.install
    endif
endif
ifeq "$(INCLUDE_MATS)" "true"
    install_all: submake.mats.install
endif
ifeq "$(INCLUDE_MLA)" "true"
    install_all: submake.mla.install
endif
ifeq "$(INCLUDE_PROTOUTIL)" "true"
    install_all: submake.protoutil.install
endif
ifeq "$(INCLUDE_LWOGTEST)" "true"
    install_all: submake.lwogtest.install
endif
ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    install_all: submake.rmapi_tegra.install
endif
ifeq "$(INCLUDE_RMAPI_TEGRA_MIRROR)" "true"
    install_all: submake.rmapi_tegra_mirror.install
endif
ifeq "$(INCLUDE_POKEGPU)" "true"
    install_all: submake.pokegpu.install
endif
ifeq "$(INCLUDE_MME64)" "true"
    install_all: submake.mme64sim.install
endif
ifeq "$(INCLUDE_MUCC)" "true"
    install_all: submake.mucc.install
endif
install_all: install

ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    uninstall_all:submake.vgpu_plugin.uninstall
endif
ifeq "$(INCLUDE_LWWATCH)" "true"
    uninstall_all:submake.lwwatch.uninstall
endif
ifeq "$(use_mods_lwlink)" "true"
    uninstall_all:submake.lwlink.uninstall
endif
ifeq "$(use_mods_lwswitch)" "true"
    uninstall_all:submake.lwswitch.uninstall
endif
ifeq "$(INCLUDE_GPU)" "true"
    uninstall_all:submake.amap.uninstall
    uninstall_all:submake.dramhal.uninstall
endif
ifeq "$(INCLUDE_FSLIB)" "true"
    uninstall_all:submake.fs.uninstall
endif
ifeq "$(INCLUDE_LWLWVID)" "true"
    uninstall_all:submake.lwlwvid.uninstall
endif
ifeq "$(INCLUDE_DRVEXPORT)" "true"
    uninstall_all:submake.drvexport.uninstall
endif
ifeq "$(INCLUDE_MDIAG)" "true"
    uninstall_all: submake.mdiag.uninstall
endif
ifeq "$(INCLUDE_MDIAGUTL)" "true"
    uninstall_all: submake.mdiagutl.uninstall
endif
ifeq "$(use_mods_rm)" "true"
    uninstall_all: submake.resman.uninstall
endif
ifeq "$(INCLUDE_FLCNDBG)" "true"
    uninstall_all: submake.lwelf.uninstall
    uninstall_all: submake.lwdwarf.uninstall
    uninstall_all: submake.flcndwarf.uninstall
    uninstall_all: submake.flcndbg.uninstall
endif
ifeq "$(INCLUDE_DP)" "true"
    uninstall_all: submake.displayport.uninstall
endif
ifeq "$(INCLUDE_LWDA)" "true"
    uninstall_all: submake.lwca.uninstall
endif
ifeq "$(INCLUDE_OGL)" "true"
    uninstall_all: submake.ogl.uninstall
endif
ifeq "$(INCLUDE_VKLAYERS)" "true"
    ifneq "$(VK_LAYERS_USE_LWMAKE)" "true"
        uninstall_all: submake.vkkhronosvalidation.uninstall
    endif
endif
ifeq "$(INCLUDE_MATS)" "true"
    uninstall_all: submake.mats.uninstall
endif
ifeq "$(INCLUDE_MLA)" "true"
    uninstall_all: submake.mla.uninstall
endif
ifeq "$(INCLUDE_PROTOUTIL)" "true"
    uninstall_all: submake.protoutil.uninstall
endif
ifeq "$(INCLUDE_LWOGTEST)" "true"
    uninstall_all: submake.lwogtest.uninstall
endif
ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    uninstall_all: submake.rmapi_tegra.uninstall
endif
ifeq "$(INCLUDE_RMAPI_TEGRA_MIRROR)" "true"
    uninstall_all: submake.rmapi_tegra_mirror.uninstall
endif
ifeq "$(INCLUDE_MME64)" "true"
    uninstall_all: submake.mme64sim.uninstall
endif
ifeq "$(INCLUDE_MUCC)" "true"
    uninstall_all: submake.mucc.uninstall
endif
uninstall_all: uninstall

HW_TREE ?= $(P4ROOT)/hw

check_registers:
	@echo "Checking registers definition : $(MCHECK_ARGS)"
	$(Q)$(PERL) $(DRIVERS_DIR)/resman/kernel/inc/tools/mcheck2/mcheck.pl --profile $(MCHECK_ARGS)

check_rmcode_exceptions:
	@echo "Checking rm code exceptions"
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/resman/arch/lwalloc/mods $@

#-------------------------------------------------------------------------------
#
# Target: start/stop build messages
#
#-------------------------------------------------------------------------------
print_start_build_message :
	@$(ECHO) "******************************************************************"
	@$(ECHO) "* Building $(BUILD_CFG) version of $(exe_name), BRANCH=$(BRANCH)."
	@$(ECHO) "* defines : $(subst -D,,$(defines))"
	@$(ECHO) "* Using GNU make $(MAKE_VERSION)"
	@$(ECHO) "******************************************************************"

implib: $(base_mods_lib_dir)/$(implib_name)

$(base_mods_lib_dir)/$(implib_name): $(def_name)
	@$(AR) $(ARFLAGS) /def:$(def_name) /machine:$(BUILD_ARCH)

ifdef CYGWIN_MAKE
    rmconfig_files_dir = $(shell $(MODS_DIR)/tools/abs2rel.sh $(o_dir)/rmconfig_files)
    rmconfig_output_dir = ../../../diag/mods/$(rmconfig_files_dir)
else ifdef MINGW_MAKE
    rmconfig_files_dir = $(o_dir)/rmconfig_files
    rmconfig_output_dir = ../../../diag/mods/$(rmconfig_files_dir)
else
    rmconfig_files_dir = $(o_dir)/rmconfig_files
    rmconfig_output_dir = $(o_dir)/rmconfig_files
endif

$(rmconfig_files_dir)/dummy.txt :                               \
    $(wildcard $(DRIVERS_DIR)/resman/config/*)                  \
    $(wildcard $(DRIVERS_DIR)/resman/config/ctrldefs/*)         \
    $(wildcard $(DRIVERS_DIR)/resman/config/haldefs/*)          \
    $(wildcard $(DRIVERS_DIR)/resman/config/srcdefs/*)          \
    $(wildcard $(DRIVERS_DIR)/resman/config/templates/*)        \
    $(wildcard $(DRIVERS_DIR)/common/chip-config/*)             \
    $(wildcard $(DRIVERS_DIR)/common/chip-config/util/*)        \
    $(wildcard $(DRIVERS_DIR)/common/chip-config/util/Getopt/*) \
    $(wildcard $(DRIVERS_DIR)/common/chip-config/util/Text/*)   \
    $(wildcard $(DRIVERS_DIR)/common/chip-config/util/Rmconfig/*)
	@-$(MKDIR_P) $(rmconfig_files_dir)
	@cd $(DRIVERS_DIR)/resman/config && $(PERL) rmconfig.pl --output-directory $(rmconfig_output_dir) --source-root ../ --profile shipping-gpus-mods --enable=GENERATE_PDB_SEPARATELY
	@-$(TOUCH) $(rmconfig_files_dir)/dummy.txt

clean_rmconfig :
	@-$(RM_RF) $(rmconfig_files_dir)

#----------------------------------------------------------------------------
#
# Target: build the exelwtable
#
#----------------------------------------------------------------------------
ifneq "$(BUILD_ARCHS)" ""
    ifeq "$(base_os)" "macosx"
        $(o_dir)/$(exe_name): $(SUBMAKE_FAT_ARCHS:%=%.all) $(o_dir)/dummy.txt
	    $(Q)$(ECHO) "Creating universal binary..."
	    $(Q)lipo -create -output $@ $(foreach arch,$(BUILD_ARCHS),$(BUILD_CFG)/$(BUILD_OS)/$(arch)/$(exe_name))

        submake.fat.%.all:
	    @echo "Building $* architecture..."
	    $(Q)$(MAKE) BUILD_ARCHS="" BUILD_ARCH=$* mods_lib_dir=$(mods_lib_dir) base_mods_lib_dir=$(base_mods_lib_dir) all
    endif
else
ifeq "$(TRIGGER_RMCFG)" "true"
    $(o_files) : $(rmconfig_files_dir)/dummy.txt
endif
ifneq "$(BOUND_JS)" "false"
    ifeq "$(INCLUDE_BYTECODE)" "true"
        $(gen_cpp_dir)/boundjs.h: $(BOUND_JS) encrypt
	    $(Q)$(ENCRYPT_TOOL) -b --binary --input $< -o $@
    else
        bound_js_args = -igpu/js -icore/js -i$(CONTRIB_DIR_UNIX) -i$(CONTRIB_DIR_UNIX)/dgx -i$(MODS_RUNSPACE) -i$(o_dir)
        ifdef DVS_SW_CHANGELIST
            bound_js_args += -DDVS_BUILD
        endif
        $(gen_cpp_dir)/boundjs.h: $(BOUND_JS) encrypt
	    $(Q)$(ENCRYPT_TOOL) -b $(bound_js_args) --input $< -o $@
    endif
endif
$(o_dir)/$(exe_name) : $(o_files)
ifeq "$(dll_build)" "false"
    ifeq "$(use_mods_rm)" "true"
        $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/librm/librm$(lib_suffix)
    endif
    ifeq "$(INCLUDE_OGL)" "true"
        ifneq "$(USE_SYSTEM_LIBS)" "true"
            $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/libmodsgl/libmodsgl$(lib_suffix)
        endif
    endif
    ifeq "$(INCLUDE_GPU)" "true"
        $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/libamap/libamap$(lib_suffix)
        $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/libdramhal/libdramhal$(lib_suffix)
    endif
    ifeq "$(INCLUDE_FSLIB)" "true"
        $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/libfs/libfs$(lib_suffix)
    endif
    ifeq "$(INCLUDE_LWLWVID)" "true"
        $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/liblwlwvid/liblwlwvid$(lib_suffix)
    endif
endif
ifeq "$(INCLUDE_RMAPI_TEGRA)" "true"
    $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/librmapi_tegra/librmapi_tegra$(lib_suffix)
endif
ifeq "$(INCLUDE_MDIAG)" "true"
    MDIAG_LIB_DIR = $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/libmdiag
    $(o_dir)/$(exe_name): $(MDIAG_LIB_DIR)/libmdiag$(lib_suffix)
    ifeq "$(INCLUDE_MDIAGUTL)" "true"
        UTL_LIB_DIR = $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/libutl
        $(o_dir)/$(exe_name): $(UTL_LIB_DIR)/libutl$(lib_suffix)
    endif
endif
ifeq "$(base_os)" "win32"
# In Windows we cannot mix debug and release C++ libraries, they must
# be separate
    CFG_DEP_LIB_PLACEMENT=/$(BUILD_CFG)
else
    CFG_DEP_LIB_PLACEMENT=
endif
$(o_dir)/$(exe_name) : $(base_mods_lib_dir)$(CFG_DEP_LIB_PLACEMENT)/libpng$(lib_suffix)
$(o_dir)/$(exe_name) : $(base_mods_lib_dir)$(CFG_DEP_LIB_PLACEMENT)/libxml$(lib_suffix)

$(o_dir)/$(exe_name) : $(base_mods_lib_dir)$(CFG_DEP_LIB_PLACEMENT)/libencryption$(lib_suffix)

ifeq "$(INCLUDE_LWDA)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/libmodslwda/libmodslwda$(lib_suffix)
    endif
endif

ifeq "$(INCLUDE_LWDART)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        $(o_dir)/$(exe_name) : $(LWDA_TESTS_DIR)/lwdart_libs/$(BUILD_ARCH)/$(LWDART_LIB_CFG)/liblwdart_static$(lib_suffix)
    endif
endif
ifeq "$(INCLUDE_FATBINLD)" "true"
    ifneq "$(USE_LWDA_SYSTEM_LIB)" "true"
        $(o_dir)/$(exe_name) : $(LWMAKE_DRV_OUT_DIR)/drivers/gpgpu/lwca/mods/fatbinaryLoader/libfatbinloader$(lib_suffix)
    endif
endif

ifeq "$(use_static_lwwatch)" "true"
    $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/liblwwatch/liblwwatch$(lib_suffix)
endif

ifeq "$(use_mods_lwlink)" "true"
    $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/liblwlink/liblwlink$(lib_suffix)
endif

ifeq "$(use_mods_lwswitch)" "true"
    $(GEN_LIB_DIR_UNIX)/liblwswitch/liblwswitch$(lib_suffix) : submake.lwswitch.build
    $(o_dir)/$(exe_name) : $(GEN_LIB_DIR_UNIX)/liblwswitch/liblwswitch$(lib_suffix)
endif

ifeq "$(INCLUDE_FM)" "true"
    $(LWMAKE_DRV_OUT_DIR)/apps/fabricmanager/mods/libfabricmanager$(lib_suffix) : submake.fabricmanager.build
    $(o_dir)/$(exe_name) : $(LWMAKE_DRV_OUT_DIR)/apps/fabricmanager/mods/libfabricmanager$(lib_suffix)
endif

ifeq "$(INCLUDE_TRANSPORT)" "true"
    $(LWMAKE_DRV_OUT_DIR)/diag/multinode/transport/libmultinode_transport$(lib_suffix) : submake.multinode_transport.build
    $(o_dir)/$(exe_name) : $(LWMAKE_DRV_OUT_DIR)/diag/multinode/transport/libmultinode_transport$(lib_suffix)
endif

ifeq "$(REDACT_STRINGS)" "true"
    $(o_dir)/$(exe_name): $(REDACT_RANDOMS) $(GUARDWORD_FILE) $(GUARDWORD_IGNORE)
endif

	@$(ECHO) "Linking object files and libraries: $(whole_libraries) $(libraries)"
	$(Q)$(ECHO) "File key for $(exe_name):"
	$(Q)$(GREP) "^ " $(gen_cpp_dir)/aes_file_key.cpp
	$(Q)$(ECHO) "Log key for $(exe_name):"
	$(Q)$(GREP) "^ " $(gen_cpp_dir)/aes_log_key.cpp
ifeq "$(BUILD_OS)" "winsim"
    ifeq "$(SERVER_MODE)" "true"
        ifdef CYGWIN_MAKE
	        $(Q)$(ECHO) $(LDFLAGS_DLL) $(SYMFLAGS) $(o_files) $(link_libs) > $(o_dir)/ld.cmd
        else
	        $(file >$(o_dir)/ld.cmd,$(LDFLAGS_DLL) $(SYMFLAGS) $(o_files) $(link_libs))
        endif
    else
        ifdef CYGWIN_MAKE
	        $(Q)$(ECHO) $(LDFLAGS_EXE) $(SYMFLAGS) $(o_files) $(link_libs) > $(o_dir)/ld.cmd
        else
	        $(file >$(o_dir)/ld.cmd,$(LDFLAGS_EXE) $(SYMFLAGS) $(o_files) $(link_libs))
        endif
    endif
	$(Q)$(LD) @$(o_dir)/ld.cmd
else
    ifeq "$(SERVER_MODE)" "true"
	$(Q)$(LD) $(LDFLAGS_DLL) $(SYMFLAGS) $(o_files) $(link_libs)
    else
        ifeq "$(base_os)" "win32"
        # WAR for 8-32KB limit to linker arguments
        ifdef CYGWIN_MAKE
	        @echo $(LDFLAGS_EXE) $(SYMFLAGS) $(subst $(o_dir)/,+/,$(o_files)) $(link_libs) | sed "s#+/#$(o_dir)/#g" > ld.arg
        else
	        $(file >ld.arg,$(LDFLAGS_EXE) $(SYMFLAGS) $(o_files) $(link_libs))
        endif
	    $(Q)$(LD) @ld.arg
        else
	    $(Q)cd $(o_dir) && $(LD) $(LDFLAGS_EXE) $(SYMFLAGS) $(foreach o_file,$(o_files),$(shell $(MODS_DIR)/tools/abs2rel.sh $(o_file) $(o_dir))) $(link_libs)
        endif
    endif
endif

ifneq "$(BUILD_CFG)" "debug"
    ifeq "$(base_os)" "macosx"
        ifeq "$(SYMBOLIZE)" "false"
	    $(Q)/usr/bin/strip -u $@
        endif
    endif

    # Redact mods exelwtable before compression
    ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) --guardword "$(DRIVERS_DIR)/common/build" "$(GUARDWORD_FILE)" "$(ENABLE_GUARDWORD)" $@ || ( rm $@ ; false )
    endif

    # Record MODS exelwtable size in DVS, before compression
    ifdef DVS_SW_CHANGELIST
	$(Q)$(PYTHON3) tools/dvssize.py $(o_dir)/size.data $(o_dir)/$(exe_name)
    endif
endif

ifeq "$(COMPRESS_REL)" "true"
	@$(ECHO) "Compressing exelwtable."
	$(Q)$(BUILD_TOOLS_DIR)/linux/upx-3.94-amd64_linux/upx -9 -qqq $@
endif

ifeq "$(base_os)" "macosx"
    ifeq "$(SYM_OR_DEBUG)" "true"
        # We have to move the symbols out of the .o files and into
        # the mods.dSYM directory which will be copied to the runspace
        # along with MODS
	$(Q)/usr/bin/dsymutil $@
    endif
endif

endif # ifeq "$(BUILD_ARCHS)" ""

#-------------------------------------------------------------------------------
#
# Target: table of redacted chip names
#
#-------------------------------------------------------------------------------

ifeq "$(REDACT_STRINGS)" "true"
# dbe files are not included in Mac builds, so disable deobfuscation for Mac builds for now
ifneq "$(base_os)" "macosx"
ifneq "$(BUILD_OS)" "cheetah"
    names_package_files += $(o_dir)/INTERNAL_names.dbe
    names_package_files += $(REDACT_RANDOMS)
    install_files += $(o_dir)/INTERNAL_names.dbe

zip: $(o_dir)/INTERNAL_names.dbe
install: $(o_dir)/INTERNAL_names.dbe

$(o_dir)/INTERNAL_names.db: $(REDACT_RANDOMS) tools/redact_future_chips.sh tools/redact_list
	@$(ECHO) "Creating $@"
	$(Q)$(REDACT_SCRIPT) --gen-internal $@
endif
endif
endif

#-------------------------------------------------------------------------------
#
# Target: clean
#
#-------------------------------------------------------------------------------

clean: $(SUBMAKE_FAT_ARCHS:%=%.clean) clean_official_build
	@echo Cleaning object, binary, and dependency files.
	@-$(RM_RF) $(gen_cpp_dir)
	@-$(RM_RF) $(o_dir)
ifeq "$(USE_SHARED_LIB_STUBS)" "true"
	@-$(RM_RF) $(LIB_STUBS_DIR)
endif
	@-$(RM) -f *.d
	@-$(RM) -f $(BUILD_OS)/boundjs.h $(gen_cpp_dir)/boundjs.h
	@-$(RM_RF) $(CLEAN_ME)

# Used when we change config variables such as the INCLUDE_* flags.
#
change_build_cfg: clean_official_build
	@-$(RM) $(o_dir)/cmdline$(obj_suffix)
	@-$(RM) $(o_dir)/jscript$(obj_suffix)
	@-$(RM) $(o_dir)/modsmain$(obj_suffix)
	@-$(RM) $(o_dir)/xp$(obj_suffix)
	@-$(RM) $(o_dir)/utility$(obj_suffix)
	@-$(RM) $(o_dir)/dither$(obj_suffix)
	@-$(RM) $(o_dir)/modsdrv$(obj_suffix)
	@-$(RM) $(o_dir)/gpu$(obj_suffix)

# Used by the modsbld scripts when anything in version.cpp changes
clean_official_build:
	@-$(RM) -f $(VERSION_CPP)

submake.fat.%.clean:
	@$(MAKE) BUILD_ARCH=$* BUILD_ARCHS="" clean

clobber: clean uninstall

#-------------------------------------------------------------------------------

ifneq "$(BOUND_JS)" "false"
    # filter out all js and h files
    external_release_files := $(filter-out %.h,$(external_release_files))
    external_release_files := $(filter-out %.js,$(external_release_files))
endif

ifeq "$(ENCRYPT_JS)" "true"
    # Replace the *.js targets with *.jse equivalents in $(o_dir)

    # move_to_odir(fileList,ext1,ext2) replaces all files in fileList
    # that end with .ext1 with $(o_dir)/filename.ext2
    move_to_odir = \
        $(foreach file,$(1), \
            $(if $(filter %.$(2),$(file)), \
                $(o_dir)/$(notdir $(file:.$(2)=.$(3))), \
                $(file)))

    # move_file_to_odir(fileList,fname1,fname2) replaces all files in fileList
    # that end with fname1 with $(o_dir)/fname2
    move_file_to_odir = \
        $(foreach file,$(1), \
            $(if $(filter $2,$(notdir $(file))), \
                $(o_dir)/$(3), \
                $(file)))

    # use_unencrypted(fileList) prepends any files that are not an absolute path
    # with mods directory in so that unencrypted versions are used
    use_unencrypted = \
        $(foreach file,$(1), $(abspath $(file)))

    external_release_files := $(call move_to_odir,$(external_release_files),js,jse)
    external_release_files := $(call move_to_odir,$(external_release_files),h,he)
    external_release_files := $(call move_to_odir,$(external_release_files),xml,xme)
    external_release_files := $(call move_to_odir,$(external_release_files),json,jsone)
    external_release_files := $(call move_to_odir,$(external_release_files),db,dbe)
    external_release_files := $(call move_file_to_odir,$(external_release_files),boarddb.spc,boarddb.spe)
    external_release_files += $(call use_unencrypted, $(unencrypted_rel_files))
    external_release_files += $(addprefix $(o_dir)/, $(notdir $(released_ref_files)))

    internal_package_files := $(internal_release_files)
    internal_package_files += $(DECRYPT_TOOL)
    internal_package_files += $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir_host)/decrypt/bypass.decrypt.INTERNAL.bin
    # Always encrypt files (even cheetah files) for non-cheetah builds
    ifneq "$(BUILD_OS)" "cheetah"
        internal_package_files += $(tegra_internal_files)
    endif

    internal_package_files := $(call move_to_odir,$(internal_package_files),js,jse)
    internal_package_files := $(call move_to_odir,$(internal_package_files),h,he)
    internal_package_files := $(call move_to_odir,$(internal_package_files),xml,xme)
    internal_package_files := $(call move_to_odir,$(internal_package_files),json,jsone)
    internal_package_files := $(call move_to_odir,$(internal_package_files),db,dbe)
    internal_package_files += $(call use_unencrypted, $(unencrypted_internal_files))

    # For cheetah builds add both the baseline cheetah files and internal cheetah files in unencrypted
    # format so that when internal cheetah images are created none of the cheetah files will be
    # encrypted
    ifeq "$(BUILD_OS)" "cheetah"
        internal_package_files += $(call use_unencrypted, $(tegra_release_files))
        internal_package_files += $(call use_unencrypted, $(tegra_internal_files))
    endif

endif

ifndef CYGWIN_MAKE # lwmake does not support Cygwin make
$(eval $(call LWMAKE_COMPONENT_DEPS,mle,shared_build_all))

mle_exe = $(GEN_LIB_DIR_UNIX)/lwmake/diag/mle/mle$(exe_suffix)

$(mle_exe) install: submake.mle.build

pvs_release_files += $(mle_exe)
install_files     += $(mle_exe)
exelwtable_files  += $(mle_exe)
endif

ifeq "$(BOUND_JS)" "false"
ifeq "$(REDACT_STRINGS)" "true"
ifneq "$(names_package_files)" ""
zip_internal: zip_names
zip_names: $(names_package_files)
	@$(ECHO) "******************************************************************"
	@$(ECHO) "* Zipping internal only MODS redacted names files"
	@$(ECHO) "******************************************************************"
	$(Q)$(RM) -f $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL.names$(zip_suffix)
ifeq "$(zip_suffix)" ".zip"
    ifeq "$(base_os)" "win32"
        ifdef CYGWIN_MAKE
	        $(Q)$(ECHO) -n > zipfilescyg.txt
	        $(foreach file,$(names_package_files),$(Q)$(ECHO) $(file) | tr ' ' '\n' >> zipfilescyg.txt$(NEWLINE))
	        $(Q)cygpath -w -f zipfilescyg.txt > zipfiles.txt
        else
	        $(Q)$(RM) zipfiles.txt
	        $(foreach file,$(names_package_files),$(Q)$(ECHO) $(file) >> zipfiles.txt$(NEWLINE))
        endif
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL.names$(zip_suffix) -@ <zipfiles.txt
    else
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL.names$(zip_suffix) $(names_package_files)
    endif
else
	$(Q)-$(RM_RF) $(TGZ_TEMP_DIR)
	$(Q)mkdir $(TGZ_TEMP_DIR)
	$(Q)cp -rp $(names_package_files) $(TGZ_TEMP_DIR)/
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL.names$(zip_suffix) -C $(TGZ_TEMP_DIR) .
	$(Q)$(RM_RF) $(TGZ_TEMP_DIR)
endif
endif
endif

zip_internal: $(internal_package_files)
ifeq "$(REDACT_STRINGS)" "true"
zip_internal: $(REDACT_RANDOMS)
endif
	@$(ECHO) "******************************************************************"
	@$(ECHO) "* Zipping internal only MODS files"
	@$(ECHO) "******************************************************************"
ifeq "$(SYMBOLIZE)" "true"
	$(Q)$(RM) -f $(PKG_OUT_DIR)/S$(PKG_NAME).INTERNAL$(zip_suffix)
else
	$(Q)$(RM) -f $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL$(zip_suffix)
endif
ifeq "$(zip_suffix)" ".zip"
    ifeq "$(base_os)" "win32"
        ifdef CYGWIN_MAKE
	        $(Q)$(ECHO) -n > zipfilescyg.txt
	        $(foreach file,$(internal_package_files),$(Q)$(ECHO) $(file) | tr ' ' '\n' >> zipfilescyg.txt$(NEWLINE))
	        $(Q)cygpath -w -f zipfilescyg.txt > zipfiles.txt
        else
	        $(Q)$(RM) zipfiles.txt
	        $(foreach file,$(internal_package_files),$(Q)$(ECHO) $(file) >> zipfiles.txt$(NEWLINE))
        endif
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL$(zip_suffix) -@ <zipfiles.txt
    else
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL$(zip_suffix) $(internal_package_files)
    endif
else
	$(Q)-$(RM_RF) $(TGZ_TEMP_DIR)
	$(Q)mkdir $(TGZ_TEMP_DIR)
    ifeq "$(base_os)" "macosx"
	$(Q)cp -r macosx/ui/Mods.app $(TGZ_TEMP_DIR)/mods-$(VERSION).app
        ifeq "$(SYM_OR_DEBUG)" "true"
	    $(Q)cp -r $(BUILD_CFG)/$(BUILD_OS)/$(BUILD_ARCH)/mods.dSYM $(TGZ_TEMP_DIR)/mods-$(VERSION).app/Contents/Resources/mods.dSYM
        endif
	$(Q)ln $(internal_package_files) $(TGZ_TEMP_DIR)/mods-$(VERSION).app/Contents/Resources/
    else
	$(Q)cp -rp $(internal_package_files) $(TGZ_TEMP_DIR)/
    endif
    ifeq "$(BUILD_OS)" "cheetah"
        ifeq "$(INCLUDE_RMTEST)" "true"
		$(Q)cp -rp $(rmtest_release_files) $(rmtest_release_directories) $(TGZ_TEMP_DIR)/
        endif
    endif
    ifeq "$(SYMBOLIZE)" "true"
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/S$(PKG_NAME).INTERNAL$(zip_suffix) -C $(TGZ_TEMP_DIR) .
    else
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL$(zip_suffix) -C $(TGZ_TEMP_DIR) .
    endif
	$(Q)$(RM_RF) $(TGZ_TEMP_DIR)
endif
    ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) --no-delete $(PKG_OUT_DIR)/$(PKG_NAME).INTERNAL$(zip_suffix)
    endif
endif # BOUND_JS == false

# Build an external release package
zip: $(external_release_files) $(unpackaged_release_files) zip_internal
ifeq "$(REDACT_STRINGS)" "true"
zip: $(REDACT_RANDOMS) $(GUARDWORD_FILE) $(GUARDWORD_IGNORE)
endif
	@$(ECHO) "******************************************************************"
	@$(ECHO) "* Zipping MODS package."
	@$(ECHO) "******************************************************************"
ifeq "$(SYMBOLIZE)" "true"
	$(Q)$(RM) -f $(PKG_OUT_DIR)/S$(PKG_NAME)$(zip_suffix)
else
	$(Q)$(RM) -f $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix)
endif

ifeq "$(zip_suffix)" ".zip"
    ifeq "$(base_os)" "win32"
        ifdef CYGWIN_MAKE
	        $(Q)$(ECHO) -n > zipfilescyg.txt
	        $(foreach file,$(external_release_files),$(Q)$(ECHO) $(file) | tr ' ' '\n' >> zipfilescyg.txt$(NEWLINE))
	        $(Q)cygpath -w -f zipfilescyg.txt > zipfiles.txt
        else
	        $(Q)$(RM) zipfiles.txt
	        $(foreach file,$(external_release_files),$(Q)$(ECHO) $(file) >> zipfiles.txt$(NEWLINE))
        endif
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix) -@ <zipfiles.txt
    else
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix) $(external_release_files)
    endif
else
	$(Q)-$(RM_RF) $(TGZ_TEMP_DIR)
	$(Q)mkdir $(TGZ_TEMP_DIR)
    ifeq "$(base_os)" "macosx"
	$(Q)cp -r macosx/ui/Mods.app $(TGZ_TEMP_DIR)/mods-$(VERSION).app
        ifeq "$(SYM_OR_DEBUG)" "true"
	    $(Q)cp -r $(BUILD_CFG)/$(BUILD_OS)/$(BUILD_ARCH)/mods.dSYM $(TGZ_TEMP_DIR)/mods-$(VERSION).app/Contents/Resources/mods.dSYM
        endif
	$(Q)ln $(external_release_files) $(TGZ_TEMP_DIR)/mods-$(VERSION).app/Contents/Resources/
    else
	$(Q)cp -rp $(external_release_files) $(TGZ_TEMP_DIR)/
    endif
    ifeq "$(SYMBOLIZE)" "true"
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/S$(PKG_NAME)$(zip_suffix) -C $(TGZ_TEMP_DIR) .
    else
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix) -C $(TGZ_TEMP_DIR) .
    endif
    ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) --no-delete --skip mods$(exe_suffix) --skip mats$(exe_suffix) --guardword "$(DRIVERS_DIR)/common/build" "$(GUARDWORD_FILE)" "$(ENABLE_GUARDWORD)" $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix)
    endif
	$(Q)$(RM_RF) $(TGZ_TEMP_DIR)
endif

pvs_exclude_files +=  mats$(exe_suffix)
ifeq "$(INCLUDE_XUSB)" "true"
    pvs_exclude_files += libusbSourceCodeOffer.pdf
endif
pvs_exclude_files += $(MODS_TOOLS_DIR)/lwpex/lwpex$(exe_suffix)
pvs_exclude_files += $(MODS_TOOLS_DIR)/lwpex/lwpex2$(exe_suffix)
pvs_exclude_files += $(shell echo $(o_dir) | sed 's=_tsan/=/=')/protoutil$(exe_suffix)

# using $(addprefix %/, $(pvs_exclude_files)) messes up the removal of the libspirv.so & librtcore.so files!
# why is it here in the first place?
#pvs_package_files = $(filter-out $(addprefix %/, $(pvs_exclude_files)), $(pvs_release_files))
pvs_package_files = $(filter-out $(pvs_exclude_files), $(pvs_release_files))

# Build a PVS release package
zip_pvs: $(pvs_package_files)

zip_pvs_nodep: $(filter $(all_gen_files), $(pvs_package_files))

ifeq "$(REDACT_STRINGS)" "true"
zip_pvs: $(REDACT_RANDOMS) $(GUARDWORD_FILE) $(GUARDWORD_IGNORE)
zip_pvs_nodep: $(REDACT_RANDOMS) $(GUARDWORD_FILE) $(GUARDWORD_IGNORE)
endif

zip_pvs zip_pvs_nodep:
	@$(ECHO) "******************************************************************"
	@$(ECHO) "* Zipping PVS regression package."
	@$(ECHO) "******************************************************************"
	$(Q)-$(RM) -f $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix)
ifneq "$(base_os)" "win32"
	$(Q)sh tools/check_exelwtable_bit.sh $(filter-out $(exelwtable_files),$(install_files) $(pvs_package_files))
    ifeq "$(SYMBOLIZE)" "true"
	$(Q)tools/separate_symbols.sh "$(OBJCOPY)" "$(STRIP)" $(pvs_package_files)
    endif
endif
ifeq "$(zip_suffix)" ".zip"
    ifeq "$(base_os)" "win32"
        ifdef CYGWIN_MAKE
	        $(Q)$(ECHO) -n > zipfilescyg.txt
	        $(foreach file,$(pvs_package_files),$(Q)$(ECHO) $(file) | tr ' ' '\n' >> zipfilescyg.txt$(NEWLINE))
	        $(Q)cygpath -w -f zipfilescyg.txt > zipfiles.txt
        else
	        $(Q)$(RM) zipfiles.txt
	        $(foreach file,$(pvs_package_files),$(Q)$(ECHO) $(file) >> zipfiles.txt$(NEWLINE))
        endif
	    $(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix) -@ <zipfiles.txt
    else
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix) $(pvs_package_files)
    endif
    ifeq "$(INCLUDE_RMTEST)" "true"
	$(Q)$(RM_RF) rmtestReleaseFiles
	$(Q)$(MKDIR_P) rmtestReleaseFiles
	$(Q)$(CP) -rf $(rmtest_release_files) $(rmtest_release_directories) rmtestReleaseFiles/.
	$(Q)cd rmtestReleaseFiles && $(ZIP) -r ../$(PKG_NAME)$(zip_suffix) *
	$(Q)$(RM_RF) rmtestReleaseFiles
    endif
else
	$(Q)-$(RM_RF) $(TGZ_TEMP_DIR)
	$(Q)mkdir $(TGZ_TEMP_DIR)
	$(Q)cp $(pvs_package_files) $(TGZ_TEMP_DIR)/
    ifeq "$(INCLUDE_RMTEST)" "true"
	cp -rf $(rmtest_release_files) $(rmtest_release_directories) $(TGZ_TEMP_DIR)/
    endif
	$(Q)$(ZIP) $(ZIPFLAGS) $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix) -C $(TGZ_TEMP_DIR) .
    ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) --no-delete --skip mods$(exe_suffix) --skip mats$(exe_suffix) --skip fileids.txt --guardword "$(DRIVERS_DIR)/common/build" "$(GUARDWORD_FILE)" "$(ENABLE_GUARDWORD)" $(PKG_OUT_DIR)/$(PKG_NAME)$(zip_suffix)
    endif
	$(Q)$(RM_RF) $(TGZ_TEMP_DIR)
endif

$(REDACT_RANDOMS):
	$(Q)$(REDACT_SCRIPT)

$(GUARDWORD_FILE):
	@$(ECHO) "Generating" "$@"
	$(Q)$(PYTHON) $(DRIVERS_DIR)/common/build/lwconfig.py --profile=$(LWCFG_PROFILE) --guardwordOutputFile=$@

$(GUARDWORD_IGNORE): tools/gw_ignore.txt
	$(Q)$(CP_IF_CHANGED) $< $@

# Create archive with MODS kernel module sources
ifeq "$(INSTALL_MODS_DRIVER)" "true"
    install: $(MODS_KERNEL_DRIVER_TGZ)
    $(MODS_KERNEL_DRIVER_TGZ): $(LINUX_DRIVER_SRC_PATH)/mods*.h $(LINUX_DRIVER_SRC_PATH)/mods*.c
	@$(ECHO) "Packaging the driver..."
	$(Q)sh $(LINUX_DRV_DIR)/package.sh $(MODS_KERNEL_DRIVER_TGZ) $(o_dir) $(LINUX_DRIVER_SRC_PATH)
endif

# Add PPC (Port Policy Controller) package to MODSA
ifeq "$(INCLUDE_XUSB)" "true"
    install: $(MODS_PPC_DRIVER_TGZ)
    $(MODS_PPC_DRIVER_TGZ) : $(LINUX_PPC_DRIVER_DIR)/package.sh
	@$(ECHO) "Packaging PPC (Port Policy Controller) driver..."
	$(Q)sh $(LINUX_PPC_DRIVER_DIR)/package.sh $(MODS_PPC_DRIVER_TGZ) $(o_dir) $(LINUX_PPC_DRIVER_DIR)
endif

# Install to a runspace directory
install: $(MODS_RUNSPACE)/dummy.txt $(filter $(all_gen_files), $(install_files))
    ifdef MINGW_MAKE
	    $(file >$(o_dir)/cat.arg,$(install_files))
	    @$(CAT) $(o_dir)/cat.arg
    else
	    @$(ECHO) $(install_files)
    endif
	@$(ECHO) "Installing MODS to $(MODS_RUNSPACE)..."
ifneq "$(base_os)" "win32"
# if not WSL
ifeq "" "$(findstring Microsoft,$(shell uname -a))"
	$(Q)sh tools/check_exelwtable_bit.sh $(filter-out $(exelwtable_files),$(install_files))
endif
endif
	@-$(CP_TO_RUNSPACE) $(CP_RM_DEST) $(install_files) $(rmtest_release_directories) $(MODS_RUNSPACE)
ifeq "$(base_os)" "macosx"
    ifeq "$(SYM_OR_DEBUG)" "true"
	# We have to copy the mods.dSYM folder along with
	# everything else for GDB
	$(Q)$(CP_TO_RUNSPACE) $(o_dir)/$(exe_name).dSYM $(MODS_RUNSPACE)/.
    endif
endif

uninstall:
	@echo "Deleting installed files."
	@-$(RM) -f $(addprefix $(MODS_RUNSPACE)/, $(notdir $(install_files)) dummy.txt)
	@-$(RM_RF) $(addprefix $(MODS_RUNSPACE)/, $(notdir $(rmtest_release_directories)))
ifeq "$(base_os)" "macosx"
    ifeq "$(SYM_OR_DEBUG)" "true"
	@-$(RM_RF) $(MODS_RUNSPACE)/$(exe_name).dSYM
    endif
endif

build_install: all
	$(Q)$(MAKE) --no-print-directory install

encrypt: submake.encrypt.all


#-------------------------------------------------------------------------------
#
# Support for builds with distcc and ccache
#
#-------------------------------------------------------------------------------

ifneq "$(filter parallel ccache,$(MAKECMDGOALS))" ""
    ifeq ($(filter noccache,$(MAKECMDGOALS)),)
        export CCACHE        ?= /home/utils/ccache-2.4lw/bin/ccache
        export CCACHE_DIR    ?= /home/scratch.archbuilder/ccache_repository
        export CCACHE_NOLINK ?= 1
        export CCACHE_UMASK  ?= 002
    endif
endif


ifeq "$(filter parallel,$(MAKECMDGOALS))" "parallel"
    ifndef PATHTOHWTOOLS
        PATHTOHWTOOLS = $(HW_TREE)/tools
        $(warning Assuming PATHTOHWTOOLS = $(PATHTOHWTOOLS))
    endif

    PYTHON ?= $(PATHTOHWTOOLS)/Python-2.4.1/bin/python
    FARMCC ?= $(PATHTOHWTOOLS)/farmcc/farmcc.py
    NICE   ?= nice
    export DISTCC ?= /home/utils/distcc-2.18.3/bin/distcc
    CLIENTPATHS += //hw/tools/Python-2.4.1/...
    CLIENTPATHS += //hw/tools/farmcc/...
endif

ccache : $(if $(subst ccache,,$(MAKECMDGOALS)),,build_all)

noccache : $(if $(subst noccache,,$(MAKECMDGOALS)),,build_all)

parallel :
	echo 'Running farmcc.py -j 20 --resources=2 -- nice '$(MAKE)' -j20 build_all'
	nice $(PYTHON) $(FARMCC) -j 20 -r 2 -- $(NICE) $(MAKE) -j20 build_all

# Colwenience rules for building object files and precompiled header:
# make object.gpusbdev.o - Build gpusbdev.o (does not work for submakes)
# make pch               - Build precompiled header
#
$(addprefix object., $(notdir $(o_files))): object.% : $(o_dir)/%

pch: $(o_dir)/$(pch_name)

# Check whether each path in $(CLIENTPATHS) is in the user's client.
# MISSING_CLIENTPATHS are paths that aren't in the user's client.
# UNKNOWN_CLIENTPATHS are paths in $(CLIENTPATHS) that we haven't checked yet.
# UNKNOWN_CLIENTPATHS should be empty when this section finishes.
#
# updateMissing and updateUnknown take 2 args: a P4 path, and the
# corresponding dir in the client.  Subdirectories that are mapped
# differently than the parent should be checked first.
#

# If 'p4' is in the path by default use 'p4 where' to determine if a location is synced
#
# The benefit of this is that if the parent directory of an existing clientpath is added to clientpaths 'check_client'
# will be able to indicate that the parent must be synced. The previous strategy only checks for the folder being present,
# so it wouldn't warn that the perforce client needs to be updated.
ifndef MINGW_MAKE
    ifneq (,$(shell which p4))
        ENABLE_P4WHERE_CHECK ?= true
    endif
endif
ENABLE_P4WHERE_CHECK ?= false

ifeq "$(ENABLE_P4WHERE_CHECK)" "true"
    updateMissing = $(MISSING_CLIENTPATHS) $(foreach CLIENTPATH,$(filter $(1) $(1)/%,$(UNKNOWN_CLIENTPATHS)),$(if $(shell p4 -q where $(patsubst $(1)%,$(abspath $(2))%,$(CLIENTPATH:/...=/DUMMYFILENAME_ELKJ26l3kj)) 2>&1),$(CLIENTPATH),))
else
    updateMissing = $(MISSING_CLIENTPATHS) $(foreach CLIENTPATH,$(filter $(1) $(1)/%,$(UNKNOWN_CLIENTPATHS)),$(if $(wildcard $(patsubst $(1)%,$(2)%,$(CLIENTPATH:/...=))),,$(CLIENTPATH)))
endif

updateUnknown = $(filter-out $(1)/%,$(UNKNOWN_CLIENTPATHS))

ifdef CYGWIN_MAKE
    BUILD_TOOLS_DIR_UNIX := $(shell cygpath -u $(BUILD_TOOLS_DIR))
else
    BUILD_TOOLS_DIR_UNIX := $(BUILD_TOOLS_DIR)
endif

# Note: these paths must match the ones added to CLIENTPATHS in MODS' makesrc.inc files
# for the warning message to show
MISSING_CLIENTPATHS :=
UNKNOWN_CLIENTPATHS := $(CLIENTPATHS)
MISSING_CLIENTPATHS := $(call updateMissing,//sw/$(BRANCH),$(MODS_DIR)/../..)
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/$(BRANCH),$(MODS_DIR)/../..)
MISSING_CLIENTPATHS := $(call updateMissing,//sw/mods/chipfuse,$(CHIP_XML_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/mods/chipfuse,$(CHIP_XML_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,//sw/mods/linuxdrv,$(LINUX_DRV_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/mods/linuxdrv,$(LINUX_DRV_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,//sw/mods/logging,$(LOGGING_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/mods/logging,$(LOGGING_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,//sw/mods/shared_modular_branch_files,$(SHARED_MODULAR_BRANCH_FILES_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/mods/shared_modular_branch_files,$(SHARED_MODULAR_BRANCH_FILES_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,//sw/mods/vulkan/validationlayers,$(VK_LAYERS_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/mods/vulkan/validationlayers,$(VK_LAYERS_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,$(SPIRV_TOOLS_DIR_P4),$(SPIRV_TOOLS_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,$(SPIRV_TOOLS_DIR_P4),$(SPIRV_TOOLS_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,$(MODS_TOOLS_DIR_P4)/makesrc.inc,$(MODS_TOOLS_DIR)/makesrc.inc)
UNKNOWN_CLIENTPATHS := $(call updateUnknown,$(MODS_TOOLS_DIR_P4)/makesrc.inc,$(MODS_TOOLS_DIR)/makesrc.inc)
MISSING_CLIENTPATHS := $(call updateMissing,$(LZ4_DIR_P4),$(LZ4_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,$(LZ4_DIR_P4),$(LZ4_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,//sw/mods/shared_files,$(SHARED_FILES_DIR))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/mods/shared_files,$(SHARED_FILES_DIR))
MISSING_CLIENTPATHS := $(call updateMissing,//sw/tools,$(BUILD_TOOLS_DIR_UNIX))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw/tools,$(BUILD_TOOLS_DIR_UNIX))
MISSING_CLIENTPATHS := $(call updateMissing,//sw,$(P4ROOT)/sw)
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//sw,$(P4ROOT)/sw)
MISSING_CLIENTPATHS := $(call updateMissing,//hw,$(HW_TREE))
UNKNOWN_CLIENTPATHS := $(call updateUnknown,//hw,$(HW_TREE))

ifdef ECHO
    ECHO_CLIENTPATH = $(ECHO)
else
    ECHO_CLIENTPATH = echo
endif

check_client:
ifneq ($(strip $(MISSING_CLIENTPATHS)),)
	@$(ECHO_CLIENTPATH) "W A R N I N G: Please sync to:"
	$(foreach PATH,$(sort $(MISSING_CLIENTPATHS)),@$(ECHO_CLIENTPATH) $(PATH)$(NEWLINE))
endif
ifneq ($(strip $(UNKNOWN_CLIENTPATHS)),)
	@$(ECHO_CLIENTPATH) "WARNING: The Makefile is not checking for:"
	$(foreach PATH,$(sort $(UNKNOWN_CLIENTPATHS)),@$(ECHO_CLIENTPATH) $(PATH)$(NEWLINE))
endif

client:
	@$(ECHO_CLIENTPATH) "The following paths must be in your client:"
	$(foreach PATH,$(sort $(CLIENTPATHS)),@$(ECHO_CLIENTPATH) $(PATH)$(NEWLINE))

official_only: $(MODS_DIR)/tools/official_only.txt

# Include dependency information.
-include $(depend_files)
