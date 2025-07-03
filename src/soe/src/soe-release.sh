#!/bin/bash

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh

if [ -z $1 ] 
then
    echo "Using default DEBUG_SIGN_ONLY=true"
    lwmake clobber build release -j16  \
        USE_HS_ENCRYPTED_FOR=debug DEBUG_SIGN_ONLY=true
else
    if [ $1 == "prod" ] 
    then 
        echo "Using DEBUG_SIGN_ONLY=false"
        lwmake clobber build release -j16  \
            SOECFG_PROFILE=soe-lr10 \
            USE_HS_ENCRYPTED_FOR=prod DEBUG_SIGN_ONLY=false
    else
        echo ERROR: Bad input parameters. Can only be run with ./soe-release.sh for debug fused ucode or ./soe-release.sh prod for prod fused ucode
        exit 1 # terminate and indicate error
    fi
fi

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
