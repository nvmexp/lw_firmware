#!/bin/bash
# Script to build the PLM/WPR test application.
#  Usage: build-tests.sh [profile]
#  Examples: build-tests.sh gh100
#            build-tests.sh

###############################################################################
## Setup

# Exit immediately if any command fails.
set -e

# Save all output to file.
exec > >(tee -i build-tests.log) 2>&1

# Set exception handler to explicitly report failures to the user.
function handleError() {
    ERRORCODE=$?

    echo "================================================================="
    echo " Build script has failed ($ERRORCODE). See build-tests.log. "
    echo "================================================================="
}
trap handleError EXIT

###############################################################################
## Constants

# Directory containing all test files.
export TEST_ROOT=`pwd`

# Directory to which output files should be installed.
export TEST_OUTPUT_ROOT=${TEST_ROOT}/bin

# Directory containing all LWRISCV SDK profiles.
export DIR_SDK_PROFILE=${TEST_ROOT}/../../profiles

# Root directory for liblwriscv.
LIBLWRISCV_DIR=${TEST_ROOT}/../../liblwriscv

###############################################################################
## Input and Validation

# Display usage information if number of arguments is incorrect.
if [ $# -gt 1 ]; then
    echo "Usage: $0 [profile]"
    echo "Example: $0 gh100"
    exit 1
fi

# Determine profiles to build.
if [ $# -eq 1 ]; then
    # Build only the profile specified in the first argument, if provided.
    TEST_PROFILE_FILES=${TEST_ROOT}/profile-${1}.elw

    # Ensure specified profile exists.
    if [ ! -f ${TEST_PROFILE_FILES} ]; then
        echo "Error: No build profile found for ${1} in ${TEST_ROOT}!"
        exit 2
    fi
else
    # Otherwise, build all profiles.
    TEST_PROFILE_FILES=${TEST_ROOT}/profile-*.elw
fi

###############################################################################
## Build

# Build each profile in the profile list.
for profile_file in $( ls ${TEST_PROFILE_FILES} ); do
    # Mark start of build process.
    echo   "================================================================="
    printf "=      Building %-45s   =\n" "$(basename ${profile_file})"
    echo   "================================================================="
    
    # Clear environment from previous profile.
    unset CHIP ENGINES EXTRA_MAKES

    # Source build parameters for the selected profile.
    source ${profile_file}

    # Validate build parameters.
    if [ -z "${CHIP}" ]; then
        echo "Error: Build profile ${profile_file} is not valid (missing CHIP)!"
        exit 3
    fi

    if [ -z "${ENGINES}" ]; then
        echo "Error: Build profile ${profile_file} is not valid (missing ENGINES)!"
        exit 4
    fi

    # Add profile as makefile dependency.
    export EXTRA_MAKES=${profile_file}

    # Build each engine specified by the current build profile.
    for engine in ${ENGINES}; do
        # Export the current engine name.
        export ENGINE=${engine}

        # Obtain path to the corresponding liblwriscv profile (basic profile).
        LIBLWRISCV_PROFILE=basic-${CHIP}-${ENGINE}.liblwriscv

        # Mark start of liblwriscv build.
        echo   "================================================================="
        printf "=      Building %-45s   =\n" "${LIBLWRISCV_PROFILE}"
        echo   "================================================================="

        # Set liblwriscv build parameters.
        export LIBLWRISCV_PROFILE_FILE=${DIR_SDK_PROFILE}/${LIBLWRISCV_PROFILE}.mk
        export LIBLWRISCV_INSTALL_DIR=${TEST_OUTPUT_ROOT}/sdk-${CHIP}-${ENGINE}.bin
        export LIBLWRISCV_BUILD_DIR=${TEST_OUTPUT_ROOT}/sdk-${CHIP}-${ENGINE}.build

        # Ensure selected liblwriscv profile exists before trying to build it.
        if [ ! -f ${LIBLWRISCV_PROFILE_FILE} ]; then
            echo "Error: Liblwriscv profile ${LIBLWRISCV_PROFILE} not found in ${DIR_SDK_PROFILE}!"
            exit 5
        fi

        # Build liblwriscv (force custom start.S/exception-handler and non-compressed ISA).
        make -C ${LIBLWRISCV_DIR} install LWRISCV_FEATURE_START=n LWRISCV_FEATURE_EXCEPTION=n LWRISCV_HAS_ARCH=rv64imf

        # Obtain fully-qualified name of the current test profile.
        export TEST_PROFILE=plm+wpr-${CHIP}-${ENGINE}

        # Mark start of test build.
        echo   "================================================================="
        printf "=      Building %-45s   =\n" "${TEST_PROFILE}"
        echo   "================================================================="

        # Build test application.
        make -C ${TEST_ROOT} install
    done
done

###############################################################################
## Wrap-Up

# Uninstall trap handler.
trap - EXIT

# Finished.
echo "================================================================="
echo " Build script has finished "
echo "================================================================="
