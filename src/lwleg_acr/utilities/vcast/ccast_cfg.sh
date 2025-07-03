#!/bin/bash
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019 by LWPU Corporation. All rights reserved. All
# information contained herein is proprietary and confidential to LWPU
# Corporation. Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

CCFG=$CCAST_PATH/CCAST_.CFG
 
echo "COVERAGE_IO_TYPE: VCAST_COVERAGE_IO_BUFFERED" > $CCFG
echo "C_COMPILER_CCFG_SOURCE: CONFIG_FILE_63" >> $CCFG
echo "C_COMPILE_CMD: $FALCON_COMPILER -c" >> $CCFG
echo "C_DEFINE_LIST: $C_DEFINE_LIST" >> $CCFG
echo "C_EDG_FLAGS: -w --gcc --gnu_version 40302" >> $CCFG
echo "C_PREPROCESS_CMD: $FALCON_COMPILER -E -C" >> $CCFG
echo "VCAST_COVERAGE_FOR_HEADERS: FALSE" >> $CCFG
echo "VCAST_COVERAGE_COLLAPSE_ALL: TRUE" >> $CCFG
echo "VCAST_COVER_STATEMENTS_BY_BLOCK: TRUE" >> $CCFG
echo "VCAST_COVERAGE_POINTS_AS_MACROS: TRUE" >> $CCFG
echo "VCAST_DISPLAY_UNINST_EXPR: FALSE" >> $CCFG
echo "VCAST_ENABLE_FUNCTION_CALL_COVERAGE: FALSE" >> $CCFG
echo "VCAST_RPTS_DEFAULT_FONT_FACE: Arial(3)" >> $CCFG
echo "VCAST_USE_EDG_PREPROCESSOR: TRUE" >> $CCFG

for i in $TESTABLE_SOURCE_DIRS; do
  echo "TESTABLE_SOURCE_DIR: $i" >> $CCFG
done
echo "TESTABLE_SOURCE_DIR: $FALCON_TOOLS/lib/gcc/falcon-elf/4.3.2/include" >> $CCFG
