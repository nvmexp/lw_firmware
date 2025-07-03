#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2009-2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

make LW_TARGET_ARCH=falcon4 MANUAL_PATHS=$RESMAN_ROOT/kernel/inc/fermi/gf119 clobber build

