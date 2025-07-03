#!/bin/sh

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

set -e

lwmake clobber build release "$@"

