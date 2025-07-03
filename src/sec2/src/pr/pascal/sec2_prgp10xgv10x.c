/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_prgp10xgv10x.c
 * @brief  Contains all PlayReady routines specific to SEC2 GP10X and GV10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objpr.h"
#include "dev_lwdec_pri.h"

#include "config/g_pr_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */

FLCN_STATUS
prIsLwdecInLsMode_GP10X
(
    LwBool *pbInLsMode
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32 data32           = 0;

    if(!pbInLsMode)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    *pbInLsMode = LW_FALSE;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PLWDEC_FALCON_SCTL, &data32));
    *pbInLsMode = FLD_TEST_DRF(_PLWDEC, _FALCON_SCTL, _LSMODE, _TRUE, data32);

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

