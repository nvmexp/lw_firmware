#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

set -e

UNIX_BUILD="$1"
shift

# Run fbflcn-release.sh inside unix-build.
UNIX_BUILD_CMD="${UNIX_BUILD} --no-devrel"
# Pass through DVS environment variables.
UNIX_BUILD_CMD="${UNIX_BUILD_CMD} --elwvar LW_DVS_BLD --elwvar CHANGELIST"
if [ ! -z "${SIGN_LOCAL}" ] ; then UNIX_BUILD_CMD="${UNIX_BUILD_CMD} --elwvar SIGN_LOCAL" ; fi
if [ ! -z "${SIGN_SERVER}" ] ; then UNIX_BUILD_CMD="${UNIX_BUILD_CMD} --elwvar SIGN_SERVER" ; fi
# Bind-mount extra directories.
# For some reason falcon GCC fails with SEGV if /proc isn't present.
UNIX_BUILD_CMD="${UNIX_BUILD_CMD} --extra ${LW_TOOLS}/falcon-gcc --extra /dvs/p4/selwresign --extra /proc"
UNIX_BUILD_CMD="${UNIX_BUILD_CMD} --extra /dev/random --extra /dev/urandom"

${UNIX_BUILD_CMD} ./fbflcn-release.sh "$@"

