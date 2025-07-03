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
RC=$?
if [ $RC -eq 0 -a -f $1/tmpsign.sh ]
then
  sh $1/tmpsign.sh
  rm -f $1/tmpsign.sh
  RC=$?
fi

exit $RC



