/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_top.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"
#include "config/g_fuse_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief Retrieve the number of enabled LTC slices.
 *
 * @param[OUT] pNumLtc   Number of enabled LTC slices..
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pNumLtcs is NULL.
 * @return FLCN_OK                     If the number of LTC slices was succesfully returned.
 */
FLCN_STATUS
fuseNumLtcsGet_GA10X
(
    LwU16   *pNumLtcs
)
{
    LwU32       fbpEnableMask;
    LwU32       maxNumLtcsPerFbp;
    LwU32       maxNumFbp;
    LwU32       ltcsEnableMask;
    LwU32       fbpIdx;
    LwU16       numLtcs = 0;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pNumLtcs == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto fuseNumLtcsGet_GA10X_exit;
    }

    // Compute the maximum number of LTC slices per FBP per the chip spec.
    maxNumLtcsPerFbp = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_LTC_PER_FBP, _VALUE) *
                       REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_SLICES_PER_LTC, _VALUE);

    // Iterate over all FBPs and accumulate the number of LTC slices.
    maxNumFbp        = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_FBPS, _VALUE);
    fbpEnableMask    = (LWBIT32(maxNumFbp) - 1) & (~fuseRead(LW_FUSE_STATUS_OPT_FBP));
    FOR_EACH_INDEX_IN_MASK(32, fbpIdx, fbpEnableMask);
    {
        ltcsEnableMask = (LWBIT32(maxNumLtcsPerFbp) - 1) &
                         (~fuseRead(LW_FUSE_STATUS_OPT_L2SLICE_FBP(fbpIdx)));
        NUMSETBITS_32(ltcsEnableMask);

        numLtcs       += ltcsEnableMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    *pNumLtcs = numLtcs;

fuseNumLtcsGet_GA10X_exit:
    return status;
}

/*!
 * @brief Retrieve the number of enabled GPCs.
 *
 * @param[OUT] pNumGpcs   Number of enabled GPCs
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pGpcs is NULL.
 * @return FLCN_OK                     If the number of GPCs succesfully returned.
 */
FLCN_STATUS
fuseNumGpcsGet_GA10X
(
    LwU16   *pNumGpcs
)
{
    LwU32       maxNumGpcs;
    LwU32       gpcsEnableMask;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pNumGpcs == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto fuseNumGpcsGet_GA10X_exit;
    }

    // Iterate over all GPCs and accumulate the number of LTC slices.
    maxNumGpcs        = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_GPCS, _VALUE);
    gpcsEnableMask    = (LWBIT32(maxNumGpcs) - 1) & (~fuseRead(LW_FUSE_STATUS_OPT_GPC));

    NUMSETBITS_32(gpcsEnableMask);

    *pNumGpcs = gpcsEnableMask;

fuseNumGpcsGet_GA10X_exit:
    return status;
}
