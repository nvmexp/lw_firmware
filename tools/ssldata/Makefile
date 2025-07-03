#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#


#-------------------------------------------------------------------------------
# MODS utility for gathering windows ssl data.
# See //sw/<tree>/diag/mods/makedefs.inc for more information about the MODS
# makefiles.
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#
# Environment variables: (set default values if not defined by user)
#
#-------------------------------------------------------------------------------
# Set variables to their default state if they are not already set.
BUILD_OS ?= winmfg

ifneq "$(BUILD_OS)" "winmfg"
    $(error ssldata is only supported with winmfg)
endif

SSLDATA_DIR     ?= .
UTIL_DIR        ?= $(SSLDATA_DIR)/../utils
MODS_DIR        ?= $(SSLDATA_DIR)/../mods
exe_build = true

base_name = ssldata
include $(MODS_DIR)/makedefs.inc

include_dirs += $(UTIL_DIR)
include_dirs += $(MODS_DIR)
include_dirs += $(SSLDATA_DIR)/../../sdk/lwpu/inc

SHOW_OUTPUT=true

libraries +=   lwdiag_utils 

lib_dirs     = $(MODS_DIR)/$(mods_lib_dir) $(MODS_DIR)/$(base_mods_lib_dir)

cpp_files += ssldata.cpp

defines += -DREGEX_MALLOC -DSTDC_HEADERS -DHAVE_CONFIG_H
LDFLAGS += /NODEFAULTLIB:libc.lib

ifeq "$(BUILD_CFG)" "debug"
  LDFLAGS += /NODEFAULTLIB:libcmt.lib
else
  LDFLAGS += /NODEFAULTLIB:libcmtd.lib
endif
extra_libs = ws2_32.lib crypt32.lib selwr32.lib gdi32.lib user32.lib advapi32.lib shell32.lib 

# Must turn on C++ exception handling when using the C++ standard library.
# Would really prefer to not have to turn this on... unwinding logic is not
# free, and we don't really want C++ exceptions.
CFLAGS += /GX

# Windows builds have some libraries that are different in debug/release
# builds.  Include those paths here
lib_dirs    += $(MODS_DIR)/$(base_mods_lib_dir)/$(BUILD_CFG)

#-------------------------------------------------------------------------------
# Targets:
#-------------------------------------------------------------------------------

.PHONY : all clean install

# The default target makes the the JavaScript library (libjs.a).
all :  $(o_dir)/$(exe_name)

include $(MODS_DIR_UNIX)/makerule.inc

$(o_dir)/$(exe_name) : $(o_files)
	$(Q)$(LD) $(LDFLAGS_EXE) $(o_files) $(link_libs)

clean : $(SUBMAKE_FAT_ARCHS:%=%.clean)
	@echo "Deleting object, lib, and dependency files."
	@-rm -rf $(gen_cpp_dir)
	@-rm -rf $(o_dir)

clobber: clean

# Include dependency information.
-include $(depend_files)

submake.fat.%.clean:
	@$(MAKE) BUILD_ARCH=$* BUILD_ARCHS="" clean
