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
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-true}
export DIR_SDK_PROFILE=${SAMPLE_ROOT}/../../profiles
APPS="baseline security-keyslots bugs revocation multi-engine"

if [ ! -z "$@" ]; then
    echo "Engine filter $@"
    FILTER=$@
fi

for app in $APPS; do
    if [ ! -f ${app}/profile.elw ] ; then
        echo "${app} is not a valid sample app"
        exit 1
    fi

    # Clean elw
    unset ENGINES
    unset CHIP
    unset SIGGEN_CHIP

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

        # Check filtering rule
        [[ $engine =~ $FILTER ]] || continue

        echo   "================================================================="
        printf "=      Building %-45s   =\n" "${app}/${profile_name}"
        echo   "================================================================="

        if [ ! -f ${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk ]; then
            echo "Profile ${profile_name} is missing in ${DIR_SDK_PROFILE}"
            exit 1
        fi

        export SAMPLE_PROFILE_FILE=${engine}
        export ENGINE=${engine}

        if [ "$SAMPLE_NEEDS_LIBCCC" = true ] ; then
            export CCC_USE_LIBLWRISCV=true
            export UPROC_ARCH="RISCV"
        fi

        if [ "$USE_PREBUILT_SDK" = false ] ; then
            # Build liblwriscv
            export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk
            export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin
            export LIBLWRISCV_BUILD_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build

            make -C ../../liblwriscv/ install

            # Build libCCC if needed
            if [ "$SAMPLE_NEEDS_LIBCCC" = true ] ; then
                export LIBCCC_OUT=${LIBLWRISCV_BUILD_DIR}
                export LIBCCC_INSTALL_DIR=${LIBLWRISCV_INSTALL_DIR}
                CCC_PROFILE=${DIR_SDK_PROFILE}/${profile_name}.libccc.elw

                # Clear old CCC elw
                unset LIBCCC_CHIP
                unset LIBCCC_USE_FSP_SE
                unset LIBCCC_ENGINE
                unset LIBCCC_CHIP_FAMILY

                # Source new one
                source ${CCC_PROFILE}

                export LIBCCC_ENGINE

                # For now FSP uses special profile
                if [ "$LIBCCC_USE_FSP_SE" = true ] ; then
                    export LIBCCC_PROFILE="${LIBCCC_CHIP}_fsp_se"
                elif [ "$LIBCCC_USE_SELITE_SE" = true ] ; then
                    export LIBCCC_PROFILE="${LIBCCC_CHIP}_selite_se"
                else
                    export LIBCCC_PROFILE="${LIBCCC_CHIP}_sechub_se"
                fi

                # Build libCCC
                make -C ../../libCCC install


            fi
        fi

        if [ "$SAMPLE_NEEDS_LIBCCC" = true ] ; then
            export LIBCCC_ENGINE_UPPERCASE=`echo ${LIBCCC_ENGINE} | tr '[:lower:]' '[:upper:]'`
        fi

        make -f ${SAMPLE_ROOT}/build-tests.mk -C ${APP_DIR} install
    done
done

trap - EXIT

echo "================================================================="
echo " Build script has finished. "
echo "================================================================="
