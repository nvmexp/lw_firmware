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
fuseNumLtcsGet_TU10X
(
    LwU16   *pNumLtcs
)
{
    LwU32       maxNumFbp;
    LwU32       fbpEnabledMask;
    LwU32       maxNumLtcsPerFbp;
    LwU32       reg;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pNumLtcs == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto fuseNumLtcsGet_TU10X_exit;
    }

    // Compute the maximum number of LTC slices per FBP per the chip spec.
    maxNumLtcsPerFbp = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_LTC_PER_FBP, _VALUE);

    // If half LTC mode is enabled, then half of the LTC slices in each LTC is disabled.
    reg = fuseRead(LW_FUSE_STATUS_OPT_HALF_LTC);
    if (FLD_TEST_DRF(_FUSE, _STATUS_OPT_HALF_LTC, _DATA, _ENABLE, reg))
    {
        maxNumLtcsPerFbp >>= 1;
    }

    maxNumLtcsPerFbp *= REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_SLICES_PER_LTC, _VALUE);

    // Compute the number of enabled LTC slices.
    maxNumFbp      = REG_FLD_RD_DRF(BAR0, _PTOP, _SCAL_NUM_FBPS, _VALUE);
    fbpEnabledMask = (LWBIT32(maxNumFbp) - 1) & (~fuseRead(LW_FUSE_STATUS_OPT_FBP));
    NUMSETBITS_32(fbpEnabledMask);
    *pNumLtcs      = fbpEnabledMask * maxNumLtcsPerFbp;

fuseNumLtcsGet_TU10X_exit:
    return status;
}
