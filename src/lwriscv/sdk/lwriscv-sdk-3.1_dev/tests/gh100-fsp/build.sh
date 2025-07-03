#!/usr/bin/elw bash
#set -x

APP_DIR=`pwd`

if [ -z $1 ]; then
    echo "Usage: ./build.sh <profile> - See Profiles dir for a list of profiles"
    exit 1
fi

if [ ! -f ${APP_DIR}/profiles/$1.mk ]; then
    echo "$1 is not a valid profile"
    exit 1
fi

export LIBLWRISCV_PROFILE_FILE=${APP_DIR}/profiles/$1.mk
export LIBLWRISCV_INSTALL_DIR=${APP_DIR}/liblwriscv.bin

export SAMPLE_PROFILE=$1

#LibCCC specific exports
export CCC_USE_LIBLWRISCV=false
export UPROC_ARCH=RISCV
export UPROC_ARCH_LOWERCASE=`echo ${UPROC_ARCH} | tr '[:upper:]' '[:lower:]'`
export LIBCCC_CHIP_FAMILY=hopper
export LIBCCC_ENGINE=${SAMPLE_PROFILE}
export LIBCCC_ENGINE_UPPERCASE=`echo ${LIBCCC_ENGINE} | tr '[:lower:]' '[:upper:]'`
if [ "${LIBCCC_ENGINE}" == "fsp" ]; then
    export LIBCCC_PROFILE=gh100_fsp_se
else
    export LIBCCC_PROFILE=gh100_sechub_se
fi
export LIBCCC_DIR=${APP_DIR}/../../libCCC/lib/${LIBCCC_CHIP_FAMILY}/${UPROC_ARCH_LOWERCASE}/${LIBCCC_PROFILE}/
export EXTRA_CFLAGS="-DUPROC_ARCH_RISCV -DCCC_ENGINE_${LIBCCC_ENGINE_UPPERCASE}"

make -C ../../liblwriscv/ install
if [ $? != 0 ]; then
    exit 1
fi

make -C ${APP_DIR} install
