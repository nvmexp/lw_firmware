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
export USE_PREBUILT_SDK=${USE_PREBUILT_SDK-false}
export DIR_SDK_PROFILE=${SAMPLE_ROOT}/../profiles
APPS="liblwriscv-basic-ga10x liblwriscv-basic-gh100 liblwriscv-basic-orin liblwriscv-basic-ls10 liblwriscv-lwpka-gh100 liblwriscv-rts-gh100 "

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

        echo   "================================================================="
        printf "=      Building %-45s   =\n" "${app}/${profile_name}"
        echo   "================================================================="

        if [ ! -f ${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk ]; then
            echo "Profile ${profile_name} is missing in ${DIR_SDK_PROFILE}"
            exit 1
        fi

        export SAMPLE_PROFILE_FILE=${engine}
        export ENGINE=${engine}

        # LIBCCC Build
        if [ "$SAMPLE_NEEDS_LIBCCC" = true ] ; then
            echo   "================================================================="
            printf "=      Building LibCCC\n"
            echo   "================================================================="
            
            export UPROC_ARCH="RISCV"
            CCC_PROFILE=${DIR_SDK_PROFILE}/${profile_name}.libccc.elw

            # Clear old CCC elw
            unset LIBCCC_CHIP
            unset LIBCCC_SE_CONFIG
            unset LIBCCC_ENGINE
            unset LIBCCC_CHIP_FAMILY
            unset LIBCCC_FSP_SE_ENABLED
            unset LIBCCC_SELITE_SE_ENABLED
            unset LIBCCC_SECHUB_SE_ENABLED

            # Source new one
            source ${CCC_PROFILE}

            export LIBCCC_ENGINE
            export LIBCCC_SE_CONFIG
            export LIBCCC_CHIP_FAMILY

            if [ "$LIBCCC_SE_CONFIG" = "FSP_SE_ONLY" ] ; then
                export LIBCCC_PROFILE="${LIBCCC_CHIP}_fsp_se"
                export LIBCCC_FSP_SE_ENABLED=1
            elif [ "$LIBCCC_SE_CONFIG" = "SELITE_SE_ONLY" ] ; then
                export LIBCCC_PROFILE="${LIBCCC_CHIP}_selite_se"
                export LIBCCC_SELITE_SE_ENABLED=1
            elif [ "$LIBCCC_SE_CONFIG" = "SECHUB_SE_ONLY" ] ; then
                export LIBCCC_PROFILE="${LIBCCC_CHIP}_sechub_se"
                export LIBCCC_SECHUB_SE_ENABLED=1
            elif [ "$LIBCCC_SE_CONFIG" = "SECHUB_AND_SELITE_SE" ] ; then
                export LIBCCC_PROFILE="${LIBCCC_CHIP}_sechub_selite_se"
                export LIBCCC_SECHUB_SE_ENABLED=1
                export LIBCCC_SELITE_SE_ENABLED=1
            else
                echo "Invalid LIBCCC_SE_CONFIG setting!"
                echo "Valid settings: <FSP_SE_ONLY | SELITE_SE_ONLY | SECHUB_SE_ONLY | SECHUB_AND_SELITE_SE>"
                exit 1
            fi
        fi

        if [ "$USE_PREBUILT_SDK" = false ] ; then
            # Build liblwriscv
            export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk
            export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin
            export LIBLWRISCV_BUILD_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build

            make -C ../liblwriscv/ install

            # Build libRTS
            if [ "$SAMPLE_NEEDS_LIBRTS" = true ] ; then
                export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin
                export LIBRTS_OUT=${LIBLWRISCV_BUILD_DIR}
                export LIBRTS_INSTALL_DIR=${LIBLWRISCV_INSTALL_DIR}

                make -C ../libRTS install
            fi

            # Build libCCC if needed
            if [ "$SAMPLE_NEEDS_LIBCCC" = true ] ; then
                export LIBCCC_OUT=${LIBLWRISCV_BUILD_DIR}
                export LIBCCC_INSTALL_DIR=${LIBLWRISCV_INSTALL_DIR}
                # Build libCCC
                make -C ../libCCC install
            fi
            
        fi

        if [ "$SAMPLE_NEEDS_LIBCCC" = true ] ; then
            export LIBCCC_ENGINE_UPPERCASE=`echo ${LIBCCC_ENGINE} | tr '[:lower:]' '[:upper:]'`
        fi

        make -f ${SAMPLE_ROOT}/build-liblwriscv-samples.mk -C ${APP_DIR} install

        
        if [ "$USE_PREBUILT_SDK" = false ] ; then
            # Build liblwriscv
            export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${profile_name}.liblwriscv.mk
            export LIBLWRISCV_INSTALL_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.bin
            export LIBLWRISCV_BUILD_DIR=${SAMPLE_ROOT}/${APP_DIR}/sdk-${CHIP}-${ENGINE}.build

            make -C ../liblwriscv/ install

        fi

        make -f ${SAMPLE_ROOT}/build-liblwriscv-samples.mk -C ${APP_DIR} install
    done
done

trap - EXIT

echo "================================================================="
echo " Build script has finished. "
echo "================================================================="
