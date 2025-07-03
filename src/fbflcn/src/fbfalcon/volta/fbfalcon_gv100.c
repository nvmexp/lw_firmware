 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include "fbflcn_helpers.h"
#include "fbflcn_defines.h"
#include "priv.h"

// include manuals
#include "dev_fbpa.h"
#include "dev_fuse.h"


//
// detectPartitions  (HAL's)
//  reads partition count, partition mask and subpartiton masks and performs a
//  quick consistency check
//

void
fbfalconDetectPartitions_GV100
(
        void
)
{
    // Read fuse to detect the available partition
    LwU32 disabledFBPAMask = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_FBPA);
    gDisabledFBPAMask = disabledFBPAMask;           // rhia is legacy code for gv100
    localEnabledFBPAMask = ~disabledFBPAMask;
    // detect the lowest partition as entry point for HBM serial priv
    detectLowestIndexValidPartition();
}

