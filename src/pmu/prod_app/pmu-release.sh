#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2009-2014 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

lwmake pmu clobber build release -j16  \
    PMUCFG_PROFILE=                    \
    PMUCFG_PROFILES=

