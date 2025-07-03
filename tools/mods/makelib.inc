#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2015 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

LIB_INSTALL_DIR = $(MODS_OUTPUT_DIR_UNIX)/$(rel_o_dir)/libs

ifeq "$(base_os)" "win32"
    # Add mods.lib as a something that this library should link against if
    # we are using msvc.  Otherwise anything that calls ModsDrv will not link
    lib_dirs += $(LIB_INSTALL_DIR) $(MODS_DIR)/$(mods_lib_dir) $(MODS_DIR)/$(base_mods_lib_dir)

    MODS_LIB_FILENAME = mods.lib
    ifeq "$(SERVER_MODE)" "true"
        ifeq "$(dll_build)" "true"
            MODS_LIB_FILENAME = mods_dll.lib
        endif
    endif
    extra_libs += $(MODS_LIB_FILENAME)
endif

.PHONY: build clean clobber install install_implib uninstall build_install

build_prereqs := $(gen_h_files) $(lib_prereqs)

ifeq "$(strip $(build_prereqs))" ""
    build: $(o_dir)/$(lib_name)
else
    ifneq "$(BUILD_ARCHS)" ""
        build: $(o_dir)/$(lib_name)
    else
        # If there are generated header files or prerequisite builds for the
        # target lib, we have to do a relwrsive make to enforce the ordering
        # constraint that all generated headers (and prerequisites) must be
        # built before any source files.
        # NOTE: gen_h_files target must be run in a sub-make to ensure
        #       the variable $(gen_h_files) is correct.
        build:
	        $(Q)$(MAKE) --no-print-directory gen_h_files lib_prereqs
	        $(Q)$(MAKE) --no-print-directory $(o_dir)/$(lib_name)
    endif
endif

# Remove the entire object directory instead of trying to cherry pick file
# extensions (which change occasionally resulting in incomplete cleaning)
clean: $(SUBMAKE_FAT_ARCHS:%=%.clean)
	@$(ECHO) "Deleting object, lib, and dependency files."
	$(Q)-$(RM) -f $(all_gen_files)
	$(Q)-$(RM_RF) $(gen_cpp_dir)
	$(Q)-$(RM_RF) $(o_dir)
	$(Q)-$(RM) -f *.d

submake.fat.%.clean:
	@$(MAKE) BUILD_ARCH=$* BUILD_ARCHS="" clean

clobber: clean uninstall

ifdef CYGWIN_MAKE
    CHMOD_LIB = $(Q)chmod 755 $1
else ifdef MINGW_MAKE
    CHMOD_LIB =
else
    CHMOD_LIB = $(Q)chmod 644 $1
endif

install:
ifeq "$(install_library)" "true"
	@$(ECHO) "Copying $(o_dir)/$(lib_name) to $(install_target_dir)..."
	$(Q)$(MKDIR_P) $(install_target_dir)
	$(call CHMOD_LIB, $(o_dir)/$(lib_name))
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $(o_dir)/$(lib_name) $(install_target_dir)/$(lib_name)
    ifeq "$(dll_build)" "true"
        ifeq "$(dll_suffix)" ".so"
	$(Q)-$(RM) -f $(LIB_INSTALL_DIR)/$(base_name)$(lib_suffix)
        endif
    endif
endif

install_implib:
ifeq "$(install_library)" "true"
	@$(ECHO) "Copying $(o_dir)/$(implib_name) to $(LIB_INSTALL_DIR)/$(implib_name)..."
	$(call CHMOD_LIB, $(o_dir)/$(lib_name))
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $(o_dir)/$(implib_name) $(LIB_INSTALL_DIR)/$(implib_name)
endif

uninstall:
ifeq "$(install_library)" "true"
	@$(ECHO) "Deleting installed files."
	$(Q)-$(RM) -f $(install_target_dir)/$(lib_name)
endif

build_install: build
ifeq "$(install_library)" "true"
	@$(ECHO) "Copying $(o_dir)/$(lib_name) to $(install_target_dir)..."
	$(Q)$(MKDIR_P) $(install_target_dir)
	$(call CHMOD_LIB, $(o_dir)/$(lib_name))
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $(o_dir)/$(lib_name) $(install_target_dir)/$(lib_name)
    ifeq "$(dll_build)" "true"
        ifeq "$(dll_suffix)" ".so"
	$(Q)-$(RM) -f $(LIB_INSTALL_DIR)/$(base_name)$(lib_suffix)
        endif
    endif
endif
