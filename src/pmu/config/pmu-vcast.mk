#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

include $(LWUPROC)/vcast/vcast.mk

# Files to exclude based on pmu_sw/prod_app folder
VCAST_EXCLUDE_FILES      :=
VCAST_EXCLUDE_FILES      += vcast/c_cover_io_wrapper.c
VCAST_EXCLUDE_FILES      += cmdmgmt/lw/cmdmgmt_vc.c
VCAST_EXCLUDE_FILES      += pmu/turing/pmu_pmutu10x.c
VCAST_EXCLUDE_FILES      += %-mock.c
VCAST_EXCLUDE_FILES      += _out/ga10x_riscv/config/g_sections_data.c
VCAST_EXCLUDE_FILES      += pmu
VCAST_EXCLUDE_FILES      += ic
VCAST_EXCLUDE_FILES      += lsf/maxwell/pmu_lsfgm20x.c
VCAST_EXCLUDE_FILES      += os
VCAST_EXCLUDE_FILES      += vbios/ampere/vbios_frts_ga10x.c

# Base folder for VCAST_FOLDERS param is pmu_sw/prod_app
# Eg. VCAST_FOLDERS="perf/lw fan/lw/fan_rpc.c"
ifdef VCAST_FOLDERS
    VCAST_TEMP_FILES := $(VCAST_FOLDERS:=/%.c)
    VCAST_INCLUDE_FILES := $(VCAST_TEMP_FILES:.c/\%.c=.c)
else
    VCAST_INCLUDE_FILES := $(SOURCES)
endif

VCAST_FILES := $(filter $(VCAST_INCLUDE_FILES), $(SOURCES))
VCAST_EXCLUDE_TEMP_FILES    := $(VCAST_EXCLUDE_FILES:=/%.c)
VCAST_EXCLUDE_LIST          := $(VCAST_EXCLUDE_TEMP_FILES:.c/\%.c=.c)
ifdef VCAST_EXCLUDE
    VCAST_EXCLUDE_TEMP  := $(VCAST_EXCLUDE:=/%.c)
    VCAST_EXCLUDE_LIST  += $(VCAST_EXCLUDE_TEMP:.c/\%.c=.c)
endif
VCAST_EXCLUDE_LIST      += pmu/falcon/start.S
VCAST_EXCLUDE_LIST      += pmu/riscv/start.S
VCAST_FILES := $(filter-out $(VCAST_EXCLUDE_LIST), $(VCAST_FILES))

VCAST_PREPROCESS := $(PERL) $(PMU_SW)/build/vcast_preprocess.pl
VCAST_POSTPROCESS := $(PERL) $(PMU_SW)/build/vcast_postprocess.pl

.PHONY: PMUVC_INIT PMUVC_ADD_SOURCE PMUVC_INSTRUMENT PMUVC_CLOBBER VCAST_ANALYZE

PMUVC_INIT:
	$(VCAST_GENERATE_CCAST_CMD)
	$(VCAST_CREATE_PROJ_CMD)
	$(VCAST_INIT_PROJ_CMD)
	$(COPY) $(VCEXTRACT_PATH)/expand_binary_coverage.py                        \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/expand_binary_coverage.py
	$(COPY) $(VCEXTRACT_PATH)/exelwte_bin_coverage.bat                         \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/exelwte_bin_coverage.bat
	$(COPY) $(VCEXTRACT_PATH)/exelwte_bin_coverage.sh                          \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/exelwte_bin_coverage.sh
	$(COPY) $(VCEXTRACT_PATH)/vcast_c_options_preprocess.py                    \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/vcast_c_options_preprocess.py
    
define max_args
  $(eval _args:=)
  $(foreach obj,$3,$(eval _args+=$(obj))$(if $(word $2,$(_args)),$1$(_args)$(EOL)$(eval _args:=)))
  $(if $(_args),$1$(_args))
endef
define EOL


endef

PMUVC_ADD_SOURCE: PMUVC_INIT $(LIB_UPROC_LWOS) $(LIB_MUTEX)
	$(VCAST_GENERATE_CCAST_CMD)
ifneq ($(LW_DVS_BLD),1)
	$(P4) edit $(addprefix ../prod_app/,                                \
        $(VCAST_FILES))
endif
	$(VCAST_PREPROCESS) --fileList $(addprefix ../prod_app/,            \
        $(VCAST_FILES)) --buildExcludeList $(IMG_PREFIX) $(LW_HOST_OS)  \
        --lwTools $(LW_TOOLS)                                           \
        --lwRoot $(LW_SOURCE)
	@$(call max_args, $(VCAST_ADD_SOURCE_CMD), 100, $(VCAST_FILES))

VCAST_ANALYZE: $(ANALYZE_DEPS) $(FLCN_SCRIPT_DEPS)
	$(ECHO) $(BUILD_PROJECT_NAME) analyzing objdump for VECTORCAST
	$(MKDIR) _out/vcast/_analyzed
	$(PERL) -I$(PMU_BUILD) -I$(BUILD_SCRIPTS) \
         -I$(LW_SOURCE)/drivers/common/chip-config                        \
         $(BUILD_SCRIPTS)/rtos-flcn-script.pl --profile $(PMUCFG_PROFILE) \
         --arch $(LW_TARGET_OS) --lwroot $(LW_SOURCE) --perl $(PERL)      \
         --verbose $(LW_VERBOSE) --analyze-objdump $(RELEASE_PATH)/$(IMG_PREFIX).objdump \
         --outfile _out/vcast/_analyzed $(ANALYZE_OVL_ARGS)

PMUVC_INSTRUMENT: PMUVC_ADD_SOURCE VCAST_ANALYZE
	$(VCAST_INSTRUMENT_ALL_CMD)
	$(VCAST_POSTPROCESS) --analysisPath _out/vcast/_analyzed \
        --imgPrefix $(IMG_PREFIX)       \
        --riscvTools $(LWRISCV_TOOLS)   \
        --lwRoot $(LW_SOURCE)

ifneq (,$(filter $(VCAST_INSTRUMENT), add_source))
    $(OBJECTS): PMUVC_ADD_SOURCE
else ifneq (,$(filter $(VCAST_INSTRUMENT), instrument))
    SOURCES += vcast/c_cover_io_wrapper.c
    $(OBJECTS): PMUVC_INSTRUMENT
endif

PMUVC_CLOBBER:
ifneq ($(wildcard $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw.vcp),)
	$(VCAST_CLOBBER_CMD)
	-$(RMDIR) -rf $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw
	-$(RM) $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw.vcp
	-$(RM) $(VCAST_ELW_BASE_DIRECTORY)/CCAST_.CFG
	-$(RMDIR) -rf $(VCAST_ELW_BASE_DIRECTORY)/_out/vcast
endif
	$(VCAST_PREPROCESS) --clean "$(PMU_SW)/prod_app"

clean: PMUVC_CLOBBER

clobber: PMUVC_CLOBBER
