#!/bin/bash
set -e

# Save log
exec > >(tee build.log) 2>&1

function handleError() {
    echo "================================================================="
    echo " Clear script has failed. Make sure to clear your workspace.   "
    echo " Clear directory is $APP_DIR"
    echo "================================================================="
}
trap handleError EXIT

SAMPLE_ROOT=`pwd`
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-false}
export DIR_SDK_PROFILE=${SAMPLE_ROOT}/../profiles
APPS="sepkern-basic-ga10x sepkern-basic-gh100 sepkern-basic-orin"

if [ ! -z "$@" ]; then
    echo "Clearing app(s) $@"
    APPS=$@
fi

function clearDir() {
    if [ -d "$1" ] ; then
        echo rm -r "$1"
        rm -r "$1"
    fi
}

for app in $APPS; do
    if [ ! -f ${app}/profile.elw ] ; then
        echo "${app} is not a valid sample app"
        exit 1
    fi

    # Clear elw
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
        printf "=      Clearing %-45s   =\n" "${app}/${profile_name}"
        echo   "================================================================="

        if [ ! -f ${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk ]; then
            echo "Profile ${profile_name} is missing in ${DIR_SDK_PROFILE}"
            exit 1
        fi

        export SAMPLE_PROFILE_FILE=${engine}
        export ENGINE=${engine}

        # Just delete all intermediate files:

        # App build directory
        clearDir "${SAMPLE_ROOT}/${APP_DIR}/app-${CHIP}-${ENGINE}.bin"
        # SDK build / install directory
        clearDir "${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin"
        clearDir "${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build"
    done
done

trap - EXIT

echo "================================================================="
echo " Clear script has finished. "
echo "================================================================="
