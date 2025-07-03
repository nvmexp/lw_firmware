#!/bin/bash
# set -e

# MK TODO: remove
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-false}

SAMPLE_ROOT=`pwd`
export DIR_SDK_PROFILE=$SAMPLE_ROOT/../../profiles

APPS="baseline"

if [ ! -z "$@" ]; then
    echo "Building app(s) $@"
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

    source ${app}/profile.elw

    if [ -z "${ENGINES}" ] ; then
        echo "${app}'s profile is not valid (missing ENGINES)"
        exit 1
    fi

    if [ -z "${CHIP}" ] ; then
        echo "${app}'s profile is not valid (missing CHIP)"
        exit 1
    fi

    APP_DIR=${app}
    for engine in ${ENGINES} ; do
        profile_name=basic-${CHIP}-${engine}

        echo   "================================================================="
        printf "=      Clearing %-45s   =\n" "${app}/${profile_name}"
        echo   "================================================================="

        if [ ! -f ${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk ]; then
            echo "Profile ${profile_name} is missing in ${DIR_SDK_PROFILE}"
            exit 1
        fi

        export SAMPLE_PROFILE_FILE=${engine}
        export ENGINE=${engine}

        # App build directory
        clearDir "${SAMPLE_ROOT}/${APP_DIR}/app-${CHIP}-${ENGINE}.bin"
        # SDK build / install directory
        clearDir "${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin"
        clearDir "${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build"
    done
done
