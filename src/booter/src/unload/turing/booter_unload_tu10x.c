/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_unload_tu10x.c
 */
//
// Includes
//
#include "booter.h"

#include "dev_gsp.h"

BOOTER_STATUS
booterUnlockWpr2_TU10X(void)
{
    LwU32 data;

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, ILWALID_WPR_ADDR_HI, data);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, data);

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, ILWALID_WPR_ADDR_LO, data);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, data);

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2, BOOTER_UNLOCK_READ_MASK, data);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2, BOOTER_UNLOCK_WRITE_MASK, data);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);

    return BOOTER_OK;
}

BOOTER_STATUS
booterPutFalconInReset_TU10X(void)
{
    LwU32 data = 0;

    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_ENGINE);
    data = FLD_SET_DRF(_PGSP, _FALCON_ENGINE, _RESET, _TRUE, data);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_ENGINE, data);

    return BOOTER_OK;
}

BOOTER_STATUS
booterBringFalconOutOfReset_TU10X(void)
{
    LwU32 data = 0;

    // Deassert Engine Reset
    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_ENGINE);
    data = FLD_SET_DRF(_PGSP, _FALCON_ENGINE, _RESET, _FALSE, data);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_ENGINE, data);

    return BOOTER_OK;
}


// TODO(suppal): Consolidate with other reset function
BOOTER_STATUS
booterFullResetGsp_TU10X(void)
{
    BOOTER_STATUS status = BOOTER_OK;
#ifndef PRI_SOURCE_ISOLATION_PLM // Only for GA10x
    LwU32 data = 0;
#endif

    // 
    // Step 1: Bring falcon out of reset so that we are able to access its registers
    // It could be the case that falcon was already in reset
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterBringFalconOutOfReset_HAL(pBooter));

    // 
    // Step 2: Set _RESET_LVLM_EN to TRUE to ensure all the falcon PLMs will be reset.
    // If we don't do this step, most of falcon will get reset but PLMs will stay as is
    // We are looking to fully reset the falcon here.
    //
#ifndef PRI_SOURCE_ISOLATION_PLM // Only for GA10x
    data = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_SCTL);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, data);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_SCTL, data);
#endif

    //
    // Step 3: Put the falcon into reset
    // NOTE: The call below will automatically pull falcons that don't have either
    //       LW_PMC_ENABLE.<FALCON> or FALCON_ENGINE_RESET out of reset
    //
    //CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterPreEngineResetSequenceBug200590866_HAL(pBooter)); //TODO for GA10X
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterPutFalconInReset_HAL(pBooter));


    //
    // Step 4: Bring the falcon back from reset state
    // So that later non-secure usage of falcon can be done
    // NOTE: We would have modified FALCON_SCTL_RESET_LVLM_EN regardless
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterBringFalconOutOfReset_HAL(pBooter));

    //CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCoreSelect_HAL(pBooter)); // TODO for GA10X

    //CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterPostEngineResetSequenceBug200590866_HAL(pBooter)); // TODO for GA10X

    // TODO: No need to poll for scrubbing during exit?
 
    return status;
}

/*!
 * @brief Main function to do Booter unload process
 */
BOOTER_STATUS booterUnload_TU10X(void)
{
    BOOTER_STATUS status;
    LwU32 i;

    // Check handoffs with other Booters / ACR
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckBooterHandoffsInSelwreScratch_HAL(pBooter));

    // Ensure WPR1 is torn down
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckIfWpr1IsTornDown_HAL(pBooter));

    // Full Reset GSP engine
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterFullResetGsp_HAL(pBooter));

    // Ilwalidate SEC sub-WPRs
    for (i = 0; i < LW_PFB_PRI_MMU_FALCON_SEC_CFGA__SIZE_1; i++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprSec_HAL(pBooter,
            i, LW_FALSE, 0, 0, BOOTER_UNLOCK_READ_MASK, BOOTER_UNLOCK_WRITE_MASK));
    }

    // Ilwalidate GSP sub-WPRs
    for (i = 0; i < LW_PFB_PRI_MMU_FALCON_GSP_CFGA__SIZE_1; i++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprGsp_HAL(pBooter,
            i, LW_FALSE, 0, 0, BOOTER_UNLOCK_READ_MASK, BOOTER_UNLOCK_WRITE_MASK));
    }

    // Ilwalidate LWDEC sub-WPR used by Booter Reload
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprLwdec_HAL(pBooter,
        LWDEC0_SUB_WPR_ID_3_GSPRM_BOOTER_WPR2, LW_FALSE,
        0, 0, BOOTER_UNLOCK_READ_MASK, BOOTER_UNLOCK_WRITE_MASK));

    // Now unloack WPR2
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterUnlockWpr2_HAL(pBooter));

    // Now clear all handoff bits
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterWriteBooterHandoffsToSelwreScratch_HAL(pBooter));

    return status;
}

