#!/bin/bash
set -e

# Save log
exec > >(tee build.log) 2>&1

function handleError() {
    echo "================================================================="
    echo " Build script has failed. "
    echo "================================================================="
}
trap handleError EXIT

SAMPLE_ROOT=`pwd`
export DIR_SDK_PROFILE=${SAMPLE_ROOT}/../profiles

export LIBOS_SOURCE=${SAMPLE_ROOT}/../os/libos

echo   "================================================================="
printf "=      Building %-45s   =\n" "libos-multipart-gh100-fsp"
echo   "================================================================="

profile_name=basic-libos-gh100-fsp
APP_DIR=libos-multipart-gh100-fsp

# Build liblwriscv
export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk
export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${profile_name}.bin
export LIBLWRISCV_BUILD_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${profile_name}.build
make -C ../liblwriscv/ install

# Build libCCC
export LIBCCC_OUT=${LIBLWRISCV_BUILD_DIR}
export LIBCCC_INSTALL_DIR=${LIBLWRISCV_INSTALL_DIR}
CCC_PROFILE=${DIR_SDK_PROFILE}/${profile_name}.libccc.elw
source ${CCC_PROFILE}
export LIBCCC_ENGINE
export LIBCCC_SE_CONFIG
export LIBCCC_CHIP_FAMILY
export LIBCCC_PROFILE="${LIBCCC_CHIP}_fsp_se"
export LIBCCC_FSP_SE_ENABLED=1
make -C ../libCCC install

# Build sample
make -C ${SAMPLE_ROOT}/${APP_DIR} USE_PREBUILT_SDK=false

trap - EXIT

echo "================================================================="
echo " Build script has finished. "
echo "================================================================="
