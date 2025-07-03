#!/bin/bash
set -e

# Save log
exec > >(tee release.log) 2>&1

function handleError() {
    echo "================================================================="
    echo " Release script has failed. Make sure to cleanup your workspace. "
    echo "================================================================="
}

trap handleError EXIT

# Sanity check for directories

if [ ! -r ${P4ROOT} ] ; then
    echo "Set P4ROOT"
    exit 1
fi

P4=`which p4`
MKDIR=`which mkdir`

DIR_RISCV_ROOT=${P4ROOT}/sw/lwriscv
DIR_RELEASE_TO_ROOT=${DIR_RISCV_ROOT}/release
DIR_RELEASE_FROM=${PWD}
FORCE_RELEASE=${FORCE_RELEASE-false}

release_version=$1

if [ -z ${release_version} ] ; then
    echo "No version provided."
    exit 1
fi

DIR_RELEASE_TO=${DIR_RELEASE_TO_ROOT}/lwriscv-sdk-${release_version}

if [ "$FORCE_RELEASE" = false ] ; then
    if [ `${P4} opened ${DIR_RELEASE_FROM}/... | wc -l` != "0" ] ; then
        echo "Source folder has is dirty. Submit files or set FORCE_RELEASE=true."
        exit 1
    fi
    if [ -d ${DIR_RELEASE_TO} ]; then
        echo "Destination folder ${DIR_RELEASE_TO} must not exist. Set FORCE_RELEASE=true to override."
        exit 1
    fi
else
    FORCE_RELEASE_DESC="re-"
fi

WORKSPACE=`${P4} info | egrep "^Client name:" | cut -d " " -f 3`
CL_FROM=`${P4} changes -m1 ${DIR_RELEASE_FROM}/...#have | cut -d " " -f 2`

CL_DESCRIPTION="LWRISCV SDK ${release_version} ${FORCE_RELEASE_DESC}release

Integrate ${DIR_RELEASE_FROM} @ CL ${CL_FROM} to ${DIR_RELEASE_TO}
Author: `getent passwd ${USER} | cut -d ':' -f 5 | cut -d ',' -f 1` (${USER}@`hostname`)
Date: `date`
Workspace: $WORKSPACE

Reviewed by:
"

# MK TODO: Check if there are opened files in current workspace (release should be done from clean tree)
# MK TODO: Check if we're at TOT

# Create CL
CL=`${P4} --field "Description=${CL_DESCRIPTION}" change -o | ${P4} change -i`
CL=( $CL )
CL=${CL[1]}
echo "Created CL $CL"

${MKDIR} -p ${DIR_RELEASE_TO}
${P4} integrate -c ${CL} ${DIR_RELEASE_FROM}/... ${DIR_RELEASE_TO}/...
trap - EXIT
echo "================================================================="
echo " Release script has finished sucesfully."
echo "================================================================="
