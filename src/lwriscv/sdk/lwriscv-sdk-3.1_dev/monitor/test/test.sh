#!/bin/bash
set -e

# Save log
exec > >(tee build.log) 2>&1

function handleError() {
    echo "================================================================="
    echo " Test Build Script Has Failed.                                   "
    echo "================================================================="
}

function EXIT() {
    exit 0
}

trap handleError EXIT


TEST_ROOT=`pwd`
export DIR_SDK_PROFILE=${TEST_ROOT}/../../profiles
export BUILD_DIR=${TEST_ROOT}/build
export LIBLWRISCV_ROOT=${TEST_ROOT}/../../liblwriscv
export LWRISCVX_SDK_ROOT=${TEST_ROOT}/../../
export PROFILE_DIR=${TEST_ROOT}/profile
export MANIFEST_DIR=${TEST_ROOT}/src/manifest
export LIBTEST_DIR=${TEST_ROOT}/src/lib

APPS="gh100-gsp gh100-fsp-cmodel"

function clearDir() {
    if [ -d "$1" ]; then
        echo rm -r "$1"
        rm -r "$1"
    fi
}

function mkDir() {
    if [ ! -d "$1" ]; then
        echo mkdir "$1"
        mkdir "$1"
    fi
}

function clearFile() {
    if [ -f "$1" ]; then
        echo rm "$1"
        rm "$1"
    fi
}

# Trigger the script with optional inputs:
# ./test.sh clean  -> clean all build artifacts
# ./test.sh        -> default, build all app/test_cases, then test them
# ./test.sh gh100-gsp set_timer false -> optional 3 inputs to specify which particular
#           (app)     (case)    (test after build)
#
ARG1=${1:-${APPS}}
ARG2=${2:-all}
ARG3=${3:-true}

if [ $ARG1 == clean ]; then
    echo "Cleaning the Build"
    clearDir ${BUILD_DIR}
    clearFile ${TEST_ROOT}/build.log
    clearFile ${TEST_ROOT}/eos_dtcm
    clearFile ${TEST_ROOT}/eos_itcm
    trap - EXIT
    exit
fi

APPS=${ARG1}
BUILD_TESTS=${ARG2}
AUTO_TEST=${ARG3}

mkDir ${BUILD_DIR}

for app in $APPS; do

    profile=${PROFILE_DIR}/${app}.elw
    app_build_dir=${BUILD_DIR}/${app}
    liblwriscv_out_dir=${app_build_dir}/liblwriscv
    sk_out_dir=${app_build_dir}/sk
    manifest_out_dir=${app_build_dir}/manifest
    testlib_out_dir=${app_build_dir}/testlib

    echo   "================================================================="
    printf "=      Building %-45s   =\n" "${app}"
    echo   "================================================================="

    if [ ! -f ${profile} ] ; then
        echo "${app} is not a supported test configuration"
        exit 1
    fi

    # Start build clean
    clearDir ${app_build_dir}
    mkDir ${app_build_dir}
    mkDir ${liblwriscv_out_dir}
    mkDir ${sk_out_dir}
    mkDir ${manifest_out_dir}
    mkDir ${testlib_out_dir}

    # Clean elw
    unset MODEL_TYPE
    source ${profile}

    # Build liblwriscv
    export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${LIBLWRISCV_PROFILE}.liblwriscv.mk
    export LIBLWRISCV_INSTALL_DIR=${liblwriscv_out_dir}/bin
    export LIBLWRISCV_BUILD_DIR=${liblwriscv_out_dir}/build

    if [ ! -f ${LIBLWRISCV_PROFILE_FILE} ]; then
        echo "Profile ${LIBLWRISCV_PROFILE_FILE} is missing"
        exit 1
    fi

    make -C ${LIBLWRISCV_ROOT} install

    # Build SK
    export SEPKERN_INSTALL_DIR=${sk_out_dir}/bin
    export OUTPUTDIR=${sk_out_dir}/build
    export BUILD_PATH=${sk_out_dir}/build
    export POLICY_PATH=${PROFILE_DIR}
    initiator="bootrom"
    if [ ${MODEL_TYPE} == cmodel ]; then
        initiator="none"
    fi
    make -C ../sepkern/ install INITIATOR=${initiator} EXTERN_SOURCE_PATH=${TEST_ROOT}/sepkern_extension ILWALID_EXT_FILE=sbi_test_extension.adb

    # Build Manifest
    make -C ${TEST_ROOT}/src/manifest -f ${TEST_ROOT}/src/manifest/manifest.mk build_manifest BUILD_DIR=${manifest_out_dir}

    # Build testlib
    make -C ${TEST_ROOT}/src/lib -f ${TEST_ROOT}/src/lib/build_libtest.mk build_libtest BUILD_DIR=${testlib_out_dir}
    export LIBTEST_BUILD_DIR=${testlib_out_dir}

    # Build Tests
    # If not specifying test cases to build, then build all available tests
    if [ ${BUILD_TESTS} == all ]; then
        BUILD_TESTS=${TEST_CASES}
    fi

    for test_case in ${BUILD_TESTS}; do
        test_build_dir=${app_build_dir}/${test_case}/build
        test_bin_dir=${app_build_dir}/${test_case}/bin
        mkDir ${app_build_dir}/${test_case}
        mkDir ${test_build_dir}
        mkDir ${test_bin_dir}
        make -C ${TEST_ROOT}/src install TEST_CASE=${test_case} BUILD_DIR=${test_build_dir} INSTALL_DIR=${test_bin_dir} MANIFEST_DIR=${manifest_out_dir}

        if [[ ${MODEL_TYPE} == cmodel ]] && [[ ${AUTO_TEST} == true ]]; then
            test_output_dir=${app_build_dir}/${test_case}/test
            mkdir ${test_output_dir}
            python ${TEST_ROOT}/scripts/cmodel_test.py --text_hex ${test_bin_dir}/${CHIP}-${ENGINE}-${test_case}.text.hex --data_hex ${test_bin_dir}/${CHIP}-${ENGINE}-${test_case}.data.hex --output ${test_output_dir}/cmodel_output
        fi
    done

done

trap - EXIT

echo "================================================================="
echo " Build script has finished. "
echo "================================================================="
