#!/bin/bash
# set -e

APP_DIR=`pwd`
source profile.elw

export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-false}
export LWRISCVX_SDK_ROOT=${APP_DIR}/../..
export LW_TOOLS=$P4ROOT/sw/tools/

MAKE=${MAKE-$P4ROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin/make-4.00}

DIRS="${APP_NAME}-${CHIP}-${ENGINE}.build ${APP_NAME}-${CHIP}-${ENGINE}.bin"

if [ "$USE_PREBUILT_SDK" = false ] ; then
    export LIBLWRISCV_INSTALL_DIR=${APP_DIR}/liblwriscv-${APP_NAME}.bin
    LIB_DIRS=" liblwriscv-${APP_NAME}.bin liblwriscv-${APP_NAME}.build"
    DIRS+=${LIB_DIRS}
else
    export LIBLWRISCV_INSTALL_DIR=${LWRISCVX_SDK_ROOT}/prebuilt/gsp-stress-test-t234
fi

function clearDir() {
    if [ -d "$1" ] ; then
        echo rm -r "$1"
        rm -r "$1"
    fi
}

${MAKE} -C ${APP_DIR} clean

for i in $DIRS; do
    echo   "================================================================="
    printf "=      Clearing %-45s   =\n" "${i}"
    echo   "================================================================="
    # App build directory
    clearDir "${APP_DIR}/${i}"
done
