#!/usr/bin/elw bash
#set -x

APP_DIR=`pwd`
MAKE=${MAKE-$P4ROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin/make-4.00}
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-true}

# Using two separate SDK profiles, as t239 GSP profile is only available on lwriscv-sdk-2.2_dev
# and the official relase SDK for t234 is set as lwriscv-sdk-2.1
if [ $2 == t239 ]; then
    ACR_SDK_BRANCH="lwriscv-sdk-2.2_dev"
elif [ $2 == t234 ]; then
    ACR_SDK_BRANCH="lwriscv-sdk-2.1"
fi

LWRISCV_BRANCH="release/${ACR_SDK_BRANCH}"

export LWRISCVX_SDK_ROOT=${P4ROOT}/sw/lwriscv/$LWRISCV_BRANCH
export LW_MANUALS_ROOT=${P4ROOT}/sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref
export LW_MANUALS_SWREF_ROOT=${P4ROOT}/sw/dev/gpu_drv/chips_a/drivers/common/inc/swref
export ACR_PROFILE=$1

# Contains a list of supported chips.
RISCV_ACR_SUPPORTED_CHIPS=( "t234" "t239" )

isChipValid () { 
    local array="$1[@]"
    local seeking=$2
    local in=0
    for element in "${!array}"; do
        if [[ $element == "$seeking" ]]; then
            in=1
            break
        fi
    done
    return $in
}

if [ -z $1 ] || [ -z $2 ] || isChipValid RISCV_ACR_SUPPORTED_CHIPS $2; then
    echo "Usage: ./build.sh <profile> <chip> - See ${LWRISCVX_SDK_ROOT}/profiles/ dir for a list of profiles"
    echo "Supported Chip values:"
    echo "${RISCV_ACR_SUPPORTED_CHIPS[*]}"
    exit 1
fi

if [ ! -f ${LWRISCVX_SDK_ROOT}/profiles/cheetah-acr-$2-$1.liblwriscv.mk ]; then
    echo "$1 is not a valid profile"
    exit 1
fi

export LIBLWRISCV=$P4ROOT/sw/lwriscv/release/${ACR_SDK_BRANCH}/liblwriscv
export LW_ENGINE_MANUALS=$P4ROOT/sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref/t23x/${CHIP}
export LW_TOOLS=$P4ROOT/sw/tools/

export LIBLWRISCV_INSTALL_DIR=${LWRISCVX_SDK_ROOT}/prebuilt/cheetah-acr-$2-$1
export ENGINE=$1
export CHIP=$2

${MAKE} -C ${APP_DIR} install
