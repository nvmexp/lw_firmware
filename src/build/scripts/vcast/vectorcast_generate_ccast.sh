#!/bin/bash

# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

# Put header to CCAST_.CFG
echo "COVERAGE_IO_TYPE: VCAST_COVERAGE_IO_BUFFERED"> $PWD/CCAST_.CFG
if [ $1 == riscv ]
then
echo "C_COMPILER_CFG_SOURCE: PY_CONFIGURATOR">> $PWD/CCAST_.CFG
else
echo "C_COMPILER_CFG_SOURCE: CONFIG_FILE_63">> $PWD/CCAST_.CFG
fi
echo "C_COMPILE_CMD: $CC -c">> $PWD/CCAST_.CFG
echo "C_DEFINE_LIST: $C_DEFINE_LIST">> $PWD/CCAST_.CFG
echo "VCAST_PREPROCESS_PREINCLUDE: $VCAST_PREPROCESS_PREINCLUDE">> $PWD/CCAST_.CFG
if [ $1 == riscv ]
then
echo "C_EDG_FLAGS: -w --gcc --gnu_version 80200">> $PWD/CCAST_.CFG
else
echo "C_EDG_FLAGS: -w --gcc --gnu_version 40302">> $PWD/CCAST_.CFG
fi
echo "C_PREPROCESS_CMD: $CC -E -CC">> $PWD/CCAST_.CFG
echo "VCAST_INSTRUMENT_UNREACHABLE_CODE: FALSE">> $PWD/CCAST_.CFG
echo "VCAST_ENABLE_DATA_CLEAR_API: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_COVERAGE_FOR_HEADERS: FALSE">> $PWD/CCAST_.CFG
echo "VCAST_COVER_STATEMENTS_BY_BLOCK: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_DISPLAY_UNINST_EXPR: FALSE">> $PWD/CCAST_.CFG
echo "VCAST_RPTS_DEFAULT_FONT_FACE: Arial(3)">> $PWD/CCAST_.CFG
echo "VCAST_USE_EDG_PREPROCESSOR: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_USE_BUFFERED_ASCII_DATA: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_COVERAGE_FOR_DECLARATIONS: FALSE">> $PWD/CCAST_.CFG
echo "VCAST_USE_STATIC_MEMORY: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_COVERAGE_POINTS_AS_MACROS: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_HANDLE_TERNARY_OPERATOR: FALSE">> $PWD/CCAST_.CFG
echo "VCAST_COVERAGE_COLLAPSE_ALL: TRUE">> $PWD/CCAST_.CFG
echo "VCAST_NO_FLOAT: TRUE">> $PWD/CCAST_.CFG

# Put TESTABLE_SOURCE_DIR list
printf 'TESTABLE_SOURCE_DIR: %s\n' $TESTABLE_SOURCE_DIRS>> $PWD/CCAST_.CFG
