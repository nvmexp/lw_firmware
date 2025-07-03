#!/bin/bash
set -e

# Save log
exec > >(tee build.log) 2>&1

function handleError() {
    echo "================================================================="
    echo " Build script has failed. Make sure to cleanup your workspace.   "
    echo " Build directory is $APP_DIR"
    echo "================================================================="
}
trap handleError EXIT

SAMPLE_ROOT=`pwd`
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-false}
export DIR_SDK_PROFILE=${SAMPLE_ROOT}/../profiles
APPS="sepkern-basic-ga10x sepkern-basic-gh100 sepkern-basic-orin sepkern-basic-ad10x"

if [ ! -z "$@" ]; then
    echo "Building app(s) $@"
    APPS=$@
fi

for app in $APPS; do
    if [ ! -f ${app}/profile.elw ] ; then
        echo "${app} is not a valid sample app"
        exit 1
    fi

    # Clean elw
    unset ENGINES
    unset CHIP
    unset SK_CHIP
    unset SIGGEN_CHIP

    source ${app}/profile.elw
    APP_DIR=${app}

    export SK_CHIP=${SK_CHIP-${CHIP}}

    for engine in ${ENGINES} ; do
        profile_name=basic-sepkern-${CHIP}-${engine}

        echo   "================================================================="
        printf "=      Building %-45s   =\n" "${app}/${profile_name}"
        echo   "================================================================="

        if [ ! -f ${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk ]; then
            echo "Profile ${profile_name} is missing in ${DIR_SDK_PROFILE}"
            exit 1
        fi

        export SAMPLE_PROFILE_FILE=${engine}
        export ENGINE=${engine}


        if [ "$USE_PREBUILT_SDK" = false ] ; then
            # Build liblwriscv
            export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk
            export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin
            export LIBLWRISCV_BUILD_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build

            make -C ../liblwriscv/ install
        fi

        # Build SK
        export SEPKERN_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin
        export OUTPUTDIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build
        export BUILD_PATH=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build
        export POLICY_PATH=${SAMPLE_ROOT}/${APP_DIR}
        export POLICY_FILE_NAME=policies_${ENGINE}_${CHIP}.ads
        make -C ../monitor/sepkern/ CHIP=${SK_CHIP} PARTITION_COUNT=2 install

        # Build app
        make -C ${APP_DIR} -f ${SAMPLE_ROOT}/build-sepkern-samples.mk install
    done
done

trap - EXIT

echo "================================================================="
echo " Build script has finished. "
echo "================================================================="
