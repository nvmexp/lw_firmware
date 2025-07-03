#!/bin/bash

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
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

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh

# Run lwmake inside unix-build.
LWMAKE_CMD="${UNIX_BUILD} --no-devrel"
# Pass through DVS environment variables.
LWMAKE_CMD="${LWMAKE_CMD} --elwvar LW_DVS_BLD --elwvar CHANGELIST"
if [ ! -z "${AS2_SEC2_SKIP_SIGN_RELEASE}" ] ; then LWMAKE_CMD="${LWMAKE_CMD} --elwvar AS2_SEC2_SKIP_SIGN_RELEASE" ; fi
if [ ! -z "${SIGN_LOCAL}" ] ; then LWMAKE_CMD="${LWMAKE_CMD} --elwvar SIGN_LOCAL" ; fi
if [ ! -z "${SIGN_SERVER}" ] ; then LWMAKE_CMD="${LWMAKE_CMD} --elwvar SIGN_SERVER" ; fi
# Bind-mount extra directories.
# For some reason falcon GCC fails with SEGV if /proc isn't present.
LWMAKE_CMD="${LWMAKE_CMD} --extra ${LW_TOOLS}/falcon-gcc --extra /dvs/p4/selwresign --extra /proc"
# Actually ilwoke lwmake.
LWMAKE_CMD="${LWMAKE_CMD} lwmake"

${LWMAKE_CMD} clobber build release -j16  \
    LWMAKE_CMD="${LWMAKE_CMD}"            \
    DEBUG_SIGN_ONLY=true                  \
    USE_HS_ENCRYPTED_FOR=debug

${LWMAKE_CMD} clobber build release -j16  \
    LWMAKE_CMD="${LWMAKE_CMD}"            \
    DEBUG_SIGN_ONLY=false                 \
    SOECFG_PROFILE=soe-lr10               \          
    USE_HS_ENCRYPTED_FOR=prod

RC=$?
cd boot

if [ $RC -eq 0 -a -f ../tmpdbgsig.sh ]
then
  sh ../tmpdbgsig.sh
  RC=$?
fi

if [ $RC -eq 0 -a -f ../tmpprodsig.sh ]
then
  sh ../tmpprodsig.sh
  RC=$?
fi
cd ..

if [ $RC -eq 0 -a -f tmppostsig.sh ]
then
  /bin/sh tmppostsig.sh
  RC=$?
fi

#rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh

if [ $RC -ne 0 ]
then
  echo "*                                                   "
  echo "****************************************************"
  echo "*                                                  *"
  echo "*  Error:  SOE build has FAILED !!!               *"
  echo "*                                                  *"
  echo "****************************************************"
  echo "*                                                   "
fi

exit $RC
