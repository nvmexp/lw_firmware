/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fbhbm2gv10xonly.c
 * @brief  FB Hal Functions for HBM on GV100-only.
 *
 * FB Hal Functions implement FB related functionalities for GV100-only.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objfb.h"
#include "pmu_objfuse.h"
#include "config/g_fb_private.h"

/* ------------------------- Static function Prototypes ---------------------*/
/* ------------------------- Macros -----------------------------------------*/
#define FB_FBPS_PER_HBM_SITE_CNT  2

/*!
 * @brief Gets the FBPA index corresponding to the first non floorswept FBPA
 *        on a given HBM site.
 *
 * This function gets the index corresponding to the first non floorswept
 * FBPA on a given HBM site
 *
 * @param[in] siteIndex     HBM site index
 * 
 */
LwU8
fbHBMSiteNonFloorsweptFBPAIdxGet_GV10X
(
    LwU8 siteIndex
)
{
    LwU32 fbpaCount;
    LwU32 activeFBPAMask;
    LwU32 numFBPAPerFBP;
    LwU32 numFBPAPerSite;
    LwU32 index;

    // Retrieve the total number of FBPAs.
    fbpaCount = REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPAS);
    fbpaCount = DRF_VAL(_PTOP, _SCAL_NUM_FBPAS, _VALUE, fbpaCount);

    // Retrieve number of FBPAs per FBP.
    numFBPAPerFBP = REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPA_PER_FBP);
    numFBPAPerFBP = DRF_VAL(_PTOP, _SCAL_NUM_FBPA_PER_FBP, _VALUE, numFBPAPerFBP);

    numFBPAPerSite = numFBPAPerFBP * FB_FBPS_PER_HBM_SITE_CNT;

    // Retrieve the active FBPA bitmask.
    activeFBPAMask = fuseRead(LW_FUSE_STATUS_OPT_FBPA);
    activeFBPAMask = DRF_VAL(_FUSE, _STATUS_OPT_FBPA, _DATA, activeFBPAMask);

    // Value is flipped since the value of an enabled FBPA is 0 in HW.
    activeFBPAMask = ~activeFBPAMask;

    // Mask away disabled FBPAs.
    activeFBPAMask &= (BIT(fbpaCount) - 1);

    for (index = siteIndex * numFBPAPerSite; 
         index < ((siteIndex + 1) * numFBPAPerSite); index++)
    {
        if ((BIT(index) & activeFBPAMask) != 0)
        {
            return index;
        }
    }

    return FB_FBPA_INDEX_ILWALID;
}
