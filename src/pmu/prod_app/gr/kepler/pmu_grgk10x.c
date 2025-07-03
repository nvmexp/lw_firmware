/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgk10x.c
 * @brief  HAL-interface for the GK10X Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_bar0.h"

#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_fbp.h"

#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Activate GR PRIV access Error Handling
 *
 * @param[in] bActivate    True: Enable the error for priv access,
 *                         False: Disable the error for priv access
 */
void
grPgPrivErrHandlingActivate_GM10X
(
    LwBool bActivate
)
{
    LwU32 sysPriBlockLo = 0;
    LwU32 gpcPriBlockLo = 0;
    LwU32 fbpPriBlockLo = 0;
    LwU32 gpcPriBlockHi = 0;

    sysPriBlockLo = Gr.privBlock.sysLo;
    gpcPriBlockLo = Gr.privBlock.gpcLo;
    gpcPriBlockHi = Gr.privBlock.gpcHi;
    fbpPriBlockLo = Gr.privBlock.fbpLo;

    //
    // LW_PPRIV_SYS_PRIV_FS_CONFIG is 64 bit bus - index 0 is for lower 32 bits.
    //
    if (bActivate)
    {
        sysPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0)) & ~sysPriBlockLo;
        gpcPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0)) & ~gpcPriBlockLo;
        gpcPriBlockHi =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1)) & ~gpcPriBlockHi;
        fbpPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0)) & ~fbpPriBlockLo;
    }
    else
    {
        sysPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0)) | sysPriBlockLo;
        gpcPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0)) | gpcPriBlockLo;
        gpcPriBlockHi =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1)) | gpcPriBlockHi;
        fbpPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0)) | fbpPriBlockLo;
    }

    // gate PRIV access
    REG_WR32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0), sysPriBlockLo);
    REG_WR32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0), gpcPriBlockLo);
    REG_WR32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1), gpcPriBlockHi);
    REG_WR32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0), fbpPriBlockLo);
}
