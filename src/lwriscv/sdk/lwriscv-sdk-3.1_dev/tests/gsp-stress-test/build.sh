#!/usr/bin/elw bash
#set -x

APP_DIR=`pwd`
export LW_TOOLS=$P4ROOT/sw/tools/
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-false}
export LWRISCVX_SDK_ROOT=${APP_DIR}/../..
export LW_MANUALS_ROOT=${P4ROOT}/sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref

source profile.elw

MAKE=${MAKE-$P4ROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin/make-4.00}

if [ ! -f ${LWRISCVX_SDK_ROOT}/profiles/gsp-stress-test-t234.liblwriscv.mk ]; then
    echo "GSP stress test profile not present"
    exit 1
fi

if [ "$USE_PREBUILT_SDK" = false ] ; then
    LIBLWRISCV=${LWRISCVX_SDK_ROOT}/liblwriscv
    export LIBLWRISCV_PROFILE_FILE=${LWRISCVX_SDK_ROOT}/profiles/gsp-stress-test-t234.liblwriscv.mk
    export LIBLWRISCV_INSTALL_DIR=${APP_DIR}/liblwriscv-${APP_NAME}.bin
    export LIBLWRISCV_BUILD_DIR=${APP_DIR}/liblwriscv-${APP_NAME}.build

    ${MAKE} -C ${LIBLWRISCV} install
    if [ $? != 0 ]; then
        exit 1
    fi
else
    export LIBLWRISCV_INSTALL_DIR=${LWRISCVX_SDK_ROOT}/prebuilt/gsp-stress-test-t234
fi

${MAKE} -C ${APP_DIR} install
