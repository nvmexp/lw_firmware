#!/bin/sh
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

# Usage: Launch with or without command-line parameters to specify incremental
# or clean/clobber build.
# Examples: ./dpu-release-parallel.sh to fire an incremental build
#           ./dpu-release-parallel.sh clobber to clobber and build

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh
lwmake dpu build release -j16 DPUCFG_PROFILE= DPUCFG_PROFILES= SERIALIZE_PROFILES="false" "$@"

RC=$?

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

if [ $RC -eq 0 ]
then
  echo "*                                                  "
  echo "***************************************************"
  echo "*                                                 *"
  echo "*  Success: All DPU builds finished successfully! *"
  echo "*                                                 *"
  echo "*   !!!  Do NOT forget to include ALL generated   *"
  echo "*   files with your vDVS submissions or with P4   *"
  echo "*   submissions into the release branches  !!!    *"
  echo "*                                                 *"
  echo "***************************************************"
  echo "*                                                  "
else
  echo "*                                                  "
  echo "***************************************************"
  echo "*                                                 *"
  echo "*  Error:  At least one DPU build has FAILED !!!  *"
  echo "*                                                 *"
  echo "***************************************************"
  echo "*                                                  "
fi
