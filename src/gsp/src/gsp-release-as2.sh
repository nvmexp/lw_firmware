#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH"

./gsp-release.sh
if [ "$?" -ne "0" ]; then
    exit 1
fi

p4 revert -a ../../../drivers/resman/kernel/inc/gsp/bin/g_gsp*
if [ "$?" -ne "0" ]; then
    exit 1
fi
