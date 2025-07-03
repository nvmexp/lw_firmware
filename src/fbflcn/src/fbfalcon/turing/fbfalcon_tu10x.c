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
#include "memory.h"

#include <rmlsfm.h>

//
// detectPartitions  (HAL's)
//  reads partition count, partition mask and subpartiton masks and performs a
//  quick consistency check
//

// for more information about fbpa/fbio fuses check out https://confluence.lwpu.com/display/GFWTU10X/Fuses
// with the differnece in gddr and hbm the grouping of fbpa's to sites the use of fbbio fuses is more consistent.
// The fbfalcon starting with TU10x is using LW_FUSE_STATUS_OPT_FBIO


LwBool
fbfalconIsHalfPartitionEnabled_TU102
(
        LwU8 partition
)
{
    LwU32 halfPartitionBit = ((localEnabledFBPAHalfPartitionMask &  (0x1 << (partition*2)) ) >> (partition*2)) & 0x1;
    return (halfPartitionBit != 0);
}


void
fbfalconDetectPartitions_TU102
(
		void
)
{
	// get number of partitions // 0x120074
	gNumberOfPartitions = REG_RD32(BAR0, LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP);
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
    // FIXME, should be replaced with a spec define for the number of pa's per fbp
    gNumberOfPartitions *= 2;   // PA per FBP  add define for this later
#endif
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

		//if we pass the number of available paritions the rest of the partition
		// mask is turned off.  needed since we don't know max_partitions for litter
		if (partitions > gNumberOfPartitions)
		{
			localEnabledFBPAMask = localEnabledFBPAMask & ( ~(0x1 << i));
		}
	}
	// check that partition mask is matching the advertised
	// number of partitions, skip if partitions is 0
	LwBool bError = (partitions < gNumberOfPartitions);
	if (bError && (partitions != 0))
	{
		FW_MBOX_WR32_STALL(11, partitions);
		FW_MBOX_WR32_STALL(12, gNumberOfPartitions);
		FW_MBOX_WR32_STALL(13, localEnabledFBPAMask);
		FBFLCN_HALT(FBFLCN_ERROR_CODE_PARTITION_CNT_AND_MASK_MISSMATCH)
	}

	// detect the lowest partition as entry point for HBM serial priv
	detectLowestIndexValidPartition();
}


/*!
 * @brief Verifies this Falcon is running in secure mode
 *
 *
 * Bug: https://lwbugswb.lwpu.com/LwBugs5/HWBug.aspx?bugid=2398590&cmtNo=
 *      https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=200446950&cmtNo=
 */

void
fbfalcolwerifyFalconSelwreRunMode_TU102
(
        void
)
{
    // the handshake value is lwrrently not defined globally
    static LwU32 SELWRE_HANDSHAKE_VALUE_ACR_ENABLED = 0x1;

    // load ACR handshake value from Mailbox0
    LwU32 mailbox = REG_RD32(CSB, LW_CFBFALCON_FALCON_MAILBOX0);

    // if handshake is not matching allow to continue
    if (mailbox != SELWRE_HANDSHAKE_VALUE_ACR_ENABLED)
    {
        return;
    }

    // Verify that we are in LSMODE
    LwU32 fbflcnSctl = REG_RD32(CSB, LW_CFBFALCON_FALCON_SCTL);

    if (FLD_TEST_DRF(_CFBFALCON, _FALCON_SCTL, _LSMODE, _TRUE, fbflcnSctl))
    {
        // Verify that UCODE_LEVEL > the init value
        if (DRF_VAL(_CFBFALCON, _FALCON_SCTL, _UCODE_LEVEL,  fbflcnSctl) >
              LW_CFBFALCON_FALCON_SCTL_UCODE_LEVEL_INIT)
        {
            //
            // Verify that the FBIF REGIONCTL for TRANSCFG(ucode) ==
            // LSF_WPR_EXPECTED_REGION_ID
            //
            CASSERT(LSF_BOOTSTRAP_CTX_DMA_FBFLCN == 0,revocation_c);

            LwU32 fbflcnFbifCfg = REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG);
            if (DRF_VAL(_CFBFALCON, _FBIF_REGIONCFG, _T0, fbflcnFbifCfg) == LSF_WPR_EXPECTED_REGION_ID)
            {
                //
                // FBFalcon IS secure.
                //

                return;
            }
        }
    }

    // FBFalcon IS NOT secure.

    // Write a falcon mode token to signal the inselwre condition.
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_MAILBOX0, LSF_FALCON_MODE_TOKEN_FLCN_INSELWRE);

    // Then halt
    FW_MBOX_WR32(13, fbflcnSctl);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_ACR_WITHOUT_LS_ERROR);
}
