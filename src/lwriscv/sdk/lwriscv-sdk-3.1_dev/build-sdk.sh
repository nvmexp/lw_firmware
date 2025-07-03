#!/bin/bash
set -e

# Save log
exec > >(tee build.log) 2>&1

function handleError() {
    echo "================================================================="
    echo " Build script has failed. Make sure to cleanup your workspace.   "
    echo " Build directory is $DIR_BUILD"
    echo "================================================================="
}

trap handleError EXIT

# Sanity check for directories

if [ ! -r ${P4ROOT} ] ; then
    echo "Set P4ROOT"
    exit 1
fi

if [ ! -z $1 ] ; then
    FILTER=$1
fi

P4=`which p4`
MKDIR=`which mkdir`

# Where prebuilts are to be stored
DIR_PREBUILT=${PWD}/prebuilt

# Where we take profiles from, this need to be exported for liblwriscv
export DIR_SDK_PROFILE=${PWD}/profiles
# Pre-set manual directories
export LW_ENGINE_MANUALS=${LW_ENGINE_MANUALS:-"${P4ROOT}/sw/resman/manuals"}

# Where we put temp stuff
DIR_BUILD=`mktemp -d -p ${PWD} _out.XXXXX`

if [ ! -z $FILTER ] ; then
    PROFILES=`ls profiles/*.liblwriscv.mk | egrep "$FILTER" || true`
else
    PROFILES=`ls profiles/*.liblwriscv.mk || true`
fi

# Build liblwriscv
for profile in ${PROFILES}; do
    profile_name=`basename $profile .liblwriscv.mk`

    export LIBLWRISCV_PROFILE_FILE=${PWD}/${profile}
    export LIBLWRISCV_INSTALL_DIR=${DIR_PREBUILT}/${profile_name}
    export LIBLWRISCV_BUILD_DIR=${DIR_BUILD}/${profile_name}

    echo "Building liblwriscv profile $profile"
    make -C liblwriscv/ install
done

if [ ! -z $FILTER ] ; then
    PROFILES=`ls profiles/*.librts.mk | egrep "$FILTER" || true`
else
    PROFILES=`ls profiles/*.librts.mk || true`
fi

# Build librts
for profile in ${PROFILES}; do
    profile_name=`basename $profile .librts.mk`

    export LIBRTS_PROFILE=${profile_name}
    export LIBLWRISCV_INSTALL_DIR=${DIR_PREBUILT}/${profile_name}
    export LIBRTS_INSTALL_DIR=${DIR_PREBUILT}/${profile_name}
    export LIBRTS_BUILD_DIR=${DIR_BUILD}/${profile_name}

    make -C libRTS install
done

if [ ! -z $FILTER ] ; then
    PROFILES=`ls profiles/*.libccc.elw | egrep "$FILTER" || true`
else
    PROFILES=`ls profiles/*.libccc.elw || true`
fi

# Build libCCC, may need to build second, fsp profile
for profile in ${PROFILES}; do
    profile_name=`basename $profile .libccc.elw`

    export LIBLWRISCV_INSTALL_DIR=${DIR_PREBUILT}/${profile_name}
    export LIBCCC_OUT=${DIR_BUILD}/${profile_name}
    export LIBCCC_INSTALL_DIR=${LIBLWRISCV_INSTALL_DIR}

    unset LIBCCC_CHIP
    unset LIBCCC_SE_CONFIG
    unset LIBCCC_ENGINE
    unset LIBCCC_CHIP_FAMILY
    unset LIBCCC_FALCON_BUILD
    unset LIBCCC_FSP_SE_ENABLED
    unset LIBCCC_SELITE_SE_ENABLED
    unset LIBCCC_SECHUB_SE_ENABLED

    source $profile

    export LIBCCC_ENGINE
    export LIBCCC_SE_CONFIG
    export LIBCCC_CHIP_FAMILY

    if [ "$LIBCCC_FALCON_BUILD" = true ] ; then
        export UPROC_ARCH="FALCON"
        LIBCCC_ENGINE_LOWERCASE=`echo ${LIBCCC_ENGINE} | tr A-Z a-z`
        export LW_ENGINE_MANUALS=${P4ROOT}/sw/resman/manuals/${LIBCCC_CHIP_FAMILY}/${LIBCCC_CHIP}
    fi

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

    echo "Building libCCC profile ${LIBCCC_PROFILE}/${LIBCCC_ENGINE} for ${profile_name}"
    make -C libCCC install

    if [ "$LIBCCC_FALCON_BUILD" = true ] ; then
        unset UPROC_ARCH
        unset LW_ENGINE_MANUALS
        unset LIBCCC_ENGINE_LOWERCASE
    fi
done

# Build Boot-plugin
if [ ! -z $FILTER ] ; then
    PROFILES=`ls profiles/*.boot-plugin.mk | egrep "$FILTER" || true`
else
    PROFILES=`ls profiles/*.boot-plugin.mk || true`
fi

for profile in ${PROFILES}; do
    profile_name=`basename $profile .boot-plugin.mk`

    export BOOT_PLUGIN_PROFILE_FILE=${PWD}/${profile}
    export BOOT_PLUGIN_INSTALL_DIR=${DIR_PREBUILT}/${profile_name}/boot-plugin
    export BOOT_PLUGIN_BUILD_DIR=${DIR_BUILD}/${profile_name}/boot-plugin
    
    make -C boot-plugin install
done

# MK TODO: check if we're TOT

trap - EXIT

echo "================================================================="
echo " Build script has finished. Make sure to cleanup your workspace. "
echo " - Remove $DIR_BUILD"
echo " - Make sure to submit prebuilds"
echo "================================================================="

echo "Removing build directory $DIR_BUILD"
rm -r $DIR_BUILD
