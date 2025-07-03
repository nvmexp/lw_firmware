#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

MODS_DIR   ?= ../../../../../../mods
FS_LIB_DIR ?= ../..
dll_build  ?= false

fsegg ?= false
ifeq "$(fsegg)" "true"
base_name = libfsegg
else
base_name = libfs
endif

include $(MODS_DIR)/makedefs.inc
include $(FS_LIB_DIR)/srcfiles.mk

include_dirs += $(MODS_DIR)/../js1.7
include_dirs += $(MODS_DIR)
include_dirs += $(FS_LIB_DIR)/include
include_dirs += $(FS_LIB_DIR)/chips

# source files
cpp_files = $(addprefix $(FS_LIB_DIR)/src/, $(FS_LIB_SOURCES))
cpp_files += $(FS_LIB_DIR)/c_wrap/fs_c_wrap.cpp

ifeq "$(base_os)" "linux"
    CFLAGS += -fPIC
endif

#-------------------------------------------------------------------------------
# Targets:
#-------------------------------------------------------------------------------

# Generic rules for a library build
include $(MODS_DIR_UNIX)/makelib.inc
include $(MODS_DIR_UNIX)/makerule.inc

# Include dependency information.
-include $(depend_files)
