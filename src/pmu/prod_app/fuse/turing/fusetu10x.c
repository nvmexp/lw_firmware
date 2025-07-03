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
 * @brief Retrieve the number of enabled TPCs.
 *
 * @param[OUT] pNumTpc   Number of enabled TPCs.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pNumTpc is NULL.
 * @return FLCN_OK                     If the number of TPCs was successfully retrieved.
 */
FLCN_STATUS
fuseNumTpcGet_TU10X
(
    LwU16   *pNumTpc
)
{
    LwU32       gpcEnableMask;
    LwU32       maxNumGpcs;
    LwU32       tpcEnableMask;
    LwU32       maxNumTpcsPerGpc;
    LwU32       gpcIdx;
    LwU16       numEnabledTpcs = 0;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pNumTpc == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto fuseNumTpcGet_TU10X_exit;
    }

    // Get the maximum number of TPCs per GPCs per the design spec.
    maxNumGpcs       = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_GPCS, _VALUE);
    maxNumTpcsPerGpc = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_TPC_PER_GPC, _VALUE);

    // Iterate over all GPCs and count the number of enabled TPCs.
    gpcEnableMask = (LWBIT32(maxNumGpcs) - 1) & (~fuseRead(LW_FUSE_STATUS_OPT_GPC));
    FOR_EACH_INDEX_IN_MASK(32, gpcIdx, gpcEnableMask)
    {
            tpcEnableMask   = (LWBIT32(maxNumTpcsPerGpc) - 1) &
                              (~fuseRead(LW_FUSE_STATUS_OPT_TPC_GPC(gpcIdx)));
            NUMSETBITS_32(tpcEnableMask);

            numEnabledTpcs += tpcEnableMask;
    }
    FOR_EACH_INDEX_IN_MASK_END

    *pNumTpc = numEnabledTpcs;

fuseNumTpcGet_TU10X_exit:
    return status;
}

/*!
 * @brief Retrieve the number of enabled FBPs.
 *
 * @param[OUT] pNumTpc   Number of enabled FBPs.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pNumFbp is NULL.
 * @return FLCN_OK                     If the number of LTC slices was successfully returned.
 */
FLCN_STATUS
fuseNumFbpGet_TU10X
(
    LwU8   *pNumFbp
)
{
    LwU32       maxNumFbp;
    LwU32       fbpEnableMask;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pNumFbp == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto fuseNumFbpGet_TU10X_exit;
    }

    // Get the number of enabled FBPs on the GPU.
    maxNumFbp     = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_FBPS, _VALUE);
    fbpEnableMask = (LWBIT32(maxNumFbp) - 1) & (~fuseRead(LW_FUSE_STATUS_OPT_FBP));
    NUMSETBITS_32(fbpEnableMask);
    *pNumFbp      = fbpEnableMask;

fuseNumFbpGet_TU10X_exit:
    return status;
}
