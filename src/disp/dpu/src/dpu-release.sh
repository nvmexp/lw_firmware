#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh
lwmake clobber build release -j16  \
    DPUCFG_PROFILE=                \
    DPUCFG_PROFILES=

RC=$?

# Call bootloader tmpsign, if it exists
if [ $RC -eq 0 -a -f ../../../bootloader/src/tmpsign.sh ]
then
  sh ../../../bootloader/src/tmpsign.sh
  rm -f ../../../bootloader/src/tmpsign.sh
  RC=$?
fi

if [ $RC -eq 0 -a -f tmpdbgsig.sh ]
then
  sh tmpdbgsig.sh
  RC=$?
fi

if [ $RC -eq 0 -a -f tmpprodsig.sh ]
then
  sh tmpprodsig.sh
  RC=$?
fi

if [ $RC -eq 0 -a -f tmppostsig.sh ]
then
  sh tmppostsig.sh
  RC=$?
fi

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh

if [ $RC -ne 0 ]
then
  echo "*                                                  "
  echo "***************************************************"
  echo "*                                                 *"
  echo "*  Error:  DPU build has FAILED !!!               *"
  echo "*                                                 *"
  echo "***************************************************"
  echo "*                                                  "
fi
