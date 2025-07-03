/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fub_auto_qs2_lwflash.c
 */
/* ------------------------- System Includes -------------------------------- */
#include "fub.h"
#include "fubreg.h"
#include "config/g_fub_private.h"
#include "dev_fuse.h"
#include "dev_fpf.h"
#include "fub_inline_static_funcs_tu10x.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "fub_inline_static_funcs_fpf_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu116.h"
#include "fub_inline_static_funcs_tu116.h"
#endif
#include "dev_se_seb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern RM_FLCN_FUB_DESC  g_desc;
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE))
/*
 * @brief: Check to verify that the board is auto sku board.
 * @param[out]: pbIsFuseBurningApplicable True when the board is auto sku board else false.
 * @return FUB_OK if the board is auto sku board, error otherwise.
 */
FUB_STATUS
fubCheckIfAutoQs2LwflashIsApplicable_TU104
(
    LwBool *pbIsFuseBurningApplicable
)
{
    FUB_STATUS  fubStatus   = FUB_OK;
    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }
    *pbIsFuseBurningApplicable = LW_FALSE;
    //
    // Check if use cases passed by RegKey includes burning fuse for LWFLASH QS1 to QS2.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }


    // Check first if AUTO_QS2_LWFLASH  fuse  is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2IsApplicableCommonChecks_HAL(pFub, pbIsFuseBurningApplicable));

    if (!*pbIsFuseBurningApplicable)
    {
        fubReportApplicabilityFailure_HAL(pFub, LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE);
    }

ErrorExit:
    return fubStatus;
}

/*
 * @brief: Check if Fuse for enabling ASB QS2 version is to be burnt and burn if check passes.
 * @param[out]: pfubUseCasMask Update this mask if fuse burn was successfull.
 * @return FUB_OK fuse burn is successfull, error otherwise.
 */
FUB_STATUS
fubCheckAndBurnAutoQs2LwflashFuse_TU104
(
    LwU32 *pfubUseCasMask
)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE)
    {
        CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_LWFLASH_ALREADY_SET_IN_MASK);
    }

    // Check first if AUTO_QS2_LWFLASH  fuse  is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2LwflashIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif
