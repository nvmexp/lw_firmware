#/*
# * LWIDIA_COPYRIGHT_BEGIN
# *
# * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
# * information contained herein is proprietary and confidential to LWPU
# * Corporation.  Any use, reproduction, or disclosure without the written
# * permission of LWPU Corporation is prohibited.
# *
# * LWIDIA_COPYRIGHT_END
# */

#-------------------------------------------------------------------------------
# Standalone MATS make file.
# See //sw/<tree>/diag/mods/makedefs.inc for more information about the MODS
# makefiles.
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# Environment variables:
#
#    COMPRESS_REL    Compress release build
#                    Defaults to "true"
#
#    DRIVERS_DIR     Drivers directory.
#                    Defaults to "../../../drivers"
#
#    MODS_DIR        MODS root directory.
#                    Defaults to "../../mods"
#
#    PMODE           Generate exelwtable to run with PMODE/DJ.
#                    Defaults to "true"
#
#    ROOT_DIR        Standalone MATS root directory.
#                    Defaults to ".".
#
#    SDK_DIR         SDK directory.
#                    Defaults to "../../../sdk"
#
#    VERSION         Software version.
#                    Defaults to "000"
#
#    LINUX_DRIVER_SRC_PATH      MODS Linux kernel driver source directory
#                               Defaults to "$(P4ROOT)/sw/mods/linuxdrv/v3.2"
#
# Targets:
#
#    all             Build the exelwtable, default target.
#    clean           Delete all object files.
#-------------------------------------------------------------------------------

DRIVERS_DIR  ?= ../../../drivers
MODS_DIR     ?= ../../mods
PMODE        ?= true
ROOT_DIR     ?= .
SDK_DIR      ?= ../../../sdk
VERSION      ?= 000
LINUX_DRIVER_SRC_PATH ?= $(P4ROOT)/sw/mods/linuxdrv/rel

dll_build = false
base_name = mats

# Default to BUILD_OS=linuxmfg on Linux
ifndef BUILD_OS
    ifeq "$(shell uname)" "Linux"
        BUILD_OS = linuxmfg
    endif
endif

include $(MODS_DIR)/makedefs.inc

# Enable UPX compression only where it is supported
ifneq (,$(filter sanity_linuxmfg_amd64 release_linuxmfg_amd64,$(BUILD_CFG)_$(BUILD_OS)_$(BUILD_ARCH)))
    COMPRESS_REL ?= true
else
    COMPRESS_REL ?= false
endif

include_dirs += $(ROOT_DIR)
include_dirs += $(ROOT_DIR)/src
include_dirs += $(MODS_DIR)
include_dirs += $(MODS_DIR)/../utils
include_dirs += $(SDK_DIR)/lwpu/inc
include_dirs += $(DRIVERS_DIR)/common/inc/swref
include_dirs += $(DRIVERS_DIR)/common/inc/hwref
include_dirs += $(DRIVERS_DIR)/resman/kernel/inc
include_dirs += $(BOOST_DIR)
include_dirs += $(DRIVERS_DIR)/common/amaplib/include
include_dirs += $(DRIVERS_DIR)/common/dram_hal/include

GEN_LIB_DIR = $(MODS_OUTPUT_DIR)/$(rel_o_dir)

install_target_dir = $(GEN_LIB_DIR)/mods

libraries += m
libraries += z
defines += -DMATS_STANDALONE
force_include_files += $(MODS_DIR)/$(base_os)/modsinc.h
force_include_files += $(MODS_DIR)/core/include/jsprotect.h

defines += -DINCLUDE_STD_NAMESPACE

libraries += matsamap matsdramhal lwdiag_utils
lib_dirs += $(GEN_LIB_DIR)/libmatsamap
lib_dirs += $(GEN_LIB_DIR)/libmatsdramhal
lib_dirs += $(MODS_DIR)/$(base_mods_lib_dir)

cpp_files =                              \
   $(MODS_DIR)/core/utility/memerror.cpp \
   $(MODS_DIR)/core/utility/memlane.cpp  \
   $(MODS_DIR)/core/utility/memtypes.cpp \
   $(MODS_DIR)/gpu/framebuf/framebuf.cpp \
   $(MODS_DIR)/gpu/framebuf/gpufb.cpp    \
   $(MODS_DIR)/gpu/framebuf/amapv1fb.cpp \
   $(MODS_DIR)/gpu/framebuf/amapv2fb.cpp \
   $(MODS_DIR)/gpu/framebuf/gf100fb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gf114fb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gk107fb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gklit1fb.cpp \
   $(MODS_DIR)/gpu/framebuf/gk208fb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gk110fb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gm10xfb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gm20xfb.cpp  \
   $(MODS_DIR)/gpu/framebuf/gmlitxadr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gmlit1adr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gmlit2adr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gf100adr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gklit1adr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gk208adr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gk107adr.cpp  \
   $(MODS_DIR)/gpu/framebuf/gklit2adr.cpp  \
   $(ROOT_DIR)/src/comnmats.cpp

ifeq "$(BUILD_OS)" "linuxmfg"
   cpp_files += $(ROOT_DIR)/src/linuxwrap.cpp
   include_dirs += $(LINUX_DRIVER_SRC_PATH)
