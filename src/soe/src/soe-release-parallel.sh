#!/bin/bash
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

# Usage: Launch with or without command-line parameters to specify incremental
# or clean/clobber build.
# Examples: ./soe-release-parallel.sh to fire an incremental build
#           ./soe-release-parallel.sh clobber to clobber and build

# Note: Only one SOECFG_PROFILE. Parallel build doesn't buy anything yet.

# Spawn off threads in parallel. The caveat is that output will be interleaved.
# Another option is to redirect the output to log files, but that would involve
# the user having to check the log files for failures, which is not necessarily
# clean either.

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh

if [ -z $1 ] 
then
         echo "Using default DEBUG_SIGN_ONLY=true"
         debugonly=true
else
	if [ $1 == "prod" ] 
	then 
		echo "Using DEBUG_SIGN_ONLY=false"
   		debugonly=false
	else
   		echo "Using DEBUG_SIGN_ONLY=true"
   		debugonly=true
	fi
fi

lwmake soe build release -j16 SOECFG_PROFILE= USE_HS_ENCRYPTED_FOR=debug DEBUG_SIGN_ONLY=$debugonly SERIALIZE_PROFILES="false"

lwmake soe build release -j16 SOECFG_PROFILE= USE_HS_ENCRYPTED_FOR=prod DEBUG_SIGN_ONLY=$debugonly SERIALIZE_PROFILES="false"

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
  sh tmppostsig.sh
  RC=$?
fi

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh

if [ $RC -eq 0 ]
then
  echo "*                                                   "
  echo "****************************************************"
  echo "*                                                  *"
  echo "*  Success: All SOE builds finished successfully! *"
  echo "*                                                 *"
  echo "*   !!!  Do NOT forget to include ALL generated   *"
  echo "*   files with your vDVS submissions or with P4   *"
  echo "*   submissions into the release branches  !!!    *"
  echo "*                                                  *"
  echo "****************************************************"
  echo "*                                                   "
else
  echo "*                                                   "
  echo "****************************************************"
  echo "*                                                  *"
  echo "*  Error:  At least one SOE build has FAILED !!!  *"
  echo "*                                                  *"
  echo "****************************************************"
  echo "*                                                   "
fi
exit $RC
