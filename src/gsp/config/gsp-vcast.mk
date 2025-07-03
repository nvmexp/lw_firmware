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

# Files to exclude based on gsp/src folder
VCAST_EXCLUDE_FILES      :=
VCAST_EXCLUDE_FILES      += kernel/start.S
VCAST_EXCLUDE_FILES      += vcast/c_cover_io_wrapper.c
VCAST_EXCLUDE_FILES      += %-mock.c

# Base folder for VCAST_FOLDERS param is gsp/src
# Eg. VCAST_FOLDERS="perf/lw fan/lw/fan_rpc.c"
ifdef VCAST_FOLDERS
    VCAST_TEMP_FILES := $(VCAST_FOLDERS:=/%.c)
    VCAST_INCLUDE_FILES := $(VCAST_TEMP_FILES:.c/\%.c=.c)
else
    VCAST_INCLUDE_FILES := $(SOURCES)
endif

#VCAST_SRC_DIR := $(LWUPROC)/gsp/src/
include $(LWUPROC)/vcast/vcast.mk

VCAST_FILES := $(filter $(VCAST_INCLUDE_FILES), $(SOURCES))
VCAST_FILES := $(filter-out $(VCAST_EXCLUDE_FILES), $(VCAST_FILES))
ifdef VCAST_EXCLUDE
    VCAST_FILES := $(filter-out $(VCAST_EXCLUDE), $(VCAST_FILES))
endif


.PHONY: GSPVC_INIT GSPVC_ADD_SOURCE GSPVC_INSTRUMENT GSPVC_CLOBBER

build.submake.MUTEX: GSPVC_INIT $(LIB_UPROC_LWOS)

build.submake.LIB_UPROC_LWOS: GSPVC_INIT

GSPVC_INIT:
	$(VCAST_GENERATE_CCAST_CMD)
	$(VCAST_CREATE_PROJ_CMD)
	$(VCAST_INIT_PROJ_CMD)
	$(COPY) $(VCEXTRACT_PATH)/expand_binary_coverage.py                    \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/expand_binary_coverage.py
	$(COPY) $(VCEXTRACT_PATH)/exelwte_bin_coverage.bat                     \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/exelwte_bin_coverage.bat
	$(COPY) $(VCEXTRACT_PATH)/exelwte_bin_coverage.sh                      \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/exelwte_bin_coverage.sh
	$(COPY) $(VCEXTRACT_PATH)/vcast_c_options_preprocess.py                \
        $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw/vcast_c_options_preprocess.py

VC_ADD_FILES: GSPVC_INIT 

GSPVC_ADD_SOURCE: GSPVC_INIT $(LIB_UPROC_LWOS) $(LIB_MUTEX)
	$(VCAST_GENERATE_CCAST_CMD)
	$(P4) edit $(addprefix $(GSP_SW)/src/,                                 \
        $(VCAST_FILES))
	$(VCAST_ADD_SOURCE_CMD) $(VCAST_FILES)

GSPVC_INSTRUMENT: GSPVC_ADD_SOURCE
	$(VCAST_INSTRUMENT_CMD) "$(VCAST_FILES)"

ifneq (,$(filter $(VCAST_INSTRUMENT), add_source))
    $(OBJECTS): GSPVC_ADD_SOURCE
else ifneq (,$(filter $(VCAST_INSTRUMENT), instrument))
    SOURCES += vcast/c_cover_io_wrapper.c
    $(OBJECTS): GSPVC_INSTRUMENT
endif

GSPVC_CLOBBER:
ifneq ($(wildcard $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw.vcp),)
	$(VCAST_CLOBBER_CMD)
	-$(RMDIR) -rf $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw
	-$(RM) $(VCAST_ELW_BASE_DIRECTORY)/vcast_elw.vcp
	-$(RM) $(VCAST_ELW_BASE_DIRECTORY)/CCAST_.CFG
endif

clean: GSPVC_CLOBBER

clobber: GSPVC_CLOBBER