endif

# Carrying these parameters over from the old standalone MATS makefile to keep
# the final .exe identical.
ifeq "$(BUILD_CFG)" "release"
   CXXFLAGS += -fno-exceptions
endif

ifeq "$(BUILD_CFG)" "debug"
    REDACT_STRINGS ?= false
else
    REDACT_STRINGS ?= true
endif

REDACT_RANDOMS = $(o_dir)/redact_randoms
REDACT_SCRIPT = $(MODS_DIR)/tools/redact_future_chips.sh $(REDACT_RANDOMS)

#-------------------------------------------------------------------------------
# Targets:
#-------------------------------------------------------------------------------

.PHONY: all clean

# The default target makes the MATS exelwtable (mats.exe).
all: build

# If there are generated header files we have to do a relwrsive make
# to enforce the ordering constraint that all generated headers (and prerequisites)
# must be built before any source files.
# NOTE: gen_h_files target must be run in a sub-make to ensure
#       the variable $(gen_h_files) is correct.
build:
	@echo "*********************************************************************"
	@echo "* Building $(BUILD_CFG) version of $(exe_name)."
	@echo "*********************************************************************"
	$(Q)$(MAKE) --no-print-directory gen_h_files
	$(Q)$(MAKE) --no-print-directory $(o_dir)/$(exe_name)
	@echo "Built $(BUILD_CFG) version of $(exe_name)."

LWCFG_ROOT      ?= $(DRIVERS_DIR)/common/build
LWCFG_OUTPUTDIR ?= $(gen_cpp_dir)
include $(LWCFG_ROOT)/lwconfig.lwmk
include_dirs += $(LWCFG_OUTPUTDIR)

REGDUMP_GEN_DIR = $(gen_cpp_dir)
include $(MODS_DIR_UNIX)/gpu/reghal/makesrc.inc

include $(MODS_DIR_UNIX)/makerule.inc

MATSVER = $(o_dir)/matsver.h
include_dirs += $(o_dir)

dep_libs  = matsamap
dep_libs += matsdramhal
dep_libs += lwdiag_utils

lib_dirs += $(GEN_LIB_DIR)/lwmake/diag/utils

$(o_dir)/$(exe_name) : $(MATSVER) $(o_files) $(addprefix submake.,$(addsuffix .build,$(dep_libs)))
	@$(ECHO) "Linking object files and libraries: $(libraries)"
	$(Q)$(LD) $(LDFLAGS_EXE) $(o_files) $(link_libs)
ifeq "$(BUILD_CFG)" "release"
    # Redact exelwtable before compression
    ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) $@
    endif
endif
ifeq "$(COMPRESS_REL)" "true"
	@$(ECHO) "Compressing exelwtable."
	$(Q)$(BUILD_TOOLS_DIR)/linux/upx-3.94-amd64_linux/upx -9 -qqq $@
endif

install:
	@$(ECHO) "Copying $(o_dir)/$(exe_name) to $(install_target_dir)..."
	$(Q)$(MKDIR_P) $(install_target_dir)
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $(o_dir)/$(exe_name) $(install_target_dir)/$(exe_name)

uninstall:
	@$(ECHO) "Deleting installed files."
	$(Q)-$(RM) -f $(install_target_dir)/$(exe_name)

submake.matsamap.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/common/amaplib/contrib_sw/mods STANDALONE_MATS="true" dll_build="false" $*

submake.matsdramhal.%:
	$(Q)$(MAKE) -C $(DRIVERS_DIR)/common/dram_hal/contrib_sw/mods STANDALONE_MATS="true" dll_build="false" $*

submake.lwdiag_utils.%:
	$(Q)$(MAKE) -C $(MODS_DIR)/../utils -f makefile.lwmk $(subst submake.lwdiag_utils.,,$@) $(LWMAKE_ARGS) $*

build_install: build
	@$(ECHO) "Copying $(o_dir)/$(exe_name) to $(install_target_dir)..."
	$(Q)$(MKDIR_P) $(install_target_dir)
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $(o_dir)/$(exe_name) $(install_target_dir)/$(exe_name)

clobber: clean submake.matsamap.clobber submake.matsdramhal.clobber submake.lwdiag_utils.clobber

$(MATSVER) : $(o_dir)/dummy.txt
	@echo "Creating matsver.h."
	@echo "//                                 " >  $(MATSVER)
	@echo "// This file is automatically      " >> $(MATSVER)
	@echo "// generated by modscore.mak.      " >> $(MATSVER)
	@echo "//                                 " >> $(MATSVER)
	@echo "#define PRODUCT   \"$(base_name)\" " >> $(MATSVER)
	@echo "#define VERSION   \"$(VERSION)\"   " >> $(MATSVER)
	@echo "#define BUILD_CFG \"$(BUILD_CFG)\" " >> $(MATSVER)
	@echo "#define EXE_NAME  \"$(exe_name)\"  " >> $(MATSVER)

clean: submake.matsamap.clean submake.matsdramhal.clean
	@echo "Deleting object, lib, and dependency files."
	$(Q)-rm -rf $(gen_cpp_dir)
	$(Q)-rm -rf $(o_dir)
	$(Q)-rm -f *.d

# Include dependency information.
-include $(depend_files)

