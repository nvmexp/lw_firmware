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
#include "dev_fbpa.h"
#include "dev_fuse.h"
#include "dev_pri_ringmaster.h"
#include "dev_lw_xve.h"
#include "memory.h"

#include <rmlsfm.h>

void
fbfalconDetectPartitions_GH100
(
		void
)
{
    // Read fuse to detect the available partition  // 0x217fc
    // only has disabled partition bits set, invalid partitions are not set
	localDisabledFbioMask = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_FBIO);
	localEnabledFBPAMask = ~localDisabledFbioMask;
    // clear invalid upper bits
    localEnabledFBPAMask &= (0xFFFFFFFF >> (32-MAX_PARTS));

#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
	// read half partition // 0x21c00
	// the half partition mask indicates if the partition is in 32-bit more or not (64-bit)
	localEnabledFBPAHalfPartitionMask = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_HALF_FBPA);
#else
	localEnabledFBPAHalfPartitionMask = 0;
	gDisabledFBPAMask = localDisabledFbioMask;
#endif
	// check that setup is consistent
	LwU8 partitions = 0;
	LwU8 i;
	for(i=0; i < MAX_PARTS; i++)
	{
		// extract partition bit
		LwU32 p = localEnabledFBPAMask & (0x1 << i);

		if (p != 0x0)
		{
			partitions++;
		}
	}
    // removed extra partitions < gNumberOfPartitions check
    gNumberOfPartitions = partitions;

	// detect the lowest partition as entry point for HBM serial priv
	detectLowestIndexValidPartition();
}
