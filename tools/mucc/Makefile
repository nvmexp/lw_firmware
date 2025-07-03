#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
#---------------------------------------------------------------------
# MuCC Assembler makefile
#
# This makefile creates two targets:
# (1) A libmucc library that can be compiled into mods.  The library
#     uses LwDiagUtils, including the EC error-codes, and puts its
#     code in the Mucc namespace.  A caller should start by including
#     mucc.h and instantiating a Mucc::Launcher.
# (2) A mucc exelwtable that runs as a standalone program.  The
#     exelwtable consists of main.cpp linked to the libmucc library,
#     so the library is compiled in the process of building the
#     exelwtable.
#---------------------------------------------------------------------

MODS_DIR         ?= ../mods
DRIVERS_DIR      ?= ../../drivers
JS_DIR           ?= ../js1.7
LWDIAG_UTILS_DIR ?= ../utils
SDK_DIR          ?= ../../sdk

dll_build = false
base_name = libmucc
include $(MODS_DIR)/makedefs.inc
exe_name = mucc$(exe_suffix)  # Override the exe_name from libmucc to mucc

# Source files for the libmucc library
cpp_files += mucccore.cpp
cpp_files += muccpgm.cpp
cpp_files += muccsim.cpp
cpp_files += muccgalit1.cpp
cpp_files += muccgalit1sim.cpp
cpp_files += muccghlit1.cpp

# Additional source files for the mucc exelwtable
exe_cpp_files += main.cpp

# Build flags
include_dirs += .
include_dirs += $(MODS_DIR)
include_dirs += $(LWDIAG_UTILS_DIR)
include_dirs += $(SDK_DIR)/lwpu/inc
include_dirs += $(DRIVERS_DIR)/common/inc/swref
include_dirs += $(DRIVERS_DIR)/common/inc/hwref
include_dirs += $(DRIVERS_DIR)/common/amaplib/include
include_dirs += $(JS_DIR)
include_dirs += $(BOOST_DIR)

libraries += lwdiag_utils

lib_dirs += $(MODS_OUTPUT_DIR)/$(rel_o_dir)/lwmake/diag/utils

ifeq "$(base_os)" "linux"
    ifneq "$(BUILD_OS_SUBTYPE)" "qnx"
        libraries += dl
    endif
endif
ifeq "$(use_pthreads)" "true"
    # Android supports POSIX threads, but not libpthread.so
    ifeq (,$(filter $(BUILD_OS_SUBTYPE), bionic qnx))
        libraries += pthread
    endif
endif

ifeq "$(base_os)" "win32"
    lib_dirs += $(MODS_DIR)/$(base_mods_lib_dir)/$(BUILD_CFG)
    extra_libs += ws2_32.lib crypt32.lib selwr32.lib
    CFLAGS += /EHsc /WX
else
    lib_dirs += $(MODS_DIR)/$(base_mods_lib_dir)
    CFLAGS += -Werror
endif

ifeq "$(DEBUG_GEN_PROFILE)" "true"
    CFLAGS += $(CFLAGS_PROF_OPTS)
    LDFLAGS += $(LDFLAGS_PROF_OPTS)
endif

#---------------------------------------------------------------------
# Targets
#---------------------------------------------------------------------

build: $(o_dir)/$(exe_name)

include $(MODS_DIR)/makelib.inc
include $(MODS_DIR)/makerule.inc

exe_o_files = $(addprefix $(o_dir)/, $(notdir $(exe_cpp_files:.cpp=$(obj_suffix))))
depend_files += $(exe_o_files:$(obj_suffix)=.d)

$(o_dir)/$(exe_name) : $(exe_o_files) $(o_dir)/$(lib_name) submake.lwdiag_utils.build
	$(Q)$(ECHO) Linking $(exe_name)...
	$(Q)$(LD) $(LDFLAGS_EXE) $(exe_o_files) $(o_dir)/$(lib_name) $(link_libs)

submake.lwdiag_utils.%:
	$(Q)$(MAKE) -C $(MODS_DIR)/../utils -f makefile.lwmk $(subst submake.lwdiag_utils.,,$@) $(LWMAKE_ARGS) $*

# Include dependency information.
-include $(depend_files)
