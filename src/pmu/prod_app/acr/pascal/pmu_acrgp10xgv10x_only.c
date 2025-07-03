/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrgp10xgv10x_only.c
 * @brief  GP10X and GV10X only Access Control Region
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#if ((PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
#include "acr.h"
#else
#include "acr/pmu_acr.h"
#endif
#include "pmu_bar0.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_pwr_csb.h"
#include "dev_pmgr.h"
#include "dev_ltc.h"
#include "dev_pri_ringmaster.h"
#include "dev_pmgr_addendum.h"
#include "dev_ltc_addendum.h"

#include "config/g_acr_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
static FLCN_STATUS s_acrVerifyCbcBase_GP10X(LwU32 cbcBase)
    GCC_ATTRIB_SECTION("imem_acr", "s_acrVerifyCbcBase_GP10X");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*
 * Get the lwrrently-programmed VPR and WPR ranges, and check that these cannot
 * be accessed from the CBC base.
 */
static FLCN_STATUS
s_acrVerifyCbcBase_GP10X
(
    LwU32 cbcBase
)
{
    FLCN_STATUS status;
    LwU64       cbcBaseFbOffset;
    LwU32       numActiveLTCs;
    LwBool      bIsVprSupported = LW_FALSE;
    LwU64       vprMinStartAddr;
    LwU64       vprMaxEndAddr;
    LwU64       wprStartAddr;
    LwU64       wprEndAddr;
    LwU32       regionId;
    LwU64       bytesIn384MB = NUM_BYTES_IN_384_MB;
    LwU64       one = 1;
    LwU64       diffRegionFromCbc;

    numActiveLTCs = REG_FLD_RD_DRF(BAR0, _PPRIV,
                               _MASTER_RING_ENUMERATE_RESULTS_ROP_L2, _COUNT);

    // cbcBase is 26 bits, numActiveLTCs is 5 bits, so product is maximum of 31 bits.
    cbcBaseFbOffset = numActiveLTCs * cbcBase;
    lw64Lsl(&cbcBaseFbOffset, &cbcBaseFbOffset,
            LW_PLTCG_LTC0_LTS0_CBC_BASE_ALIGNMENT_SHIFT);

    // Find out the max VPR range possible on this chip.
    bIsVprSupported = LW_FALSE;
    status = acrGetMaxVprRange_HAL(&Acr, &bIsVprSupported,
                                   &vprMinStartAddr, &vprMaxEndAddr);

    //
    // If the check returns NOT_SUPPORTED it means that the VPR
    // range is not supported on this chip, and so cannot conflict
    // with the CBC base. In that case, consider it as OK.
    //
    if ((status != FLCN_OK) && (status != FLCN_ERR_NOT_SUPPORTED))
    {
        return status;
    }

    if (bIsVprSupported)
    {
        //
        // The CBC region can be a maximum of 384MB on Pascal.
        // Check that:
        // 1. CBC base is not within the VPR region
        // 2. If CBC region is below the VPR region, the VPR region
        //    must start at least 384MB above the CBC base.
        // 3. If the CBC region is above the VPR region, the VPR start
        //    must be least 384MB, to prevent a wrap-around attack.
        //
        if (lwU64CmpGE(&cbcBaseFbOffset, &vprMinStartAddr) &&
            lwU64CmpLE(&cbcBaseFbOffset, &vprMaxEndAddr))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        lw64Sub(&diffRegionFromCbc, &vprMinStartAddr, &cbcBaseFbOffset);
        if (lwU64CmpLT(&cbcBaseFbOffset, &vprMinStartAddr) &&
            lwU64CmpLT(&diffRegionFromCbc, &bytesIn384MB))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        if (lwU64CmpGT(&cbcBaseFbOffset, &vprMaxEndAddr) &&
            lwU64CmpLT(&vprMinStartAddr, &bytesIn384MB))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    // Check CBC base against all WPR ranges
    for (regionId = 1;
         regionId < LW_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_READ_WPR__SIZE_1;
         regionId++)
    {
        status = acrGetRegionDetails_HAL(&Acr, regionId,
                                         &wprStartAddr, &wprEndAddr,
                                         NULL, NULL);
        if (status != FLCN_OK)
        {
            return status;
        }

        //
        // acrGetRegionDetails sets wprEndAddr as the offset just after the
        // end of the WPR region.
        //
        lw64Sub(&wprEndAddr, &wprEndAddr, &one);

        if (lwU64CmpGE(&wprStartAddr, &wprEndAddr))
        {
            continue;
        }

        //
        // The CBC region can be a maximum of 384MB on Pascal.
        // Check that:
        // 1. CBC base is not within the WPR region
        // 2. If CBC region is below the WPR region, the WPR region
        //    must start at least 384MB above the CBC base.
        // 3. If the CBC region is above the WPR region, the WPR start
        //    must be least 384MB, to prevent a wrap-around attack.
        //
        if (lwU64CmpGE(&cbcBaseFbOffset, &wprStartAddr) &&
            lwU64CmpLE(&cbcBaseFbOffset, &wprEndAddr))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        lw64Sub(&diffRegionFromCbc, &wprStartAddr, &cbcBaseFbOffset);
        if (lwU64CmpLT(&cbcBaseFbOffset, &wprStartAddr) &&
            lwU64CmpLT(&diffRegionFromCbc, &bytesIn384MB))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        if (lwU64CmpGT(&cbcBaseFbOffset, &wprEndAddr) &&
            lwU64CmpLT(&wprStartAddr, &bytesIn384MB))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    return FLCN_OK;
}

/*!
 * Sanity check the CBC base address provided by RM against the VPR region
 * and write the base addr to chip if ok.
 *
 * @param [in]  cbcBase   The CBC base value to write to the CBC_BASE register.
 * @return      FLCN_OK    : If backing store base addr is successfully written.
 * @return      FLCN_ERR_ILWALID_ARGUMENT : If there was a conflict between the base addr provided
 *                          by RM with the VPR region, and the addr is rejected.
 */
FLCN_STATUS
acrWriteCbcBase_GP10X
(
    LwU32 cbcBase
)
{
    FLCN_STATUS     status = FLCN_OK;
    LwU32           bCbcBaseWritten;
    LwU32           scratchVal;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libAcr)
    };

    /* The CBC base cannot be written if the PMU is not in LS mode */
    if ((REG_FLD_RD_DRF(CSB, _CPWR, _FALCON_SCTL, _LSMODE) !=
         LW_CPWR_FALCON_SCTL_LSMODE_TRUE) ||
        !acrCanReadWritePmgrSelwreScratch_HAL(&Acr))
    {
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    bCbcBaseWritten = REG_FLD_TEST_DRF_DEF(BAR0, _PMGR, _SELWRE_WR_SCRATCH_3,
                                   _PMU_CBC_BASE_WRITTEN, _TRUE);
    if (bCbcBaseWritten)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Truncate cbcBase if it's too large to fit in the field
        cbcBase &= DRF_SHIFTMASK(LW_PLTCG_LTCS_LTSS_CBC_BASE_ADDRESS);

        if ((status = s_acrVerifyCbcBase_GP10X(cbcBase)) != FLCN_OK)
        {
            goto acrWriteCbcBase_exit;
        }

        scratchVal = REG_RD32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_3);
        scratchVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3, _PMU_CBC_BASE_WRITTEN,
                                 _TRUE, scratchVal);

        if (s_acrVerifyCbcBase_GP10X(DRF_DEF(_PLTCG, _LTCS_LTSS_CBC_BASE,
                                            _ADDRESS, _ZERO)) == FLCN_OK)
        {
            scratchVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3,
                                     _PMU_CBC_BASE_ZERO_PERMITTED, _TRUE,
                                     scratchVal);
        }
        else
        {
            scratchVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3,
                                     _PMU_CBC_BASE_ZERO_PERMITTED, _FALSE,
                                     scratchVal);
        }

        // All checks done, and the CBC base address looks legit. Let's write it out.
        REG_WR32(BAR0, LW_PLTCG_LTCS_LTSS_CBC_BASE, cbcBase);

        REG_WR32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_3, scratchVal);

acrWriteCbcBase_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * Reset the CBC_BASE register back to its init value. This should be called
 * during cleanup.
 */
void
acrResetCbcBase_GP10X(void)
{
    LwU32  scratchVal;

    if ((REG_FLD_RD_DRF(CSB, _CPWR, _FALCON_SCTL, _LSMODE) !=
         LW_CPWR_FALCON_SCTL_LSMODE_TRUE) ||
        !acrCanReadWritePmgrSelwreScratch_HAL(&Acr))
    {
        return;
    }

    scratchVal = REG_RD32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_3);

    if (FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_3, _PMU_CBC_BASE_ZERO_PERMITTED,
                     _TRUE, scratchVal))
    {
        REG_WR32(BAR0, LW_PLTCG_LTCS_LTSS_CBC_BASE,
                 DRF_DEF(_PLTCG, _LTCS_LTSS_CBC_BASE, _ADDRESS, _ZERO));
    }

    scratchVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3, _PMU_CBC_BASE_ZERO_PERMITTED, _FALSE,
                scratchVal);
    scratchVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3, _PMU_CBC_BASE_WRITTEN, _FALSE,
                scratchVal);

    REG_WR32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_3, scratchVal);
}

