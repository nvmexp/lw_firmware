#!/bin/bash

export COVERITY_HOME=$LW_TOOLS/Coverity/2018.12/Linux64
export COVERITY_OUT=$COVERITY_HOME/idirs/libos-kernel

export FORMAT_OUT=cov_format
export ANALYZE_OUT=cov_analyze

export COMPILER=$LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-unknown-elf-gcc

cd $LIBOS_SOURCE/kernel

rm -rf ${COVERITY_OUT}

$COVERITY_HOME/bin/cov-configure -co $LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-gcc --  -mcmodel=medlow -march=rv64im -mabi=lp64 -ffreestanding --std=gnu11

if [ $? -ne 0 ]
then
   echo "***** cov-configure failed! *****"
   exit 1
fi

#$COVERITY_HOME/bin/cov-configure \
#   --compiler gcc \
#   --comptype gcc \
#   --template

make clean

$COVERITY_HOME/bin/cov-build \
   --dir $COVERITY_OUT \
   --add-arg \
   --c99 \
   --delete-stale-tus \
   --return-emit-failures \
   --parse-error-threshold 100 \
   make 2>&1 | tee /tmp/cov.out

#
# cov-build status bits:
#  1 : build returned an error code
#  2 : build terminated uexpectedly
#  4 : no files emitted (i.e. nothing to do)
#  8 : some files failed to compile
# 16 : command line error
#
# If we get a status=4 then we stop processing
#
COV_BUILD_STATUS=${PIPESTATUS[0]}

if [ ${COV_BUILD_STATUS} -eq 4 ]
then
    echo "***** cov-build emitted no files *****"
    echo "***** nothing to do!!! *****"
    exit 0
fi

if [ ${COV_BUILD_STATUS} -ne 0 ]
then
    echo "***** cov-build failed! *****"
    echo "***** see /tmp/cov.out for details! *****"
    exit 1
fi

if [ -f ${FORMAT_OUT}.txt ]
then
    echo "moving ${FORMAT_OUT}.txt -> ${FORMAT_OUT}.txt.old"
    mv ${FORMAT_OUT}.txt ${FORMAT_OUT}.txt.old
fi

if [ -f ${ANALYZE_OUT}.txt ]
then
    echo "moving ${ANALYZE_OUT}.txt -> ${ANALYZE_OUT}.txt.old"
    mv ${ANALYZE_OUT}.txt ${ANALYZE_OUT}.txt.old
fi

MISRA_CONFIG=${LIBOS_SOURCE}/tests/coverity/MISRA_C2012_lwgpu.config

$COVERITY_HOME/bin/cov-analyze \
     --disable-default  \
     --misra-config $MISRA_CONFIG \
     --dir $COVERITY_OUT \
     --jobs max16 \
     --max-mem 768 | tee ${ANALYZE_OUT}.txt

if [ ${PIPESTATUS[0]} -ne 0 ]
then
    echo "***** cov-analyze failed! *****"
    exit 1
fi

$COVERITY_HOME/bin/cov-format-errors \
     --dir $COVERITY_HOME/idirs/libos-kernel \
     --emacs-style > ${FORMAT_OUT}.txt

if [ $? -ne 0 ]
then
    echo "***** cov-format-errors failed! *****"
    exit 1
fi
