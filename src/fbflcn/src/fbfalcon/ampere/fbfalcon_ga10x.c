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
#include "dev_fbfalcon_csb.h"

extern LwU8 gPlatformType;
extern LwU8 gFbflcnLsMode;

LwBool
fbfalconIsHalfPartitionEnabled_GA102
(
        LwU8 partition
)
{
    LwU32 halfPartitionBit = (localEnabledFBPAHalfPartitionMask &  (0x1 << partition));
    return (halfPartitionBit != 0);
}
