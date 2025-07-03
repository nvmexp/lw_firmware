/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: BSI related functions are handled
 */

/* ------------------------- System Includes -------------------------------- */
#include "selwrescrubovl.h"
#include "dev_fb.h"
#include "dev_lwdec_csb.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"
#include "rmselwrescrubif.h"
#include "rmbsiscratch.h"
#include "selwrescrubreg.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define NEXT_OFFSET(offset,i)                           (offset+(i*sizeof(LwU32)))
/*-------------------------Function Prototypes--------------------------------*/

/*!
 * @brief: Return the VPR range saved in BSI secure scratch registers.
 */
SELWRESCRUB_RC
selwrescrubGetVPRRangeFromBsiScratch_GP10X(PVPR_ACCESS_CMD pVprCapsBsiScratch)
{
    LwU64 vprSizeInBytes;

    //
    // The following was producing incorrect results. Breaking the math into 2 steps eliminated the problem
    // pVprCapsBsiScratch->vprRangeEndInBytes    = pVprCapsBsiScratch->vprRangeStartInBytes +
    //                                                 (((LwU64)REG_RD_DRF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB)) << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT) - 1;
    //

    vprSizeInBytes = (((LwU64)REG_RD_DRF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB)) << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT);

    if (vprSizeInBytes)
    {
        pVprCapsBsiScratch->vprRangeStartInBytes  = REG_RD_DRF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_MAX_RANGE_START_ADDR_MB_ALIGNED);
        pVprCapsBsiScratch->vprRangeStartInBytes  <<= VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;
        pVprCapsBsiScratch->vprRangeEndInBytes    = pVprCapsBsiScratch->vprRangeStartInBytes + vprSizeInBytes - 1;
        pVprCapsBsiScratch->bVprEnabled           = LW_TRUE;
    }
    else
    {
        pVprCapsBsiScratch->bVprEnabled             = LW_FALSE;
        pVprCapsBsiScratch->vprRangeStartInBytes    = 0;
        pVprCapsBsiScratch->vprRangeEndInBytes      = 0;
    }

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief: Return the size of memory, in MB, synchronously scrubbed by vbios just above the VPR range that is asyncronously scrubbed.
 * NOTE: It is assumed that this memory is adjoining the VPR range left in MMU registers by vbios. No gap is expected else it
 * will lead to a security hole. Vbios guarantees this.
 */
SELWRESCRUB_RC
selwrescrubGetVbiosVprSyncScrubSize_GP10X(LwU32 *pSyncScrubSizeMB)
{
    if (!pSyncScrubSizeMB)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    *pSyncScrubSizeMB = REG_RD_DRF(BAR0, _THERM, _SELWRE_WR_SCRATCH_7, _SYNC_SCRUBBED_REGION_SIZE_MB_AT_VPR_END);
    return SELWRESCRUB_RC_OK;
}

