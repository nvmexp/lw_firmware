#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

if [ $# -lt 1 ] ; then
  UNIX_BUILD="unix-build"
else
  UNIX_BUILD="$1"
fi

# Run lwmake inside unix-build.
LWMAKE_CMD="${UNIX_BUILD} --no-devrel"
# Pass through DVS environment variables.
LWMAKE_CMD="${LWMAKE_CMD} --elwvar LW_DVS_BLD --elwvar CHANGELIST"
if [ ! -z "${SIGN_LOCAL}" ] ; then LWMAKE_CMD="${LWMAKE_CMD} --elwvar SIGN_LOCAL" ; fi
if [ ! -z "${SIGN_SERVER}" ] ; then LWMAKE_CMD="${LWMAKE_CMD} --elwvar SIGN_SERVER" ; fi
# Bind-mount extra directories.
# For some reason falcon GCC fails with SEGV if /proc isn't present.
LWMAKE_CMD="${LWMAKE_CMD} --extra ${LW_TOOLS}/riscv --extra /dvs/p4/selwresign --extra /proc"
# Actually ilwoke lwmake.
LWMAKE_CMD="${LWMAKE_CMD} lwmake"

${LWMAKE_CMD} clobber build release -j16  \
    LWMAKE_CMD="${LWMAKE_CMD}"            \
    SEC2CFG_PROFILE=                      \
    SEC2CFG_PROFILES=   
