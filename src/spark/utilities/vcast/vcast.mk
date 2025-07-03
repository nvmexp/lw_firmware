#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

ifndef P4ROOT 
    $(error P4ROOT is not defined)
endif

ifndef VECTORCAST_DIR
    $(error VECTORCAST_DIR is not defined)
endif

VCAST_COVERAGE_TYPE       = ASIL_B/C

TESTABLE_SOURCE_DIR := $(SOURCE_DIRS)
export TESTABLE_SOURCE_DIR

# Commands
VCAST_GENERATE_CCAST_CMD := cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
       $(LW_SOURCE)uproc/Spark/utilities/vcast/generate_ccast.bat

VCAST_CREATE_PROJ_CMD    := cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast cover elw create vcast_elw

VCAST_INIT_PROJ_CMD      := cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover options in_place y

VCAST_ADD_SOURCE_CMD     += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
$(VECTORCAST_DIR)/clicast -e vcast_elw cover source relwrsive_add

VCAST_INSTRUMENT_CMD     += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
$(VECTORCAST_DIR)/clicast -e vcast_elw cover instrument $(VCAST_COVERAGE_TYPE) \

VCAST_CLOBBER_CMD        += cd $(VCAST_ELW_BASE_DIRECTORY) &&                  \
    $(VECTORCAST_DIR)/clicast -e vcast_elw cover instrument none
