# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#-------------------------------------------------------------------------------
# MODS UTL library make file.
# See //sw/<tree>/diag/mods/makedefs.inc for more information about the MODS
# makefiles.
#-------------------------------------------------------------------------------

MODS_DIR = ../..
dll_build = false
base_name = libutl

include $(MODS_DIR)/makedefs.inc

# Do not install library to improve build times
install_library := false

ifeq "$(toolchain)" "msvc"
    CFLAGS += /EHsc
    defines += -DWIN32 -D_WINDOWS
else
    CPPFLAGS += -Werror
endif

ifeq ($(base_os),win32)
    CFLAGS += /bigobj
endif

ifeq "$(toolchain)" "gnu"
    CFLAGS += -fvisibility=hidden
endif

# Some of the shared makefiles depend on LW_SOURCE, which may not be set
ifdef CYGWIN_MAKE
    LW_SOURCE ?= $(shell $(MODS_DIR_UNIX)/tools/abs2rel.sh "$(MODS_DIR_UNIX)/../..")
else
    LW_SOURCE ?= $(abspath $(MODS_DIR))/../..
endif

LW_RMSHARED_DISABLE_LWLOCKVAL := 1
LW_RMSHARED_DISABLE_LWTLS := 1
LW_RMSHARED_DISABLE_RESSERV := 1

include $(LW_SOURCE)/drivers/common/shared/shared.lwmk
# Append the shared sources/includes/defines to the main variables

defines += $(addprefix -D,$(LW_RMSHARED_DEFINES))
include_dirs += $(LW_RMSHARED_INCLUDES)
c_files += $(LW_RMSHARED_SOURCES)

defines += -DINCLUDE_GPU=1
ifeq "$(INCLUDE_LWSWITCH)" "true"
    defines += -DINCLUDE_LWSWITCH
endif
ifeq "$(INCLUDE_LWLINK)" "true"
    defines += -DINCLUDE_LWLINK
endif
defines += -DUTL_LIBRARY
defines += -DXML_STATIC
defines += -DINCLUDE_STD_NAMESPACE

defines += -DINCLUDE_MDIAGUTL=1

include_dirs += $(MODS_DIR)
include_dirs += $(MODS_DIR)/../js1.7

include_dirs += ../../../../drivers/common/inc
include_dirs += ../../../../drivers/common/inc/hwref
include_dirs += ../../../../drivers/resman/interface
include_dirs += ../../../../drivers/resman/arch/lwalloc/mods/inc
include_dirs += ../../../../drivers/resman/arch/lwalloc/common/inc

ifeq "$(INCLUDE_LWSWITCH)" "true"
    include_dirs += ../../../../drivers/lwswitch/interface
endif
ifeq "$(INCLUDE_LWLINK)" "true"
    include_dirs += ../../../../drivers/lwlink/interface
endif

include_dirs += $(MODS_DIR)/../utils
include_dirs += $(BOOST_DIR)

ifeq "$(base_os)" "linux"
    ifneq (,$(filter $(BUILD_ARCH), amd64 ppc64le))
        include_dirs += $(BUILD_TOOLS_DIR)/libpython-3.6.3/Include
        include_dirs += $(BUILD_TOOLS_DIR)/pybind11-2.2.3/include
    endif
endif

# Always include the OS's modsinc.h
force_include_files = $(base_os)/modsinc.h
force_include_files += core/include/jsprotect.h

ifneq "$(filter 7.% 9.%,$(toolchain_rev))" ""
    $(o_dir)/util-compiler-switch$(obj_suffix): CFLAGS += -Wno-frame-address
endif

cpp_files += utl.cpp

ifeq "$(base_os)" "linux"
    ifneq (,$(filter $(BUILD_ARCH), amd64 ppc64le))
        cpp_files += utlchannel.cpp
        cpp_files += utlchanwrap.cpp
        cpp_files += utlevent.cpp
		cpp_files += utlvectorevent.cpp
        cpp_files += utlgpu.cpp
        cpp_files += utlgpupartition.cpp
        cpp_files += utlhelp.cpp
        cpp_files += utlkwargsmgr.cpp
        cpp_files += utlregctrl.cpp
        ifeq "$(INCLUDE_LWSWITCH)" "true"
            cpp_files += utllwswitch.cpp
            cpp_files += utllwswitchregctrl.cpp
        endif
        cpp_files += utlgpuregctrl.cpp
        cpp_files += utlsmcengine.cpp
        cpp_files += utlsurface.cpp
        cpp_files += utlvidmemaccessbitbuffer.cpp
        cpp_files += utlperfmonbuffer.cpp
        cpp_files += utltest.cpp
        cpp_files += utlthread.cpp
        cpp_files += utltsg.cpp
        cpp_files += utlusertest.cpp
        cpp_files += utlutil.cpp
        cpp_files += utlvftest.cpp
        cpp_files += utlengine.cpp
        cpp_files += utlsubctx.cpp
        cpp_files += utlmutex.cpp
        cpp_files += utldebug.cpp
        cpp_files += utlgpuverif.cpp
        cpp_files += utlmmu.cpp
        cpp_files += utlvaspace.cpp
        cpp_files += utlrawmemory.cpp
        cpp_files += utlmemctrl.cpp
        cpp_files += utlfbinfo.cpp
        cpp_files += utlhwpm.cpp
        cpp_files += utlinterrupt.cpp
        cpp_files += utldpc.cpp
    endif
endif

.DEFAULT_GOAL := build
SKIP_REGHAL_SRC = true
DRIVERS_DIR ?= ../../../../drivers

LWCFG_ROOT      = $(DRIVERS_DIR)/common/build
LWCFG_OUTPUTDIR = $(gen_cpp_dir)
include $(LWCFG_ROOT)/lwconfig.lwmk
include_dirs += $(LWCFG_OUTPUTDIR)

GEN_LIB_DIR = $(MODS_OUTPUT_DIR)/$(rel_o_dir)
include ../../gpu/reghal/makesrc.inc

# Generic rules for a library build
include $(MODS_DIR_UNIX)/makelib.inc
include $(MODS_DIR_UNIX)/makerule.inc

# Include dependency information.
-include $(depend_files)
