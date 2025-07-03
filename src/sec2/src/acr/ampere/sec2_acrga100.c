/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_acrga10x.c
 * @brief  ACR interfaces for AMPERE and later chips.
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_bar0.h"
#include "sec2_csb.h"
#include "sec2_hs.h"

#include "dev_smcarb.h"
#include "dev_smcarb_addendum.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objacr.h"
#include "config/g_acr_private.h"
#include "config/g_sec2_hal.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

FLCN_STATUS
acrIsSmcActiveHs_GA100
(
    LwBool *pIsSmcActive
)
{
    FLCN_STATUS   flcnStatus = FLCN_OK;
    LwU32         data       = 0;

    *pIsSmcActive = LW_FALSE;
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PSMCARB_SYS_PIPE_INFO, &data));

    if (FLD_TEST_DRF(_PSMCARB, _SYS_PIPE_INFO, _MODE, _SMC, data))
        *pIsSmcActive = LW_TRUE;

ErrorExit:
    return flcnStatus;    
}
