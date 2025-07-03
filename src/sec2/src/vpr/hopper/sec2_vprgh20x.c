/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprgh20x.c
 * @brief  VPR HAL Functions for GH20X
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "dev_fb.h"
/* ------------------------- System Includes -------------------------------- */
#include "csb.h"
#include "flcnretval.h"
#include "sec2_bar0_hs.h"
#include "dev_sec_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objvpr.h"
#include "config/g_vpr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief vprEnableVprInMmu_GH20X: 
 *  Enable VPR in MMU
 *
 * @param [in] bEnableVpr   Updated value of the IN_USE bit of LW_PFB_PRI_MMU_VPR_MODE
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprEnableVprInMmu_GH20X
(
    LwBool bEnableVpr
)
{
    LwU32 vprMode = 0;
    FLCN_STATUS flcnStatus = FLCN_ERROR;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, &vprMode));
    vprMode = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_MODE, _IN_USE, bEnableVpr, vprMode);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, vprMode));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief vprIsVprEnableInMmu_GH20X: 
 *  Checks whether VPR is enable in MMU
 *
 * @return FLCN_ERR_VPR_APP_VPR_IS_ALREADY_ENABLED if vpr is already enabled
 *         else FLCN_OK
 */
FLCN_STATUS
vprIsVprEnableInMmu_GH20X
(
    void
)
{
    LwU32 vprMode = 0;
    FLCN_STATUS flcnStatus = FLCN_ERROR;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, &vprMode));
    if (FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_MODE, _IN_USE, _TRUE, vprMode))
    {
        flcnStatus = FLCN_ERR_VPR_APP_VPR_IS_ALREADY_ENABLED;
    }
ErrorExit:
        return flcnStatus;

}
