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
export LIBOS_SOURCE=${SAMPLE_ROOT}/../os/libos-next
export LIBLWRISCV_ROOT=${SAMPLE_ROOT}/../liblwriscv

profile_name=basic-libos-ga10x-gsp
APP_DIR=libos-next-ga10x-gsp

export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk
export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${profile_name}.bin
export LIBLWRISCV_BUILD_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${profile_name}.build

function clearDir() {
    if [ -d "$1" ] ; then
        echo rm -r "$1"
        rm -r "$1"
    fi
}

function clean_builds() {
    clearDir ${APP_DIR}/_out
    clearDir ${APP_DIR}/bin
    clearDir ${APP_DIR}/mods/_out
    clearDir ${APP_DIR}/mods/bin
    clearDir ${LIBLWRISCV_INSTALL_DIR}
    clearDir ${LIBLWRISCV_BUILD_DIR}
    clearDir ${APP_DIR}/elwos_basic/debug-kernel-*
    clearDir ${APP_DIR}/elwos_basic/debug-bootloader-*
    clearDir ${APP_DIR}/elwos_basic/debug-sim-*
}

if [ "$@" == "clean" ]; then
    echo "Cleaning apps $@"
    clean_builds
    trap - EXIT
    exit 0
fi

echo   "================================================================="
printf "=      Building %-45s   =\n" "libos-next-ga10x-gsp"
echo   "================================================================="

# Start a clean build
clean_builds

# Build liblwriscv
make -C ../liblwriscv/ install

# Build sample
make -C ${SAMPLE_ROOT}/${APP_DIR} USE_PREBUILT_SDK=false LIBLWRISCV_PROFILE_NAME=${profile_name}

# Build mods image
make -C ${SAMPLE_ROOT}/${APP_DIR}/mods install

trap - EXIT

echo "================================================================="
echo " Build script has finished. "
echo "================================================================="
