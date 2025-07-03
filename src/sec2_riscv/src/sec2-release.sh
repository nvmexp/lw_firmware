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

rm -f tmpprodsig.sh tmpdbgsig.sh tmppostsig.sh
lwmake clean build release -j16 SEC2CFG_PROFILE=  \
                                SEC2CFG_PROFILES= 

