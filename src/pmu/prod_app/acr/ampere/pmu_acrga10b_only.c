/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrga10b_only.c
 * @brief  GA10B Access Control Region
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)
#include "acr.h"
#else
#include "acr/pmu_acr.h"
#include "acr/pmu_acrutils.h"
#endif
#include "dev_fb.h"
#include "pmu_bar0.h"
#include "config/g_acr_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Setting default alignment to 128KB
 */
#define LW_ACR_DEFAULT_ALIGNMENT        0x20000

// WPR indexes used while referring g_desc.regions.regionProps which is filled by RM
#define ACR_WPR1_REGION_IDX               (0x0)
#define ACR_WPR2_REGION_IDX               (0x1)

/*!
 * @brief acrGetRegionDetails_GA10B
 *          returns ACR region details
 *
 * @param [in]  regionID ACR region ID
 * @param [out] pStart   Start address of ACR region
 * @param [out] pEnd     End address of ACR region
 * @param [out] pRmask   Read permission of ACR region
 * @param [out] pWmask   Write permission of ACR region
 *
 * @return      FLCN_OK : If proper register details are found
 *              FLCN_ERR_ILWALID_ARGUMENT : invalid Region ID
 */
FLCN_STATUS
acrGetRegionDetails_GA10B
(
    LwU32      regionID,
    LwU64      *pStart,
    LwU64      *pEnd,
    LwU16      *pRmask,
    LwU16      *pWmask
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32      numAcrRegions;
    LwU32      regVal;
    numAcrRegions = (LW_PFB_PRI_MMU_WPR_ALLOW_READ_WPR__SIZE_1 - 1);

    // Region Id 0 is unprotected FB, hence will lead to failues.
    if ((regionID == 0) || (regionID > numAcrRegions))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    if (!pStart || !pEnd)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if ((regionID - 1) == ACR_WPR1_REGION_IDX)
    {
        // Read start address for WPR1
        //CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR1_ADDR_LO, &regVal));
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, regVal);
        *pStart = (LwU64)regVal << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT;
        // Read end address for WPR1
        //CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR1_ADDR_HI, &regVal));
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, regVal);
        *pEnd = (LwU64)regVal << LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT;
    }
    else if ((regionID - 1) == ACR_WPR2_REGION_IDX)
    {
        // Read start address for WPR2
        //CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR2_ADDR_LO, &regVal));
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, regVal);
        *pStart = (LwU64)regVal << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;
        // Read end address for WPR2
        //CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR2_ADDR_HI, &regVal));
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, regVal);
        *pEnd = (LwU64)regVal << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;
    }
    else
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check for a valid ACR region
    if (*pStart <= *pEnd)
    {
        // End address always point to start of the last aligned address
        lw64Add32_MOD(pEnd, LW_ACR_DEFAULT_ALIGNMENT);
    }
    else
    {
        // Not a valid ACR region.
        flcnStatus = FLCN_ERROR;
        goto ErrorExit;
    }

    // Read ReadMask
    if (pRmask != NULL)
    {
        //CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_ALLOW_READ, &regVal));
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
        *pRmask = (LwU16)DRF_IDX_VAL(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR, regionID, regVal);
    }

    // Read WriteMask
    if (pWmask != NULL)
    {
        //CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, &regVal));
        regVal = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
        *pWmask = (LwU16)DRF_IDX_VAL(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR, regionID, regVal);
    }

ErrorExit:
    if (flcnStatus != FLCN_OK)
    {
        // Ilwalidate start and end addresses
        *pStart = 0;
        *pEnd   = 0;
    }
    return flcnStatus;
}
