#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

REGHAL_GZIP ?= true
# Disable gzip in builds where UPX is supported, because gzip causes
# size of exe to increase when UPX is in use.
ifeq "$(COMPRESS_REL)" "true"
    REGHAL_GZIP = false
endif

reghal_gen_dir  = $(GEN_LIB_DIR)/reghal
reghal_dump_dir = $(GEN_LIB_DIR)/lwmake/diag/mods/gpu/reghal/dump
reghal_dump_exe = $(reghal_dump_dir)/reghaldump
ifeq ($(HOST_OS),win32)
    reghal_dump_exe := $(reghal_dump_exe).exe
endif
reghal_tables_h = $(reghal_gen_dir)/reghal_tables.h

reghal_src  = $(MODS_DIR_UNIX)/tools/genreghalimpl.py
reghal_src += $(MODS_DIR_UNIX)/tools/jsreghaltemplates.py
reghal_src += $(MODS_DIR_UNIX)/gpu/reghal/reghal.def
reghal_src += $(LWCFG_HEADER)
reghal_src += $(MODS_DIR_UNIX)/gpu/reghal/makesrc.inc

$(reghal_gen_dir):
	$(Q)$(MKDIR_P) $@

$(reghal_gen_dir)/reghal_makesrc.inc: $(reghal_src) | $(reghal_gen_dir)
	@$(ECHO) Creating $(notdir $@)
	$(Q)$(PYTHON3) $(MODS_DIR)/tools/genreghalimpl.py $(MODS_DIR) $(DRIVERS_DIR) $@

-include $(reghal_gen_dir)/reghal_makesrc.inc

$(reghal_gen_dir)/reghal_chip_sources.inc: $(reghal_gen_dir)/reghal_makesrc.inc | $(reghal_gen_dir)
	$(file >$(reghal_gen_dir)/reghal_chip_sources.inc,reghal_sources = $(chip_gen_reg_hal_files))

# When compiling standalone mats, pass lwconfig dir from mats_gen dir, because mods_gen dir doesn't exist yet
REGDUMP_ARGS =
ifdef REGDUMP_GEN_DIR
    REGDUMP_ARGS += MODS_GEN_DIR=$(REGDUMP_GEN_DIR)
endif

$(reghal_dump_exe): $(reghal_gen_dir)/reghal_chip_sources.inc $(LWCFG_HEADER) $(MODS_DIR_UNIX)/gpu/reghal/dump/reghaldump.cpp $(addprefix $(reghal_gen_dir)/,$(chip_gen_reg_hal_files))
	$(Q)$(MAKE) -C $(MODS_DIR_UNIX)/gpu/reghal/dump -f makefile.lwmk $(LWMAKE_ARGS) $(REGDUMP_ARGS)

$(reghal_tables_h): $(reghal_dump_exe)
	$(Q)$^ $@ --gzip=$(REGHAL_GZIP)

clean: reghal.clean

reghal.clean:
	$(Q)$(MAKE) -C $(MODS_DIR_UNIX)/gpu/reghal/dump -f makefile.lwmk $(LWMAKE_ARGS) clean
	$(Q)-$(RM_RF) $(reghal_gen_dir)

gen_reg_hal_files += mods_reg_hal_regs.cpp
gen_reg_hal_files += mods_reg_hal_fields.cpp
gen_reg_hal_files += mods_reg_hal_values.cpp

define GEN_REG_HAL
$(reghal_gen_dir)/$1: $(reghal_src) $(reghal_gen_dir)/mods_reg_hal.h | $(reghal_gen_dir)
	@$(ECHO) Generating $$(notdir $$@)
	$(Q)$(PYTHON3) $(MODS_DIR)/tools/genreghalimpl.py -d $(o_dir)/$1.d $(MODS_DIR) $(DRIVERS_DIR) $$@
    depend_files += $(o_dir)/$1.d
endef

$(foreach reg_hal,$(gen_reg_hal_files) $(chip_gen_reg_hal_files),$(eval $(call GEN_REG_HAL,$(reg_hal))))

gen_h_files += $(reghal_gen_dir)/mods_reg_hal.h
include_dirs += $(reghal_gen_dir)

$(reghal_gen_dir)/mods_reg_hal.h: $(reghal_src) | $(reghal_gen_dir)
	@$(ECHO) Creating mods_reg_hal.h
	$(Q)$(PYTHON3) $(MODS_DIR)/tools/genreghalimpl.py $(MODS_DIR) $(DRIVERS_DIR) $@

gen_js_files += $(o_dir)/reghalconst.js
basic_release_files += $(o_dir)/reghalconst.js

$(o_dir)/reghalconst.js: $(reghal_src) $(MODS_DIR_UNIX)/gpu/reghal/allowlist.def | $(reghal_gen_dir)
	@$(ECHO) Creating reghalconst.js
	$(Q)$(PYTHON3) $(MODS_DIR)/tools/genreghalimpl.py $(MODS_DIR) $(DRIVERS_DIR) $@

gen_js_files += $(o_dir)/reghalinternal.js
internal_release_files += $(o_dir)/reghalinternal.js

$(o_dir)/reghalinternal.js: $(reghal_src) | $(reghal_gen_dir)
	@$(ECHO) Creating reghalinternal.js
	$(Q)$(PYTHON3) $(MODS_DIR)/tools/genreghalimpl.py $(MODS_DIR) $(DRIVERS_DIR) $@

.PHONY: reghal
reghal: $(reghal_gen_dir)/mods_reg_hal.h $(reghal_gen_dir)/reghal_makesrc.inc $(o_dir)/reghalconst.js
reghal: $(o_dir)/reghalinternal.js $(reghal_tables_h) $(reghal_gen_dir)/reghal_chip_sources.inc
reghal: $(reghal_dump_exe)

ifndef SKIP_REGHAL_SRC
    gen_cpp_files   += $(addprefix $(reghal_gen_dir)/,$(gen_reg_hal_files))
    cpp_files       += $(addprefix $(reghal_gen_dir)/,$(gen_reg_hal_files))
    cpp_files       += $(MODS_DIR)/gpu/reghal/reghaltable.cpp
    cpp_files       += $(MODS_DIR)/gpu/reghal/reghal.cpp
    ifneq "$(base_name)" "mats"
        cpp_files       += $(MODS_DIR)/gpu/reghal/js_reghal.cpp
        cpp_files       += $(MODS_DIR)/gpu/reghal/smcreghal.cpp
        $(o_dir)/$(exe_name): $(addprefix $(o_dir)/,$(gen_reg_hal_files:.cpp=$(obj_suffix)))
    endif

    $(o_dir)/reghaltable$(obj_suffix): $(reghal_tables_h)
endif

# vim:ft=make
