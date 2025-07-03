#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2004-2018,2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

MODS_DIR ?= ../..

dll_build = true
base_name = libdrvexport

ifeq "$(INCLUDE_MDIAG)" "true"
    defines += -DINCLUDE_MDIAG=1
endif

include $(MODS_DIR)/makedefs.inc

DRIVERS_DIR       ?= $(MODS_DIR)/../../drivers

# For modsdrv.h
include_dirs += $(MODS_DIR)
include_dirs += $(MODS_DIR)/../../sdk/lwpu/inc

# For jsapi.h etc.
include_dirs += $(MODS_DIR)/../js1.7

ifeq "$(use_mods_lwlink)" "true"
    include_dirs += $(DRIVERS_DIR)/lwlink/interface
    include_dirs += $(DRIVERS_DIR)/common/inc
    include_dirs += $(DRIVERS_DIR)/lwlink/user/lwlink
endif
    
ifeq "$(use_mods_lwswitch)" "true"
    include_dirs += $(DRIVERS_DIR)/lwswitch/interface
    include_dirs += $(DRIVERS_DIR)/lwswitch/user
endif

ifeq "$(da_build)" "true"
    EXPORT_RM_API ?= true
endif
ifeq "$(BUILD_OS)" "linux"
    EXPORT_RM_API ?= true
endif
EXPORT_RM_API ?= false

# Source files
c_files = drvexport.c


ifeq "$(use_mods_lwlink)" "true"
    c_files += lwlinkexport.c
endif
ifeq "$(use_mods_lwswitch)" "true"
    c_files += lwswitchexport.c
endif
ifeq "$(EXPORT_RM_API)" "true"
    c_files += fakermexport.c
endif

ifeq "$(base_os)"  "linux"
   ifeq "$(BUILD_ARCH)" "amd64"
      CFLAGS += -fPIC
   endif
endif

include $(MODS_DIR_UNIX)/makerule.inc
include $(MODS_DIR_UNIX)/makelib.inc

# Generate the lwConfig data: LWCFG_* makefile vars and C #defines in g_lwconfig.h
# Note: We cannot include lwconfig.lwmk before makelib.inc in this makefile because the latter
#        defines the overall build target -- and that has to be the 1st target in the makefile

LWCFG_ROOT      = ../../../../drivers/common/build
LWCFG_OUTPUTDIR = $(gen_cpp_dir)
include $(LWCFG_ROOT)/lwconfig.lwmk
include_dirs += $(LWCFG_OUTPUTDIR)

# include dependency information
-include $(depend_files)

