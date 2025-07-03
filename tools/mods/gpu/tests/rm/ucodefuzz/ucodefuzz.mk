#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

UCODEFUZZ_OUTDIR      ?= $(o_dir)/ucodefuzz

cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/ucodefuzzersub.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ucodefuzz/mqdatainterface.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ucodefuzz/ucodefuzzdriver.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ucodefuzz/rtosucodefuzzdriver.cpp

cpp_files$(ONLY_DGPU) += gpu/tests/rm/ucodefuzz/pmudriver.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ucodefuzz/sec2driver.cpp