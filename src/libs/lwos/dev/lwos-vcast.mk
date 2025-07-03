#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

ifdef VCAST_INSTRUMENT
ifeq (pmu-tu10x, $(PMUCFG_PROFILE))

include $(LW_SOURCE)/uproc/vcast/vcast.mk

    VCAST_EXCLUDE_FILES :=
    VCAST_EXCLUDE_FILES += src/lwostrace.c
    VCAST_EXCLUDE_FILES += %-mock.c
    FILTERED_SOURCES    := $(filter-out $(VCAST_EXCLUDE_FILES), $(SOURCES))
    
    ifneq (,$(filter $(VCAST_INSTRUMENT), add_source))
        $(OBJECTS): LWOSVC_ADD_FILES
    else ifneq (,$(filter $(VCAST_INSTRUMENT), instrument))
        $(OBJECTS): LWOSVC_INSTRUMENT
    endif
    
    LWOSVC_ADD_FILES:
	    $(P4) edit                                                                 \
	        $(addprefix $(LW_SOURCE)/uproc/libs/lwos/dev/, $(FILTERED_SOURCES))
	    $(VCAST_ADD_SOURCE_CMD)                                                    \
	        $(addprefix $(LW_SOURCE)/uproc/libs/lwos/dev/, $(FILTERED_SOURCES))

    LWOSVC_INSTRUMENT: LWOSVC_ADD_FILES
	    $(VCAST_INSTRUMENT_CMD)                                                    \
	        "$(addprefix $(LW_SOURCE)/uproc/libs/lwos/dev/, $(FILTERED_SOURCES))"
endif
endif
