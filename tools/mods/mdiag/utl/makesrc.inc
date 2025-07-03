#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

basic_release_files += mdiag/utl/utllib.py

libraries += utl
target_lib_dirs += $(MODS_OUTPUT_DIR)/$(rel_o_dir)/libutl

ifeq "$(base_os)" "linux"
    ifneq (,$(filter $(BUILD_ARCH), amd64 ppc64le))
        libraries += python pthread dl util rt m
        CLIENTPATHS += //sw/tools/libpython-3.6.3/Include/...
        CLIENTPATHS += //sw/tools/pybind11-2.2.3/...
    endif
else
	libraries += python
endif
