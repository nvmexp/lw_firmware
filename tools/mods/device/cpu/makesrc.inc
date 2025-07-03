#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#-------------------------------------------------------------------------------
#
# Tests
#
#-------------------------------------------------------------------------------

ifeq "$(BUILD_ARCH)" "amd64"
    cpp_files += device/cpu/tests/cpustress/cpustress_amd64.cpp
    ifeq "$(toolchain)" "msvc"
        $(o_dir)/cpustress_amd64$(obj_suffix): CFLAGS += /arch:AVX
    else
        $(o_dir)/cpustress_amd64$(obj_suffix): CFLAGS += -mavx2 -mfma
    endif
else
ifeq "$(BUILD_ARCH)" "aarch64"
    cpp_files += device/cpu/tests/cpustress/cpustress_aarch64.cpp
else
    cpp_files += device/cpu/tests/cpustress/cpustress_stub.cpp
endif
endif

#-------------------------------------------------------------------------------
#
# Utility
#
#-------------------------------------------------------------------------------

ifeq "$(BUILD_OS)_$(BUILD_ARCH)" "linuxmfg_amd64"
    cpp_files += device/cpu/utility/cpusensors_amd64.cpp
else
    cpp_files += device/cpu/utility/cpusensors_stub.cpp
endif
cpp_files     += device/cpu/utility/js_cpusensors.cpp
